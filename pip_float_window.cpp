// ======================= pip_float_window.cpp =======================
#include "pip_float_window.h"

#include <QVideoWidget>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QRegion>

PipFloatWindow::PipFloatWindow(QWidget *parent)
    : QWidget(parent)
{
    //setWindowFlags(Qt::FramelessWindowHint |
   //                Qt::WindowStaysOnTopHint |
   //                Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);

    m_videoWidget = new QVideoWidget(this);
    m_videoWidget->setAspectRatioMode(Qt::KeepAspectRatio);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_videoWidget);

    resize(260, 180);
}

QVideoWidget *PipFloatWindow::videoWidget() const
{
    return m_videoWidget;
}

void PipFloatWindow::setCircleShape(bool enabled)
{
    m_circle = enabled;
    if (m_circle) {
        setMask(QRegion(rect(), QRegion::Ellipse));
    } else {
        clearMask();
    }
}

void PipFloatWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragOffset = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

void PipFloatWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPos() - m_dragOffset);
        event->accept();
    }
}

void PipFloatWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (m_circle) {
        setMask(QRegion(rect(), QRegion::Ellipse));
    } else {
        clearMask();
    }
}
