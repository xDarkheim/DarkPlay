#ifndef SETTINGDIALOG_H
#define SETTINGDIALOG_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

namespace  DarkPlay::UI {

    class SettingDialog : public QDialog {
        Q_OBJECT

    public:
        explicit SettingDialog(QWidget *parent = nullptr);
        ~SettingDialog() override = default;

    private slots:
        void onAccepted();
        void onRejected();
        void onApplyClicked();

    private:
        void setupUI();
        void createGeneralTab();
        void createMediaTab();
        void createInterfaceTab();
        void createPluginsTab();

        QTabWidget* m_tabWidget;
        QDialogButtonBox* m_buttonBox;
        QPushButton m_applyButton;
    };
} // namespace DarkPlay::UI
#endif //SETTINGDIALOG_H
