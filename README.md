# DarkPlay Media Player

A modern, extensible media player built with Qt 6 and modern C++ architecture, designed for stable video rendering and optimal performance.

## Features

- **Stable Video Rendering** - Optimized Qt environment setup to eliminate visual artifacts and flickering
- **Modern Architecture** - Clean C++20 codebase with RAII and proper memory management
- **Extensible Plugin System** - Modular design for easy feature expansion
- **Cross-Platform** - Built with Qt 6 for Linux, Windows, and macOS support
- **Hardware Acceleration** - OpenGL integration for smooth video playback

## Quick Start

### Prerequisites

- **Qt 6** with Multimedia, Widgets, and OpenGL components
- **CMake 3.31+**
- **C++20 compatible compiler** (GCC, Clang, or MSVC)
- **FFmpeg** (recommended for multimedia support)

### Building

```bash
# Clone the repository
git clone <repository-url>
cd DarkPlay

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
cmake --build . --target DarkPlay -j$(nproc)
```

### Running

**Option 1: Direct execution**
```bash
./cmake-build-debug/DarkPlay
```

**Option 2: Optimized launch script (recommended)**
```bash
./scripts/run_darkplay.sh
```

The launch script automatically sets optimal environment variables for stable video rendering.

## Project Structure

```
DarkPlay/
├── main.cpp                 # Application entry point
├── CMakeLists.txt           # Build configuration
├── README.md                # This file
├── .gitignore              # Git ignore rules
├── include/                # Header files
│   ├── core/               # Core application classes
│   ├── ui/                 # User interface components
│   ├── media/              # Media handling interfaces
│   ├── controllers/        # Application controllers
│   └── utils/              # Utility functions
├── src/                    # Source files
│   ├── core/               # Core implementation
│   ├── ui/                 # UI implementation
│   ├── media/              # Media implementation
│   ├── controllers/        # Controllers implementation
│   └── utils/              # Utilities implementation
├── config/                 # Configuration files
│   ├── qt.conf             # Qt configuration
│   ├── .env                # Environment variables
│   └── CMakePresets.json   # CMake presets
├── scripts/                # Build and run scripts
│   └── run_darkplay.sh     # Optimized launch script
├── resources/              # Application resources
│   ├── icons/              # Application icons
│   ├── themes/             # UI themes
│   └── translations/       # Localization files
└── docs/                   # Documentation
```

## Configuration

### Environment Variables

For optimal performance, especially on Linux, the following environment variables are automatically set:

- `QT_QPA_PLATFORM=xcb` - Use XCB platform for better X11 integration
- `QT_OPENGL=desktop` - Use desktop OpenGL for hardware acceleration
- `QT_MULTIMEDIA_PREFERRED_PLUGINS=ffmpeg` - Prefer FFmpeg for multimedia
- `QSG_RENDER_LOOP=basic` - Use basic render loop for stability

See `config/.env` for the complete list of optimized settings.

### CLion Setup

When running from CLion, the application automatically configures optimal Qt environment variables. For manual configuration, add the variables from `config/.env` to your Run Configuration.

## Development

### Architecture Overview

- **Core Layer** - Application lifecycle, configuration, and plugin management
- **UI Layer** - Qt-based user interface with optimized video widget
- **Media Layer** - Abstracted media engine interface for extensibility
- **Controllers** - Business logic and UI coordination
- **Utils** - Common utilities and Qt environment optimization

### Key Components

- **QtEnvironmentSetup** - Automatic Qt optimization for stable video rendering
- **MediaController** - Central media playback coordination
- **MainWindow** - Primary UI with anti-flicker video widget
- **PluginManager** - Extensible plugin architecture

## Troubleshooting

### Video Artifacts / Flickering

The application includes automatic fixes for common video rendering issues:

1. **Automatic Environment Setup** - Qt variables are optimized automatically
2. **Widget Optimization** - Video widget uses anti-flicker attributes
3. **Launch Script** - Use `scripts/run_darkplay.sh` for maximum stability

### Build Issues

- Ensure Qt 6 with Multimedia components is installed
- Check CMake version (3.31+ required)
- Verify C++20 compiler support

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- Built with Qt 6 framework
- Uses FFmpeg for multimedia support
- Inspired by modern media player design principles
