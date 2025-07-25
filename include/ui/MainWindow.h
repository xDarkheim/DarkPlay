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

// Include necessary headers for Media types
#include "media/IMediaEngine.h"

// Forward declarations
namespace DarkPlay {
namespace Core { class Application; }
namespace Controllers { class MediaController; }
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
    void onStateChanged(Media::IMediaEngine::State state);
    void onMediaStatusChanged(Media::IMediaEngine::MediaStatus status);
    void onErrorOccurred(const QString& error);

    // UI controls
    void onVolumeChanged(int value);
    void onSeekPositionChanged(int value);
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

    // Settings management
    void loadSettings();
    void saveSettings();

    // Helper methods
    void updatePlayPauseButton();
    void updateRecentFilesMenu();
    void addToRecentFiles(const QString& filePath);
    void setAdaptiveLayout();
    [[nodiscard]] static QString formatTime(qint64 milliseconds);

    // Core application reference
    Core::Application* m_app;

    // Media controller
    std::unique_ptr<Controllers::MediaController> m_mediaController;

    // UI Components - managed by Qt parent-child system
    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    QVideoWidget* m_videoWidget;

    // Controls
    QWidget* m_controlsWidget;
    QVBoxLayout* m_controlsMainLayout;
    QHBoxLayout* m_controlsLayout;
    QHBoxLayout* m_progressLayout;
    QHBoxLayout* m_controlButtonsLayout;

    // Control elements
    QPushButton* m_playPauseButton;
    QPushButton* m_stopButton;
    QPushButton* m_previousButton;
    QPushButton* m_nextButton;
    QPushButton* m_openFileButton;

    QSlider* m_positionSlider;
    QLabel* m_currentTimeLabel;
    QLabel* m_totalTimeLabel;

    QSlider* m_volumeSlider;
    QLabel* m_volumeLabel;

    QProgressBar* m_loadingProgressBar;

    // Menu components
    QMenu* m_recentFilesMenu;
    QAction* m_clearRecentAction;

    // State management
    QStringList m_recentFiles;
    bool m_isSeekingByUser;
    std::unique_ptr<QTimer> m_updateTimer;

    // Constants
    static constexpr int UPDATE_INTERVAL_MS = 100;
    static constexpr int MAX_RECENT_FILES = 10;
};

} // namespace DarkPlay::UI

#endif // DARKPLAY_UI_MAINWINDOW_H
