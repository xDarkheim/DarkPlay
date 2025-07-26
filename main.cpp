#include <QApplication>
#include "core/Application.h"
#include "ui/MainWindow.h"
#include "utils/QtEnvironmentSetup.h"
#include <QMessageBox>
#include <QDebug>
#include <memory>

int main(int argc, char *argv[])
{
    // Setup optimal Qt environment BEFORE creating QApplication
    DarkPlay::Utils::setupOptimalQtEnvironment();

    try {
        // Create application instance with RAII
        DarkPlay::Core::Application app(argc, argv);

        // Optional: output environment info for debugging
        #ifdef QT_DEBUG
        DarkPlay::Utils::logQtEnvironmentInfo();
        #endif

        // Initialize core application systems with proper error handling
        if (!app.initialize()) {
            qCritical() << "Failed to initialize application";
            QMessageBox::critical(nullptr, "Initialization Error",
                                "Failed to initialize DarkPlay application.\n"
                                "Please check the installation and try again.");
            return -1;
        }

        // Create main window with exception safety
        std::unique_ptr<DarkPlay::UI::MainWindow> window;
        try {
            window = std::make_unique<DarkPlay::UI::MainWindow>();
            window->show();
        } catch (const std::exception& e) {
            qCritical() << "Failed to create main window:" << e.what();
            QMessageBox::critical(nullptr, "UI Error",
                                QString("Failed to create main window:\n%1").arg(e.what()));
            return -2;
        }

        // Run application event loop
        const int result = QApplication::exec();

        // Explicit cleanup (though RAII handles this automatically)
        window.reset();

        return result;

    } catch (const std::exception& e) {
        qCritical() << "Critical error in main:" << e.what();
        QMessageBox::critical(nullptr, "Critical Error",
                            QString("A critical error occurred:\n%1").arg(e.what()));
        return -3;
    } catch (...) {
        qCritical() << "Unknown critical error in main";
        QMessageBox::critical(nullptr, "Critical Error",
                            "An unknown critical error occurred during startup.");
        return -4;
    }
}
