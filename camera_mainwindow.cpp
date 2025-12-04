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
#include <QStyle>
#include <QApplication>
#include <QCloseEvent>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTimer>
#include <QResizeEvent>
#include <QRegion>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QVideoWidget>

// -------------------------------------------------------------
// Constructor / Destructor
// -------------------------------------------------------------
CameraMainWindow::CameraMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::CameraMainWindow)
{
    ui->setupUi(this);

    // ---------- UI SETUP ----------

    // Mode selector: 0 = Camera, 1 = Screen, 2 = Cam + Screen (PiP)
    ui->mode_comboBox->clear();
    ui->mode_comboBox->addItem(QIcon(":/res/icons/camera.png"),"Camera", 0);
    ui->mode_comboBox->addItem(QIcon(":/res/icons/screen.png"),"Screen", 1);
    ui->mode_comboBox->addItem(QIcon(":/res/icons/camscreen.png"),"Cam + Screen (PiP)", 2);

    // Video widget
    ui->video_widget->setAspectRatioMode(Qt::KeepAspectRatio);

    // Resolution
    ui->resolution_comboBox->clear();
    ui->resolution_comboBox->addItem("640×480", QSize(640, 480));
    ui->resolution_comboBox->addItem("1280×720 (HD)", QSize(1280, 720));
    ui->resolution_comboBox->addItem("1920×1080 (Full HD)", QSize(1920, 1080));
    ui->resolution_comboBox->addItem("2560×1440 (QHD)", QSize(2560, 1440));
    ui->resolution_comboBox->addItem("3840×2160 (4K)", QSize(3840, 2160));

    // Bitrate
    ui->bitrate_comboBox->clear();
    ui->bitrate_comboBox->addItem("4 Mbps",  4'000'000);
    ui->bitrate_comboBox->addItem("6 Mbps",  6'000'000);
    ui->bitrate_comboBox->addItem("8 Mbps",  8'000'000);
    ui->bitrate_comboBox->addItem("10 Mbps", 10'000'000);
    ui->bitrate_comboBox->addItem("15 Mbps", 15'000'000);
    ui->bitrate_comboBox->addItem("20 Mbps (High)", 20'000'000);

    // FPS
    ui->fps_comboBox->clear();
    ui->fps_comboBox->addItem("24 fps (Cinema)", 24);
    ui->fps_comboBox->addItem("30 fps", 30);
    ui->fps_comboBox->addItem("60 fps (Smooth)", 60);

    // Defaults
    ui->resolution_comboBox->setCurrentIndex(1);    // 1280x720
    ui->bitrate_comboBox->setCurrentIndex(2);       // 8 Mbps
    ui->fps_comboBox->setCurrentIndex(1);           // 30 fps
    ui->record_audio_checkBox->setChecked(true);
    ui->hardware_acceleration_checkBox->setChecked(true);
    ui->show_live_preview_checkBox->setChecked(true);

    ui->record_timer_label->hide();

    // ---------- PiP controls ----------
    if (ui->pip_position_comboBox) {
        ui->pip_position_comboBox->clear();
        ui->pip_position_comboBox->addItem("Bottom Right");
        ui->pip_position_comboBox->addItem("Bottom Left");
        ui->pip_position_comboBox->addItem("Top Right");
        ui->pip_position_comboBox->addItem("Top Left");
        ui->pip_position_comboBox->setCurrentIndex(0);
    }
    if (ui->pip_size_slider) {
        ui->pip_size_slider->setMinimum(15);   // 15% of video size
        ui->pip_size_slider->setMaximum(50);   // up to 50%
        ui->pip_size_slider->setValue(25);     // default 25%
    }
    if (ui->pip_show_checkBox) {
        ui->pip_show_checkBox->setChecked(true);
    }
    if (ui->pip_shape_comboBox) {
        ui->pip_shape_comboBox->clear();
        ui->pip_shape_comboBox->addItem("Square");
        ui->pip_shape_comboBox->addItem("Circle");
        ui->pip_shape_comboBox->setCurrentIndex(0);
    }
    if (ui->pip_floating_checkBox) {
        ui->pip_floating_checkBox->setChecked(false); // default: embedded PiP
    }

    // Create PiP video widget inside pip_overlay_widget (embedded PiP)
    pipVideoWidget = new QVideoWidget(ui->pip_overlay_widget);
    pipVideoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    pipVideoWidget->setAspectRatioMode(Qt::KeepAspectRatio);
    pipVideoWidget->setGeometry(ui->pip_overlay_widget->rect());
    pipVideoWidget->show();
    ui->pip_overlay_widget->hide(); // hidden unless CamScreen mode + enabled

    // ---------- MEDIA SETUP ----------
    mediaRecorder = new QMediaRecorder(this);
    captureSession.setRecorder(mediaRecorder);

    recordTimer = new QTimer(this);
    recordTimer->setInterval(1000);
    connect(recordTimer, &QTimer::timeout, this, &CameraMainWindow::updateRecordTime);

    // ---------- CONNECTIONS ----------

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

    connect(ui->capture_photo_pushButton, &QPushButton::clicked,
            this, &CameraMainWindow::capturePhoto);

    connect(ui->start_record_pushButton, &QPushButton::clicked,
            this, &CameraMainWindow::startRecording);

    connect(ui->stop_record_pushButton, &QPushButton::clicked,
            this, &CameraMainWindow::stopRecording);

    connect(ui->record_audio_checkBox, &QCheckBox::toggled,
            this, [this](bool){ setupAudioFromSelection(); });

    // PiP controls -> embedded overlay
    connect(ui->pip_show_checkBox, &QCheckBox::toggled,
            this, [this](bool){ updatePipOverlayGeometry(); });

    connect(ui->pip_size_slider, &QSlider::valueChanged,
            this, [this](int){ updatePipOverlayGeometry(); });

    connect(ui->pip_position_comboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){ updatePipOverlayGeometry(); });

    connect(ui->pip_shape_comboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){ updatePipOverlayGeometry(); });

    connect(ui->pip_floating_checkBox, &QCheckBox::toggled,
            this, [this](bool enabled){
                int mode = ui->mode_comboBox->currentData().toInt();
                bool pipChecked = ui->pip_show_checkBox->isChecked();

                // Floating PiP only works in CamScreen mode and PiP enabled
                if (!(mode == 2 && pipChecked)) {
                    if (pipFloatWindow) pipFloatWindow->hide();
                    ui->pip_overlay_widget->show();
                    return;
                }

                if (enabled) {
                    // Switch to floating PiP
                    ui->pip_overlay_widget->hide();
                    showFloatingPip();
                } else {
                    // Switch back to embedded PiP
                    if (pipFloatWindow) pipFloatWindow->hide();
                    pipSession.setVideoOutput(pipVideoWidget);
                    updatePipOverlayGeometry();
                }
            });

    // Tray + devices + initial mode
    createTrayIcon();
    refreshDevices();

    // After devices are loaded, switch to camera mode
    QTimer::singleShot(80, this, [this]{
        ui->mode_comboBox->setCurrentIndex(0);
        onModeChanged(0);
    });

    setWindowTitle("XCamScreen (Camera / Screen Recorder)");
    resize(1100, 700);
}

CameraMainWindow::~CameraMainWindow()
{
    if (pipFloatWindow) {
        pipFloatWindow->close();
        delete pipFloatWindow;
        pipFloatWindow = nullptr;
    }
    delete ui;
}

// -------------------------------------------------------------
// Resize: keep PiP in corner
// -------------------------------------------------------------
void CameraMainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updatePipOverlayGeometry();
}

// -------------------------------------------------------------
// Tray Icon
// -------------------------------------------------------------
void CameraMainWindow::createTrayIcon()
{
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));

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

    connect(actionShow, &QAction::triggered, this, [this]{
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
        if (isHidden()) showNormal();
        else hide();
    }
}

// -------------------------------------------------------------
// Close Event – ask user, protect recording
// -------------------------------------------------------------
void CameraMainWindow::closeEvent(QCloseEvent *event)
{
    if (isRecording) {
        auto r = QMessageBox::question(
            this, "Recording in progress",
            "Recording is still running.\nStop recording and exit?",
            QMessageBox::Yes | QMessageBox::No
            );
        if (r == QMessageBox::No) {
            event->ignore();
            return;
        }
        stopRecording();
    }

    auto r = QMessageBox::question(
        this, "Exit",
        "Do you want to exit the application?",
        QMessageBox::Yes | QMessageBox::No
        );
    (r == QMessageBox::Yes) ? event->accept() : event->ignore();
}

// -------------------------------------------------------------
// Device refresh
// -------------------------------------------------------------
void CameraMainWindow::refreshDevices()
{
    cameraDevices = QMediaDevices::videoInputs();
    audioDevices  = QMediaDevices::audioInputs();
    screenList    = QGuiApplication::screens();

    // Cameras
    ui->camera_comboBox->clear();
    for (const auto &dev : cameraDevices)
        ui->camera_comboBox->addItem(dev.description());
    if (cameraDevices.isEmpty())
        ui->camera_comboBox->addItem("No camera found");

    // Microphones
    ui->mic_comboBox->clear();
    for (const auto &dev : audioDevices)
        ui->mic_comboBox->addItem(dev.description());
    if (audioDevices.isEmpty())
        ui->mic_comboBox->addItem("No microphone found");

    // Screens
    ui->screen_comboBox->clear();
    for (int i = 0; i < screenList.size(); ++i) {
        QScreen *s = screenList.at(i);
        ui->screen_comboBox->addItem(
            QString("Screen %1 (%2x%3)")
                .arg(i + 1)
                .arg(s->size().width())
                .arg(s->size().height())
            );
    }
    if (screenList.isEmpty())
        ui->screen_comboBox->addItem("No screen found");
}

// -------------------------------------------------------------
// Mode & selection handlers
// -------------------------------------------------------------
void CameraMainWindow::onModeChanged(int idx)
{
    if (isRecording)
        return; // don’t change mode while recording

    int mode = ui->mode_comboBox->itemData(idx).toInt(); // 0 = camera, 1 = screen, 2 = CamScreen
    switch (mode) {
    case 0: setupCameraMode();    break;
    case 1: setupScreenMode();    break;
    case 2: setupCamScreenMode(); break;
    default: setupCameraMode();   break;
    }
}

void CameraMainWindow::onCameraSelectionChanged(int)
{
    if (isRecording) return;

    int mode = ui->mode_comboBox->currentData().toInt();
    if      (mode == 0) setupCameraMode();
    else if (mode == 2) setupCamScreenMode();
}

void CameraMainWindow::onMicSelectionChanged(int)
{
    if (!isRecording)
        setupAudioFromSelection();
}

void CameraMainWindow::onScreenSelectionChanged(int)
{
    if (isRecording) return;

    int mode = ui->mode_comboBox->currentData().toInt();
    if      (mode == 1) setupScreenMode();
    else if (mode == 2) setupCamScreenMode();
}

// -------------------------------------------------------------
// Audio – Qt 6.10 safe (no special APIs)
// -------------------------------------------------------------
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
    audioInput->setVolume(0.55);     // small reduction to avoid clipping

    captureSession.setAudioInput(audioInput);
}

// -------------------------------------------------------------
// Camera / Screen setup
// -------------------------------------------------------------
void CameraMainWindow::setupCameraMode()
{
    // Stop PiP floating window if any
    if (pipFloatWindow) {
        pipFloatWindow->close();
        delete pipFloatWindow;
        pipFloatWindow = nullptr;
    }

    // Stop PiP session
    pipSession.setCamera(nullptr);
    pipSession.setVideoOutput(nullptr);
    if (pipVideoWidget)
        ui->pip_overlay_widget->hide();

    // remove screen capture if any
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

    int idx = ui->camera_comboBox->currentIndex();
    if (idx < 0 || idx >= cameraDevices.size()) {
        ui->capture_photo_pushButton->setEnabled(false);
        return;
    }

    camera = new QCamera(cameraDevices.at(idx), this);
    imageCapture = new QImageCapture(this);

    captureSession.setCamera(camera);
    captureSession.setImageCapture(imageCapture);
    captureSession.setVideoOutput(ui->video_widget);

    setupAudioFromSelection();

    camera->start();
    ui->capture_photo_pushButton->setEnabled(true);
}

void CameraMainWindow::setupScreenMode()
{
    // Stop PiP floating window
    if (pipFloatWindow) {
        pipFloatWindow->close();
        delete pipFloatWindow;
        pipFloatWindow = nullptr;
    }

    // Stop PiP session
    pipSession.setCamera(nullptr);
    pipSession.setVideoOutput(nullptr);
    if (pipVideoWidget)
        ui->pip_overlay_widget->hide();

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

    int idx = ui->screen_comboBox->currentIndex();
    if (idx < 0 || idx >= screenList.size()) {
        ui->capture_photo_pushButton->setEnabled(false);
        return;
    }

    screenCapture = new QScreenCapture(this);
    screenCapture->setScreen(screenList.at(idx));

    captureSession.setScreenCapture(screenCapture);
    captureSession.setVideoOutput(ui->video_widget);

    setupAudioFromSelection();

    screenCapture->start();
    ui->capture_photo_pushButton->setEnabled(false); // can't take still photo from screen
}

// -------------------------------------------------------------
// CamScreen mode: Screen recording + camera PiP (preview only)
// -------------------------------------------------------------
void CameraMainWindow::setupCamScreenMode()
{
    // Stop PiP floating if any
    if (pipFloatWindow) {
        pipFloatWindow->close();
        delete pipFloatWindow;
        pipFloatWindow = nullptr;
    }

    // Clear previous main session
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

    // Main video = Screen
    int screenIdx = ui->screen_comboBox->currentIndex();
    if (screenIdx < 0 || screenIdx >= screenList.size()) {
        ui->capture_photo_pushButton->setEnabled(false);
        return;
    }

    screenCapture = new QScreenCapture(this);
    screenCapture->setScreen(screenList.at(screenIdx));

    captureSession.setScreenCapture(screenCapture);
    captureSession.setVideoOutput(ui->video_widget);
    setupAudioFromSelection();
    screenCapture->start();

    ui->capture_photo_pushButton->setEnabled(false); // still photo only in Camera mode

    // Separate camera session → PiP overlay (preview)
    int camIdx = ui->camera_comboBox->currentIndex();
    if (camIdx < 0 || camIdx >= cameraDevices.size()) {
        ui->pip_overlay_widget->hide();
        pipSession.setCamera(nullptr);
        pipSession.setVideoOutput(nullptr);
        return;
    }

    if (camera) {
        camera->stop();
        delete camera;
        camera = nullptr;
    }
    camera = new QCamera(cameraDevices.at(camIdx), this);

    pipSession.setCamera(camera);

    // embedded vs floating decided by checkbox
    if (ui->pip_floating_checkBox && ui->pip_floating_checkBox->isChecked()) {
        ui->pip_overlay_widget->hide();
        showFloatingPip();
    } else {
        pipSession.setVideoOutput(pipVideoWidget);
        updatePipOverlayGeometry();
    }

    camera->start();
}

// -------------------------------------------------------------
// PiP overlay geometry (embedded preview)
// -------------------------------------------------------------
void CameraMainWindow::updatePipOverlayGeometry()
{
    if (!ui->pip_overlay_widget || !pipVideoWidget)
        return;

    int mode = ui->mode_comboBox->currentData().toInt();

    bool pipChecked     = ui->pip_show_checkBox && ui->pip_show_checkBox->isChecked();
    bool floatingChecked = ui->pip_floating_checkBox && ui->pip_floating_checkBox->isChecked();

    // Embedded overlay is only visible in CamScreen mode, PiP ON, and floating OFF
    bool showEmbedded = (mode == 2) && pipChecked && !floatingChecked;

    if (!showEmbedded) {
        ui->pip_overlay_widget->hide();
        ui->pip_overlay_widget->clearMask();
        return;
    }

    // Place overlay inside video_widget area
    QRect area = ui->video_widget->geometry();
    if (area.width() <= 0 || area.height() <= 0) {
        ui->pip_overlay_widget->hide();
        return;
    }

    int percent = ui->pip_size_slider ? ui->pip_size_slider->value() : 25;
    if (percent < 10) percent = 10;
    if (percent > 80) percent = 80;

    int w = area.width() * percent / 100;
    int h = area.height() * percent / 100;

    // Keep 16:9 for camera overlay
    if (w * 9 > h * 16)
        h = w * 9 / 16;
    else
        w = h * 16 / 9;

    int posIdx = ui->pip_position_comboBox ? ui->pip_position_comboBox->currentIndex() : 0;

    int x = 0, y = 0;
    const int margin = 10;

    switch (posIdx) {
    case 0: // Bottom Right
        x = area.right() - w - margin;
        y = area.bottom() - h - margin;
        break;
    case 1: // Bottom Left
        x = area.left() + margin;
        y = area.bottom() - h - margin;
        break;
    case 2: // Top Right
        x = area.right() - w - margin;
        y = area.top() + margin;
        break;
    case 3: // Top Left
        x = area.left() + margin;
        y = area.top() + margin;
        break;
    default:
        x = area.right() - w - margin;
        y = area.bottom() - h - margin;
        break;
    }

    QRect pipRect(x, y, w, h);
    ui->pip_overlay_widget->setGeometry(pipRect);
    ui->pip_overlay_widget->raise();
    ui->pip_overlay_widget->show();

    // make inner video fill the container
    pipVideoWidget->setGeometry(ui->pip_overlay_widget->rect());

    // Shape for embedded PiP
    bool circle = ui->pip_shape_comboBox &&
                  ui->pip_shape_comboBox->currentText().contains("circle", Qt::CaseInsensitive);

    if (circle) {
        ui->pip_overlay_widget->setMask(QRegion(ui->pip_overlay_widget->rect(), QRegion::Ellipse));
    } else {
        ui->pip_overlay_widget->clearMask();
    }
}

// -------------------------------------------------------------
// Helper: output filename
// -------------------------------------------------------------
QString CameraMainWindow::buildOutputFileName(const QString &prefix, const QString &ext)
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (base.isEmpty())
        base = QDir::homePath();

    QDir dir(base);
    if (!dir.exists("Captures"))
        dir.mkpath("Captures");

    return dir.filePath(
        QString("%1_%2.%3")
            .arg(prefix,
                 QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"),
                 ext)
        );
}

// -------------------------------------------------------------
// Photo capture (with flash animation)
// -------------------------------------------------------------
void CameraMainWindow::capturePhoto()
{
    if (!imageCapture)
        return;

    QString base = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (base.isEmpty())
        base = QDir::homePath();

    QDir dir(base);
    if (!dir.exists("Photos"))
        dir.mkpath("Photos");

    QString file = dir.filePath("photo_" +
                                QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") +
                                ".jpg");

    imageCapture->captureToFile(file);

    // flash effect
    auto *fx = new QGraphicsOpacityEffect(ui->video_widget);
    ui->video_widget->setGraphicsEffect(fx);
    auto *anim = new QPropertyAnimation(fx, "opacity");
    anim->setDuration(400);
    anim->setStartValue(1.0);
    anim->setKeyValueAt(0.3, 0.0);
    anim->setEndValue(1.0);
    anim->start(QPropertyAnimation::DeleteWhenStopped);

    QMessageBox::information(this, "📸 Photo Captured", "Saved to:\n" + file);
}

// -------------------------------------------------------------
// Recording
// -------------------------------------------------------------
void CameraMainWindow::startRecording()
{
    if (!mediaRecorder || isRecording)
        return;

    QSize res = ui->resolution_comboBox->currentData().toSize();
    int bitrate = ui->bitrate_comboBox->currentData().toInt();
    int fps     = ui->fps_comboBox->currentData().toInt();

    QMediaFormat fmt;
    fmt.setFileFormat(QMediaFormat::MPEG4);
    fmt.setVideoCodec(QMediaFormat::VideoCodec::H264);
    if (ui->record_audio_checkBox->isChecked())
        fmt.setAudioCodec(QMediaFormat::AudioCodec::AAC);

    mediaRecorder->setMediaFormat(fmt);
    mediaRecorder->setVideoResolution(res);
    mediaRecorder->setVideoBitRate(bitrate);
    mediaRecorder->setVideoFrameRate(fps);

    QString prefix = captureSession.camera() ? "camera" : "screen";
    QString file = buildOutputFileName(prefix, "mp4");
    mediaRecorder->setOutputLocation(QUrl::fromLocalFile(file));

    int mode = ui->mode_comboBox->currentData().toInt();
    bool camScreenMode = (mode == 2);

    bool pipChecked     = ui->pip_show_checkBox && ui->pip_show_checkBox->isChecked();
    bool floatingChecked = ui->pip_floating_checkBox && ui->pip_floating_checkBox->isChecked();

    if (camScreenMode && pipChecked && floatingChecked) {
        ui->pip_overlay_widget->hide();
        showFloatingPip();   // reuse same logic as preview
    } else if (camScreenMode && pipChecked) {
        pipSession.setVideoOutput(pipVideoWidget);
        updatePipOverlayGeometry();
    }

    if (!ui->show_live_preview_checkBox->isChecked())
        captureSession.setVideoOutput(nullptr);

    mediaRecorder->record();

    elapsedSeconds = 0;
    ui->record_timer_label->setText("00:00");
    ui->record_timer_label->show();
    recordTimer->start();
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

    // Close floating PiP and restore embedded mode
    if (pipFloatWindow) {
        pipSession.setVideoOutput(pipVideoWidget);
        updatePipOverlayGeometry();

        pipFloatWindow->close();
        delete pipFloatWindow;
        pipFloatWindow = nullptr;
    }

    if (!ui->show_live_preview_checkBox->isChecked())
        captureSession.setVideoOutput(ui->video_widget);

    setUiRecordingState(false);
    isRecording = false;

    QMessageBox::information(this, "🎬 Recording Saved",
                             "Saved to:\n" + mediaRecorder->actualLocation().toLocalFile());
}

void CameraMainWindow::updateRecordTime()
{
    ++elapsedSeconds;
    int mins = elapsedSeconds / 60;
    int secs = elapsedSeconds % 60;

    ui->record_timer_label->setText(
        QString("%1:%2").arg(mins, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'))
        );
}

// -------------------------------------------------------------
// UI state when recording
// -------------------------------------------------------------
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

    ui->start_record_pushButton->setText(recording ? "Recording..." : "Start Recording");

    if (actionStart && actionStop) {
        actionStart->setEnabled(!recording);
        actionStop->setEnabled(recording);
    }
}

// -------------------------------------------------------------
// Floating PiP window creation / update
// -------------------------------------------------------------
void CameraMainWindow::showFloatingPip()
{
    if (!pipFloatWindow) {
        pipFloatWindow = new PipFloatWindow();

        // Size based on slider
        int percent = ui->pip_size_slider ? ui->pip_size_slider->value() : 25;
        if (percent < 10) percent = 10;
        if (percent > 40) percent = 40;

        QScreen *screen = QGuiApplication::primaryScreen();
        QRect s = screen->geometry();
        int w = s.width() * percent / 100;
        int h = w * 9 / 16;
        pipFloatWindow->resize(w, h);

        // shape
        bool circle = ui->pip_shape_comboBox &&
                      ui->pip_shape_comboBox->currentText().contains("circle", Qt::CaseInsensitive);
        pipFloatWindow->setCircleShape(circle);
    }

    // Send camera preview to floating window
    pipSession.setVideoOutput(pipFloatWindow->videoWidget());
    pipFloatWindow->show();
    pipFloatWindow->raise();
}

