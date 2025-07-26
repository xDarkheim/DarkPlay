#ifndef DARKPLAY_UTILS_QTENVIRONMENTSETUP_H
#define DARKPLAY_UTILS_QTENVIRONMENTSETUP_H

namespace DarkPlay::Utils {

/**
 * @brief Sets up optimal Qt environment variables for stable video rendering
 *
 * This function establishes environment variables that ensure:
 * - Stable video rendering without visual artifacts
 * - Proper OpenGL and hardware acceleration functionality
 * - Compatibility with various graphics systems (X11/Wayland)
 * - Minimized interface flickering and lag
 *
 * Variables are only set if they haven't been defined externally,
 * allowing users to override settings when necessary.
 *
 * @note Must be called BEFORE creating QApplication
 */
void setupOptimalQtEnvironment();

/**
 * @brief Outputs current Qt environment settings to log for debugging
 */
void logQtEnvironmentInfo();

} // namespace DarkPlay::Utils

#endif // DARKPLAY_UTILS_QTENVIRONMENTSETUP_H
