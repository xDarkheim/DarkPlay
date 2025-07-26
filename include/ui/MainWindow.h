#ifndef DARKPLAY_UI_MAINWINDOW_H
#define DARKPLAY_UI_MAINWINDOW_H

#include <QMainWindow>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QVideoWidget>
#include <QProgressBar>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QStringList>
#include <QPointer>
#include <memory>
#include <atomic>
#include <mutex>

// Include necessary headers for Media types
#include "media/IMediaEngine.h"

// Forward declarations
namespace DarkPlay {
namespace Core { class Application; }
namespace Controllers { class MediaController; }
namespace UI { class ClickableSlider; }
}

namespace DarkPlay::UI {

/**
 * @brief Modern, extensible main window with improved RAII and memory management
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Delete copy and move operations for safety
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;
    MainWindow(MainWindow&&) = delete;
    MainWindow& operator=(MainWindow&&) = delete;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    // File operations
    void openFile();
    void openRecentFile();
    void clearRecentFiles();

    // Playback controls
    void togglePlayPause();
    void stopPlayback();
    void previousTrack();
    void nextTrack();

    // Media controller signal handlers
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onStateChanged(Media::PlaybackState state);
    void onErrorOccurred(const QString& error);

    // UI controls
    void onVolumeChanged(int value);
    void updateTimeLabels();

    // Settings and dialogs
    void showPreferences();
    void showPluginManager();

    // Theme handling
    void onThemeChanged(const QString& themeName);

private:
    // UI setup methods
    void setupUI();
    void setupVideoWidget();
    void setupMediaControls();
    void setupMenuBar();
    void setupStatusBar();
    void connectSignals();

    // Video widget optimization methods
    void optimizeVideoWidgetRendering();
    void connectVideoOutput();

    // Settings management
    void loadSettings();
    void saveSettings();

    // Helper methods
    void updatePlayPauseButton();
    void updateRecentFilesMenu();
    void addToRecentFiles(const QString& filePath);
    void setAdaptiveLayout();
    void toggleFullScreen();
    void createFullScreenOverlay();
    void updateOverlayPosition();
    void resetControlsHideTimer();
    // Unified fullscreen UI management
    void showFullScreenUI();
    void hideFullScreenUI();
    [[nodiscard]] static QString formatTime(qint64 milliseconds);


private:
    // Core application reference
    Core::Application* m_app;

    // Media controller - критический компонент, используем умный указатель
    std::unique_ptr<Controllers::MediaController> m_mediaController;

    // UI Components - используем Qt parent-child систему для автоматической очистки
    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    QVideoWidget* m_videoWidget;
    QWidget* m_controlsWidget;

    // Layout components managed by Qt
    QVBoxLayout* m_controlsMainLayout;
    QHBoxLayout* m_controlsLayout;
    QHBoxLayout* m_progressLayout;
    QHBoxLayout* m_controlButtonsLayout;

    // Control widgets
    QPushButton* m_playPauseButton;
    QPushButton* m_previousButton;
    QPushButton* m_nextButton;
    QPushButton* m_openFileButton;

    // Custom slider - используем умный указатель для безопасности
    std::unique_ptr<ClickableSlider> m_positionSlider;

    // Labels and controls
    QLabel* m_currentTimeLabel;
    QLabel* m_totalTimeLabel;
    QSlider* m_volumeSlider;
    QLabel* m_volumeLabel;
    QProgressBar* m_loadingProgressBar;

    // Menu components with proper lifetime management
    QPointer<QMenu> m_recentFilesMenu;
    QPointer<QAction> m_clearRecentAction;

    // State management
    QStringList m_recentFiles;
    bool m_isSeekingByUser;
    bool m_isFullScreen;
    bool m_controlsVisible;
    bool m_cursorHidden; // Track cursor state to avoid redundant setCursor calls

    // ULTIMATE CRASH PROTECTION: Add flags and mutex for safe slider updates
    std::atomic<bool> m_sliderUpdatesEnabled{true};
    std::atomic<bool> m_isDestructing{false};
    mutable std::recursive_mutex m_sliderMutex;

    // CRITICAL FIX: Use QPointer for safe widget access during destruction
    QPointer<QWidget> m_fullScreenControlsOverlay;
    QPointer<QLabel> m_fullScreenCurrentTimeLabel;
    QPointer<QLabel> m_fullScreenTotalTimeLabel;
    QPointer<QPushButton> m_fullScreenPlayPauseButton;
    QPointer<ClickableSlider> m_fullScreenProgressSlider;
    QPointer<QSlider> m_fullScreenVolumeSlider;

    std::unique_ptr<QTimer> m_updateTimer;
    std::unique_ptr<QTimer> m_controlsHideTimer;
    std::unique_ptr<QTimer> m_mouseMoveDebounceTimer; // Timer to prevent too frequent UI updates

    // Constants
    static constexpr int UPDATE_INTERVAL_MS = 100;
    static constexpr int MAX_RECENT_FILES = 10;
    static constexpr int CONTROLS_HIDE_TIMEOUT_MS = 3000; // 3 seconds
    static constexpr int MOUSE_MOVE_DEBOUNCE_MS = 100; // Debounce mouse move events
};

} // namespace DarkPlay::UI

#endif // DARKPLAY_UI_MAINWINDOW_H
