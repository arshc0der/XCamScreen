// ======================= camera_mainwindow.cpp =======================
#include "camera_mainwindow.h"
#include "ui_camera_mainwindow.h"

#include <QStandardPaths>
#include <QDateTime>
#include <QUrl>
#include <QMessageBox>
#include <QMediaDevices>
#include <QMediaFormat>
#include <QGuiApplication>
#include <QScreen>
#include <QDir>
#include <QMenu>
#include <QAction>
#include <QStyle>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QVideoWidget>

CameraMainWindow::CameraMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::CameraMainWindow)
{
    ui->setupUi(this);

    // --------- UI setup from .ui widgets ---------

    // Mode selector
    ui->mode_comboBox->clear();
    ui->mode_comboBox->addItem("Camera", 0);
    ui->mode_comboBox->addItem("Screen", 1);

    // Video widget
    ui->video_widget->setMinimumSize(760, 340);
    ui->video_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->video_widget->setAspectRatioMode(Qt::KeepAspectRatio);

    // Resolution combo
    ui->resolution_comboBox->clear();
    ui->resolution_comboBox->addItem("640×480", QSize(640, 480));
    ui->resolution_comboBox->addItem("1280×720 (HD)", QSize(1280, 720));
    ui->resolution_comboBox->addItem("1920×1080 (Full HD)", QSize(1920, 1080));
    ui->resolution_comboBox->addItem("2560×1440 (QHD)", QSize(2560, 1440));
    ui->resolution_comboBox->addItem("3840×2160 (4K)", QSize(3840, 2160));

    // Bitrate combo
    ui->bitrate_comboBox->clear();
    ui->bitrate_comboBox->addItem("4 Mbps",  4000000);
    ui->bitrate_comboBox->addItem("6 Mbps",  6000000);
    ui->bitrate_comboBox->addItem("8 Mbps",  8000000);
    ui->bitrate_comboBox->addItem("10 Mbps", 10000000);
    ui->bitrate_comboBox->addItem("15 Mbps", 15000000);
    ui->bitrate_comboBox->addItem("20 Mbps (High)", 20000000);

    // FPS combo
    ui->fps_comboBox->clear();
    ui->fps_comboBox->addItem("24 fps (Cinema)", 24);
    ui->fps_comboBox->addItem("30 fps", 30);
    ui->fps_comboBox->addItem("60 fps (Smooth)", 60);

    // Checkboxes defaults
    ui->record_audio_checkBox->setChecked(true);
    ui->hardware_acceleration_checkBox->setChecked(true);
    ui->show_live_preview_checkBox->setChecked(true);

    // Timer label
    ui->record_timer_label->setText("00:00");
    ui->record_timer_label->setStyleSheet("font-size: 18px; font-weight: bold; color: red;");
    ui->record_timer_label->hide();

    // --------- Media setup ---------

    mediaRecorder = new QMediaRecorder(this);
    captureSession.setRecorder(mediaRecorder);

    // Timer
    recordTimer = new QTimer(this);
    recordTimer->setInterval(1000);
    connect(recordTimer, &QTimer::timeout, this, &CameraMainWindow::updateRecordTime);

    // --------- Connections ---------

    connect(ui->mode_comboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraMainWindow::onModeChanged);

    connect(ui->camera_comboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraMainWindow::onCameraSelectionChanged);

    connect(ui->mic_comboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraMainWindow::onMicSelectionChanged);

    connect(ui->screen_comboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraMainWindow::onScreenSelectionChanged);

    connect(ui->capture_photo_pushButton,
            &QPushButton::clicked,
            this, &CameraMainWindow::capturePhoto);

    connect(ui->start_record_pushButton,
            &QPushButton::clicked,
            this, &CameraMainWindow::startRecording);

    connect(ui->stop_record_pushButton,
            &QPushButton::clicked,
            this, &CameraMainWindow::stopRecording);

    connect(ui->record_audio_checkBox,
            &QCheckBox::toggled,
            this, [this](bool) { setupAudioFromSelection(); });

    // Defaults
    ui->resolution_comboBox->setCurrentIndex(1); // 1280x720
    ui->bitrate_comboBox->setCurrentIndex(2);    // 8 Mbps
    ui->fps_comboBox->setCurrentIndex(1);        // 30 fps

    createTrayIcon();
    refreshDevices();

    // Start in camera mode
    onModeChanged(0);

    setWindowTitle("Pro Capture (Camera / Screen Recorder)");
    resize(1100, 700);
}

CameraMainWindow::~CameraMainWindow()
{
    if (recordTimer)
        recordTimer->stop();

    if (mediaRecorder)
        mediaRecorder->stop();

    if (camera)
        camera->stop();

    if (screenCapture)
        screenCapture->stop();

    delete ui;
}

void CameraMainWindow::closeEvent(QCloseEvent *event)
{
    // Minimize to tray instead of exiting, useful while recording
    if (trayIcon && trayIcon->isVisible()) {
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}

// ---------------------- Tray Icon ----------------------

void CameraMainWindow::createTrayIcon()
{
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    trayIcon->setToolTip("Pro Capture");

    trayMenu  = new QMenu(this);
    actionShow  = new QAction("Show Window", this);
    actionStart = new QAction("Start Recording", this);
    actionStop  = new QAction("Stop Recording", this);
    actionQuit  = new QAction("Quit", this);

    trayMenu->addAction(actionShow);
    trayMenu->addSeparator();
    trayMenu->addAction(actionStart);
    trayMenu->addAction(actionStop);
    trayMenu->addSeparator();
    trayMenu->addAction(actionQuit);

    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();

    connect(trayIcon, &QSystemTrayIcon::activated,
            this, &CameraMainWindow::trayIconActivated);

    connect(actionShow, &QAction::triggered, this, [this]() {
        showNormal();
        activateWindow();
    });

    connect(actionStart, &QAction::triggered,
            this, &CameraMainWindow::startRecording);

    connect(actionStop, &QAction::triggered,
            this, &CameraMainWindow::stopRecording);

    connect(actionQuit, &QAction::triggered,
            qApp, &QApplication::quit);
}

void CameraMainWindow::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        if (isHidden()) {
            showNormal();
            activateWindow();
        } else {
            hide();
        }
    }
}

// ---------------------- Devices ----------------------

void CameraMainWindow::refreshDevices()
{
    // Cameras
    cameraDevices = QMediaDevices::videoInputs();
    ui->camera_comboBox->clear();
    for (int i = 0; i < cameraDevices.size(); ++i) {
        ui->camera_comboBox->addItem(cameraDevices.at(i).description());
    }

    // Microphones
    audioDevices = QMediaDevices::audioInputs();
    ui->mic_comboBox->clear();
    for (int i = 0; i < audioDevices.size(); ++i) {
        ui->mic_comboBox->addItem(audioDevices.at(i).description());
    }

    // Screens
    screenList = QGuiApplication::screens();
    ui->screen_comboBox->clear();
    for (int i = 0; i < screenList.size(); ++i) {
        QScreen *s = screenList.at(i);
        QString label = QString("Screen %1 (%2x%3)")
                            .arg(i + 1)
                            .arg(s->size().width())
                            .arg(s->size().height());
        ui->screen_comboBox->addItem(label);
    }

    if (ui->camera_comboBox->count() == 0)
        ui->camera_comboBox->addItem("No camera found");
    if (ui->mic_comboBox->count() == 0)
        ui->mic_comboBox->addItem("No microphone found");
    if (ui->screen_comboBox->count() == 0)
        ui->screen_comboBox->addItem("No screen found");
}

// ---------------------- Mode & Selections ----------------------

void CameraMainWindow::onModeChanged(int idx)
{
    if (isRecording) {
        QMessageBox::information(this, "Recording in progress",
                                 "Stop recording before changing mode.");
        // revert combo selection to previous mode
        int currentMode = captureSession.camera() ? 0 : 1;
        ui->mode_comboBox->blockSignals(true);
        ui->mode_comboBox->setCurrentIndex(currentMode);
        ui->mode_comboBox->blockSignals(false);
        return;
    }

    int mode = ui->mode_comboBox->itemData(idx).toInt();
    if (mode == 0) {
        setupCameraMode();
    } else {
        setupScreenMode();
    }
}

void CameraMainWindow::onCameraSelectionChanged(int /*index*/)
{
    if (isRecording)
        return; // do not change while recording

    int mode = ui->mode_comboBox->currentData().toInt();
    if (mode == 0) { // camera mode
        setupCameraMode();
    }
}

void CameraMainWindow::onMicSelectionChanged(int /*index*/)
{
    if (isRecording)
        return;
    setupAudioFromSelection();
}

void CameraMainWindow::onScreenSelectionChanged(int /*index*/)
{
    if (isRecording)
        return;

    int mode = ui->mode_comboBox->currentData().toInt();
    if (mode == 1) { // screen mode
        setupScreenMode();
    }
}

// ---------------------- Audio / Video Setup ----------------------

void CameraMainWindow::setupAudioFromSelection()
{
    if (audioInput) {
        captureSession.setAudioInput(nullptr);
        delete audioInput;
        audioInput = nullptr;
    }

    if (!ui->record_audio_checkBox->isChecked())
        return;

    int idx = ui->mic_comboBox->currentIndex();
    if (idx < 0 || idx >= audioDevices.size())
        return;

    QAudioDevice dev = audioDevices.at(idx);
    audioInput = new QAudioInput(dev, this);

    // slightly lower gain to reduce noise / clipping
    audioInput->setVolume(0.6);
    captureSession.setAudioInput(audioInput);
}

void CameraMainWindow::setupCameraMode()
{
    // Remove previous things
    if (screenCapture) {
        screenCapture->stop();
        captureSession.setScreenCapture(nullptr);
        delete screenCapture;
        screenCapture = nullptr;
    }
    if (camera) {
        camera->stop();
        captureSession.setCamera(nullptr);
        delete camera;
        camera = nullptr;
    }
    if (imageCapture) {
        captureSession.setImageCapture(nullptr);
        delete imageCapture;
        imageCapture = nullptr;
    }

    int camIdx = ui->camera_comboBox->currentIndex();
    if (camIdx < 0 || camIdx >= cameraDevices.size()) {
        QMessageBox::warning(this, "Error", "No camera devices available");
        ui->capture_photo_pushButton->setEnabled(false);
        return;
    }

    camera = new QCamera(cameraDevices.at(camIdx), this);
    imageCapture = new QImageCapture(this);

    // Bind first → start later
    captureSession.setCamera(camera);
    captureSession.setImageCapture(imageCapture);
    captureSession.setScreenCapture(nullptr);

    // full working preview
    captureSession.setVideoOutput(ui->video_widget);
    ui->video_widget->show();

    setupAudioFromSelection();

    camera->start();      // MUST be after binding
    ui->video_widget->update();
    ui->capture_photo_pushButton->setEnabled(true);
}

void CameraMainWindow::setupScreenMode()
{
    // Clean up camera resources
    if (camera) {
        camera->stop();
        captureSession.setCamera(nullptr);
        delete camera;
        camera = nullptr;
    }
    if (imageCapture) {
        captureSession.setImageCapture(nullptr);
        delete imageCapture;
        imageCapture = nullptr;
    }

    if (screenCapture) {
        screenCapture->stop();
        captureSession.setScreenCapture(nullptr);
        delete screenCapture;
        screenCapture = nullptr;
    }

    int screenIdx = ui->screen_comboBox->currentIndex();
    if (screenIdx < 0 || screenIdx >= screenList.size()) {
        QMessageBox::warning(this, "Error", "No screens available to capture");
        ui->capture_photo_pushButton->setEnabled(false);
        return;
    }

    screenCapture = new QScreenCapture(this);
    screenCapture->setScreen(screenList.at(screenIdx));

    captureSession.setScreenCapture(screenCapture);
    captureSession.setVideoOutput(ui->video_widget);
    captureSession.setCamera(nullptr);
    captureSession.setImageCapture(nullptr);

    setupAudioFromSelection();

    screenCapture->start();
    ui->capture_photo_pushButton->setEnabled(false); // cannot capture still photo from screen
}

// ---------------------- File Helpers ----------------------

QString CameraMainWindow::buildOutputFileName(const QString &prefix, const QString &ext)
{
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (baseDir.isEmpty())
        baseDir = QDir::homePath();

    QDir dir(baseDir);
    if (!dir.exists("Captures"))
        dir.mkpath("Captures");

    QString filePath = dir.filePath(
        QString("%1_%2.%3")
            .arg(prefix,
                 QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"),
                 ext));
    return filePath;
}

// ---------------------- Photo Capture ----------------------

void CameraMainWindow::capturePhoto()
{
    if (!imageCapture)
        return;

    QString file = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (file.isEmpty())
        file = QDir::homePath();

    QDir dir(file);
    if (!dir.exists("Photos"))
        dir.mkpath("Photos");

    file = dir.filePath("photo_" +
                        QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") +
                        ".jpg");

    imageCapture->captureToFile(file);
    QMessageBox::information(this, "Photo Saved", "Saved to:\n" + file);
}

// ---------------------- Recording ----------------------

void CameraMainWindow::startRecording()
{
    if (!mediaRecorder || isRecording)
        return;

    int mode = ui->mode_comboBox->currentData().toInt();

    // Configure media format and quality
    QSize res = ui->resolution_comboBox->currentData().toSize();
    int bitrate = ui->bitrate_comboBox->currentData().toInt();
    int fps     = ui->fps_comboBox->currentData().toInt();

    QMediaFormat format;
    format.setFileFormat(QMediaFormat::MPEG4);
    format.setVideoCodec(QMediaFormat::VideoCodec::H264);
    if (ui->record_audio_checkBox->isChecked()) {
        format.setAudioCodec(QMediaFormat::AudioCodec::AAC);
    }

    mediaRecorder->setMediaFormat(format);
    mediaRecorder->setVideoResolution(res.width(), res.height());
    mediaRecorder->setVideoBitRate(bitrate);
    mediaRecorder->setVideoFrameRate(fps);

    if (ui->record_audio_checkBox->isChecked()) {
        mediaRecorder->setAudioBitRate(96000); // 96 kbps, good quality but not heavy
    }

    // Encoding mode tuned for old PCs
    if (ui->hardware_acceleration_checkBox->isChecked()) {
        mediaRecorder->setEncodingMode(QMediaRecorder::ConstantBitRateEncoding);
        mediaRecorder->setQuality(QMediaRecorder::HighQuality);
    } else {
        mediaRecorder->setEncodingMode(QMediaRecorder::ConstantQualityEncoding);
        mediaRecorder->setQuality(QMediaRecorder::NormalQuality);
    }

    QString prefix = (mode == 0) ? "camera" : "screen";
    QString outFile = buildOutputFileName(prefix, "mp4");
    mediaRecorder->setOutputLocation(QUrl::fromLocalFile(outFile));

    // Optional performance tweak: disable preview while recording
    if (!ui->show_live_preview_checkBox->isChecked()) {
        captureSession.setVideoOutput(nullptr);
    }

    mediaRecorder->record();

    elapsedSeconds = 0;
    ui->record_timer_label->setText("00:00");
    ui->record_timer_label->show();
    recordTimer->start();

    if (trayIcon) {
        trayIcon->setToolTip("Recording 00:00");
    }

    setUiRecordingState(true);
    isRecording = true;
}

void CameraMainWindow::stopRecording()
{
    if (!mediaRecorder || !isRecording)
        return;

    mediaRecorder->stop();

    recordTimer->stop();
    ui->record_timer_label->hide();

    // Reattach preview if it was disabled during recording
    if (!ui->show_live_preview_checkBox->isChecked()) {
        captureSession.setVideoOutput(ui->video_widget);
    }

    setUiRecordingState(false);
    isRecording = false;

    if (trayIcon) {
        trayIcon->setToolTip("Pro Capture");
    }

    QString file = mediaRecorder->actualLocation().toLocalFile();
    if (!file.isEmpty()) {
        QMessageBox::information(this, "Recording saved",
                                 "Saved to:\n" + file);
    }
}

void CameraMainWindow::updateRecordTime()
{
    ++elapsedSeconds;

    int mins = elapsedSeconds / 60;
    int secs = elapsedSeconds % 60;

    QString timeStr =
        QString("%1:%2")
            .arg(mins, 2, 10, QLatin1Char('0'))
            .arg(secs, 2, 10, QLatin1Char('0'));

    ui->record_timer_label->setText(timeStr);

    if (trayIcon) {
        trayIcon->setToolTip("Recording " + timeStr);
    }
}

void CameraMainWindow::setUiRecordingState(bool recording)
{
    ui->start_record_pushButton->setEnabled(!recording);
    ui->stop_record_pushButton->setEnabled(recording);

    ui->mode_comboBox->setEnabled(!recording);
    ui->resolution_comboBox->setEnabled(!recording);
    ui->bitrate_comboBox->setEnabled(!recording);
    ui->fps_comboBox->setEnabled(!recording);
    ui->record_audio_checkBox->setEnabled(!recording);
    ui->hardware_acceleration_checkBox->setEnabled(!recording);
    ui->show_live_preview_checkBox->setEnabled(!recording);

    ui->camera_comboBox->setEnabled(!recording);
    ui->mic_comboBox->setEnabled(!recording);
    ui->screen_comboBox->setEnabled(!recording);

    if (recording) {
        ui->start_record_pushButton->setText("⏺ Recording...");
    } else {
        ui->start_record_pushButton->setText("⏺ Start Recording");
    }

    if (actionStart && actionStop) {
        actionStart->setEnabled(!recording);
        actionStop->setEnabled(recording);
    }
}
