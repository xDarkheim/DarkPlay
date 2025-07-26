#ifndef DARKPLAY_UI_CLICKABLESLIDER_H
#define DARKPLAY_UI_CLICKABLESLIDER_H

#include <QSlider>
#include <QMouseEvent>

namespace DarkPlay::UI {

/**
 * @brief A custom slider that allows clicking anywhere on the track to jump to that position
 */
class ClickableSlider : public QSlider
{
    Q_OBJECT

public:
    explicit ClickableSlider(Qt::Orientation orientation, QWidget *parent = nullptr);
    ~ClickableSlider() override = default;

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    int calculateValueFromPosition(int position) const;
};

} // namespace DarkPlay::UI

#endif // DARKPLAY_UI_CLICKABLESLIDER_H
