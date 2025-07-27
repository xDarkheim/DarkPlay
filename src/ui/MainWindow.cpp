#include "ui/MainWindow.h"
#include "ui/ClickableSlider.h"
#include "controllers/MediaController.h"
#include "core/Application.h"
#include "core/ConfigManager.h"
#include "core/PluginManager.h"
#include "core/ThemeManager.h"
#include "media/IMediaEngine.h"
#include "media/QtMediaEngine.h"

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
    // Create central widget
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    // Create main layout
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Setup video widget and controls
    setupVideoWidget();
    setupMediaControls();
}

void MainWindow::setupVideoWidget()
{
    m_videoWidget = new QVideoWidget(this);
    m_videoWidget->setMinimumSize(480, 270); // 16:9 aspect ratio minimum
    m_videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_videoWidget->setStyleSheet("background-color: black;");

    // Enable mouse tracking for fullscreen controls
    m_videoWidget->setMouseTracking(true);

    // Add video widget to main layout with higher stretch factor
    m_mainLayout->addWidget(m_videoWidget, 100); // Higher stretch factor

    // CRITICAL FIX: Connect video widget to media controller
    if (m_mediaController) {
        connectVideoOutput();
        optimizeVideoWidgetRendering();
    }
}

void MainWindow::setupMediaControls()
{
    // Create controls widget
    m_controlsWidget = new QWidget(this);
    m_controlsWidget->setObjectName("controlsWidget"); // Set object name for theming
    m_controlsWidget->setFixedHeight(120);

    // Add controls widget to main layout
    m_mainLayout->addWidget(m_controlsWidget);

    // Create main controls layout
    m_controlsMainLayout = new QVBoxLayout(m_controlsWidget);
    m_controlsMainLayout->setContentsMargins(10, 10, 10, 10);
    m_controlsMainLayout->setSpacing(5);

    // Create progress layout
    m_progressLayout = new QHBoxLayout();

    // Time labels with object names for theming
    m_currentTimeLabel = new QLabel("00:00", this);
    m_currentTimeLabel->setObjectName("timeLabel");
    m_currentTimeLabel->setMinimumWidth(50);
    m_currentTimeLabel->setAlignment(Qt::AlignCenter);

    // Create position slider - remove custom styling, use theme
    m_positionSlider = std::make_unique<ClickableSlider>(Qt::Horizontal, this);
    m_positionSlider->setMinimum(0);
    m_positionSlider->setMaximum(0);
    m_positionSlider->setValue(0);

    m_totalTimeLabel = new QLabel("00:00", this);
    m_totalTimeLabel->setObjectName("timeLabel");
    m_totalTimeLabel->setMinimumWidth(50);
    m_totalTimeLabel->setAlignment(Qt::AlignCenter);

    // Add to progress layout
    m_progressLayout->addWidget(m_currentTimeLabel);
    m_progressLayout->addWidget(m_positionSlider.get(), 1);
    m_progressLayout->addWidget(m_totalTimeLabel);

    // Create control buttons layout
    m_controlButtonsLayout = new QHBoxLayout();

    // Create control buttons with object names for theming
    m_openFileButton = new QPushButton("📁 Open", this);
    m_previousButton = new QPushButton("⏮", this);
    m_previousButton->setObjectName("mediaButton");
    m_playPauseButton = new QPushButton("▶", this);
    m_playPauseButton->setObjectName("playPauseButton");
    m_nextButton = new QPushButton("⏭", this);
    m_nextButton->setObjectName("mediaButton");

    // Set fixed sizes for consistency
    m_previousButton->setFixedSize(50, 50);
    m_playPauseButton->setFixedSize(60, 60);
    m_nextButton->setFixedSize(50, 50);
    m_openFileButton->setFixedHeight(40);

    // Volume controls - remove custom styling, use theme
    m_volumeLabel = new QLabel("🔊", this);
    m_volumeLabel->setStyleSheet(R"(
        QLabel {
            font-size: 18px;
            padding: 5px;
            min-width: 30px;
            min-height: 30px;
        }
    )");

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(70);
    m_volumeSlider->setMaximumWidth(120);

    // Loading progress bar
    m_loadingProgressBar = new QProgressBar(this);
    m_loadingProgressBar->setVisible(false);
    m_loadingProgressBar->setMaximumHeight(4);

    // Add widgets to control buttons layout
    m_controlButtonsLayout->addWidget(m_openFileButton);
    m_controlButtonsLayout->addStretch();
    m_controlButtonsLayout->addWidget(m_previousButton);
    m_controlButtonsLayout->addWidget(m_playPauseButton);
    m_controlButtonsLayout->addWidget(m_nextButton);
    m_controlButtonsLayout->addStretch();
    m_controlButtonsLayout->addWidget(m_volumeLabel);
    m_controlButtonsLayout->addWidget(m_volumeSlider);

    // Add layouts to main controls layout
    m_controlsMainLayout->addLayout(m_progressLayout);
    m_controlsMainLayout->addLayout(m_controlButtonsLayout);
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
    connect(m_positionSlider.get(), &QSlider::sliderPressed, [this]() {
        m_isSeekingByUser = true;
    });

    connect(m_positionSlider.get(), &QSlider::sliderReleased, [this]() {
        m_isSeekingByUser = false;
        // Perform seek only when user releases the slider
        if (m_mediaController) {
            m_mediaController->seek(m_positionSlider->value());
        }
    });

    // Handle clicks on the slider track (instant seeking)
    connect(m_positionSlider.get(), &QSlider::sliderMoved, [this](int value) {
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
    disconnect(m_controlsHideTimer.get(), &QTimer::timeout, this, nullptr);
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

        // Connect to theme manager signals
        if (auto* themeManager = m_app->themeManager()) {
            connect(themeManager, &Core::ThemeManager::themeChanged,
                    this, &MainWindow::onThemeChanged);
            connect(themeManager, &Core::ThemeManager::systemThemeChanged,
                    this, [this](bool isDark) {
                        QString themeType = isDark ? "dark" : "light";
                        statusBar()->showMessage(QString("System theme changed to: %1").arg(themeType), 2000);
                    });

            // Apply theme to window frame
            themeManager->adaptWindowFrame(this);
        }
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

    // Add debug info to understand what's happening
    qint64 currentPosition = m_mediaController->position();
    qint64 duration = m_mediaController->duration();
    Media::PlaybackState currentState = m_mediaController->state();

    qDebug() << "togglePlayPause: State=" << static_cast<int>(currentState)
             << "Position=" << currentPosition
             << "Duration=" << duration
             << "IsFullScreen=" << m_isFullScreen;

    auto state = m_mediaController->state();
    if (state == Media::PlaybackState::Playing) {
        m_mediaController->pause();
    } else {
        // FIX: If media has ended (stopped and at the end), reset to beginning
        if (state == Media::PlaybackState::Stopped &&
            duration > 0 && currentPosition >= duration - 1000) { // 1 second tolerance

            qDebug() << "togglePlayPause: Media has ended, resetting to beginning";
            m_mediaController->seek(0);

            // Update UI sliders immediately
            if (m_positionSlider) {
                m_positionSlider->setValue(0);
            }
            if (m_fullScreenProgressSlider) {
                m_fullScreenProgressSlider->setValue(0);
            }
        }

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
                    QPointer<ClickableSlider> safeSlider(m_positionSlider.get()); // Используем .get()
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

        // CRITICAL FIX: Periodically sync play/pause button state to prevent desync
        // This ensures button state is correct even if signals are missed
        static int updateCounter = 0;
        updateCounter++;
        if (updateCounter % 10 == 0) { // Every 10th update (roughly every second with default 100ms timer)
            Media::PlaybackState currentState = m_mediaController->state();
            QString expectedButtonText = (currentState == Media::PlaybackState::Playing) ? "⏸" : "▶";

            // Check main button
            if (m_playPauseButton && m_playPauseButton->text() != expectedButtonText) {
                m_playPauseButton->setText(expectedButtonText);
                qDebug() << "updateTimeLabels: Fixed main button desync to:" << expectedButtonText;
            }

            // Check fullscreen button
            if (m_fullScreenPlayPauseButton && m_fullScreenPlayPauseButton->text() != expectedButtonText) {
                m_fullScreenPlayPauseButton->setText(expectedButtonText);
                qDebug() << "updateTimeLabels: Fixed fullscreen button desync to:" << expectedButtonText;
            }
        }
    }
}

void MainWindow::updatePlayPauseButton()
{
    if (!m_mediaController) return;

    // Get current state with additional validation
    Media::PlaybackState currentState = m_mediaController->state();
    bool isPlaying = (currentState == Media::PlaybackState::Playing);
    QString buttonText = isPlaying ? "⏸" : "▶";

    // Update main window button
    if (m_playPauseButton) {
        m_playPauseButton->setText(buttonText);
        // Add debug info for troubleshooting
        qDebug() << "MainWindow: Updated main play/pause button to:" << buttonText
                 << "State:" << static_cast<int>(currentState);
    }

    // Update fullscreen button if it exists
    if (m_fullScreenPlayPauseButton) {
        m_fullScreenPlayPauseButton->setText(buttonText);
        qDebug() << "MainWindow: Updated fullscreen play/pause button to:" << buttonText;
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
                    disconnect(m_positionSlider.get(), nullptr, m_fullScreenProgressSlider.data(), nullptr);
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
        } else if (event->button() == Qt::RightButton) {
            // Show context menu on right click in fullscreen
            showContextMenu(event->globalPos());
            return;
        }
    } else {
        // Handle right click in normal mode
        if (event->button() == Qt::RightButton) {
            showContextMenu(event->globalPos());
            return;
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

void MainWindow::optimizeVideoWidgetRendering()
{
    if (!m_videoWidget) {
        return;
    }

    // Enable hardware acceleration if available
    m_videoWidget->setAttribute(Qt::WA_OpaquePaintEvent, true);
    m_videoWidget->setAttribute(Qt::WA_NoSystemBackground, true);

    // Set optimal size policy for video rendering
    m_videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Enable mouse tracking for fullscreen controls
    m_videoWidget->setMouseTracking(true);

    qDebug() << "Video widget rendering optimized";
}

void MainWindow::connectVideoOutput()
{
    if (!m_videoWidget || !m_mediaController) {
        qWarning() << "connectVideoOutput: Video widget or media controller not available";
        return;
    }

    // Get the media manager from the media controller
    auto* mediaManager = m_mediaController->mediaManager();
    if (!mediaManager) {
        qWarning() << "connectVideoOutput: Media manager not available";
        return;
    }

    // Get the current media engine
    auto* mediaEngine = mediaManager->mediaEngine();
    if (!mediaEngine) {
        qWarning() << "connectVideoOutput: Media engine not available";
        return;
    }

    // Try to cast to QtMediaEngine to access setVideoSink method
    if (auto* qtMediaEngine = dynamic_cast<Media::QtMediaEngine*>(mediaEngine)) {
        // Set the video sink to connect video output to the widget
        qtMediaEngine->setVideoSink(m_videoWidget->videoSink());
        qDebug() << "connectVideoOutput: Video output connected successfully";
    } else {
        qWarning() << "connectVideoOutput: Media engine is not QtMediaEngine, cannot connect video output";
    }
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
    m_fullScreenControlsOverlay->setStyleSheet(generateFullScreenStyleSheet());

    // Create layout for overlay
    auto* overlayLayout = new QVBoxLayout(m_fullScreenControlsOverlay);
    overlayLayout->setContentsMargins(20, 12, 20, 12);
    overlayLayout->setSpacing(10);

    // Progress section
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

    auto* totalTimeLabel = new QLabel("00:00", m_fullScreenControlsOverlay);
    totalTimeLabel->setObjectName("TimeLabel");
    totalTimeLabel->setMinimumWidth(55);
    totalTimeLabel->setAlignment(Qt::AlignCenter);

    m_fullScreenCurrentTimeLabel = currentTimeLabel;
    m_fullScreenTotalTimeLabel = totalTimeLabel;
    m_fullScreenProgressSlider = progressSlider;

    // CRITICAL: Connect fullscreen progress slider signals for seeking
    connect(progressSlider, &QSlider::sliderPressed, [this]() {
        m_isSeekingByUser = true;
        if (m_controlsHideTimer) {
            m_controlsHideTimer->stop(); // Keep controls visible while seeking
        }
    });

    connect(progressSlider, &QSlider::sliderReleased, [this]() {
        m_isSeekingByUser = false;
        // Perform seek when user releases the slider
        if (m_mediaController && m_fullScreenProgressSlider) {
            qint64 seekPosition = m_fullScreenProgressSlider->value();
            qint64 duration = m_mediaController->duration();

            // FIX: If user tries to seek to the very end, and media has ended, reset to beginning
            if (duration > 0 && seekPosition >= duration - 100 &&
                m_mediaController->state() == Media::PlaybackState::Stopped) {
                seekPosition = 0;
                m_fullScreenProgressSlider->setValue(0);
                // Also start playing automatically when seeking from end to beginning
                m_mediaController->play();
            } else {
                m_mediaController->seek(seekPosition);
            }
        }
        resetControlsHideTimer(); // Restart hide timer after seeking
    });

    // Handle clicks on the slider track for instant seeking in fullscreen
    connect(progressSlider, &QSlider::sliderMoved, [this](int value) {
        // Allow instant seeking when user clicks on the track in fullscreen
        if (m_mediaController && m_isSeekingByUser) {
            m_mediaController->seek(value);
        }
        resetControlsHideTimer(); // Keep controls visible during interaction
    });

    // Sync fullscreen slider range with main slider when available
    if (m_positionSlider) {
        progressSlider->setRange(m_positionSlider->minimum(), m_positionSlider->maximum());
        progressSlider->setValue(m_positionSlider->value());
    }

    progressLayout->addWidget(currentTimeLabel);
    progressLayout->addWidget(progressSlider, 1);
    progressLayout->addWidget(totalTimeLabel);

    // Control buttons section
    auto* buttonsLayout = new QHBoxLayout();
    buttonsLayout->setSpacing(15);

    // Previous track button
    auto* previousBtn = new QPushButton("⏮", m_fullScreenControlsOverlay);
    previousBtn->setObjectName("MediaButton");
    previousBtn->setFixedSize(50, 50);

    connect(previousBtn, &QPushButton::clicked, [this]() {
        previousTrack();
        resetControlsHideTimer();
    });

    // Play/pause button with enhanced styling
    auto* playPauseBtn = new QPushButton("▶", m_fullScreenControlsOverlay);
    playPauseBtn->setObjectName("PlayPauseButton");
    playPauseBtn->setFixedSize(70, 70); // Slightly larger than media buttons

    connect(playPauseBtn, &QPushButton::clicked, [this]() {
        // Always use the main togglePlayPause method to ensure consistent behavior
        togglePlayPause();
        resetControlsHideTimer();
    });

    m_fullScreenPlayPauseButton = playPauseBtn;

    // CRITICAL FIX: Sync fullscreen button with current playback state immediately
    if (m_mediaController) {
        Media::PlaybackState currentState = m_mediaController->state();
        QString buttonText = (currentState == Media::PlaybackState::Playing) ? "⏸" : "▶";
        playPauseBtn->setText(buttonText);
        qDebug() << "createFullScreenOverlay: Synced fullscreen button with current state:"
                 << buttonText << "State:" << static_cast<int>(currentState);
    }

    // Next track button
    auto* nextBtn = new QPushButton("⏭", m_fullScreenControlsOverlay);
    nextBtn->setObjectName("MediaButton");
    nextBtn->setFixedSize(50, 50);

    connect(nextBtn, &QPushButton::clicked, [this]() {
        nextTrack();
        resetControlsHideTimer();
    });

    // Add volume control to fullscreen overlay
    auto* volumeLabel = new QLabel("🔊", m_fullScreenControlsOverlay);
    volumeLabel->setStyleSheet(R"(
        QLabel {
            font-size: 18px;
            color: white;
            padding: 8px;
            min-width: 30px;
            border-radius: 6px;
            background: rgba(0, 0, 0, 100);
        }
    )");

    auto* volumeSlider = new QSlider(Qt::Horizontal, m_fullScreenControlsOverlay);
    volumeSlider->setObjectName("VolumeSlider");
    volumeSlider->setRange(0, 100);
    volumeSlider->setMaximumWidth(120);

    // Sync with main volume slider
    if (m_volumeSlider) {
        volumeSlider->setValue(m_volumeSlider->value());
    }

    // Connect volume slider
    connect(volumeSlider, &QSlider::valueChanged, [this](int value) {
        onVolumeChanged(value);
        // Also update main volume slider to keep them in sync
        if (m_volumeSlider) {
            m_volumeSlider->blockSignals(true);
            m_volumeSlider->setValue(value);
            m_volumeSlider->blockSignals(false);
        }
        resetControlsHideTimer();
    });

    m_fullScreenVolumeSlider = volumeSlider;

    // Layout the buttons: Media controls in center, volume controls on right
    buttonsLayout->addStretch(1); // Left stretch for centering
    buttonsLayout->addWidget(previousBtn);
    buttonsLayout->addWidget(playPauseBtn);
    buttonsLayout->addWidget(nextBtn);
    buttonsLayout->addStretch(1); // Right stretch for centering
    buttonsLayout->addWidget(volumeLabel);
    buttonsLayout->addWidget(volumeSlider);

    // Assemble final overlay layout
    overlayLayout->addLayout(progressLayout);
    overlayLayout->addLayout(buttonsLayout);

    // Position overlay at bottom center of screen
    QScreen* currentScreen = QApplication::screenAt(this->pos());
    if (!currentScreen) {
        currentScreen = QApplication::primaryScreen();
    }

    QRect screenGeometry = currentScreen->geometry();
    int overlayWidth = qMin(screenGeometry.width() - 80, 800);
    int overlayHeight = 120;

    int x = screenGeometry.left() + (screenGeometry.width() - overlayWidth) / 2;
    int y = screenGeometry.bottom() - overlayHeight - 50;

    m_fullScreenControlsOverlay->setFixedSize(overlayWidth, overlayHeight);
    m_fullScreenControlsOverlay->move(x, y);

    // Make overlay visible
    m_fullScreenControlsOverlay->show();
    m_fullScreenControlsOverlay->raise();
    m_fullScreenControlsOverlay->activateWindow();

    qDebug() << "createFullScreenOverlay: Overlay created successfully";
}

void MainWindow::showFullScreenUI()
{
    if (!m_isFullScreen) {
        return;
    }

    // Show cursor
    if (m_cursorHidden) {
        setCursor(Qt::ArrowCursor);
        m_cursorHidden = false;
    }

    // Ensure overlay exists
    if (!m_fullScreenControlsOverlay) {
        if (isFullScreen()) {
            createFullScreenOverlay();
        }
        if (!m_fullScreenControlsOverlay) {
            return;
        }
    }

    // Show controls overlay
    if (!m_controlsVisible) {
        m_fullScreenControlsOverlay->show();
        m_fullScreenControlsOverlay->raise();
        m_controlsVisible = true;
    }

    // Reset hide timer
    resetControlsHideTimer();
}

void MainWindow::hideFullScreenUI()
{
    if (!m_isFullScreen) {
        return;
    }

    // Hide controls overlay
    if (m_fullScreenControlsOverlay && m_controlsVisible) {
        m_fullScreenControlsOverlay->hide();
        m_controlsVisible = false;
    }

    // Hide cursor
    if (!m_cursorHidden) {
        setCursor(Qt::BlankCursor);
        m_cursorHidden = true;
    }
}

void MainWindow::updateOverlayPosition()
{
    if (!m_fullScreenControlsOverlay || ! m_isFullScreen) {
        return;
    }

    // Get current screen
    QPoint windowCenter = QPoint(x() + width() / 2, y() + height() / 2);
    QScreen* currentScreen = QApplication::screenAt(windowCenter);
    if (!currentScreen) {
        currentScreen = QApplication::primaryScreen();
    }

    QRect screenGeometry = currentScreen->geometry();
    int overlayWidth = qMin(screenGeometry.width() - 80, 800);
    int overlayHeight = 120;

    int x = screenGeometry.left() + (screenGeometry.width() - overlayWidth) / 2;
    int y = screenGeometry.bottom() - overlayHeight - 50;

    m_fullScreenControlsOverlay->setFixedSize(overlayWidth, overlayHeight);
    m_fullScreenControlsOverlay->move(x, y);
    m_fullScreenControlsOverlay->raise();
}

void MainWindow::resetControlsHideTimer()
{
    if (!m_isFullScreen || !m_controlsHideTimer) {
        return;
    }

    if (m_isDestructing.load()) {
        return;
    }

    try {
        m_controlsHideTimer->stop();
        if (m_isFullScreen && !m_isDestructing.load()) {
            m_controlsHideTimer->start(CONTROLS_HIDE_TIMEOUT_MS);
        }
    } catch (const std::exception& e) {
        qWarning() << "resetControlsHideTimer: Exception handling timer:" << e.what();
    }
}

void MainWindow::showPreferences()
{
    QMessageBox::information(this, "Preferences",
                           "Preferences dialog will be implemented in a future version.\n\n"
                           "For now, you can change themes from the View menu.");
}

void MainWindow::showPluginManager()
{
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
        QMessageBox::warning(this, "Plugin Manager", "Plugin manager is not available.");
    }
}

void MainWindow::showContextMenu(const QPoint& position)
{
    // Create context menu
    QMenu contextMenu(this);
    contextMenu.setObjectName("contextMenu"); // For theming

    // File operations section
    auto* openAction = contextMenu.addAction("📁 Open File...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);

    // Recent files submenu
    if (!m_recentFiles.isEmpty()) {
        auto* recentMenu = contextMenu.addMenu("Recent Files");
        for (const QString& file : m_recentFiles) {
            QFileInfo fileInfo(file);
            auto* recentAction = recentMenu->addAction(fileInfo.fileName());
            recentAction->setData(file);
            recentAction->setToolTip(file);
            connect(recentAction, &QAction::triggered, this, &MainWindow::openRecentFile);
        }
    }

    contextMenu.addSeparator();

    // Playback controls section
    if (m_mediaController && m_mediaController->hasMedia()) {
        // Play/Pause
        auto state = m_mediaController->state();
        QString playPauseText = (state == Media::PlaybackState::Playing) ? "⏸ Pause" : "▶ Play";
        auto* playPauseAction = contextMenu.addAction(playPauseText);
        playPauseAction->setShortcut(Qt::Key_Space);
        connect(playPauseAction, &QAction::triggered, this, &MainWindow::togglePlayPause);

        // Stop
        auto* stopAction = contextMenu.addAction("⏹ Stop");
        connect(stopAction, &QAction::triggered, this, &MainWindow::stopPlayback);

        contextMenu.addSeparator();

        // Seek controls
        auto* seekBackAction = contextMenu.addAction("⏪ Seek Back 10s");
        seekBackAction->setShortcut(Qt::Key_Left);
        connect(seekBackAction, &QAction::triggered, [this]() {
            if (m_mediaController) {
                m_mediaController->seek(m_mediaController->position() - 10000);
            }
        });

        auto* seekForwardAction = contextMenu.addAction("⏩ Seek Forward 10s");
        seekForwardAction->setShortcut(Qt::Key_Right);
        connect(seekForwardAction, &QAction::triggered, [this]() {
            if (m_mediaController) {
                m_mediaController->seek(m_mediaController->position() + 10000);
            }
        });

        contextMenu.addSeparator();
    }

    // Volume controls
    auto* volumeMenu = contextMenu.addMenu("🔊 Volume");

    // Volume presets
    auto* volume25Action = volumeMenu->addAction("25%");
    connect(volume25Action, &QAction::triggered, [this]() {
        if (m_volumeSlider) m_volumeSlider->setValue(25);
    });

    auto* volume50Action = volumeMenu->addAction("50%");
    connect(volume50Action, &QAction::triggered, [this]() {
        if (m_volumeSlider) m_volumeSlider->setValue(50);
    });

    auto* volume75Action = volumeMenu->addAction("75%");
    connect(volume75Action, &QAction::triggered, [this]() {
        if (m_volumeSlider) m_volumeSlider->setValue(75);
    });

    auto* volume100Action = volumeMenu->addAction("100%");
    connect(volume100Action, &QAction::triggered, [this]() {
        if (m_volumeSlider) m_volumeSlider->setValue(100);
    });

    volumeMenu->addSeparator();

    auto* muteAction = volumeMenu->addAction("🔇 Mute");
    muteAction->setCheckable(true);
    muteAction->setChecked(m_volumeSlider && m_volumeSlider->value() == 0);
    connect(muteAction, &QAction::triggered, [this](bool mute) {
        if (m_volumeSlider) {
            static int lastVolume = 70; // Remember last volume
            if (mute) {
                lastVolume = m_volumeSlider->value();
                m_volumeSlider->setValue(0);
            } else {
                m_volumeSlider->setValue(lastVolume > 0 ? lastVolume : 70);
            }
        }
    });

    // View options section
    contextMenu.addSeparator();

    auto* fullscreenAction = contextMenu.addAction(m_isFullScreen ? "🗗 Exit Fullscreen" : "🗖 Fullscreen");
    fullscreenAction->setShortcut(Qt::Key_F11);
    connect(fullscreenAction, &QAction::triggered, this, &MainWindow::toggleFullScreen);

    // Aspect ratio submenu
    auto* aspectMenu = contextMenu.addMenu("📐 Aspect Ratio");
    auto* aspectGroup = new QActionGroup(this);

    auto* autoAspectAction = aspectMenu->addAction("Auto");
    autoAspectAction->setCheckable(true);
    autoAspectAction->setChecked(true);
    aspectGroup->addAction(autoAspectAction);
    connect(autoAspectAction, &QAction::triggered, [this]() {
        if (m_videoWidget) {
            m_videoWidget->setAspectRatioMode(Qt::KeepAspectRatio);
        }
    });

    auto* stretchAspectAction = aspectMenu->addAction("Stretch to Fill");
    stretchAspectAction->setCheckable(true);
    aspectGroup->addAction(stretchAspectAction);
    connect(stretchAspectAction, &QAction::triggered, [this]() {
        if (m_videoWidget) {
            m_videoWidget->setAspectRatioMode(Qt::IgnoreAspectRatio);
        }
    });

    auto* aspect43Action = aspectMenu->addAction("4:3");
    aspect43Action->setCheckable(true);
    aspectGroup->addAction(aspect43Action);
    connect(aspect43Action, &QAction::triggered, [this]() {
        if (m_videoWidget) {
            m_videoWidget->setAspectRatioMode(Qt::KeepAspectRatioByExpanding);
        }
    });

    auto* aspect169Action = aspectMenu->addAction("16:9");
    aspect169Action->setCheckable(true);
    aspectGroup->addAction(aspect169Action);
    connect(aspect169Action, &QAction::triggered, [this]() {
        if (m_videoWidget) {
            m_videoWidget->setAspectRatioMode(Qt::KeepAspectRatio);
        }
    });

    // Theme submenu
    auto* themeMenu = contextMenu.addMenu("🎨 Theme");
    auto* themeGroup = new QActionGroup(this);

    if (m_app && m_app->themeManager()) {
        QStringList themes = m_app->themeManager()->availableThemes();
        QString currentTheme = m_app->themeManager()->currentTheme();

        for (const QString& theme : themes) {
            auto* themeAction = themeMenu->addAction(theme);
            themeAction->setCheckable(true);
            themeAction->setChecked(theme == currentTheme);
            themeGroup->addAction(themeAction);

            connect(themeAction, &QAction::triggered, [this, theme]() {
                if (m_app && m_app->themeManager()) {
                    m_app->themeManager()->loadTheme(theme);
                }
            });
        }
    }

    // Tools section
    contextMenu.addSeparator();

    auto* preferencesAction = contextMenu.addAction("⚙️ Preferences...");
    preferencesAction->setShortcut(QKeySequence::Preferences);
    connect(preferencesAction, &QAction::triggered, this, &MainWindow::showPreferences);

    auto* pluginManagerAction = contextMenu.addAction("🔌 Plugin Manager...");
    connect(pluginManagerAction, &QAction::triggered, this, &MainWindow::showPluginManager);

    // About section
    contextMenu.addSeparator();

    auto* aboutAction = contextMenu.addAction("ℹ️ About DarkPlay");
    connect(aboutAction, &QAction::triggered, [this]() {
        QMessageBox::about(this, "About DarkPlay",
                          QString("DarkPlay Media Player v%1\n\n"
                                 "A modern, extensible media player built with Qt 6\n"
                                 "and modern C++ architecture.\n\n"
                                 "Right-click for context menu\n"
                                 "Double-click video area for fullscreen")
                          .arg(QApplication::applicationVersion()));
    });

    // Show context menu at the requested position
    // In fullscreen mode, show the UI controls when context menu is shown
    if (m_isFullScreen) {
        showFullScreenUI();
    }

    contextMenu.exec(position);
}

QString MainWindow::generateFullScreenStyleSheet() const
{
    // Determine if we're using dark theme
    bool isDarkTheme = false;
    if (m_app && m_app->themeManager()) {
        isDarkTheme = m_app->themeManager()->isSystemDarkTheme();
    }

    // Define colors based on theme
    QString overlayBg1, overlayBg2, borderColor, accentColor, accentHover, accentPressed;
    QString buttonBg1, buttonBg2, buttonHoverBg1, buttonHoverBg2, buttonPressedBg1, buttonPressedBg2;
    QString textColor, timeLabelBg, progressGroove, progressHandle;

    if (isDarkTheme) {
        // Black-red color scheme for dark theme
        overlayBg1 = "rgba(26, 26, 26, 245)";      // Near black
        overlayBg2 = "rgba(18, 18, 18, 235)";      // Darker black
        borderColor = "rgba(231, 76, 60, 80)";      // Red border
        accentColor = "rgba(231, 76, 60, 220)";     // Bright red
        accentHover = "rgba(192, 57, 43, 200)";     // Darker red hover
        accentPressed = "rgba(169, 50, 38, 180)";   // Even darker red pressed

        buttonBg1 = "rgba(45, 45, 45, 200)";        // Dark gray
        buttonBg2 = "rgba(35, 35, 35, 180)";        // Darker gray
        buttonHoverBg1 = "rgba(231, 76, 60, 180)";  // Red hover
        buttonHoverBg2 = "rgba(192, 57, 43, 160)";  // Darker red hover
        buttonPressedBg1 = "rgba(169, 50, 38, 160)"; // Pressed red
        buttonPressedBg2 = "rgba(148, 44, 33, 140)"; // Darker pressed red

        textColor = "white";
        timeLabelBg = "rgba(18, 18, 18, 150)";
        progressGroove = "rgba(255, 255, 255, 100)";
        progressHandle = "rgba(231, 76, 60, 255)";   // Red handle
    } else {
        // White-red color scheme for light theme
        overlayBg1 = "rgba(255, 255, 255, 245)";    // White
        overlayBg2 = "rgba(248, 248, 248, 235)";    // Light gray
        borderColor = "rgba(220, 53, 69, 80)";      // Red border
        accentColor = "rgba(220, 53, 69, 220)";     // Red
        accentHover = "rgba(167, 30, 42, 200)";     // Darker red hover
        accentPressed = "rgba(143, 26, 36, 180)";   // Even darker red pressed

        buttonBg1 = "rgba(248, 248, 248, 200)";     // Light gray
        buttonBg2 = "rgba(240, 240, 240, 180)";     // Lighter gray
        buttonHoverBg1 = "rgba(220, 53, 69, 180)";  // Red hover
        buttonHoverBg2 = "rgba(167, 30, 42, 160)";  // Darker red hover
        buttonPressedBg1 = "rgba(143, 26, 36, 160)"; // Pressed red
        buttonPressedBg2 = "rgba(128, 23, 32, 140)"; // Darker pressed red

        textColor = "black";
        timeLabelBg = "rgba(255, 255, 255, 150)";
        progressGroove = "rgba(0, 0, 0, 100)";
        progressHandle = "rgba(220, 53, 69, 255)";   // Red handle
    }

    return QString(
        // Main overlay container
        "QWidget#FullScreenOverlay {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "        stop:0 %1, stop:1 %2);"
        "    border: 2px solid %3;"
        "    border-radius: 16px;"
        "}"

        // Play/Pause button
        "QPushButton#PlayPauseButton {"
        "    background: qradialGradient(cx:0.5, cy:0.5, radius:0.8, "
        "        stop:0 %4, stop:1 %5);"
        "    border: 3px solid rgba(255, 255, 255, 120);"
        "    border-radius: 30px;"
        "    color: %6;"
        "    font-size: 24px;"
        "    min-width: 60px;"
        "    min-height: 60px;"
        "}"
        "QPushButton#PlayPauseButton:hover {"
        "    background: qradialGradient(cx:0.5, cy:0.5, radius:0.8, "
        "        stop:0 %7, stop:1 %8);"
        "}"
        "QPushButton#PlayPauseButton:pressed {"
        "    background: qradialGradient(cx:0.5, cy:0.5, radius:0.8, "
        "        stop:0 %9, stop:1 %10);"
        "}"

        // Progress slider
        "QSlider#ProgressSlider::groove:horizontal {"
        "    background: %11;"
        "    height: 6px;"
        "    border-radius: 3px;"
        "}"
        "QSlider#ProgressSlider::handle:horizontal {"
        "    background: %12;"
        "    width: 16px;"
        "    height: 16px;"
        "    border-radius: 8px;"
        "    border: 2px solid %3;"
        "    margin: -7px 0;"
        "}"
        "QSlider#ProgressSlider::sub-page:horizontal {"
        "    background: %4;"
        "    border-radius: 3px;"
        "}"

        // Time labels
        "QLabel#TimeLabel {"
        "    background: %13;"
        "    border: 1px solid %3;"
        "    border-radius: 8px;"
        "    padding: 6px 12px;"
        "    color: %6;"
        "    font-family: monospace;"
        "    font-size: 13px;"
        "    min-width: 55px;"
        "}"

        // Media control buttons
        "QPushButton#MediaButton {"
        "    background: qradialGradient(cx:0.5, cy:0.5, radius:0.8, "
        "        stop:0 %14, stop:1 %15);"
        "    border: 2px solid rgba(255, 255, 255, 100);"
        "    border-radius: 25px;"
        "    color: %6;"
        "    font-size: 20px;"
        "    font-weight: bold;"
        "}"
        "QPushButton#MediaButton:hover {"
        "    background: qradialGradient(cx:0.5, cy:0.5, radius:0.8, "
        "        stop:0 %16, stop:1 %17);"
        "    border: 2px solid rgba(255, 255, 255, 150);"
        "}"
        "QPushButton#MediaButton:pressed {"
        "    background: qradialGradient(cx:0.5, cy:0.5, radius:0.8, "
        "        stop:0 %18, stop:1 %19);"
        "}"

        // Volume slider
        "QSlider#VolumeSlider::groove:horizontal {"
        "    background: %11;"
        "    height: 4px;"
        "    border-radius: 2px;"
        "}"
        "QSlider#VolumeSlider::handle:horizontal {"
        "    background: rgba(255, 255, 255, 255);"
        "    width: 14px;"
        "    height: 14px;"
        "    border-radius: 7px;"
        "    border: 1px solid %4;"
        "    margin: -6px 0;"
        "}"
        "QSlider#VolumeSlider::handle:horizontal:hover {"
        "    background: %4;"
        "    width: 16px;"
        "    height: 16px;"
        "    border-radius: 8px;"
        "    margin: -7px 0;"
        "}"
        "QSlider#VolumeSlider::sub-page:horizontal {"
        "    background: %4;"
        "    border-radius: 2px;"
        "}"
    ).arg(overlayBg1, overlayBg2, borderColor, accentColor, accentHover,        // 1-5
          textColor, buttonHoverBg1, buttonHoverBg2, buttonPressedBg1,         // 6-9
          buttonPressedBg2, progressGroove, progressHandle, timeLabelBg,       // 10-13
          buttonBg1, buttonBg2, buttonHoverBg1, buttonHoverBg2,               // 14-17
          buttonPressedBg1, buttonPressedBg2);                                // 18-19
}

} // namespace DarkPlay::UI

