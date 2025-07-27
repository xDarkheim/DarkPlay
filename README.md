<div align="center">
  <img src="logo.png" alt="DarkPlay Logo" width="128" height="128">
  
  # DarkPlay Media Player
  
  *Modern Qt 6 media player with optimized architecture*
  
  [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
  ![Qt](https://img.shields.io/badge/Qt-6-green.svg)
  ![C++](https://img.shields.io/badge/C++-20-blue.svg)
</div>

## Features

- 🎥 Stable video playback without artifacts
- ⚡ Hardware acceleration with OpenGL
- 🔧 Extensible plugin architecture  
- 🌐 Cross-platform (Linux, Windows, macOS)
- 🎨 Modern Qt 6 interface with themes

## Quick Start

**Requirements:** Qt 6, CMake 3.31+, C++20 compiler

```bash
git clone <repository-url>
cd DarkPlay
mkdir build && cd build
cmake .. && cmake --build . -j$(nproc)
./cmake-build-debug/DarkPlay
```

## Controls

- **Space** - Play/Pause
- **F11** - Fullscreen
- **Left/Right** - Seek
- **Right-click** - Context menu

## License

MIT License
