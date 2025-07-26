#include "ui/MainWindow.h"
#include "ui/ClickableSlider.h"
#include "controllers/MediaController.h"
#include "core/Application.h"
#include "core/ConfigManager.h"
#include "core/PluginManager.h"
#include "core/ThemeManager.h"
#include "media/IMediaEngine.h"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QStandardPaths>
#include <QStatusBar>
#include <stdexcept>


namespace DarkPlay::UI {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_app(Core::Application::instance())
    , m_centralWidget(nullptr)
    , m_mainLayout(nullptr)
    , m_videoWidget(nullptr)
    , m_controlsWidget(nullptr)
    , m_controlsMainLayout(nullptr)
    , m_controlsLayout(nullptr)
    , m_progressLayout(nullptr)
    , m_controlButtonsLayout(nullptr)
    , m_playPauseButton(nullptr)
    , m_previousButton(nullptr)
    , m_nextButton(nullptr)
    , m_openFileButton(nullptr)
    , m_positionSlider(nullptr)
    , m_currentTimeLabel(nullptr)
    , m_totalTimeLabel(nullptr)
    , m_volumeSlider(nullptr)
    , m_volumeLabel(nullptr)
    , m_loadingProgressBar(nullptr)
    , m_recentFilesMenu(nullptr)
    , m_clearRecentAction(nullptr)
    , m_isSeekingByUser(false)
    , m_isFullScreen(false)
    , m_updateTimer(std::make_unique<QTimer>(this))
{
    // Check if application instance is available
    if (!m_app) {
        throw std::runtime_error("Application instance not available");
    }

    // Initialize with exception safety
    try {
        // Create media controller first as it's needed by UI setup
        m_mediaController = std::make_unique<Controllers::MediaController>(this);

        setupUI();
        setupMenuBar();
        setupStatusBar();
        connectSignals();
        loadSettings();

        // Start update timer
        m_updateTimer->start(UPDATE_INTERVAL_MS);
        
    } catch (const std::exception& e) {
        qCritical() << "Failed to initialize MainWindow:" << e.what();
        // Clean up any partially constructed state
        m_mediaController.reset();
        throw;
    }
}

MainWindow::~MainWindow()
{
    // Stop timer first to prevent callbacks during destruction
    if (m_updateTimer) {
        m_updateTimer->stop();
    }

    try {
        saveSettings();
    } catch (const std::exception& e) {
        qWarning() << "Error saving settings during destruction:" << e.what();
    }

    // MediaController cleanup is automatic due to unique_ptr
    // Qt parent-child system handles UI cleanup
}

void MainWindow::setupUI()
{
    // Create central widget - Qt will manage its lifetime
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    // Create main layout - owned by central widget
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);

    setupVideoWidget();
    setupMediaControls();
}

void MainWindow::setupVideoWidget()
{
    // Create video widget - owned by layout parent
    m_videoWidget = new QVideoWidget(this);
    m_videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_videoWidget->setMinimumSize(320, 240);

    // Delayed initialization to prevent startup flickering
    m_videoWidget->hide();

    // Apply comprehensive visual optimization
    optimizeVideoWidgetRendering();

    // Set up video output through media controller
    if (m_mediaController) {
        m_mediaController->setVideoSink(m_videoWidget->videoSink());
    }

    m_mainLayout->addWidget(m_videoWidget, 1); // Give it stretch factor

    // Show widget only after complete initialization
    QTimer::singleShot(100, this, [this]() {
        if (m_videoWidget) {
            m_videoWidget->show();
        }
    });
}

void MainWindow::optimizeVideoWidgetRendering()
{
    if (!m_videoWidget) {
        return;
    }

    // Core attributes to prevent flickering
    m_videoWidget->setAttribute(Qt::WA_OpaquePaintEvent, true);
    m_videoWidget->setAttribute(Qt::WA_NoSystemBackground, true);
    m_videoWidget->setAttribute(Qt::WA_PaintOnScreen, false);
    m_videoWidget->setAttribute(Qt::WA_DontCreateNativeAncestors, true);
    m_videoWidget->setAttribute(Qt::WA_NativeWindow, false);

    // Critical settings to eliminate lag
    m_videoWidget->setAttribute(Qt::WA_DeleteOnClose, false);
    m_videoWidget->setAttribute(Qt::WA_NoChildEventsForParent, true);
    m_videoWidget->setAttribute(Qt::WA_DontShowOnScreen, false);

    // Disable automatic background filling
    m_videoWidget->setAutoFillBackground(false);

    // Set a stable black background and remove borders
    m_videoWidget->setStyleSheet(
        "QVideoWidget { "
        "background-color: #000000; "
        "border: none; "
        "margin: 0px; "
        "padding: 0px; "
        "}"
    );

    // Additional stability attributes
    m_videoWidget->setAttribute(Qt::WA_StaticContents, true);
    m_videoWidget->setAttribute(Qt::WA_TranslucentBackground, false);
    m_videoWidget->setAttribute(Qt::WA_AlwaysShowToolTips, false);

    // Optimize mouse and focus event handling
    m_videoWidget->setMouseTracking(false);
    m_videoWidget->setFocusPolicy(Qt::NoFocus);

    // Force update and size
    m_videoWidget->setUpdatesEnabled(true);
    m_videoWidget->setVisible(true);

    // Set a fixed size policy for stability
    m_videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Force apply changes
    m_videoWidget->update();
    m_videoWidget->repaint();
}

void MainWindow::setupMediaControls()
{
    // Create controls widget
    m_controlsWidget = new QWidget(this);
    m_controlsWidget->setFixedHeight(120);
    m_mainLayout->addWidget(m_controlsWidget);

    // Main controls layout
    m_controlsMainLayout = new QVBoxLayout(m_controlsWidget);
    m_controlsMainLayout->setContentsMargins(5, 5, 5, 5);
    m_controlsMainLayout->setSpacing(5);

    // Progress layout
    m_progressLayout = new QHBoxLayout();
    m_controlsMainLayout->addLayout(m_progressLayout);

    // Time labels and position slider
    m_currentTimeLabel = new QLabel("00:00", this);
    m_currentTimeLabel->setMinimumWidth(50);
    m_currentTimeLabel->setAlignment(Qt::AlignCenter);

    m_positionSlider = new ClickableSlider(Qt::Horizontal, this);
    m_positionSlider->setRange(0, 0);
    m_positionSlider->setValue(0);

    // Enable clicking on the slider track for instant seeking
    m_positionSlider->setTracking(true);
    m_positionSlider->setPageStep(5000); // 5 second jumps with page up/down
    m_positionSlider->setSingleStep(1000); // 1 second jumps with arrow keys

    // Make the slider more responsive to clicks
    m_positionSlider->setStyleSheet(
        "QSlider::groove:horizontal {"
        "    border: 1px solid #999999;"
        "    height: 8px;"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #B1B1B1, stop:1 #c4c4c4);"
        "    margin: 2px 0;"
        "    border-radius: 4px;"
        "}"
        "QSlider::handle:horizontal {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #b4b4b4, stop:1 #8f8f8f);"
        "    border: 1px solid #5c5c5c;"
        "    width: 14px;"
        "    margin: -2px 0;"
        "    border-radius: 7px;"
        "}"
        "QSlider::handle:horizontal:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #d4d4d4, stop:1 #afafaf);"
        "}"
        "QSlider::sub-page:horizontal {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #66e066, stop:1 #33cc33);"
        "    border: 1px solid #777;"
        "    height: 8px;"
        "    border-radius: 4px;"
        "}"
    );

    m_totalTimeLabel = new QLabel("00:00", this);
    m_totalTimeLabel->setMinimumWidth(50);
    m_totalTimeLabel->setAlignment(Qt::AlignCenter);

    m_progressLayout->addWidget(m_currentTimeLabel);
    m_progressLayout->addWidget(m_positionSlider, 1);
    m_progressLayout->addWidget(m_totalTimeLabel);

    // Control buttons layout
    m_controlButtonsLayout = new QHBoxLayout();
    m_controlsMainLayout->addLayout(m_controlButtonsLayout);

    // Create control buttons
    m_openFileButton = new QPushButton("Open", this);
    m_previousButton = new QPushButton("⏮", this);
    m_playPauseButton = new QPushButton("▶", this);
    m_nextButton = new QPushButton("⏭", this);

    // Set button properties
    const QSize buttonSize(40, 40);
    for (auto* button : {m_previousButton, m_playPauseButton, m_nextButton}) {
        button->setFixedSize(buttonSize);
    }

    // Volume controls
    m_volumeLabel = new QLabel("🔊", this);
    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(95); // Set to 95% for high volume by default
    m_volumeSlider->setMaximumWidth(100);

    // Loading progress bar
    m_loadingProgressBar = new QProgressBar(this);
    m_loadingProgressBar->setVisible(false);
    m_loadingProgressBar->setMaximumHeight(4);

    // Add widgets to layout
    m_controlButtonsLayout->addWidget(m_openFileButton);
    m_controlButtonsLayout->addStretch();
    m_controlButtonsLayout->addWidget(m_previousButton);
    m_controlButtonsLayout->addWidget(m_playPauseButton);
    m_controlButtonsLayout->addWidget(m_nextButton);
    m_controlButtonsLayout->addStretch();
    m_controlButtonsLayout->addWidget(m_volumeLabel);
    m_controlButtonsLayout->addWidget(m_volumeSlider);

    // Add loading progress bar
    m_controlsMainLayout->addWidget(m_loadingProgressBar);
}

void MainWindow::setupMenuBar()
{
    // File menu
    auto* fileMenu = menuBar()->addMenu("&File");

    auto* openAction = fileMenu->addAction("&Open File...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);

    fileMenu->addSeparator();

    // Recent files submenu
    m_recentFilesMenu = fileMenu->addMenu("Recent Files");
    updateRecentFilesMenu();

    fileMenu->addSeparator();

    auto* exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    // View menu
    auto* viewMenu = menuBar()->addMenu("&View");

    // Theme submenu
    auto* themeMenu = viewMenu->addMenu("&Theme");
    auto* themeGroup = new QActionGroup(this);

    QStringList themes = m_app->themeManager()->availableThemes();
    QString currentTheme = m_app->themeManager()->currentTheme();

    for (const QString& theme : themes) {
        auto* themeAction = themeMenu->addAction(theme);
        themeAction->setCheckable(true);
        themeAction->setChecked(theme == currentTheme);
        themeGroup->addAction(themeAction);

        connect(themeAction, &QAction::triggered, [this, theme]() {
            m_app->themeManager()->loadTheme(theme);
        });
    }

    // Tools menu
    auto* toolsMenu = menuBar()->addMenu("&Tools");

    auto* preferencesAction = toolsMenu->addAction("&Preferences...");
    preferencesAction->setShortcut(QKeySequence::Preferences);
    connect(preferencesAction, &QAction::triggered, this, &MainWindow::showPreferences);

    auto* pluginManagerAction = toolsMenu->addAction("&Plugin Manager...");
    connect(pluginManagerAction, &QAction::triggered, this, &MainWindow::showPluginManager);

    toolsMenu->addSeparator();

    // Auto-play toggle
    auto* autoPlayAction = toolsMenu->addAction("&Auto-play on file open");
    autoPlayAction->setCheckable(true);

    // Load current auto-play setting
    bool autoPlay = m_app->configManager()->getValue("playback/autoPlay", true).toBool();
    autoPlayAction->setChecked(autoPlay);

    connect(autoPlayAction, &QAction::triggered, [this](bool checked) {
        if (auto* configManager = m_app->configManager()) {
            configManager->setValue("playback/autoPlay", checked);
            QString message = checked ? "Auto-play enabled" : "Auto-play disabled";
            statusBar()->showMessage(message, 2000);
        }
    });

    // Help menu
    auto* helpMenu = menuBar()->addMenu("&Help");

    auto* aboutAction = helpMenu->addAction("&About");
    connect(aboutAction, &QAction::triggered, [this]() {
        QMessageBox::about(this, "About DarkPlay",
                          QString("DarkPlay Media Player v%1\n\n"
                                 "A modern, extensible media player built with Qt 6\n"
                                 "and modern C++ architecture.")
                          .arg(QApplication::applicationVersion()));
    });
}

void MainWindow::setupStatusBar()
{
    m_loadingProgressBar = new QProgressBar(this);
    m_loadingProgressBar->setVisible(false);
    m_loadingProgressBar->setMaximumHeight(16);
    statusBar()->addPermanentWidget(m_loadingProgressBar);

    statusBar()->showMessage("Ready - Open a media file to begin playback");
}

void MainWindow::connectSignals()
{
    if (!m_mediaController) {
        qWarning() << "MediaController not available for signal connections";
        return;
    }

    // Media controller signals
    connect(m_mediaController.get(), &Controllers::MediaController::positionChanged,
            this, &MainWindow::onPositionChanged);
    connect(m_mediaController.get(), &Controllers::MediaController::durationChanged,
            this, &MainWindow::onDurationChanged);
    connect(m_mediaController.get(), &Controllers::MediaController::stateChanged,
            this, &MainWindow::onStateChanged);
    connect(m_mediaController.get(), &Controllers::MediaController::errorOccurred,
            this, &MainWindow::onErrorOccurred);

    // UI control signals
    connect(m_openFileButton, &QPushButton::clicked, this, &MainWindow::openFile);
    connect(m_playPauseButton, &QPushButton::clicked, this, &MainWindow::togglePlayPause);
    connect(m_previousButton, &QPushButton::clicked, this, &MainWindow::previousTrack);
    connect(m_nextButton, &QPushButton::clicked, this, &MainWindow::nextTrack);

    // Slider signals - improved logic for seeking
    connect(m_positionSlider, &QSlider::sliderPressed, [this]() {
        m_isSeekingByUser = true;
    });

    connect(m_positionSlider, &QSlider::sliderReleased, [this]() {
        m_isSeekingByUser = false;
        // Perform seek only when user releases the slider
        if (m_mediaController) {
            m_mediaController->seek(m_positionSlider->value());
        }
    });

    // Handle clicks on the slider track (instant seeking)
    connect(m_positionSlider, &QSlider::sliderMoved, [this](int value) {
        // This allows instant seeking when user clicks on the track
        if (m_mediaController && m_isSeekingByUser) {
            m_mediaController->seek(value);
        }
    });

    // Volume slider signal
    connect(m_volumeSlider, &QSlider::valueChanged, this, &MainWindow::onVolumeChanged);

    // Update timer
    connect(m_updateTimer.get(), &QTimer::timeout, this, &MainWindow::updateTimeLabels);

    // Application signals
    if (m_app) {
        connect(m_app, &Core::Application::initializationFailed,
                this, [this](const QString& error) {
                    QMessageBox::critical(this, "Initialization Error",
                                        "Failed to initialize application:\n" + error);
                });
    }
}

void MainWindow::openFile()
{
    auto* configManager = m_app->configManager();
    if (!configManager) {
        qWarning() << "ConfigManager not available";
        return;
    }

    const QString lastDir = configManager->getValue("files/lastDirectory",
        QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)).toString();

    const QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Open Media File"),
        lastDir,
        tr("Media Files (*.mp4 *.avi *.mkv *.mov *.wmv *.mp3 *.wav *.flac *.ogg);;All Files (*)")
    );

    if (!fileName.isEmpty() && m_mediaController) {
        if (m_mediaController->openFile(fileName)) {
            const QFileInfo fileInfo(fileName);
            addToRecentFiles(fileName);
            configManager->setValue("files/lastDirectory", fileInfo.absolutePath());

            // Auto-play feature: automatically start playback if enabled
            bool autoPlay = configManager->getValue("playback/autoPlay", true).toBool();
            if (autoPlay) {
                // Start playback after a small delay to ensure media is ready
                QTimer::singleShot(100, this, [this]() {
                    if (m_mediaController && m_mediaController->hasMedia()) {
                        m_mediaController->play();
                        statusBar()->showMessage("Auto-playing media file...", 2000);
                    }
                });
            }
        }
    }
}

void MainWindow::openRecentFile()
{
    auto* action = qobject_cast<QAction*>(sender());
    if (!action || !m_mediaController) {
        return;
    }

    const QString fileName = action->data().toString();
    if (fileName.isEmpty()) {
        return;
    }

    if (m_mediaController->openFile(fileName)) {
        const QFileInfo fileInfo(fileName);
        statusBar()->showMessage(tr("Loaded: %1").arg(fileInfo.fileName()));

        // Auto-play feature for recent files too
        if (auto* configManager = m_app->configManager()) {
            bool autoPlay = configManager->getValue("playback/autoPlay", true).toBool();
            if (autoPlay) {
                // Start playback after a small delay to ensure media is ready
                QTimer::singleShot(100, this, [this]() {
                    if (m_mediaController && m_mediaController->hasMedia()) {
                        m_mediaController->play();
                        statusBar()->showMessage("Auto-playing recent file...", 2000);
                    }
                });
            }
        }
    } else {
        // Remove invalid file from recent files
        m_recentFiles.removeAll(fileName);
        if (auto* configManager = m_app->configManager()) {
            configManager->setValue("files/recentFiles", m_recentFiles);
        }
        updateRecentFilesMenu();
        statusBar()->showMessage(tr("Failed to load: %1").arg(QFileInfo(fileName).fileName()), 3000);
    }
}

void MainWindow::clearRecentFiles()
{
    m_recentFiles.clear();
    if (auto* configManager = m_app->configManager()) {
        configManager->setValue("files/recentFiles", m_recentFiles);
    }
    updateRecentFilesMenu();
}

void MainWindow::togglePlayPause()
{
    if (!m_mediaController) return;

    if (!m_mediaController->hasMedia()) {
        openFile();
        return;
    }

    auto state = m_mediaController->state();
    if (state == Media::PlaybackState::Playing) {
        m_mediaController->pause();
    } else {
        m_mediaController->play();
    }
}

void MainWindow::stopPlayback()
{
    if (m_mediaController) {
        m_mediaController->stop();
    }
}

void MainWindow::previousTrack()
{
    // TODO: Implement playlist functionality
    statusBar()->showMessage("Previous track - Playlist functionality coming soon", 2000);
}

void MainWindow::nextTrack()
{
    // TODO: Implement playlist functionality
    statusBar()->showMessage("Next track - Playlist functionality coming soon", 2000);
}

void MainWindow::onPositionChanged(qint64 position)
{
    if (!m_isSeekingByUser && m_positionSlider) {
        m_positionSlider->setValue(static_cast<int>(position));
    }
}

void MainWindow::onDurationChanged(qint64 duration)
{
    if (m_positionSlider) {
        m_positionSlider->setRange(0, static_cast<int>(duration));
    }
}

void MainWindow::onStateChanged(Media::PlaybackState state)
{
    updatePlayPauseButton();

    QString statusText;
    switch (state) {
        case Media::PlaybackState::Playing:
            statusText = "Playing";
            break;
        case Media::PlaybackState::Paused:
            statusText = "Paused";
            break;
        case Media::PlaybackState::Stopped:
            statusText = "Stopped";
            break;
        case Media::PlaybackState::Buffering:
            statusText = "Buffering...";
            break;
        case Media::PlaybackState::Error:
            statusText = "Error";
            break;
    }

    if (!statusText.isEmpty()) {
        statusBar()->showMessage(statusText);
    }
}


void MainWindow::onErrorOccurred(const QString& error)
{
    QMessageBox::warning(this, "Media Player Error", error);
    statusBar()->showMessage(QString("Error: %1").arg(error));
    m_loadingProgressBar->setVisible(false);
}

void MainWindow::onVolumeChanged(int value)
{
    if (m_mediaController) {
        const float volume = static_cast<float>(value) / 100.0f;
        m_mediaController->setVolume(volume);

        // Update volume icon
        QString icon = "🔇";
        if (value > 50) icon = "🔊";
        else if (value > 0) icon = "🔉";
        m_volumeLabel->setText(icon);

        // Save to config
        if (auto* configManager = m_app->configManager()) {
            configManager->setValue("media/volume", volume);
        }
    }
}


void MainWindow::updateTimeLabels()
{
    if (m_mediaController && m_mediaController->hasMedia()) {
        m_currentTimeLabel->setText(formatTime(m_mediaController->position()));
        m_totalTimeLabel->setText(formatTime(m_mediaController->duration()));
    }
}

void MainWindow::updatePlayPauseButton()
{
    if (!m_mediaController) return;

    if (m_mediaController->state() == Media::PlaybackState::Playing) {
        m_playPauseButton->setText("⏸");
    } else {
        m_playPauseButton->setText("▶");
    }
}

void MainWindow::saveSettings()
{
    if (m_app && m_app->configManager()) {
        m_app->configManager()->setValue("ui/windowGeometry", saveGeometry());
        m_app->configManager()->setValue("ui/windowState", saveState());
    }
}

void MainWindow::setAdaptiveLayout()
{
    const int windowWidth = width();

    if (windowWidth < 900) {
        m_mainLayout->setContentsMargins(5, 5, 5, 5);
        m_controlsWidget->setMaximumHeight(100);
    } else {
        m_mainLayout->setContentsMargins(10, 10, 10, 10);
        m_controlsWidget->setMaximumHeight(120);
    }
}

void MainWindow::onThemeChanged(const QString& themeName)
{
    // Apply the new theme to the UI
    statusBar()->showMessage(QString("Theme changed to: %1").arg(themeName), 2000);

    // The actual theme application is handled by the ThemeManager
    // This slot just provides UI feedback
    qDebug() << "Theme changed to:" << themeName;
}

void MainWindow::updateRecentFilesMenu()
{
    m_recentFilesMenu->clear();

    if (m_recentFiles.isEmpty()) {
        auto* emptyAction = m_recentFilesMenu->addAction("No recent files");
        emptyAction->setEnabled(false);
    } else {
        for (const QString& file : m_recentFiles) {
            QFileInfo fileInfo(file);
            auto* action = m_recentFilesMenu->addAction(fileInfo.fileName());
            action->setData(file);
            action->setToolTip(file);
            connect(action, &QAction::triggered, this, &MainWindow::openRecentFile);
        }

        m_recentFilesMenu->addSeparator();
        m_clearRecentAction = m_recentFilesMenu->addAction("Clear Recent Files");
        connect(m_clearRecentAction, &QAction::triggered, this, &MainWindow::clearRecentFiles);
    }
}

void MainWindow::addToRecentFiles(const QString& filePath)
{
    m_recentFiles.removeAll(filePath); // Remove if already exists
    m_recentFiles.prepend(filePath);   // Add to front

    const int maxRecent = MAX_RECENT_FILES;
    while (m_recentFiles.size() > maxRecent) {
        m_recentFiles.removeLast();
    }

    if (m_app && m_app->configManager()) {
        m_app->configManager()->setValue("files/recentFiles", m_recentFiles);
    }
    updateRecentFilesMenu();
}

void MainWindow::loadSettings()
{
    if (!m_app || !m_app->configManager()) {
        qWarning() << "ConfigManager not available for loading settings";
        return;
    }

    auto* configManager = m_app->configManager();

    // Window geometry
    QByteArray geometry = configManager->getValue("ui/windowGeometry").toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    } else {
        setMinimumSize(800, 600);
        resize(1200, 800);
    }

    QByteArray windowState = configManager->getValue("ui/windowState").toByteArray();
    if (!windowState.isEmpty()) {
        restoreState(windowState);
    }

    // Recent files
    m_recentFiles = configManager->getValue("files/recentFiles").toStringList();
    updateRecentFilesMenu();

    // Volume - use MediaController instead of direct engine access
    float volume = configManager->getValue("media/volume", 0.85).toFloat(); // Increased from 0.7 to 0.85
    m_volumeSlider->setValue(static_cast<int>(volume * 100));
    if (m_mediaController) {
        m_mediaController->setVolume(volume);
    }
}

QString MainWindow::formatTime(qint64 milliseconds)
{
    qint64 seconds = milliseconds / 1000;
    qint64 minutes = seconds / 60;
    seconds %= 60;

    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    setAdaptiveLayout();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
        case Qt::Key_Space:
            togglePlayPause();
            event->accept();
            break;
        case Qt::Key_Escape:
            if (m_isFullScreen) {
                toggleFullScreen();
                event->accept();
            } else {
                QMainWindow::keyPressEvent(event);
            }
            break;
        case Qt::Key_F11:
            toggleFullScreen();
            event->accept();
            break;
        default:
            QMainWindow::keyPressEvent(event);
            break;
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    event->accept();
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    // Check if double click occurred on the video widget area
    if (m_videoWidget && m_videoWidget->geometry().contains(event->pos())) {
        toggleFullScreen();
        event->accept();
    } else {
        QMainWindow::mouseDoubleClickEvent(event);
    }
}

void MainWindow::toggleFullScreen()
{
    if (m_isFullScreen) {
        // Exit fullscreen mode
        showNormal();

        // Show controls and menu bar
        if (m_controlsWidget) {
            m_controlsWidget->show();
        }
        menuBar()->show();
        statusBar()->show();

        // Restore normal margins
        if (m_mainLayout) {
            m_mainLayout->setContentsMargins(10, 10, 10, 10);
        }

        m_isFullScreen = false;
        statusBar()->showMessage("Exited fullscreen mode", 1500);
    } else {
        // Enter fullscreen mode
        showFullScreen();

        // Hide controls, menu bar and status bar for true fullscreen experience
        if (m_controlsWidget) {
            m_controlsWidget->hide();
        }
        menuBar()->hide();
        statusBar()->hide();

        // Remove margins for maximum video area
        if (m_mainLayout) {
            m_mainLayout->setContentsMargins(0, 0, 0, 0);
        }

        m_isFullScreen = true;
    }
}

void MainWindow::showPreferences()
{
    // TODO: Implement preferences dialog
    QMessageBox::information(this, "Preferences",
                           "Preferences dialog will be implemented in a future version.\n\n"
                           "For now, you can change themes from the View menu.");
}

void MainWindow::showPluginManager()
{
    // TODO: Implement plugin manager dialog
    if (auto* pluginManager = m_app->pluginManager()) {
        QStringList plugins = pluginManager->availablePlugins();

        QString message = "Plugin Manager\n\n";
        if (plugins.isEmpty()) {
            message += "No plugins currently loaded.";
        } else {
            message += QString("Loaded plugins (%1):\n").arg(plugins.size());
            for (const QString& plugin : plugins) {
                message += "• " + plugin + "\n";
            }
        }

        message += "\nFull plugin management interface coming soon.";

        QMessageBox::information(this, "Plugin Manager", message);
    } else {
        QMessageBox::warning(this, "Plugin Manager",
                           "Plugin manager is not available.");
    }
}

} // namespace DarkPlay::UI
