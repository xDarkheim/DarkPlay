# DarkPlay Media Player

A modern, extensible media player built with Qt 6 and modern C++ architecture.

## Features

- **Modern UI**: Clean and intuitive interface with dark theme support
- **Multiple Media Formats**: Support for popular video and audio formats (MP4, AVI, MKV, MOV, WMV, MP3, WAV, FLAC, OGG)
- **Extensible Architecture**: Plugin system for adding new functionality
- **Theme System**: Multiple themes with easy switching
- **Recent Files**: Quick access to recently opened media files
- **Keyboard Controls**: Space for play/pause, F11 for fullscreen, Escape to exit fullscreen
- **Volume Control**: Integrated volume slider with visual feedback
- **Time Display**: Current time and total duration with seek functionality

## Requirements

- **Qt 6.8+** with the following modules:
  - Qt6Core
  - Qt6Widgets
  - Qt6Multimedia
  - Qt6MultimediaWidgets
  - Qt6Network
- **CMake 3.31+**
- **C++20 compatible compiler**
- **FFmpeg** (for multimedia support)

## Building

### Linux (Ubuntu/Debian)

1. Install dependencies:
```bash
sudo apt update
sudo apt install cmake build-essential qt6-base-dev qt6-multimedia-dev qt6-tools-dev libgl1-mesa-dev ninja-build
```

2. Clone and build:
```bash
git clone <repository-url> DarkPlay
cd DarkPlay
mkdir cmake-build-debug
cd cmake-build-debug
cmake -G Ninja ..
ninja
```

3. Run:
```bash
./DarkPlay
```

### Alternative Build Methods

#### Using Make (traditional)
```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

#### Using CLion IDE
1. Open the project folder in CLion
2. CLion will automatically detect CMakeLists.txt
3. Configure the project (CLion uses Ninja by default)
4. Build using Ctrl+F9 or the build button

#### Clean Build
If you encounter generator compatibility issues:
```bash
rm -rf cmake-build-debug  # or your build directory
mkdir cmake-build-debug
cd cmake-build-debug
cmake -G Ninja ..
ninja
```

## Usage

### Basic Controls
- **Open File**: Click "Open" button or use File → Open File (Ctrl+O)
- **Play/Pause**: Click play button or press Space
- **Stop**: Click stop button
- **Volume**: Use volume slider
- **Seek**: Click on progress bar or drag slider
- **Fullscreen**: Press F11 (Escape to exit)

### Menu Options
- **File Menu**: Open files, access recent files, exit
- **View Menu**: Switch between available themes
- **Tools Menu**: Access preferences and plugin manager
- **Help Menu**: About information

## Architecture

DarkPlay is built with a modular architecture:

```
DarkPlay/
├── src/
│   ├── core/           # Core application logic
│   ├── ui/             # User interface components
│   ├── media/          # Media management
│   ├── controllers/    # Media controllers
│   └── plugins/        # Plugin system
├── include/            # Header files
├── resources/          # Resources (themes, icons, translations)
└── CMakeLists.txt      # Build configuration
```

### Key Components

- **Application**: Main application class with RAII management
- **MainWindow**: Primary UI with media controls
- **MediaController**: Handles media playback logic
- **MediaManager**: Manages media engines and formats
- **PluginManager**: Loads and manages plugins
- **ThemeManager**: Handles UI themes
- **ConfigManager**: Manages application settings

## Plugin Development

DarkPlay supports plugins through the `IPlugin` interface. Plugins are loaded from the `plugins/` directory and can extend functionality.

## Configuration

Settings are automatically saved and include:
- Window geometry and state
- Recent files list
- Volume level
- Last used directory
- Theme selection

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## License

[Add your license information here]

## Troubleshooting

### Common Issues

**Media files won't play**
- Ensure FFmpeg is installed and Qt6Multimedia can access it
- Check that the file format is supported

**UI appears corrupted**
- Try switching themes from View → Theme menu
- Check Qt installation and graphics drivers

**Application crashes on startup**
- Verify all Qt dependencies are installed
- Check console output for error messages

## Development Status

DarkPlay is actively developed with the following planned features:
- Playlist support
- Advanced plugin system
- More themes and customization options
- Subtitle support
- Audio equalizer
- Video filters and effects

---

Built with ❤️ using Qt 6 and modern C++20
