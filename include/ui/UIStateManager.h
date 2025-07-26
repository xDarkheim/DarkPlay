#ifndef DARKPLAY_UI_UISTATEMANAGER_H
#define DARKPLAY_UI_UISTATEMANAGER_H

#include <QObject>
#include <QTimer>
#include <QPointer>
#include <QWidget>
#include <atomic>
#include <memory>

namespace DarkPlay::UI {

/**
 * @brief Dedicated manager for UI state transitions and fullscreen behavior
 * Separates UI state logic from MainWindow for better maintainability
 */
class UIStateManager : public QObject
{
    Q_OBJECT

public:
    explicit UIStateManager(QWidget* mainWindow, QObject* parent = nullptr);
    ~UIStateManager() override = default;

    // Delete copy and move operations
    UIStateManager(const UIStateManager&) = delete;
    UIStateManager& operator=(const UIStateManager&) = delete;
    UIStateManager(UIStateManager&&) = delete;
    UIStateManager& operator=(UIStateManager&&) = delete;

    // State queries
    [[nodiscard]] bool isFullScreen() const noexcept { return m_isFullScreen.load(); }
    [[nodiscard]] bool areControlsVisible() const noexcept { return m_controlsVisible.load(); }
    [[nodiscard]] bool isCursorHidden() const noexcept { return m_cursorHidden.load(); }

    // State management
    void toggleFullScreen();
    void enterFullScreen();
    void exitFullScreen();
    void showControls();
    void hideControls();
    void resetControlsHideTimer();

    // Mouse activity tracking
    void onMouseActivity();
    void onMouseLeave();

public slots:
    void onMediaStateChanged(bool hasMedia);

signals:
    void fullScreenToggled(bool isFullScreen);
    void controlsVisibilityChanged(bool visible);
    void cursorVisibilityChanged(bool visible);

private slots:
    void onControlsHideTimeout();
    void onMouseMoveDebounce();

private:
    void setupTimers();
    void updateCursorVisibility();

    QPointer<QWidget> m_mainWindow;

    // State flags with atomic operations for thread safety
    std::atomic<bool> m_isFullScreen{false};
    std::atomic<bool> m_controlsVisible{true};
    std::atomic<bool> m_cursorHidden{false};
    std::atomic<bool> m_hasMedia{false};

    // Timers for UI behavior
    std::unique_ptr<QTimer> m_controlsHideTimer;
    std::unique_ptr<QTimer> m_mouseMoveDebounceTimer;

    // Constants
    static constexpr int CONTROLS_HIDE_TIMEOUT_MS = 3000;
    static constexpr int MOUSE_MOVE_DEBOUNCE_MS = 100;
};

} // namespace DarkPlay::UI

#endif // DARKPLAY_UI_UISTATEMANAGER_H
