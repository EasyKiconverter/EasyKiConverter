<h1 align="center">EasyKiConverter</h1>
<p align="center">
  <a href="README_en.md">English</a> | <a href="README.md">中文</a>
</p>


<p align="center">
    <a href="https://flathub.org/apps/details/io.github.tangsangsimida.easykiconverter"><img width="200" alt="Download on Flathub" src="https://flathub.org/assets/badges/flathub-badge-i-en.svg"/></a>
    <br/>
    <img src="https://github.com/EasyKiconverter/EasyKiConverter/actions/workflows/build.yml/badge.svg" alt="Build Status" />
    <img src="https://github.com/EasyKiconverter/EasyKiConverter/actions/workflows/security.yml/badge.svg" alt="Security Scan" />
    <img src="https://img.shields.io/github/v/release/EasyKiconverter/EasyKiConverter" alt="GitHub release" />
    <img src="https://img.shields.io/github/downloads/EasyKiconverter/EasyKiConverter/total" alt="GitHub downloads (total)" />
    <img src="https://img.shields.io/github/license/EasyKiconverter/EasyKiConverter" alt="License" />
    <img src="https://img.shields.io/github/stars/EasyKiconverter/EasyKiConverter" alt="Stars" />
    <img src="https://img.shields.io/github/issues/EasyKiconverter/EasyKiConverter" alt="Issues" />
</p>



**EasyKiConverter** is a modern C++ desktop tool based on Qt 6 and MVVM architecture, designed for electronics engineers to convert LCSC and EasyEDA component data into symbol libraries, footprint libraries, and independent 3D model files for multiple EDA targets. It supports both GUI and CLI modes.

### Version and Capability Boundaries

- The latest stable release is **v3.1.11**. Stable downloads and capabilities are defined by the [v3.1.11 Release](https://github.com/EasyKiconverter/EasyKiConverter/releases/tag/v3.1.11).
- **v3.1.12** is currently a pre-release. It contains subsequent cross-platform packaging and Xpedition improvements and is not the stable release line.
- `master` is the development branch. The Altium, Xpedition, and unified IR capabilities described below reflect the source branch; check the relevant Release notes before using a stable package.

## Key Features

*   **Multiple Targets**: Supports the implemented library export paths for KiCad, Altium, Xpedition, PADS, Eagle, P-CAD, and CADSTAR; each target's symbol, footprint, component-association, and 3D model scope is documented separately, and unsupported data produces diagnostics.
*   **Multi-unit Symbols**: Support for multi-unit symbol conversion.
*   **Efficient Batch Processing**: Multi-threaded parallel conversion and BOM file import support, fully utilizing multi-core performance.
*   **Modern Experience**: Fluid UI based on Qt Quick, supporting dark/light theme switching.
*   **Smart Configuration**: Real-time auto-save of settings, supporting breakpoint memory and debug mode state restoration.
*   **Smart Assistance**: Support for intelligent extraction of component IDs from clipboard.
*   **LCSC Preview Images**: Automatically fetch LCSC component preview images, supporting thumbnail display and hover preview.
*   **CLI Mode**: Pure command-line mode for batch processing and automation scripts.

## Quick Start

### Installation
Please visit the [Releases](https://github.com/EasyKiconverter/EasyKiConverter/releases) page to download the version for your platform:

*   **Windows**: amd64 (the filenames use `x64`) and arm64 builds are available; download the matching `.exe` installer or `.zip` portable package for your device.
*   **Linux**: Download the x86_64/ARM64 `.AppImage`. LoongArch64 currently requires separate build infrastructure and is not included in official release packages yet.
*   **macOS**: Download `.dmg` image file.
*   **Arch Linux**: `yay -S easykiconverter`

### Build from Source
If you are a developer or wish to compile yourself, please refer to the [Build Guide](docs/developer/BUILD_en.md).

## Documentation

**User Guide**
*   [Getting Started](docs/user/GETTING_STARTED.md) | [User Manual](docs/user/USER_GUIDE.md) | [FAQ](docs/user/FAQ.md)
*   [Detailed Features](docs/user/FEATURES.md)

**Developer Resources**

*   [Contributing Guide](docs/developer/CONTRIBUTING.md) | [Architecture](docs/developer/ARCHITECTURE.md) | [Build Guide](docs/developer/BUILD.md)


## Contribution & Acknowledgment

### Contributors

We thank the following developers for their contributions to EasyKiConverter:

<a href="https://github.com/EasyKiconverter/EasyKiConverter/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=EasyKiconverter/EasyKiConverter&max=50" />
</a>

Issues and Pull Requests are welcome! Please see the [Contributing Guide](docs/developer/CONTRIBUTING.md) for details.

## License

This project is licensed under **GPL-3.0**. See the [LICENSE](LICENSE) file for details.
