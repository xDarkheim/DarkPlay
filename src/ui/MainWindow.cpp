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
#include <QPointer>
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
    , m_controlsVisible(true)
    , m_cursorHidden(false)
    , m_fullScreenControlsOverlay(nullptr)
    , m_fullScreenCurrentTimeLabel(nullptr)
    , m_fullScreenTotalTimeLabel(nullptr)
    , m_fullScreenPlayPauseButton(nullptr)
    , m_fullScreenProgressSlider(nullptr)
    , m_fullScreenVolumeSlider(nullptr)
    , m_updateTimer(std::make_unique<QTimer>(this))
    , m_controlsHideTimer(std::make_unique<QTimer>(this))
    , m_mouseMoveDebounceTimer(std::make_unique<QTimer>(this))
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
    // ULTIMATE CRASH PROTECTION: Set destruction flag and disable all slider updates
    m_isDestructing = true;
    m_sliderUpdatesEnabled = false;

    // Lock mutex to ensure no slider operations are in progress
    std::lock_guard<std::recursive_mutex> lock(m_sliderMutex);

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

    // Enable mouse tracking for fullscreen controls functionality
    setMouseTracking(true);

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

    // Enable mouse tracking for the video widget too
    m_videoWidget->setMouseTracking(true);

    // Delayed initialization to prevent startup flickering
    m_videoWidget->hide();

    // Apply comprehensive visual optimization
    optimizeVideoWidgetRendering();

    // Set up video output through media controller
    if (m_mediaController) {
        m_mediaController->setVideoSink(m_videoWidget->videoSink());
    }

    // Add with maximum stretch factor for full screen usage
    m_mainLayout->addWidget(m_videoWidget, 100); // Higher stretch factor

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

    // Keep mouse tracking enabled for fullscreen controls
    // m_videoWidget->setMouseTracking(false); // Removed this line
    m_videoWidget->setFocusPolicy(Qt::NoFocus);

    // Force update and size
    m_videoWidget->setUpdatesEnabled(true);
    m_videoWidget->setVisible(true);

    // Set expanding size policy for maximum video area utilization
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

    // CRITICAL FIX: Initialize with a valid range instead of (0,0)
    m_positionSlider->setRange(0, 100);  // Start with a valid range
    m_positionSlider->setValue(0);

    // Ensure the slider is properly initialized before any further operations
    m_positionSlider->setEnabled(true);
    m_positionSlider->setVisible(true);

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

    // Controls hide timer for fullscreen mode
    m_controlsHideTimer->setSingleShot(true);
    // Disconnect any existing connections before reconnecting to prevent duplicates
    disconnect(m_controlsHideTimer.get(), &QTimer::timeout, this, &MainWindow::hideFullScreenUI);
    connect(m_controlsHideTimer.get(), &QTimer::timeout, this, &MainWindow::hideFullScreenUI);

    // Mouse move debounce timer to prevent too frequent UI updates
    m_mouseMoveDebounceTimer->setSingleShot(true);
    connect(m_mouseMoveDebounceTimer.get(), &QTimer::timeout, this, [this]() {
        if (m_isFullScreen) {
            showFullScreenUI();
        }
    });

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
        // CRITICAL FIX: Stop any existing overlay timers before loading new file
        if (m_controlsHideTimer) {
            m_controlsHideTimer->stop();
        }

        if (m_mediaController->openFile(fileName)) {
            const QFileInfo fileInfo(fileName);
            addToRecentFiles(fileName);
            configManager->setValue("files/lastDirectory", fileInfo.absolutePath());

            // Auto-play feature: automatically start playback if enabled
            bool autoPlay = configManager->getValue("playback/autoPlay", true).toBool();
            if (autoPlay) {
                // SAFE: Use QPointer to protect against object deletion
                QPointer<MainWindow> safeThis = this;
                QTimer::singleShot(100, [safeThis]() {
                    // Check if object still exists before accessing
                    if (safeThis && safeThis->m_mediaController && safeThis->m_mediaController->hasMedia()) {
                        safeThis->m_mediaController->play();
                        if (safeThis->statusBar()) {
                            safeThis->statusBar()->showMessage("Auto-playing media file...", 2000);
                        }
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

    // CRITICAL FIX: Stop any existing overlay timers before loading new file
    if (m_controlsHideTimer) {
        m_controlsHideTimer->stop();
    }

    if (m_mediaController->openFile(fileName)) {
        const QFileInfo fileInfo(fileName);
        statusBar()->showMessage(tr("Loaded: %1").arg(fileInfo.fileName()));

        // Auto-play feature for recent files too
        if (auto* configManager = m_app->configManager()) {
            bool autoPlay = configManager->getValue("playback/autoPlay", true).toBool();
            if (autoPlay) {
                // SAFE: Use QPointer to protect against object deletion
                QPointer<MainWindow> safeThis = this;
                QTimer::singleShot(100, [safeThis]() {
                    // Check if object still exists before accessing
                    if (safeThis && safeThis->m_mediaController && safeThis->m_mediaController->hasMedia()) {
                        safeThis->m_mediaController->play();
                        if (safeThis->statusBar()) {
                            safeThis->statusBar()->showMessage("Auto-playing recent file...", 2000);
                        }
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
    // ULTIMATE CRASH PROTECTION: Check atomic flags first
    if (m_isDestructing.load() || !m_sliderUpdatesEnabled.load()) {
        return; // Silently ignore updates during destruction or when disabled
    }

    // Lock mutex to ensure thread safety
    std::lock_guard<std::recursive_mutex> lock(m_sliderMutex);

    // Double-check flags after acquiring lock
    if (m_isDestructing.load() || !m_sliderUpdatesEnabled.load()) {
        return;
    }

    // THREAD-SAFE: Ultra-safe regular position slider update
    if (!m_isSeekingByUser && m_positionSlider) {
        try {
            // CRITICAL VALIDATION: Check if slider object is valid before any operations
            if (!m_positionSlider) {
                return;
            }

            // Additional safety checks for regular slider with widget validity
            if (m_positionSlider->isVisible() &&
                m_positionSlider->isEnabled() &&
                !m_positionSlider->signalsBlocked()) {

                // Get slider range safely with exception handling
                int minVal = 0, maxVal = 100;
                try {
                    minVal = m_positionSlider->minimum();
                    maxVal = m_positionSlider->maximum();
                } catch (...) {
                    // Silent fallback to defaults
                }

                // CRITICAL FIX: Validate that the slider has a valid range before setValue
                if (maxVal <= minVal) {
                    return;
                }

                // Validate position value within acceptable bounds
                int newValue = static_cast<int>(position);
                if (newValue >= minVal && newValue <= maxVal) {
                    // ULTIMATE SAFETY: Use QMetaObject::invokeMethod with QPointer protection
                    QPointer<ClickableSlider> safeSlider(m_positionSlider);
                    QMetaObject::invokeMethod(this, [safeSlider, newValue, this]() {
                        // Triple-check everything is still valid inside the queued call
                        if (!m_isDestructing.load() && m_sliderUpdatesEnabled.load() &&
                            safeSlider && !safeSlider.isNull() && safeSlider.data() &&
                            !m_isSeekingByUser &&
                            safeSlider->isVisible() && safeSlider->isEnabled()) {

                            // FINAL SAFETY CHECK: Validate range again before setValue
                            try {
                                int currentMin = safeSlider->minimum();
                                int currentMax = safeSlider->maximum();

                                if (currentMax > currentMin && newValue >= currentMin && newValue <= currentMax) {
                                    safeSlider->setValue(newValue);
                                }
                            } catch (const std::exception& e) {
                                // Silent error handling - just skip the update
                            } catch (...) {
                                // Silent error handling - just skip the update
                            }
                        }
                    }, Qt::QueuedConnection);
                }
            }
        } catch (const std::exception& e) {
            // Silent error handling - just skip the update
        } catch (...) {
            // Silent error handling - just skip the update
        }
    }

    // THREAD-SAFE: Update fullscreen slider only when controls are visible and we're still valid
    if (!m_isSeekingByUser && m_isFullScreen && m_controlsVisible &&
        !m_isDestructing.load() && m_sliderUpdatesEnabled.load()) {

        // Create completely isolated lambda with QPointer protection
        auto safeUpdateLambda = [this, position]() {
            // Ultra-safe checks with atomic flags
            try {
                // Verify we're still in valid state with atomic checks
                if (m_isDestructing.load() || !m_sliderUpdatesEnabled.load() ||
                    !m_isFullScreen || m_isSeekingByUser || !m_controlsVisible) {
                    return;
                }

                // Lock for thread safety in lambda
                std::lock_guard<std::recursive_mutex> lambdaLock(m_sliderMutex);

                // Verify QPointer validity with multiple comprehensive checks
                if (!m_fullScreenProgressSlider ||
                    !m_fullScreenProgressSlider.data() ||
                    m_fullScreenProgressSlider.isNull()) {
                    return; // Silent return - expected when controls are hidden
                }

                // Check widget state safely
                QWidget* sliderWidget = m_fullScreenProgressSlider.data();
                if (!sliderWidget->isVisible() ||
                    sliderWidget->signalsBlocked() ||
                    !sliderWidget->isEnabled()) {
                    return; // Silent return - normal when controls are hidden
                }

                // Verify parent overlay validity
                if (!m_fullScreenControlsOverlay ||
                    !m_fullScreenControlsOverlay.data() ||
                    !m_fullScreenControlsOverlay->isVisible()) {
                    return; // Silent return - overlay might be hidden
                }

                // Get slider range with exception protection
                int minVal = 0, maxVal = 100;
                try {
                    minVal = m_fullScreenProgressSlider->minimum();
                    maxVal = m_fullScreenProgressSlider->maximum();
                } catch (...) {
                    qDebug() << "onPositionChanged: Exception getting fullscreen slider range, using defaults";
                }

                // Validate position value
                int newValue = static_cast<int>(position);
                if (newValue < minVal || newValue > maxVal) {
                    // Only log if the range seems reasonable (not default 0-100)
                    if (maxVal > 100) {
                        qDebug() << "onPositionChanged: Fullscreen position value" << newValue
                                 << "is out of range [" << minVal << "," << maxVal << "]";
                    }
                    return;
                }

                // Final ultra-safe setValue with QPointer protection
                QPointer<QWidget> safeWidget(sliderWidget);
                QMetaObject::invokeMethod(sliderWidget, [safeWidget, newValue, this]() {
                    try {
                        // Ultimate validation inside the final queued call
                        if (!m_isDestructing.load() && m_sliderUpdatesEnabled.load() &&
                            safeWidget && safeWidget->isVisible() && safeWidget->isEnabled()) {

                            // Use blockSignals to prevent recursive calls
                            bool wasBlocked = safeWidget->signalsBlocked();
                            safeWidget->blockSignals(true);

                            // Safe casting and setValue call
                            if (auto* slider = qobject_cast<QSlider*>(safeWidget.data())) {
                                slider->setValue(newValue);
                            } else if (auto* clickableSlider = qobject_cast<ClickableSlider*>(safeWidget.data())) {
                                clickableSlider->setValue(newValue);
                            }

                            // Restore signal blocking state
                            safeWidget->blockSignals(wasBlocked);
                        }
                    } catch (const std::exception& e) {
                        qDebug() << "onPositionChanged: CRITICAL - Exception in final fullscreen setValue:" << e.what();
                    } catch (...) {
                        qDebug() << "onPositionChanged: CRITICAL - Unknown exception in final fullscreen setValue";
                    }
                }, Qt::QueuedConnection);

            } catch (const std::exception& e) {
                qDebug() << "onPositionChanged: Exception in fullscreen safe update lambda:" << e.what();
            } catch (...) {
                qDebug() << "onPositionChanged: Unknown exception in fullscreen safe update lambda";
            }
        };

        // Execute the safe lambda with ultimate protection
        try {
            safeUpdateLambda();
        } catch (...) {
            qDebug() << "onPositionChanged: FATAL - Exception even in isolated fullscreen lambda execution";
        }
    }
}

void MainWindow::onDurationChanged(qint64 duration)
{
    // Regular duration slider update with basic safety
    if (m_positionSlider) {
        try {
            m_positionSlider->setRange(0, static_cast<int>(duration));
        } catch (const std::exception& e) {
            qDebug() << "onDurationChanged: Exception in regular slider setRange:" << e.what();
        } catch (...) {
            qDebug() << "onDurationChanged: Unknown exception in regular slider setRange";
        }
    }

    // OPTIMIZED CRASH PROTECTION: Update fullscreen slider range only when controls are visible
    if (m_isFullScreen && m_controlsVisible) {
        // Create a completely isolated lambda for range setting
        auto safeRangeUpdateLambda = [this, duration]() {
            try {
                // Check if we're still in a valid state
                if (!this || !m_isFullScreen || !m_controlsVisible) {
                    return;
                }

                // Verify QPointer validity with multiple checks
                if (!m_fullScreenProgressSlider ||
                    !m_fullScreenProgressSlider.data() ||
                    m_fullScreenProgressSlider.isNull()) {
                    return; // Silent return - this is expected when controls are hidden
                }

                // Check if widget is being destroyed or in invalid state
                QWidget* sliderWidget = m_fullScreenProgressSlider.data();
                if (!sliderWidget->isVisible() ||
                    sliderWidget->signalsBlocked() ||
                    !sliderWidget->isEnabled()) {
                    return; // Silent return - this is normal when controls are hidden
                }

                // Verify parent overlay is also valid
                if (!m_fullScreenControlsOverlay ||
                    !m_fullScreenControlsOverlay.data() ||
                    !m_fullScreenControlsOverlay->isVisible()) {
                    return; // Silent return - overlay might be hidden
                }

                // Validate duration value
                int newMaxValue = static_cast<int>(duration);
                if (newMaxValue < 0) {
                    qDebug() << "onDurationChanged: Invalid duration value" << newMaxValue;
                    return;
                }

                // Final ultra-safe setRange call with exception isolation
                QMetaObject::invokeMethod(sliderWidget, [sliderWidget, newMaxValue]() {
                    try {
                        // Triple-check widget validity inside the final call
                        if (sliderWidget && sliderWidget->isVisible() && sliderWidget->isEnabled()) {
                            // Use blockSignals to prevent recursive calls
                            bool wasBlocked = sliderWidget->signalsBlocked();
                            sliderWidget->blockSignals(true);

                            // Cast to QSlider safely and call setRange
                            if (auto* slider = qobject_cast<QSlider*>(sliderWidget)) {
                                slider->setRange(0, newMaxValue);
                            } else if (auto* clickableSlider = qobject_cast<ClickableSlider*>(sliderWidget)) {
                                clickableSlider->setRange(0, newMaxValue);
                            }

                            // Restore signal blocking state
                            sliderWidget->blockSignals(wasBlocked);
                        }
                    } catch (const std::exception& e) {
                        qDebug() << "onDurationChanged: CRITICAL - Exception in final setRange call:" << e.what();
                    } catch (...) {
                        qDebug() << "onDurationChanged: CRITICAL - Unknown exception in final setRange call";
                    }
                }, Qt::QueuedConnection);

            } catch (const std::exception& e) {
                qDebug() << "onDurationChanged: Exception in safe range update lambda:" << e.what();
            } catch (...) {
                qDebug() << "onDurationChanged: Unknown exception in safe range update lambda";
            }
        };

        // Execute the safe lambda with additional protection
        try {
            safeRangeUpdateLambda();
        } catch (...) {
            qDebug() << "onDurationChanged: FATAL - Exception even in isolated lambda execution";
        }
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
        QString currentTime = formatTime(m_mediaController->position());
        QString totalTime = formatTime(m_mediaController->duration());

        // Update main window labels
        if (m_currentTimeLabel) {
            m_currentTimeLabel->setText(currentTime);
        }
        if (m_totalTimeLabel) {
            m_totalTimeLabel->setText(totalTime);
        }

        // Update fullscreen labels if they exist
        if (m_fullScreenCurrentTimeLabel) {
            m_fullScreenCurrentTimeLabel->setText(currentTime);
        }
        if (m_fullScreenTotalTimeLabel) {
            m_fullScreenTotalTimeLabel->setText(totalTime);
        }
    }
}

void MainWindow::updatePlayPauseButton()
{
    if (!m_mediaController) return;

    QString buttonText = (m_mediaController->state() == Media::PlaybackState::Playing) ? "⏸" : "▶";

    // Update main window button
    if (m_playPauseButton) {
        m_playPauseButton->setText(buttonText);
    }

    // Update fullscreen button if it exists
    if (m_fullScreenPlayPauseButton) {
        m_fullScreenPlayPauseButton->setText(buttonText);
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

    if (m_isFullScreen) {
        // Update video widget size in fullscreen
        if (m_videoWidget) {
            m_videoWidget->setGeometry(0, 0, width(), height());
        }

        // Update overlay position and size properly for multi-screen setup
        if (m_fullScreenControlsOverlay) {
            updateOverlayPosition();
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (m_isFullScreen) {
        switch (event->key()) {
        case Qt::Key_Escape:
            toggleFullScreen();
            return;
        case Qt::Key_Space:
            togglePlayPause();
            showFullScreenUI();
            return;
        case Qt::Key_Left:
            if (m_mediaController) {
                m_mediaController->seek(m_mediaController->position() - 10000); // -10 seconds
                showFullScreenUI();
            }
            return;
        case Qt::Key_Right:
            if (m_mediaController) {
                m_mediaController->seek(m_mediaController->position() + 10000); // +10 seconds
                showFullScreenUI();
            }
            return;
        case Qt::Key_Up:
            if (m_volumeSlider) {
                int newVolume = qMin(100, m_volumeSlider->value() + 5);
                m_volumeSlider->setValue(newVolume);
                showFullScreenUI();
            }
            return;
        case Qt::Key_Down:
            if (m_volumeSlider) {
                int newVolume = qMax(0, m_volumeSlider->value() - 5);
                m_volumeSlider->setValue(newVolume);
                showFullScreenUI();
            }
            return;
        }
    }

    QMainWindow::keyPressEvent(event);
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
        // Exit fullscreen mode - CRITICAL: Set flag first to prevent race conditions
        m_isFullScreen = false;
        m_sliderUpdatesEnabled = false; // CRITICAL: Disable slider updates immediately

        // CRITICAL: Stop and disconnect timer COMPLETELY to prevent any callbacks
        if (m_controlsHideTimer) {
            m_controlsHideTimer->stop();
            disconnect(m_controlsHideTimer.get(), nullptr, this, nullptr);
        }

        // CRITICAL FIX: Safely destroy overlay with comprehensive cleanup
        if (m_fullScreenControlsOverlay) {
            // Block all signals from the overlay to prevent any callbacks during destruction
            m_fullScreenControlsOverlay->blockSignals(true);

            // CRITICAL FIX: Disconnect ALL connections safely before any destruction
            if (m_fullScreenProgressSlider) {
                m_fullScreenProgressSlider->blockSignals(true);
                // Disconnect all signals from/to this widget using the proper Qt method
                m_fullScreenProgressSlider->disconnect();
                // Also disconnect from the main slider to prevent cross-references
                if (m_positionSlider) {
                    disconnect(m_positionSlider, nullptr, m_fullScreenProgressSlider.data(), nullptr);
                }
            }

            if (m_fullScreenVolumeSlider) {
                m_fullScreenVolumeSlider->blockSignals(true);
                // Disconnect all signals from/to this widget using the proper Qt method
                m_fullScreenVolumeSlider->disconnect();
                // Also disconnect from the main volume slider
                if (m_volumeSlider) {
                    disconnect(m_volumeSlider, nullptr, m_fullScreenVolumeSlider.data(), nullptr);
                }
            }

            if (m_fullScreenPlayPauseButton) {
                m_fullScreenPlayPauseButton->blockSignals(true);
                // Disconnect all signals from/to this widget using the proper Qt method
                m_fullScreenPlayPauseButton->disconnect();
            }

            // Disconnect time labels safely - they only receive signals
            if (m_fullScreenCurrentTimeLabel) {
                m_fullScreenCurrentTimeLabel->disconnect();
            }
            if (m_fullScreenTotalTimeLabel) {
                m_fullScreenTotalTimeLabel->disconnect();
            }

            // Hide the overlay immediately to prevent any paint events
            m_fullScreenControlsOverlay->setVisible(false);
            m_fullScreenControlsOverlay->hide();

            // Reset all child widget pointers BEFORE deleting parent
            m_fullScreenCurrentTimeLabel = nullptr;
            m_fullScreenTotalTimeLabel = nullptr;
            m_fullScreenPlayPauseButton = nullptr;
            m_fullScreenProgressSlider = nullptr;
            m_fullScreenVolumeSlider = nullptr;

            // Process any pending events before deletion
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 50);

            // CRITICAL FIX: Use deleteLater() instead of immediate delete for safer cleanup
            m_fullScreenControlsOverlay->setParent(nullptr);
            m_fullScreenControlsOverlay->deleteLater();
            m_fullScreenControlsOverlay = nullptr;
        }

        // Process events again with timeout
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 50);

        // Use QTimer::singleShot to defer the rest of the cleanup with safety check
        QTimer::singleShot(50, this, [this]() {
            if (!m_isFullScreen) { // Double check we're still exiting fullscreen
                // Exit fullscreen AFTER cleanup
                showNormal();

                // Show normal controls and UI elements
                if (m_controlsWidget) {
                    m_controlsWidget->show();
                    m_controlsVisible = true;
                }
                menuBar()->show();
                statusBar()->show();

                // Restore normal cursor state when exiting fullscreen
                setCursor(Qt::ArrowCursor);
                m_cursorHidden = false;

                // Restore normal margins and spacing
                if (m_mainLayout) {
                    m_mainLayout->setContentsMargins(10, 10, 10, 10);
                    m_mainLayout->setSpacing(10);
                }

                // Reset video widget to normal size
                if (m_videoWidget) {
                    m_videoWidget->setParent(m_centralWidget);
                    m_mainLayout->insertWidget(0, m_videoWidget);
                }

                // Re-enable slider updates AFTER everything is restored
                m_sliderUpdatesEnabled = true;

                // Reconnect the timer after cleanup with safety checks
                if (m_controlsHideTimer) {
                    disconnect(m_controlsHideTimer.get(), nullptr, this, nullptr);
                    m_controlsHideTimer->setSingleShot(true);
                    connect(m_controlsHideTimer.get(), &QTimer::timeout, this, &MainWindow::hideFullScreenUI);
                }

                statusBar()->showMessage("Exited fullscreen mode", 1500);
            }
        });
    } else {
        // Enter fullscreen mode
        showFullScreen();

        // CRITICAL FIX: Set fullscreen flag BEFORE creating overlay
        m_isFullScreen = true;

        // CRITICAL: Reset cursor state when entering fullscreen to ensure clean state
        m_cursorHidden = false;
        setCursor(Qt::ArrowCursor);
        qDebug() << "toggleFullScreen: Entering fullscreen, cursor state reset";

        // Hide menu bar, status bar and normal controls immediately
        menuBar()->hide();
        statusBar()->hide();
        if (m_controlsWidget) {
            m_controlsWidget->hide();
        }

        // Remove all margins and spacing for maximum video area
        if (m_mainLayout) {
            m_mainLayout->setContentsMargins(0, 0, 0, 0);
            m_mainLayout->setSpacing(0);
        }

        // Make video widget fill entire window
        if (m_videoWidget) {
            m_videoWidget->setParent(this);
            m_videoWidget->setGeometry(0, 0, width(), height());
            m_videoWidget->show();
            m_videoWidget->raise();
        }

        // Create fullscreen overlay controls AFTER setting m_isFullScreen = true
        createFullScreenOverlay();

        m_controlsVisible = false;

        // Show controls briefly, then start hide timer with delay
        QTimer::singleShot(100, this, [this]() {
            if (m_isFullScreen) { // Safety check
                showFullScreenUI();
            }
        });

        // Set focus to catch key events
        setFocus();

        statusBar()->showMessage("Entered fullscreen mode - Press ESC to exit", 2000);
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    // In fullscreen mode, use debounced approach to show controls when mouse moves
    if (m_isFullScreen) {
        // Don't call showFullScreenUI() directly - use debounce timer instead
        if (!m_mouseMoveDebounceTimer->isActive()) {
            m_mouseMoveDebounceTimer->start(MOUSE_MOVE_DEBOUNCE_MS);
        }
    }

    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (m_isFullScreen) {
        if (event->button() == Qt::LeftButton) {
            // Toggle controls visibility on click
            if (m_controlsVisible) {
                hideFullScreenUI();
            } else {
                showFullScreenUI();
            }
        }
    }

    QMainWindow::mousePressEvent(event);
}

void MainWindow::leaveEvent(QEvent *event)
{
    // When mouse leaves the window in fullscreen mode, start hiding controls
    if (m_isFullScreen && m_controlsVisible) {
        // Start timer to hide controls when mouse leaves window
        resetControlsHideTimer();
    }

    QMainWindow::leaveEvent(event);
}

void MainWindow::createFullScreenOverlay()
{
    // CRITICAL FIX: Properly clean up existing overlay
    if (m_fullScreenControlsOverlay) {
        qDebug() << "createFullScreenOverlay: Overlay already exists, destroying old one first";

        // Stop any timers related to the overlay
        if (m_controlsHideTimer) {
            m_controlsHideTimer->stop();
        }

        // Hide and delete safely
        m_fullScreenControlsOverlay->hide();
        m_fullScreenControlsOverlay->deleteLater();
        m_fullScreenControlsOverlay = nullptr;

        // Reset pointers to child widgets
        m_fullScreenCurrentTimeLabel = nullptr;
        m_fullScreenTotalTimeLabel = nullptr;
        m_fullScreenPlayPauseButton = nullptr;
        m_fullScreenProgressSlider = nullptr;
        m_fullScreenVolumeSlider = nullptr;
    }

    qDebug() << "createFullScreenOverlay: Creating overlay for window size" << size()
             << "isFullScreen flag:" << m_isFullScreen;

    // Create overlay as a separate top-level window (NOT child widget)
    m_fullScreenControlsOverlay = new QWidget(nullptr); // No parent!
    m_fullScreenControlsOverlay->setObjectName("FullScreenOverlay");

    // CRITICAL: Top-level window flags for visibility over fullscreen
    m_fullScreenControlsOverlay->setWindowFlags(
        Qt::Window |
        Qt::FramelessWindowHint |
        Qt::WindowStaysOnTopHint |
        Qt::X11BypassWindowManagerHint
    );

    // Essential attributes for top-level overlay window
    m_fullScreenControlsOverlay->setAttribute(Qt::WA_ShowWithoutActivating, true);
    m_fullScreenControlsOverlay->setAttribute(Qt::WA_TranslucentBackground, true);
    m_fullScreenControlsOverlay->setAttribute(Qt::WA_NoSystemBackground, true);
    m_fullScreenControlsOverlay->setAttribute(Qt::WA_OpaquePaintEvent, false);

    // Mouse and focus handling
    m_fullScreenControlsOverlay->setAttribute(Qt::WA_MouseTracking, true);
    m_fullScreenControlsOverlay->setFocusPolicy(Qt::NoFocus);

    // Enhanced styling with modern media player design
    m_fullScreenControlsOverlay->setStyleSheet(
        // Main overlay container - sleek dark design with subtle border
        "QWidget#FullScreenOverlay {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "        stop:0 rgba(18, 18, 22, 245), "
        "        stop:1 rgba(12, 12, 16, 235));"
        "    border: 2px solid rgba(255, 255, 255, 60);"
        "    border-radius: 16px;"
        "    box-shadow: 0 8px 32px rgba(0, 0, 0, 180);"
        "}"

        // Play/Pause button - prominent circular design
        "QPushButton#PlayPauseButton {"
        "    background: qradialGradient(cx:0.5, cy:0.5, radius:0.8, "
        "        stop:0 rgba(70, 130, 255, 220), "
        "        stop:1 rgba(50, 100, 200, 180));"
        "    border: 3px solid rgba(255, 255, 255, 120);"
        "    border-radius: 30px;"
        "    color: white;"
        "    font-size: 24px;"
        "    font-weight: bold;"
        "    min-width: 60px;"
        "    min-height: 60px;"
        "}"
        "QPushButton#PlayPauseButton:hover {"
        "    background: qradialGradient(cx:0.5, cy:0.5, radius:0.8, "
        "        stop:0 rgba(90, 150, 255, 240), "
        "        stop:1 rgba(70, 120, 220, 200));"
        "    border: 3px solid rgba(255, 255, 255, 180);"
        "    transform: scale(1.1);"
        "}"
        "QPushButton#PlayPauseButton:pressed {"
        "    background: qradialGradient(cx:0.5, cy:0.5, radius:0.8, "
        "        stop:0 rgba(50, 110, 200, 200), "
        "        stop:1 rgba(30, 80, 160, 160));"
        "    transform: scale(0.95);"
        "}"

        // Secondary buttons - smaller, elegant style
        "QPushButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "        stop:0 rgba(60, 65, 75, 200), "
        "        stop:1 rgba(45, 50, 60, 180));"
        "    border: 2px solid rgba(255, 255, 255, 80);"
        "    border-radius: 10px;"
        "    color: rgba(255, 255, 255, 240);"
        "    font-size: 16px;"
        "    font-weight: 600;"
        "    padding: 8px 16px;"
        "    min-width: 40px;"
        "    min-height: 40px;"
        "}"
        "QPushButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "        stop:0 rgba(80, 85, 95, 220), "
        "        stop:1 rgba(65, 70, 80, 200));"
        "    border: 2px solid rgba(255, 255, 255, 120);"
        "}"
        "QPushButton:pressed {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "        stop:0 rgba(40, 45, 55, 200), "
        "        stop:1 rgba(25, 30, 40, 180));"
        "}"

        // Progress slider - YouTube-style design
        "QSlider#ProgressSlider::groove:horizontal {"
        "    background: rgba(255, 255, 255, 100);"
        "    height: 6px;"
        "    border-radius: 3px;"
        "    border: 1px solid rgba(0, 0, 0, 40);"
        "}"
        "QSlider#ProgressSlider::handle:horizontal {"
        "    background: qradialGradient(cx:0.5, cy:0.5, radius:0.7, "
        "        stop:0 rgba(255, 255, 255, 255), "
        "        stop:1 rgba(220, 220, 220, 220));"
        "    width: 16px;"
        "    height: 16px;"
        "    border-radius: 8px;"
        "    border: 2px solid rgba(70, 130, 255, 200);"
        "    margin: -7px 0;"
        "    box-shadow: 0 2px 6px rgba(0, 0, 0, 120);"
        "}"
        "QSlider#ProgressSlider::handle:horizontal:hover {"
        "    background: qradialGradient(cx:0.5, cy:0.5, radius:0.7, "
        "        stop:0 rgba(255, 255, 255, 255), "
        "        stop:1 rgba(240, 240, 240, 240));"
        "    width: 20px;"
        "    height: 20px;"
        "    border-radius: 10px;"
        "    margin: -9px 0;"
        "    border: 3px solid rgba(70, 130, 255, 240);"
        "}"
        "QSlider#ProgressSlider::sub-page:horizontal {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "        stop:0 rgba(70, 130, 255, 240), "
        "        stop:1 rgba(50, 110, 200, 200));"
        "    border-radius: 3px;"
        "    border: 1px solid rgba(70, 130, 255, 160);"
        "}"
        "QSlider#ProgressSlider::add-page:horizontal {"
        "    background: rgba(255, 255, 255, 80);"
        "    border-radius: 3px;"
        "}"

        // Volume slider - compact design
        "QSlider#VolumeSlider::groove:horizontal {"
        "    background: rgba(255, 255, 255, 80);"
        "    height: 4px;"
        "    border-radius: 2px;"
        "}"
        "QSlider#VolumeSlider::handle:horizontal {"
        "    background: rgba(255, 255, 255, 240);"
        "    width: 12px;"
        "    height: 12px;"
        "    border-radius: 6px;"
        "    border: 2px solid rgba(70, 130, 255, 180);"
        "    margin: -5px 0;"
        "}"
        "QSlider#VolumeSlider::handle:horizontal:hover {"
        "    background: rgba(255, 255, 255, 255);"
        "    width: 14px;"
        "    height: 14px;"
        "    border-radius: 7px;"
        "    margin: -6px 0;"
        "}"
        "QSlider#VolumeSlider::sub-page:horizontal {"
        "    background: rgba(70, 130, 255, 200);"
        "    border-radius: 2px;"
        "}"
        "QSlider#VolumeSlider::add-page:horizontal {"
        "    background: rgba(255, 255, 255, 60);"
        "    border-radius: 2px;"
        "}"

        // Time labels - clean monospace design
        "QLabel#TimeLabel {"
        "    background: rgba(0, 0, 0, 150);"
        "    border: 1px solid rgba(255, 255, 255, 60);"
        "    border-radius: 8px;"
        "    padding: 6px 12px;"
        "    color: rgba(255, 255, 255, 255);"
        "    font-family: 'Consolas', 'Roboto Mono', 'Monaco', monospace;"
        "    font-size: 13px;"
        "    font-weight: 600;"
        "    min-width: 55px;"
        "}"

        // Volume icon label
        "QLabel#VolumeIcon {"
        "    color: rgba(255, 255, 255, 200);"
        "    font-size: 18px;"
        "    padding: 4px;"
        "}"

        // Separator line between progress and controls
        "QFrame#ControlsSeparator {"
        "    background: rgba(255, 255, 255, 60);"
        "    border: none;"
        "    height: 1px;"
        "    margin: 8px 0;"
        "}"
    );

    // Create layout for overlay with improved structure
    auto* overlayLayout = new QVBoxLayout(m_fullScreenControlsOverlay);
    overlayLayout->setContentsMargins(20, 12, 20, 12);
    overlayLayout->setSpacing(10);

    // Progress section with improved spacing
    auto* progressLayout = new QHBoxLayout();
    progressLayout->setSpacing(12);

    auto* currentTimeLabel = new QLabel("00:00", m_fullScreenControlsOverlay);
    currentTimeLabel->setObjectName("TimeLabel");
    currentTimeLabel->setMinimumWidth(55);
    currentTimeLabel->setAlignment(Qt::AlignCenter);

    auto* progressSlider = new ClickableSlider(Qt::Horizontal, m_fullScreenControlsOverlay);
    progressSlider->setObjectName("ProgressSlider");
    progressSlider->setRange(0, 0);
    progressSlider->setValue(0);
    progressSlider->setMinimumHeight(20);
    progressSlider->setTracking(true);

    // Enable better interaction
    progressSlider->setTracking(true);
    progressSlider->setPageStep(5000); // 5 second jumps
    progressSlider->setSingleStep(1000); // 1 second jumps

    // Connect to media controller with improved functionality
    connect(progressSlider, &QSlider::sliderPressed, [this]() {
        m_isSeekingByUser = true;
        resetControlsHideTimer();
    });

    connect(progressSlider, &QSlider::sliderReleased, [this, progressSlider]() {
        m_isSeekingByUser = false;
        if (m_mediaController) {
            m_mediaController->seek(progressSlider->value());
        }
        resetControlsHideTimer();
    });

    connect(progressSlider, &QSlider::sliderMoved, [this, progressSlider](int value) {
        if (m_mediaController && m_isSeekingByUser) {
            m_mediaController->seek(value);
        }
        resetControlsHideTimer();
    });

    m_fullScreenProgressSlider = progressSlider;

    // Sync with main slider
    connect(m_positionSlider, &QSlider::valueChanged, [this, progressSlider](int value) {
        if (!m_isSeekingByUser && progressSlider) {
            progressSlider->setValue(value);
        }
    });

    connect(m_positionSlider, &QSlider::rangeChanged, [progressSlider](int min, int max) {
        if (progressSlider) {
            progressSlider->setRange(min, max);
        }
    });

    // Initialize with current values
    if (m_positionSlider) {
        progressSlider->setRange(m_positionSlider->minimum(), m_positionSlider->maximum());
        progressSlider->setValue(m_positionSlider->value());
    }

    auto* totalTimeLabel = new QLabel("00:00", m_fullScreenControlsOverlay);
    totalTimeLabel->setObjectName("TimeLabel");
    totalTimeLabel->setMinimumWidth(55);
    totalTimeLabel->setAlignment(Qt::AlignCenter);

    m_fullScreenCurrentTimeLabel = currentTimeLabel;
    m_fullScreenTotalTimeLabel = totalTimeLabel;

    // Update labels with current values
    if (m_mediaController && m_mediaController->hasMedia()) {
        currentTimeLabel->setText(formatTime(m_mediaController->position()));
        totalTimeLabel->setText(formatTime(m_mediaController->duration()));
    }

    progressLayout->addWidget(currentTimeLabel);
    progressLayout->addWidget(progressSlider, 1);
    progressLayout->addWidget(totalTimeLabel);

    // Add separator line between progress and controls
    auto* separator = new QFrame(m_fullScreenControlsOverlay);
    separator->setObjectName("ControlsSeparator");
    separator->setFrameShape(QFrame::HLine);
    separator->setFixedHeight(1);

    // Control buttons section with better organization
    auto* buttonsLayout = new QHBoxLayout();
    buttonsLayout->setSpacing(15);

    // Left side - additional controls (future: previous/next track)
    auto* leftControlsLayout = new QHBoxLayout();
    leftControlsLayout->setSpacing(8);

    // Center - main play/pause button
    auto* playPauseBtn = new QPushButton("▶", m_fullScreenControlsOverlay);
    playPauseBtn->setObjectName("PlayPauseButton");
    playPauseBtn->setFixedSize(60, 60);
    playPauseBtn->setToolTip("Play/Pause (Space)");

    connect(playPauseBtn, &QPushButton::clicked, [this]() {
        togglePlayPause();
        resetControlsHideTimer();
    });

    m_fullScreenPlayPauseButton = playPauseBtn;

    // Initialize with current state
    if (m_mediaController) {
        QString buttonText = (m_mediaController->state() == Media::PlaybackState::Playing) ? "⏸" : "▶";
        playPauseBtn->setText(buttonText);
    }

    // Right side - volume controls
    auto* volumeControlsLayout = new QHBoxLayout();
    volumeControlsLayout->setSpacing(8);

    auto* volumeLabel = new QLabel("🔊", m_fullScreenControlsOverlay);
    volumeLabel->setObjectName("VolumeIcon");

    auto* volumeSlider = new QSlider(Qt::Horizontal, m_fullScreenControlsOverlay);
    volumeSlider->setObjectName("VolumeSlider");
    volumeSlider->setRange(0, 100);
    volumeSlider->setMaximumWidth(120);
    volumeSlider->setMinimumHeight(20);
    volumeSlider->setTracking(true);
    volumeSlider->setToolTip("Volume");

    if (m_volumeSlider) {
        volumeSlider->setValue(m_volumeSlider->value());
    }

    connect(volumeSlider, &QSlider::valueChanged, [this](int value) {
        onVolumeChanged(value);
        resetControlsHideTimer();
    });

    connect(volumeSlider, &QSlider::sliderPressed, [this]() {
        resetControlsHideTimer();
    });

    connect(m_volumeSlider, &QSlider::valueChanged, volumeSlider, &QSlider::setValue);

    m_fullScreenVolumeSlider = volumeSlider;

    // Assemble volume controls
    volumeControlsLayout->addWidget(volumeLabel);
    volumeControlsLayout->addWidget(volumeSlider);

    // Assemble main buttons layout
    buttonsLayout->addLayout(leftControlsLayout);
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(playPauseBtn);
    buttonsLayout->addStretch();
    buttonsLayout->addLayout(volumeControlsLayout);

    // Assemble final overlay layout
    overlayLayout->addLayout(progressLayout);
    overlayLayout->addWidget(separator);
    overlayLayout->addLayout(buttonsLayout);

    // Calculate position relative to the screen where the main window is located
    QScreen* currentScreen = QApplication::screenAt(this->pos());
    if (!currentScreen) {
        currentScreen = QApplication::primaryScreen();
    }
    
    QRect screenGeometry = currentScreen->geometry();
    QPoint mainWindowPos = this->pos();
    
    // Calculate overlay dimensions
    int overlayWidth = qMin(screenGeometry.width() - 80, 1000);
    int overlayHeight = 160;
    
    // Position overlay relative to the current screen (not all screens combined)
    int x = screenGeometry.left() + (screenGeometry.width() - overlayWidth) / 2;
    int y = screenGeometry.bottom() - overlayHeight - 50; // 50px from bottom of current screen
    
    // Additional safety checks to ensure overlay stays within current screen bounds
    x = qMax(screenGeometry.left() + 10, qMin(x, screenGeometry.right() - overlayWidth - 10));
    y = qMax(screenGeometry.top() + 10, qMin(y, screenGeometry.bottom() - overlayHeight - 10));

    qDebug() << "createFullScreenOverlay: Using screen" << currentScreen->name()
             << "with geometry" << screenGeometry
             << "Final position" << QPoint(x, y)
             << "overlay size" << QSize(overlayWidth, overlayHeight);

    m_fullScreenControlsOverlay->setFixedSize(overlayWidth, overlayHeight);
    m_fullScreenControlsOverlay->move(x, y);

    // CRITICAL: Make overlay visible as top-level window
    m_fullScreenControlsOverlay->show();
    m_fullScreenControlsOverlay->raise();
    m_fullScreenControlsOverlay->activateWindow();

    // Force update
    m_fullScreenControlsOverlay->update();

    qDebug() << "createFullScreenOverlay: Overlay created successfully and shown";
}

void MainWindow::showFullScreenUI()
{
    if (!m_isFullScreen) {
        qDebug() << "showFullScreenUI: Not in fullscreen mode, skipping";
        return;
    }

    qDebug() << "showFullScreenUI: Starting unified show operation";

    // Step 1: Show cursor first
    if (m_cursorHidden) {
        try {
            setCursor(Qt::ArrowCursor);
            m_cursorHidden = false;
            qDebug() << "showFullScreenUI: Cursor shown";
        } catch (const std::exception& e) {
            qWarning() << "showFullScreenUI: Failed to show cursor:" << e.what();
        }
    }

    // Step 2: Ensure overlay exists
    if (!m_fullScreenControlsOverlay) {
        qDebug() << "showFullScreenUI: No overlay exists - trying to create one";
        if (isFullScreen()) {
            createFullScreenOverlay();
        }
        if (!m_fullScreenControlsOverlay) {
            qDebug() << "showFullScreenUI: Failed to create overlay";
            return;
        }
    }

    // Step 3: Show controls overlay
    if (!m_controlsVisible) {
        m_fullScreenControlsOverlay->show();
        m_fullScreenControlsOverlay->raise();
        m_controlsVisible = true;
        qDebug() << "showFullScreenUI: Controls shown";
    }

    // Step 4: Reset hide timer
    resetControlsHideTimer();

    qDebug() << "showFullScreenUI: Unified show operation completed";
}

void MainWindow::hideFullScreenUI()
{
    if (!m_isFullScreen) {
        qDebug() << "hideFullScreenUI: Not in fullscreen mode, skipping";
        return;
    }

    qDebug() << "hideFullScreenUI: Starting unified hide operation";

    // Step 1: Hide controls overlay
    if (m_fullScreenControlsOverlay && m_controlsVisible) {
        m_fullScreenControlsOverlay->hide();
        m_controlsVisible = false;
        qDebug() << "hideFullScreenUI: Controls hidden";
    }

    // Step 2: Hide cursor
    if (!m_cursorHidden) {
        try {
            setCursor(Qt::BlankCursor);
            m_cursorHidden = true;
            qDebug() << "hideFullScreenUI: Cursor hidden";
        } catch (const std::exception& e) {
            qWarning() << "hideFullScreenUI: Failed to hide cursor:" << e.what();
            m_cursorHidden = false; // Reset state on failure
        }
    }

    qDebug() << "hideFullScreenUI: Unified hide operation completed";
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

void MainWindow::updateOverlayPosition()
{
    if (!m_fullScreenControlsOverlay || !m_isFullScreen) {
        return;
    }

    // Get the screen where the main window is currently located
    // Use window geometry center for more accurate screen detection
    QPoint windowCenter = QPoint(x() + width() / 2, y() + height() / 2);
    QScreen* currentScreen = QApplication::screenAt(windowCenter);

    // Fallback methods if screenAt fails
    if (!currentScreen) {
        // Try using the screen containing the window's top-left corner
        currentScreen = QApplication::screenAt(this->pos());
    }

    if (!currentScreen) {
        // Last resort - use primary screen
        currentScreen = QApplication::primaryScreen();
    }

    if (!currentScreen) {
        qWarning() << "updateOverlayPosition: Could not determine current screen";
        return;
    }

    QRect screenGeometry = currentScreen->geometry();

    qDebug() << "updateOverlayPosition: Window geometry:" << this->geometry()
             << "Window center:" << windowCenter
             << "Current screen:" << currentScreen->name()
             << "Screen geometry:" << screenGeometry;

    // Calculate overlay dimensions relative to current screen
    int overlayWidth = qMin(screenGeometry.width() - 80, 1000);
    int overlayHeight = 160;

    // Position overlay relative to the current screen (not all screens combined)
    int x = screenGeometry.left() + (screenGeometry.width() - overlayWidth) / 2;
    int y = screenGeometry.bottom() - overlayHeight - 50; // 50px from bottom of current screen

    // Additional safety checks to ensure overlay stays within current screen bounds
    x = qMax(screenGeometry.left() + 10, qMin(x, screenGeometry.right() - overlayWidth - 10));
    y = qMax(screenGeometry.top() + 10, qMin(y, screenGeometry.bottom() - overlayHeight - 10));

    m_fullScreenControlsOverlay->setFixedSize(overlayWidth, overlayHeight);
    m_fullScreenControlsOverlay->move(x, y);

    // Ensure it stays visible and on top after position update
    m_fullScreenControlsOverlay->raise();
    m_fullScreenControlsOverlay->activateWindow();
}

void MainWindow::resetControlsHideTimer()
{
    if (!m_isFullScreen || !m_controlsHideTimer) {
        return;
    }

    // CRITICAL FIX: Add safety checks to prevent timer access during destruction
    if (m_isDestructing.load()) {
        return;
    }

    // Stop current timer and start new one with safety checks
    try {
        m_controlsHideTimer->stop();

        // Only start timer if we're still in fullscreen mode and not destructing
        if (m_isFullScreen && !m_isDestructing.load()) {
            m_controlsHideTimer->start(CONTROLS_HIDE_TIMEOUT_MS);
        }
    } catch (const std::exception& e) {
        qWarning() << "resetControlsHideTimer: Exception handling timer:" << e.what();
    } catch (...) {
        qWarning() << "resetControlsHideTimer: Unknown exception handling timer";
    }
}

} // namespace DarkPlay::UI

