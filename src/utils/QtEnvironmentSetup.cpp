#include "utils/QtEnvironmentSetup.h"
#include <QDebug>
#include <QLoggingCategory>
#include <cstdlib>

Q_LOGGING_CATEGORY(qtEnvSetup, "darkplay.utils.qtenv")

namespace DarkPlay::Utils {

void setupOptimalQtEnvironment() {
    qCDebug(qtEnvSetup) << "Setting up optimal Qt environment for stable video rendering...";

    // Platform settings
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "xcb");
        qCDebug(qtEnvSetup) << "Set QT_QPA_PLATFORM=xcb";
    }

    // OpenGL settings for stable rendering
    if (!qEnvironmentVariableIsSet("QT_OPENGL")) {
        qputenv("QT_OPENGL", "desktop");
        qCDebug(qtEnvSetup) << "Set QT_OPENGL=desktop";
    }

    // Multimedia settings
    if (!qEnvironmentVariableIsSet("QT_MULTIMEDIA_PREFERRED_PLUGINS")) {
        qputenv("QT_MULTIMEDIA_PREFERRED_PLUGINS", "ffmpeg");
        qCDebug(qtEnvSetup) << "Set QT_MULTIMEDIA_PREFERRED_PLUGINS=ffmpeg";
    }

    // Qt Quick settings for stability
    if (!qEnvironmentVariableIsSet("QT_QUICK_BACKEND")) {
        qputenv("QT_QUICK_BACKEND", "software");
        qCDebug(qtEnvSetup) << "Set QT_QUICK_BACKEND=software";
    }

    if (!qEnvironmentVariableIsSet("QSG_RENDER_LOOP")) {
        qputenv("QSG_RENDER_LOOP", "basic");
        qCDebug(qtEnvSetup) << "Set QSG_RENDER_LOOP=basic";
    }

    // X11/XCB specific settings
    if (!qEnvironmentVariableIsSet("QT_XCB_GL_INTEGRATION")) {
        qputenv("QT_XCB_GL_INTEGRATION", "xcb_glx");
        qCDebug(qtEnvSetup) << "Set QT_XCB_GL_INTEGRATION=xcb_glx";
    }

    // Disable debug output for performance
    if (!qEnvironmentVariableIsSet("QT_LOGGING_RULES")) {
        qputenv("QT_LOGGING_RULES", "*.debug=false;qt.qpa.xcb.glx.debug=false");
        qCDebug(qtEnvSetup) << "Set QT_LOGGING_RULES to disable excessive logging";
    }

    // X11 optimizations to prevent artifacts
    if (!qEnvironmentVariableIsSet("QT_X11_NO_MITSHM")) {
        qputenv("QT_X11_NO_MITSHM", "1");
        qCDebug(qtEnvSetup) << "Set QT_X11_NO_MITSHM=1 (disable MIT-SHM for stability)";
    }

    // Disable automatic scaling
    if (!qEnvironmentVariableIsSet("QT_AUTO_SCREEN_SCALE_FACTOR")) {
        qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");
        qCDebug(qtEnvSetup) << "Set QT_AUTO_SCREEN_SCALE_FACTOR=0";
    }

    qCInfo(qtEnvSetup) << "Qt environment optimized for stable video rendering";
}

void logQtEnvironmentInfo() {
    qCInfo(qtEnvSetup) << "=== Current Qt Environment Settings ===";
    qCInfo(qtEnvSetup) << "QT_QPA_PLATFORM:" << qEnvironmentVariable("QT_QPA_PLATFORM", "not set");
    qCInfo(qtEnvSetup) << "QT_OPENGL:" << qEnvironmentVariable("QT_OPENGL", "not set");
    qCInfo(qtEnvSetup) << "QT_MULTIMEDIA_PREFERRED_PLUGINS:" << qEnvironmentVariable("QT_MULTIMEDIA_PREFERRED_PLUGINS", "not set");
    qCInfo(qtEnvSetup) << "QT_QUICK_BACKEND:" << qEnvironmentVariable("QT_QUICK_BACKEND", "not set");
    qCInfo(qtEnvSetup) << "QSG_RENDER_LOOP:" << qEnvironmentVariable("QSG_RENDER_LOOP", "not set");
    qCInfo(qtEnvSetup) << "QT_XCB_GL_INTEGRATION:" << qEnvironmentVariable("QT_XCB_GL_INTEGRATION", "not set");
    qCInfo(qtEnvSetup) << "QT_X11_NO_MITSHM:" << qEnvironmentVariable("QT_X11_NO_MITSHM", "not set");
    qCInfo(qtEnvSetup) << "QT_AUTO_SCREEN_SCALE_FACTOR:" << qEnvironmentVariable("QT_AUTO_SCREEN_SCALE_FACTOR", "not set");
    qCInfo(qtEnvSetup) << "=======================================";
}

} // namespace DarkPlay::Utils
