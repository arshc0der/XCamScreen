#ifndef CAMERA_MAINWINDOW_H
#define CAMERA_MAINWINDOW_H

#include <QMainWindow>
#include <QCamera>
#include <QScreenCapture>
#include <QMediaCaptureSession>
#include <QImageCapture>
#include <QMediaRecorder>
#include <QAudioInput>
#include <QVideoWidget>
#include <QTimer>
#include <QSystemTrayIcon>

#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QList>

class QMenu;
class QAction;
class QCloseEvent;
class QScreen;
class QCameraDevice;
class QAudioDevice;

class CameraMainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit CameraMainWindow(QWidget *parent = nullptr);
    ~CameraMainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void capturePhoto();
    void startRecording();
    void stopRecording();
    void onModeChanged(int index);
    void onCameraSelectionChanged(int index);
    void onMicSelectionChanged(int index);
    void onScreenSelectionChanged(int index);
    void updateRecordTime();
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void setupCameraMode();
    void setupScreenMode();
    void setupAudioFromSelection();
    void refreshDevices();

    QString buildOutputFileName(const QString &prefix, const QString &ext);
    void setUiRecordingState(bool recording);
    void createTrayIcon();

    // Capture session and recorder
    QMediaCaptureSession captureSession;
    QMediaRecorder *mediaRecorder = nullptr;

    // Sources
    QCamera *camera = nullptr;
    QImageCapture *imageCapture = nullptr;
    QScreenCapture *screenCapture = nullptr;
    QAudioInput *audioInput = nullptr;

    // Device lists
    QList<QCameraDevice> cameraDevices;
    QList<QAudioDevice> audioDevices;
    QList<QScreen*> screenList;

    // Recording timer / state
    QTimer *recordTimer = nullptr;
    QLabel *labelTimer = nullptr;
    int elapsedSeconds = 0;
    bool isRecording = false;

    // System tray
    QSystemTrayIcon *trayIcon = nullptr;
    QMenu *trayMenu = nullptr;
    QAction *actionShow = nullptr;
    QAction *actionStart = nullptr;
    QAction *actionStop = nullptr;
    QAction *actionQuit = nullptr;

    // UI Controls
    QComboBox *comboMode = nullptr;       // Camera / Screen
    QVideoWidget *videoWidget = nullptr;

    QComboBox *comboCamera = nullptr;     // camera device
    QComboBox *comboMic = nullptr;        // microphone device
    QComboBox *comboScreen = nullptr;     // which screen to capture

    QComboBox *comboResolution = nullptr;
    QComboBox *comboBitrate = nullptr;
    QComboBox *comboFps = nullptr;

    QCheckBox *checkAudio = nullptr;
    QCheckBox *checkHwAccel = nullptr;
    QCheckBox *checkPreviewWhileRecording = nullptr;

    QPushButton *btnPhoto = nullptr;
    QPushButton *btnRecord = nullptr;
    QPushButton *btnStop = nullptr;
};

#endif // CAMERA_MAINWINDOW_H
