// ======================= pip_float_window.h =======================
#ifndef PIP_FLOAT_WINDOW_H
#define PIP_FLOAT_WINDOW_H

#include <QWidget>
#include <QPoint>

class QVideoWidget;
class QMouseEvent;
class QResizeEvent;

class PipFloatWindow : public QWidget
{
    Q_OBJECT
public:
    explicit PipFloatWindow(QWidget *parent = nullptr);

    QVideoWidget *videoWidget() const;
    void setCircleShape(bool enabled);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QVideoWidget *m_videoWidget = nullptr;
    QPoint        m_dragOffset;
    bool          m_circle = false;
};

#endif // PIP_FLOAT_WINDOW_H
