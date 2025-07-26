#include "ui/ClickableSlider.h"
#include <QStyle>
#include <QStyleOptionSlider>

namespace DarkPlay::UI {

ClickableSlider::ClickableSlider(Qt::Orientation orientation, QWidget *parent)
    : QSlider(orientation, parent)
{
}

void ClickableSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int value = calculateValueFromPosition(event->pos().x());
        setValue(value);
        emit sliderPressed();
        emit sliderMoved(value);
        emit valueChanged(value);
        event->accept();
    } else {
        QSlider::mousePressEvent(event);
    }
}

int ClickableSlider::calculateValueFromPosition(int position) const
{
    QStyleOptionSlider opt;
    initStyleOption(&opt);

    QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
    QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

    int sliderMin = groove.x();
    int sliderMax = groove.right() - handle.width() + 1;
    int sliderLength = sliderMax - sliderMin;

    // Clamp position to valid range
    position = qBound(sliderMin, position, sliderMax);

    // Calculate value based on position
    double ratio = static_cast<double>(position - sliderMin) / sliderLength;
    int value = minimum() + static_cast<int>(ratio * (maximum() - minimum()));

    return qBound(minimum(), value, maximum());
}

} // namespace DarkPlay::UI
