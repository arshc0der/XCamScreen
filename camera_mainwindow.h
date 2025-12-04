// ======================= camera_mainwindow.h =======================
#ifndef CAMERA_MAINWINDOW_H
#define CAMERA_MAINWINDOW_H

#include <QMainWindow>
#include <QMediaCaptureSession>
#include <QMediaRecorder>
#include <QCamera>
#include <QScreenCapture>
#include <QImageCapture>
#include <QAudioInput>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QList>
#include <QCameraDevice>
#include <QAudioDevice>
#include <QScreen>
#include <QMenu>
#include <QVideoWidget>
#include "pip_float_window.h"

class QCloseEvent;
class QResizeEvent;

namespace Ui {
class CameraMainWindow;
}

class CameraMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit CameraMainWindow(QWidget *parent = nullptr);
    ~CameraMainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);

    void onModeChanged(int idx);
    void onCameraSelectionChanged(int index);
    void onMicSelectionChanged(int index);
    void onScreenSelectionChanged(int index);

    void capturePhoto();
    void startRecording();
    void stopRecording();
    void updateRecordTime();

private:
    void createTrayIcon();
    void refreshDevices();

    void setupAudioFromSelection();
    void setupCameraMode();
    void setupScreenMode();
    void setupCamScreenMode();          // Cam + Screen (PiP)

    void updatePipOverlayGeometry();    // Embedded PiP overlay
    QString buildOutputFileName(const QString &prefix, const QString &ext);
    void setUiRecordingState(bool recording);
    void showFloatingPip();

private:
    Ui::CameraMainWindow *ui = nullptr;

    // Main recording session
    QMediaRecorder       *mediaRecorder      = nullptr;
    QCamera              *camera             = nullptr;
    QScreenCapture       *screenCapture      = nullptr;
    QImageCapture        *imageCapture       = nullptr;
    QAudioInput          *audioInput         = nullptr;
    QMediaCaptureSession  captureSession;

    // Separate session for PiP camera preview
    QMediaCaptureSession  pipSession;
    QVideoWidget         *pipVideoWidget     = nullptr;   // embedded PiP
    PipFloatWindow       *pipFloatWindow     = nullptr;   // floating PiP


    QTimer *recordTimer    = nullptr;
    int     elapsedSeconds = 0;
    bool    isRecording    = false;

    QSystemTrayIcon *trayIcon    = nullptr;
    QMenu           *trayMenu    = nullptr;
    QAction         *actionShow  = nullptr;
    QAction         *actionStart = nullptr;
    QAction         *actionStop  = nullptr;
    QAction         *actionQuit  = nullptr;

    QList<QCameraDevice> cameraDevices;
    QList<QAudioDevice>  audioDevices;
    QList<QScreen*>      screenList;


};

#endif // CAMERA_MAINWINDOW_H
