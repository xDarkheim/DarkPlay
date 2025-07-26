#include "ui/UIStateManager.h"
#include <QWidget>
#include <QApplication>
#include <QCursor>

namespace DarkPlay::UI {

UIStateManager::UIStateManager(QWidget* mainWindow, QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
    if (!m_mainWindow) {
        throw std::invalid_argument("MainWindow cannot be null");
    }

    setupTimers();
}

void UIStateManager::setupTimers()
{
    // Timer for auto-hiding controls in fullscreen
    m_controlsHideTimer = std::make_unique<QTimer>(this);
    m_controlsHideTimer->setSingleShot(true);
    m_controlsHideTimer->setInterval(CONTROLS_HIDE_TIMEOUT_MS);
    connect(m_controlsHideTimer.get(), &QTimer::timeout,
            this, &UIStateManager::onControlsHideTimeout);

    // Timer for debouncing mouse move events
    m_mouseMoveDebounceTimer = std::make_unique<QTimer>(this);
    m_mouseMoveDebounceTimer->setSingleShot(true);
    m_mouseMoveDebounceTimer->setInterval(MOUSE_MOVE_DEBOUNCE_MS);
    connect(m_mouseMoveDebounceTimer.get(), &QTimer::timeout,
            this, &UIStateManager::onMouseMoveDebounce);
}

void UIStateManager::toggleFullScreen()
{
    if (m_isFullScreen.load()) {
        exitFullScreen();
    } else {
        enterFullScreen();
    }
}

void UIStateManager::enterFullScreen()
{
    if (!m_mainWindow || m_isFullScreen.load()) {
        return;
    }

    m_isFullScreen.store(true);
    m_mainWindow->showFullScreen();

    // Start auto-hide timer for controls in fullscreen
    if (m_hasMedia.load()) {
        resetControlsHideTimer();
    }

    emit fullScreenToggled(true);
}

void UIStateManager::exitFullScreen()
{
    if (!m_mainWindow || !m_isFullScreen.load()) {
        return;
    }

    m_isFullScreen.store(false);
    m_mainWindow->showNormal();

    // Always show controls when exiting fullscreen
    showControls();
    m_controlsHideTimer->stop();

    emit fullScreenToggled(false);
}

void UIStateManager::showControls()
{
    if (m_controlsVisible.load()) {
        return;
    }

    m_controlsVisible.store(true);
    updateCursorVisibility();
    emit controlsVisibilityChanged(true);
}

void UIStateManager::hideControls()
{
    if (!m_controlsVisible.load() || !m_isFullScreen.load() || !m_hasMedia.load()) {
        return;
    }

    m_controlsVisible.store(false);
    updateCursorVisibility();
    emit controlsVisibilityChanged(false);
}

void UIStateManager::resetControlsHideTimer()
{
    if (!m_isFullScreen.load() || !m_hasMedia.load()) {
        return;
    }

    showControls();
    m_controlsHideTimer->start();
}

void UIStateManager::onMouseActivity()
{
    if (m_isFullScreen.load()) {
        // Debounce mouse move events to avoid excessive UI updates
        m_mouseMoveDebounceTimer->start();
    }
}

void UIStateManager::onMouseLeave()
{
    // Immediately hide controls when mouse leaves window in fullscreen
    if (m_isFullScreen.load() && m_hasMedia.load()) {
        hideControls();
    }
}

void UIStateManager::onMediaStateChanged(bool hasMedia)
{
    m_hasMedia.store(hasMedia);

    if (!hasMedia) {
        // Always show controls when no media is loaded
        showControls();
        m_controlsHideTimer->stop();
    } else if (m_isFullScreen.load()) {
        // Start auto-hide timer when media is loaded in fullscreen
        resetControlsHideTimer();
    }
}

void UIStateManager::onControlsHideTimeout()
{
    hideControls();
}

void UIStateManager::onMouseMoveDebounce()
{
    if (m_isFullScreen.load() && m_hasMedia.load()) {
        resetControlsHideTimer();
    }
}

void UIStateManager::updateCursorVisibility()
{
    if (!m_mainWindow) {
        return;
    }

    const bool shouldHideCursor = m_isFullScreen.load() &&
                                  !m_controlsVisible.load() &&
                                  m_hasMedia.load();

    if (shouldHideCursor != m_cursorHidden.load()) {
        m_cursorHidden.store(shouldHideCursor);

        if (shouldHideCursor) {
            m_mainWindow->setCursor(Qt::BlankCursor);
        } else {
            m_mainWindow->setCursor(Qt::ArrowCursor);
        }

        emit cursorVisibilityChanged(!shouldHideCursor);
    }
}

} // namespace DarkPlay::UI
