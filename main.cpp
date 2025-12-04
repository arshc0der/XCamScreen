#include "camera_mainwindow.h"

#include <QApplication>

//#include <QMediaDevices>
//#include <QMediaFormat>
//#include <QDebug>
//#include <QStringList>

int main(int argc, char *argv[])
{
    qputenv("QT_FFMPEG_ENCODING_HW_DEVICE_TYPES", "");
    QApplication a(argc, argv);
    CameraMainWindow w;
/*
    QMediaFormat mediaFormat;

    // Use ConversionMode::Encode to check what can be encoded
    QMediaFormat::ConversionMode mode = QMediaFormat::Encode;

    const auto videoCodecs = mediaFormat.supportedVideoCodecs(mode);
    qDebug() << "Supported video codecs for encoding:";
    for (auto codec : videoCodecs) {
        qDebug() << " - " << QMediaFormat::videoCodecName(codec);
    }

    const auto fileFormats = mediaFormat.supportedFileFormats(mode);
    qDebug() << "Supported container formats for encoding:";
    for (auto fmt : fileFormats) {
        qDebug() << " - " << QMediaFormat::fileFormatName(fmt);
    }
*/
    w.show();
    return a.exec();
}
