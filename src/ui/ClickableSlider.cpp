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
        // Safety check: ensure the widget is properly initialized
        if (width() <= 0 || height() <= 0 || minimum() >= maximum()) {
            QSlider::mousePressEvent(event);
            return;
        }

        int position = (orientation() == Qt::Horizontal) ? event->pos().x() : event->pos().y();
        int value = calculateValueFromPosition(position);

        // Additional safety check before setting value
        if (value >= minimum() && value <= maximum()) {
            setValue(value);
            emit sliderPressed();
            emit sliderMoved(value);
            emit valueChanged(value);
        }
        event->accept();
    } else {
        QSlider::mousePressEvent(event);
    }
}

int ClickableSlider::calculateValueFromPosition(int position) const
{
    // Safety checks
    if (minimum() >= maximum() || width() <= 0 || height() <= 0) {
        return minimum();
    }

    QStyleOptionSlider opt;
    initStyleOption(&opt);

    QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
    QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

    int sliderMin, sliderMax, sliderLength;

    if (orientation() == Qt::Horizontal) {
        sliderMin = groove.x();
        sliderMax = groove.right() - handle.width() + 1;
        sliderLength = sliderMax - sliderMin;
    } else {
        sliderMin = groove.y();
        sliderMax = groove.bottom() - handle.height() + 1;
        sliderLength = sliderMax - sliderMin;
    }

    // Additional safety check for geometry
    if (sliderLength <= 0) {
        return minimum();
    }

    // Clamp position to valid range
    position = qBound(sliderMin, position, sliderMax);

    // Calculate value based on position
    double ratio = static_cast<double>(position - sliderMin) / sliderLength;
    int value = minimum() + static_cast<int>(ratio * (maximum() - minimum()));

    return qBound(minimum(), value, maximum());
}

} // namespace DarkPlay::UI
