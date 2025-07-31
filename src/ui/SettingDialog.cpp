#include "ui/SettingDialog.h"
#include "core/Application.h"
#include "core/ConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSlider>
#include <QLineEdit>
#include <QFormLayout>
#include <QFileDialog>
#include <QStandardPaths>
#include <QMessageBox>

namespace DarkPlay::UI {

    SettingDialog::SettingDialog(QWidget* parent)
        : QDialog(parent)
        , m_tabWidget(nullptr)
        , m_buttonBox(nullptr)
        , m_applyButton(nullptr)
        // General Settings
        , m_autoPlayCheckBox(nullptr)
        , m_rememberPositionCheckBox(nullptr)
        , m_defaultDirectoryEdit(nullptr)
        , m_browseButton(nullptr)
        , m_recentFilesCountSpinBox(nullptr)
        // Media Settings
        , m_defaultVolumeSlider(nullptr)
        , m_volumeLabel(nullptr)
        , m_hardwareAccelerationCheckBox(nullptr)
        , m_audioOutputComboBox(nullptr)
        , m_subtitleAutoLoadCheckBox(nullptr)
        // Interface Settings
        , m_showStatusBarCheckBox(nullptr)
        , m_hideControlsInFullscreenCheckBox(nullptr)
        , m_controlsHideDelaySpinBox(nullptr)
        // Manager
        , m_configManager(nullptr)
    {
        // Get config manager from Application
        auto* app = Core::Application::instance();
        if (app) {
            m_configManager = app->configManager();
        }

        setupUI();
        loadSettings();

        setWindowTitle("Settings - DarkPlay");
        setModal(true);
        resize(600, 500);
    }

    void SettingDialog::setupUI()
    {
        auto* mainLayout = new QVBoxLayout(this);

        // Create the tab widget
        m_tabWidget = new QTabWidget(this);

        // Create the tabs
        createGeneralTab();
        createMediaTab();
        createInterfaceTab();

        // Create the button box
        m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        m_applyButton = new QPushButton("Apply", this);
        m_buttonBox->addButton(m_applyButton, QDialogButtonBox::ActionRole);

        // Layout
        mainLayout->addWidget(m_tabWidget);
        mainLayout->addWidget(m_buttonBox);

        // Connect signals and slots
        connect(m_buttonBox, &QDialogButtonBox::accepted, this, &SettingDialog::onAccepted);
        connect(m_buttonBox, &QDialogButtonBox::rejected, this, &SettingDialog::onRejected);
        connect(m_applyButton, &QPushButton::clicked, this, &SettingDialog::onApplyClicked);
    }

    void SettingDialog::createGeneralTab()
    {
        auto* generalWidget = new QWidget(this);
        auto* layout = new QVBoxLayout(generalWidget);

        // Playback Settings Group
        auto* playbackGroup = new QGroupBox("Playback Settings", generalWidget);
        auto* playbackLayout = new QFormLayout(playbackGroup);

        m_autoPlayCheckBox = new QCheckBox("Auto-play files when opened");
        m_rememberPositionCheckBox = new QCheckBox("Remember playback position");

        playbackLayout->addRow(m_autoPlayCheckBox);
        playbackLayout->addRow(m_rememberPositionCheckBox);

        // File Management Group
        auto* fileGroup = new QGroupBox("File Management", generalWidget);
        auto* fileLayout = new QFormLayout(fileGroup);

        auto* dirLayout = new QHBoxLayout();
        m_defaultDirectoryEdit = new QLineEdit();
        m_browseButton = new QPushButton("Browse...");
        dirLayout->addWidget(m_defaultDirectoryEdit);
        dirLayout->addWidget(m_browseButton);

        m_recentFilesCountSpinBox = new QSpinBox();
        m_recentFilesCountSpinBox->setRange(0, 50);
        m_recentFilesCountSpinBox->setValue(10);

        fileLayout->addRow("Default directory:", dirLayout);
        fileLayout->addRow("Recent files count:", m_recentFilesCountSpinBox);

        layout->addWidget(playbackGroup);
        layout->addWidget(fileGroup);
        layout->addStretch();

        m_tabWidget->addTab(generalWidget, "General");

        // Connect signals
        connect(m_autoPlayCheckBox, &QCheckBox::toggled, this, &SettingDialog::onAutoPlayToggled);
        connect(m_browseButton, &QPushButton::clicked, this, &SettingDialog::browseForDirectory);
    }

    void SettingDialog::createMediaTab()
    {
        auto* mediaWidget = new QWidget(this);
        auto* layout = new QVBoxLayout(mediaWidget);

        // Audio Settings Group
        auto* audioGroup = new QGroupBox("Audio Settings", mediaWidget);
        auto* audioLayout = new QFormLayout(audioGroup);

        auto* volumeLayout = new QHBoxLayout();
        m_defaultVolumeSlider = new QSlider(Qt::Horizontal);
        m_defaultVolumeSlider->setRange(0, 100);
        m_defaultVolumeSlider->setValue(70);
        m_volumeLabel = new QLabel("70%");
        volumeLayout->addWidget(m_defaultVolumeSlider);
        volumeLayout->addWidget(m_volumeLabel);

        m_audioOutputComboBox = new QComboBox();
        m_audioOutputComboBox->addItems({"Default", "DirectSound", "WASAPI", "ALSA", "PulseAudio"});

        audioLayout->addRow("Default volume:", volumeLayout);
        audioLayout->addRow("Audio output:", m_audioOutputComboBox);

        // Video Settings Group
        auto* videoGroup = new QGroupBox("Video Settings", mediaWidget);
        auto* videoLayout = new QFormLayout(videoGroup);

        m_hardwareAccelerationCheckBox = new QCheckBox("Enable hardware acceleration");

        videoLayout->addRow(m_hardwareAccelerationCheckBox);

        // Subtitle Settings Group
        auto* subtitleGroup = new QGroupBox("Subtitle Settings", mediaWidget);
        auto* subtitleLayout = new QFormLayout(subtitleGroup);

        m_subtitleAutoLoadCheckBox = new QCheckBox("Auto-load subtitle files");

        subtitleLayout->addRow(m_subtitleAutoLoadCheckBox);

        layout->addWidget(audioGroup);
        layout->addWidget(videoGroup);
        layout->addWidget(subtitleGroup);
        layout->addStretch();

        m_tabWidget->addTab(mediaWidget, "Media");

        // Connect signals
        connect(m_defaultVolumeSlider, &QSlider::valueChanged, this, &SettingDialog::onVolumeChanged);
    }

    void SettingDialog::createInterfaceTab()
    {
        auto* interfaceWidget = new QWidget(this);
        auto* layout = new QVBoxLayout(interfaceWidget);

        // Interface Elements Group
        auto* elementsGroup = new QGroupBox("Interface Elements", interfaceWidget);
        auto* elementsLayout = new QFormLayout(elementsGroup);

        m_showStatusBarCheckBox = new QCheckBox("Show status bar");

        elementsLayout->addRow(m_showStatusBarCheckBox);

        // Fullscreen Settings Group
        auto* fullscreenGroup = new QGroupBox("Fullscreen Settings", interfaceWidget);
        auto* fullscreenLayout = new QFormLayout(fullscreenGroup);

        m_hideControlsInFullscreenCheckBox = new QCheckBox("Auto-hide controls in fullscreen");
        m_controlsHideDelaySpinBox = new QSpinBox();
        m_controlsHideDelaySpinBox->setRange(1, 10);
        m_controlsHideDelaySpinBox->setValue(3);
        m_controlsHideDelaySpinBox->setSuffix(" seconds");

        fullscreenLayout->addRow(m_hideControlsInFullscreenCheckBox);
        fullscreenLayout->addRow("Hide delay:", m_controlsHideDelaySpinBox);

        layout->addWidget(elementsGroup);
        layout->addWidget(fullscreenGroup);
        layout->addStretch();

        m_tabWidget->addTab(interfaceWidget, "Interface");
    }

    void SettingDialog::loadSettings()
    {
        if (!m_configManager) return;

        // Load General Settings
        m_autoPlayCheckBox->setChecked(m_configManager->getValue("playback/autoPlay", true).toBool());
        m_rememberPositionCheckBox->setChecked(m_configManager->getValue("playback/rememberPosition", true).toBool());
        m_defaultDirectoryEdit->setText(m_configManager->getValue("files/lastDirectory",
                                       QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)).toString());
        m_recentFilesCountSpinBox->setValue(m_configManager->getValue("files/maxRecentFiles", 10).toInt());

        // Load Media Settings
        int volume = static_cast<int>(m_configManager->getValue("media/volume", 0.7).toFloat() * 100);
        m_defaultVolumeSlider->setValue(volume);
        m_volumeLabel->setText(QString("%1%").arg(volume));
        m_hardwareAccelerationCheckBox->setChecked(m_configManager->getValue("media/hardwareAcceleration", true).toBool());

        QString audioOutput = m_configManager->getValue("media/audioOutput", "Default").toString();
        int audioIndex = m_audioOutputComboBox->findText(audioOutput);
        if (audioIndex >= 0) m_audioOutputComboBox->setCurrentIndex(audioIndex);

        m_subtitleAutoLoadCheckBox->setChecked(m_configManager->getValue("media/subtitleAutoLoad", true).toBool());

        // Load Interface Settings
        m_showStatusBarCheckBox->setChecked(m_configManager->getValue("ui/showStatusBar", true).toBool());
        m_hideControlsInFullscreenCheckBox->setChecked(m_configManager->getValue("ui/hideControlsInFullscreen", true).toBool());
        m_controlsHideDelaySpinBox->setValue(m_configManager->getValue("ui/controlsHideDelay", 3).toInt());
    }

    void SettingDialog::saveSettings()
    {
        if (!m_configManager) return;

        // Save General Settings
        m_configManager->setValue("playback/autoPlay", m_autoPlayCheckBox->isChecked());
        m_configManager->setValue("playback/rememberPosition", m_rememberPositionCheckBox->isChecked());
        m_configManager->setValue("files/lastDirectory", m_defaultDirectoryEdit->text());
        m_configManager->setValue("files/maxRecentFiles", m_recentFilesCountSpinBox->value());

        // Save Media Settings
        float volume = static_cast<float>(m_defaultVolumeSlider->value()) / 100.0f;
        m_configManager->setValue("media/volume", volume);
        m_configManager->setValue("media/hardwareAcceleration", m_hardwareAccelerationCheckBox->isChecked());
        m_configManager->setValue("media/audioOutput", m_audioOutputComboBox->currentText());
        m_configManager->setValue("media/subtitleAutoLoad", m_subtitleAutoLoadCheckBox->isChecked());

        // Save Interface Settings
        m_configManager->setValue("ui/showStatusBar", m_showStatusBarCheckBox->isChecked());
        m_configManager->setValue("ui/hideControlsInFullscreen", m_hideControlsInFullscreenCheckBox->isChecked());
        m_configManager->setValue("ui/controlsHideDelay", m_controlsHideDelaySpinBox->value());
    }

    void SettingDialog::applySettings()
    {
        saveSettings();
        // Settings are applied immediately through ConfigManager
    }

    void SettingDialog::onAccepted()
    {
        applySettings();
        accept();
    }

    void SettingDialog::onRejected()
    {
        reject();
    }

    void SettingDialog::onApplyClicked()
    {
        applySettings();
    }

    void SettingDialog::onAutoPlayToggled(bool enabled)
    {
        Q_UNUSED(enabled)
        // Auto-play toggle handled automatically through UI binding
    }

    void SettingDialog::onVolumeChanged(int volume)
    {
        m_volumeLabel->setText(QString("%1%").arg(volume));
    }

    void SettingDialog::browseForDirectory()
    {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Default Directory",
                                                      m_defaultDirectoryEdit->text());
        if (!dir.isEmpty()) {
            m_defaultDirectoryEdit->setText(dir);
        }
    }

} // namespace DarkPlay::UI
