#ifndef SETTINGDIALOG_H
#define SETTINGDIALOG_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QLineEdit>
#include <QLabel>
#include <QGroupBox>
#include <QFormLayout>
#include <QFileDialog>

namespace DarkPlay::Core {
    class ConfigManager;
}

namespace DarkPlay::UI {

    class SettingDialog : public QDialog {
        Q_OBJECT

    public:
        explicit SettingDialog(QWidget *parent = nullptr);
        ~SettingDialog() override = default;

    private slots:
        void onAccepted();
        void onRejected();
        void onApplyClicked();
        void onAutoPlayToggled(bool enabled);
        void onVolumeChanged(int volume);
        void browseForDirectory();

    private:
        void setupUI();
        void createGeneralTab();
        void createMediaTab();
        void createInterfaceTab();
        void loadSettings();
        void saveSettings();
        void applySettings();

        // UI Elements
        QTabWidget* m_tabWidget;
        QDialogButtonBox* m_buttonBox;
        QPushButton* m_applyButton;

        // General Settings
        QCheckBox* m_autoPlayCheckBox;
        QCheckBox* m_rememberPositionCheckBox;
        QLineEdit* m_defaultDirectoryEdit;
        QPushButton* m_browseButton;
        QSpinBox* m_recentFilesCountSpinBox;

        // Media Settings
        QSlider* m_defaultVolumeSlider;
        QLabel* m_volumeLabel;
        QCheckBox* m_hardwareAccelerationCheckBox;
        QComboBox* m_audioOutputComboBox;
        QCheckBox* m_subtitleAutoLoadCheckBox;

        // Interface Settings
        QCheckBox* m_showStatusBarCheckBox;
        QCheckBox* m_hideControlsInFullscreenCheckBox;
        QSpinBox* m_controlsHideDelaySpinBox;

        // Manager
        Core::ConfigManager* m_configManager;
    };
} // namespace DarkPlay::UI
#endif //SETTINGDIALOG_H
