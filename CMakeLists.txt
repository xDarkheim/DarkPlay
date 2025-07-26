cmake_minimum_required(VERSION 3.31)
project(DarkPlay)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt 6 components - добавим OpenGL для лучшей работы с видео
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia MultimediaWidgets Network OpenGL OpenGLWidgets)

# Enable automatic MOC, UIC, and RCC processing
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Compiler-specific optimizations for video rendering
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Оптимизации для лучшей производительности видео
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -ffast-math")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native")
endif()

# Platform-specific settings for video acceleration
if(WIN32)
    # Windows-specific video acceleration
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWIN32_LEAN_AND_MEAN")
endif()

# Include directories
include_directories(include)

# Core source files
set(CORE_SOURCES
    src/core/Application.cpp
    src/core/PluginManager.cpp
    src/core/ThemeManager.cpp
    src/core/ConfigManager.cpp
)

set(CORE_HEADERS
    include/core/Application.h
    include/core/PluginManager.h
    include/core/ThemeManager.h
    include/core/ConfigManager.h
)

# Media source files
set(MEDIA_SOURCES
    src/media/MediaManager.cpp
)

set(MEDIA_HEADERS
    include/media/IMediaEngine.h
    include/media/MediaManager.h
)

# UI source files
set(UI_SOURCES
    src/ui/MainWindow.cpp
)

set(UI_HEADERS
    include/ui/MainWindow.h
)

# Controller source files
set(CONTROLLER_SOURCES
    src/controllers/MediaController.cpp
)

set(CONTROLLER_HEADERS
    include/controllers/MediaController.h
)

# Plugin headers
set(PLUGIN_HEADERS
    include/plugins/IPlugin.h
)

# All source files
set(ALL_SOURCES
    main.cpp
    ${CORE_SOURCES}
    ${MEDIA_SOURCES}
    ${UI_SOURCES}
    ${CONTROLLER_SOURCES}
)

set(ALL_HEADERS
    ${CORE_HEADERS}
    ${MEDIA_HEADERS}
    ${UI_HEADERS}
    ${CONTROLLER_HEADERS}
    ${PLUGIN_HEADERS}
)

# Create executable
add_executable(DarkPlay ${ALL_SOURCES} ${ALL_HEADERS})

# Link Qt libraries
target_link_libraries(DarkPlay
    Qt6::Core
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

# Platform-specific compile definitions (после создания target'а)
if(UNIX AND NOT APPLE)
    # Linux-specific settings for better X11/Wayland video support
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(X11 x11)
        if(X11_FOUND)
            target_compile_definitions(DarkPlay PRIVATE HAVE_X11)
        endif()
    endif()
endif()

# Create plugins directory in build folder
add_custom_command(TARGET DarkPlay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/plugins
)

# Set output directory for executable
set_target_properties(DarkPlay PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
