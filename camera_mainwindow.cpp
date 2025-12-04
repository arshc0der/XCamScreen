#include "camera_mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
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
#include <QCameraDevice>
#include <QAudioDevice>

CameraMainWindow::CameraMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QWidget *container = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(container);

    // Mode selector
    QHBoxLayout *modeLayout = new QHBoxLayout();
    QLabel *lblMode = new QLabel("Mode:", this);
    comboMode = new QComboBox(this);
    comboMode->addItem("Camera", 0);
    comboMode->addItem("Screen", 1);
    modeLayout->addWidget(lblMode);
    modeLayout->addWidget(comboMode);
    modeLayout->addStretch();
    mainLayout->addLayout(modeLayout);

    // Video preview widget
    videoWidget = new QVideoWidget(this);
    videoWidget->setMinimumSize(760, 340);
    videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(videoWidget);

    // Device selection layout
    QHBoxLayout *deviceLayout = new QHBoxLayout();

    comboCamera = new QComboBox(this);
    comboMic    = new QComboBox(this);
    comboScreen = new QComboBox(this);

    deviceLayout->addWidget(new QLabel("Camera:", this));
    deviceLayout->addWidget(comboCamera);

    deviceLayout->addWidget(new QLabel("Microphone:", this));
    deviceLayout->addWidget(comboMic);

    deviceLayout->addWidget(new QLabel("Screen:", this));
    deviceLayout->addWidget(comboScreen);

    mainLayout->addLayout(deviceLayout);

    // Settings layout
    QHBoxLayout *settings = new QHBoxLayout();

    comboResolution = new QComboBox(this);
    comboResolution->addItem("640×480", QSize(640, 480));
    comboResolution->addItem("1280×720 (HD)", QSize(1280, 720));
    comboResolution->addItem("1920×1080 (Full HD)", QSize(1920, 1080));
    comboResolution->addItem("2560×1440 (QHD)", QSize(2560, 1440));
    comboResolution->addItem("3840×2160 (4K)", QSize(3840, 2160)); // 4K
    settings->addWidget(new QLabel("Resolution:", this));
    settings->addWidget(comboResolution);

    comboBitrate = new QComboBox(this);
    comboBitrate->addItem("4 Mbps",  4000000);
    comboBitrate->addItem("6 Mbps",  6000000);
    comboBitrate->addItem("8 Mbps",  8000000);
    comboBitrate->addItem("10 Mbps", 10000000);
    comboBitrate->addItem("15 Mbps", 15000000);
    comboBitrate->addItem("20 Mbps (High)", 20000000);
    settings->addWidget(new QLabel("Bitrate:", this));
    settings->addWidget(comboBitrate);

    comboFps = new QComboBox(this);
    comboFps->addItem("24 fps (Cinema)", 24);
    comboFps->addItem("30 fps", 30);
    comboFps->addItem("60 fps (Smooth)", 60);
    settings->addWidget(new QLabel("FPS:", this));
    settings->addWidget(comboFps);

    checkAudio = new QCheckBox("Record Audio", this);
    checkAudio->setChecked(true);
    settings->addWidget(checkAudio);

    checkHwAccel = new QCheckBox("Hardware acceleration / Low CPU mode (if available)", this);
    checkHwAccel->setChecked(true);
    settings->addWidget(checkHwAccel);

    mainLayout->addLayout(settings);

    // Preview performance option
    checkPreviewWhileRecording = new QCheckBox("Show live preview while recording (more CPU)", this);
    checkPreviewWhileRecording->setChecked(true);
    mainLayout->addWidget(checkPreviewWhileRecording);

    // Recording timer
    QHBoxLayout *timerLayout = new QHBoxLayout();
    QLabel *timerTitle = new QLabel("Recording Time:", this);
    labelTimer = new QLabel("00:00", this);
    labelTimer->setStyleSheet("font-size: 18px; font-weight: bold; color: red;");
    labelTimer->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    timerLayout->addWidget(timerTitle);
    timerLayout->addWidget(labelTimer);
    timerLayout->addStretch();
    mainLayout->addLayout(timerLayout);
    labelTimer->hide(); // Hidden when not recording

    // Buttons
    QHBoxLayout *buttons = new QHBoxLayout();
    btnPhoto  = new QPushButton("📸 Capture Photo", this);
    btnRecord = new QPushButton("⏺ Start Recording", this);
    btnStop   = new QPushButton("⏹ Stop Recording", this);
    btnStop->setEnabled(false);

    buttons->addWidget(btnPhoto);
    buttons->addWidget(btnRecord);
    buttons->addWidget(btnStop);
    mainLayout->addLayout(buttons);

    container->setLayout(mainLayout);
    setCentralWidget(container);

    // Create recorder and attach session
    mediaRecorder = new QMediaRecorder(this);
    captureSession.setRecorder(mediaRecorder);

    // Timer for recording time
    recordTimer = new QTimer(this);
    recordTimer->setInterval(1000);
    connect(recordTimer, &QTimer::timeout, this, &CameraMainWindow::updateRecordTime);

    // Device selection change
    connect(comboMode,   QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,        &CameraMainWindow::onModeChanged);
    connect(comboCamera, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,        &CameraMainWindow::onCameraSelectionChanged);
    connect(comboMic,    QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,        &CameraMainWindow::onMicSelectionChanged);
    connect(comboScreen, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,        &CameraMainWindow::onScreenSelectionChanged);

    // Buttons
    connect(btnPhoto,  &QPushButton::clicked, this, &CameraMainWindow::capturePhoto);
    connect(btnRecord, &QPushButton::clicked, this, &CameraMainWindow::startRecording);
    connect(btnStop,   &QPushButton::clicked, this, &CameraMainWindow::stopRecording);

    connect(checkAudio, &QCheckBox::toggled, this, [this](bool) {
        setupAudioFromSelection();
    });

    // Better defaults for quality but still OK for older PCs
    comboResolution->setCurrentIndex(1); // 1280x720
    comboBitrate->setCurrentIndex(2);    // 8 Mbps
    comboFps->setCurrentIndex(1);        // 30 fps

    createTrayIcon();
    refreshDevices();

    // Start in camera mode by default
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

void CameraMainWindow::createTrayIcon()
{
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    trayIcon->setToolTip("Pro Capture");

    trayMenu = new QMenu(this);

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
    connect(actionStart, &QAction::triggered, this, &CameraMainWindow::startRecording);
    connect(actionStop,  &QAction::triggered, this, &CameraMainWindow::stopRecording);
    connect(actionQuit,  &QAction::triggered, qApp, &QApplication::quit);
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

void CameraMainWindow::refreshDevices()
{
    // Cameras
    cameraDevices = QMediaDevices::videoInputs();
    comboCamera->clear();
    for (int i = 0; i < cameraDevices.size(); ++i) {
        comboCamera->addItem(cameraDevices.at(i).description());
    }

    // Microphones
    audioDevices = QMediaDevices::audioInputs();
    comboMic->clear();
    for (int i = 0; i < audioDevices.size(); ++i) {
        comboMic->addItem(audioDevices.at(i).description());
    }

    // Screens
    screenList = QGuiApplication::screens();
    comboScreen->clear();
    for (int i = 0; i < screenList.size(); ++i) {
        QScreen *s = screenList.at(i);
        QString label = QString("Screen %1 (%2x%3)")
                            .arg(i + 1)
                            .arg(s->size().width())
                            .arg(s->size().height());
        comboScreen->addItem(label);
    }

    if (comboCamera->count() == 0)
        comboCamera->addItem("No camera found");

    if (comboMic->count() == 0)
        comboMic->addItem("No microphone found");

    if (comboScreen->count() == 0)
        comboScreen->addItem("No screen found");
}

void CameraMainWindow::onModeChanged(int idx)
{
    if (isRecording) {
        QMessageBox::information(this, "Recording in progress",
                                 "Stop recording before changing mode.");
        // revert combo selection to previous mode
        int currentMode = captureSession.camera() ? 0 : 1;
        comboMode->blockSignals(true);
        comboMode->setCurrentIndex(currentMode);
        comboMode->blockSignals(false);
        return;
    }

    int mode = comboMode->itemData(idx).toInt();
    if (mode == 0) {
        setupCameraMode();
    } else {
        setupScreenMode();
    }
}

void CameraMainWindow::onCameraSelectionChanged(int index)
{
    if (isRecording)
        return; // do not change while recording

    int mode = comboMode->currentData().toInt();
    if (mode == 0) { // camera mode
        setupCameraMode();
    }
}

void CameraMainWindow::onMicSelectionChanged(int)
{
    if (isRecording)
        return;
    setupAudioFromSelection();
}

void CameraMainWindow::onScreenSelectionChanged(int index)
{
    Q_UNUSED(index)
    if (isRecording)
        return;

    int mode = comboMode->currentData().toInt();
    if (mode == 1) { // screen mode
        setupScreenMode();
    }
}

void CameraMainWindow::setupAudioFromSelection()
{
    if (audioInput) {
        captureSession.setAudioInput(nullptr);
        delete audioInput;
        audioInput = nullptr;
    }

    if (!checkAudio->isChecked())
        return;

    int idx = comboMic->currentIndex();
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

    int camIdx = comboCamera->currentIndex();
    if (camIdx < 0 || camIdx >= cameraDevices.size()) {
        QMessageBox::warning(this, "Error", "No camera devices available");
        btnPhoto->setEnabled(false);
        return;
    }

    camera = new QCamera(cameraDevices.at(camIdx), this);
    imageCapture = new QImageCapture(this);

    // Bind first → start later
    captureSession.setCamera(camera);
    captureSession.setImageCapture(imageCapture);
    captureSession.setScreenCapture(nullptr);

    // full working preview
    captureSession.setVideoOutput(videoWidget);
    videoWidget->setAspectRatioMode(Qt::KeepAspectRatio);
    videoWidget->show();

    setupAudioFromSelection();

    camera->start();      // MUST be after binding
    videoWidget->update();
    btnPhoto->setEnabled(true);
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

    int screenIdx = comboScreen->currentIndex();
    if (screenIdx < 0 || screenIdx >= screenList.size()) {
        QMessageBox::warning(this, "Error", "No screens available to capture");
        btnPhoto->setEnabled(false);
        return;
    }

    screenCapture = new QScreenCapture(this);
    screenCapture->setScreen(screenList.at(screenIdx));

    captureSession.setScreenCapture(screenCapture);
    captureSession.setVideoOutput(videoWidget);
    captureSession.setCamera(nullptr);
    captureSession.setImageCapture(nullptr);

    setupAudioFromSelection();

    screenCapture->start();
    btnPhoto->setEnabled(false); // cannot capture still photo from screen
}

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

void CameraMainWindow::startRecording()
{
    if (!mediaRecorder || isRecording)
        return;

    int mode = comboMode->currentData().toInt();

    // Configure media format and quality
    QSize res = comboResolution->currentData().toSize();
    int bitrate = comboBitrate->currentData().toInt();
    int fps     = comboFps->currentData().toInt();

    QMediaFormat format;
    format.setFileFormat(QMediaFormat::MPEG4);
    format.setVideoCodec(QMediaFormat::VideoCodec::H264);
    if (checkAudio->isChecked()) {
        format.setAudioCodec(QMediaFormat::AudioCodec::AAC);
    }

    mediaRecorder->setMediaFormat(format);
    mediaRecorder->setVideoResolution(res.width(), res.height());
    mediaRecorder->setVideoBitRate(bitrate);
    mediaRecorder->setVideoFrameRate(fps);

    if (checkAudio->isChecked()) {
        mediaRecorder->setAudioBitRate(96000); // 96 kbps, good quality but not heavy
    }

    // Encoding mode tuned for old PCs
    if (checkHwAccel->isChecked()) {
        mediaRecorder->setEncodingMode(QMediaRecorder::ConstantBitRateEncoding);
        mediaRecorder->setQuality(QMediaRecorder::HighQuality);
    } else {
        mediaRecorder->setEncodingMode(QMediaRecorder::ConstantQualityEncoding);
        mediaRecorder->setQuality(QMediaRecorder::NormalQuality);
    }

    // Optional performance tweak: if screen mode + low CPU, lower resolution a bit
    if (mode == 1 && checkHwAccel->isChecked()) {
        // If user selected very high resolution, we keep it but encoding is CBRE.
        // (You can clamp res here if you want even more performance)
    }

    QString prefix = (mode == 0) ? "camera" : "screen";
    QString outFile = buildOutputFileName(prefix, "mp4");
    mediaRecorder->setOutputLocation(QUrl::fromLocalFile(outFile));

    // Optionally disable preview while recording (less GPU/CPU)
    if (!checkPreviewWhileRecording->isChecked()) {
        captureSession.setVideoOutput(nullptr);
    }

    mediaRecorder->record();

    elapsedSeconds = 0;
    labelTimer->setText("00:00");
    labelTimer->show();
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
    labelTimer->hide();

    // Reattach preview if it was disabled during recording
    if (!checkPreviewWhileRecording->isChecked()) {
        int mode = comboMode->currentData().toInt();
        if (mode == 0) {
            captureSession.setVideoOutput(videoWidget);
        } else {
            captureSession.setVideoOutput(videoWidget);
        }
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

    labelTimer->setText(timeStr);

    if (trayIcon) {
        trayIcon->setToolTip("Recording " + timeStr);
    }
}

void CameraMainWindow::setUiRecordingState(bool recording)
{
    btnRecord->setEnabled(!recording);
    btnStop->setEnabled(recording);

    comboMode->setEnabled(!recording);
    comboResolution->setEnabled(!recording);
    comboBitrate->setEnabled(!recording);
    comboFps->setEnabled(!recording);
    checkAudio->setEnabled(!recording);
    checkHwAccel->setEnabled(!recording);
    checkPreviewWhileRecording->setEnabled(!recording);

    comboCamera->setEnabled(!recording);
    comboMic->setEnabled(!recording);
    comboScreen->setEnabled(!recording);

    if (recording) {
        btnRecord->setText("⏺ Recording...");
    } else {
        btnRecord->setText("⏺ Start Recording");
    }

    if (actionStart && actionStop) {
        actionStart->setEnabled(!recording);
        actionStop->setEnabled(recording);
    }
}
