cmake_minimum_required(VERSION 3.28)
project(DarkPlay VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6 packages
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Multimedia
    MultimediaWidgets
    Network
    OpenGL
    OpenGLWidgets
)

# Qt6 automoc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Include directories
include_directories(include)

# Source files
set(CORE_SOURCES
    src/core/Application.cpp
    src/core/ConfigManager.cpp
    src/core/PluginManager.cpp
    src/core/ThemeManager.cpp
)

set(MEDIA_SOURCES
    src/media/MediaManager.cpp
    src/media/QtMediaEngine.cpp
)

set(CONTROLLERS_SOURCES
    src/controllers/MediaController.cpp
)

set(UI_SOURCES
    src/ui/MainWindow.cpp
    src/ui/ClickableSlider.cpp
        src/ui/SettingDialog.cpp
)

set(UTILS_SOURCES
    src/utils/QtEnvironmentSetup.cpp
)

# Header files for MOC
set(HEADERS
    include/core/Application.h
    include/core/ConfigManager.h
    include/core/PluginManager.h
    include/core/ThemeManager.h
    include/media/IMediaEngine.h
    include/media/MediaManager.h
    include/media/QtMediaEngine.h
    include/controllers/MediaController.h
    include/ui/MainWindow.h
    include/ui/ClickableSlider.h
    include/plugins/IPlugin.h
        include/ui/SettingDialog.h
)

# All sources
set(ALL_SOURCES
    main.cpp
    ${CORE_SOURCES}
    ${MEDIA_SOURCES}
    ${CONTROLLERS_SOURCES}
    ${UI_SOURCES}
    ${UTILS_SOURCES}
    ${HEADERS}
)

# Create executable
add_executable(DarkPlay ${ALL_SOURCES})

# Link Qt6 libraries
target_link_libraries(DarkPlay
    Qt6::Core
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

# Compiler flags
target_compile_options(DarkPlay PRIVATE
    -Wall
    -Wextra
    -O2
    -ffast-math
)

# Create plugins directory
add_custom_command(TARGET DarkPlay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    ${CMAKE_CURRENT_BINARY_DIR}/plugins
)
