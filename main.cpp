#include "camera_mainwindow.h"

#include <QApplication>
#include <QStyleHints>
#include <QPalette>

//#include <QMediaDevices>
//#include <QMediaFormat>
//#include <QDebug>
//#include <QStringList>
void applyDarkTheme()
{
    QPalette palette;

    palette.setColor(QPalette::Window, QColor("#121212"));
    palette.setColor(QPalette::WindowText, QColor("#E0E0E0"));
    palette.setColor(QPalette::Base, QColor("#1E1E1E"));
    palette.setColor(QPalette::AlternateBase, QColor("#121212"));
    palette.setColor(QPalette::Text, QColor("#E0E0E0"));
    palette.setColor(QPalette::PlaceholderText, QColor("#888888"));
    palette.setColor(QPalette::Button, QColor("#1E1E1E"));
    palette.setColor(QPalette::ButtonText, QColor("#E0E0E0"));
    palette.setColor(QPalette::Highlight, QColor("#3B6BE8"));
    palette.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));

    qApp->setPalette(palette);
}

int main(int argc, char *argv[])
{
    qputenv("QT_FFMPEG_ENCODING_HW_DEVICE_TYPES", "");
    QApplication a(argc, argv);

    if (a.styleHints()->colorScheme() == Qt::ColorScheme::Light)
        applyDarkTheme();
    else
        applyDarkTheme();

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
