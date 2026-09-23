# User Guide

This manual provides detailed instructions for using all features of EasyKiConverter.

## Table of Contents

- [Installation](#system-requirements)
- [Interface Overview](#main-interface)
- [Basic Operations](#adding-components)
- [Advanced Features](#batch-conversion)
- [Configuration Options](#output-settings)
- [Troubleshooting](#common-issues)

## Installation

### System Requirements

- Operating System: Windows 10/11 (recommended), macOS 10.15+, or Linux
- Memory: 4GB RAM minimum, 8GB recommended
- Disk Space: 100MB for application, additional space for converted files
- Network: Internet connection for downloading component data and 3D models

### Installation Steps

#### Windows

1. Download the latest version from [GitHub Releases](https://github.com/EasyKiconverter/EasyKiConverter/releases)
2. Extract the downloaded archive
3. Double-click `EasyKiConverter.exe` to run the application

#### macOS

1. Download the latest version from [GitHub Releases](https://github.com/EasyKiconverter/EasyKiConverter/releases)
2. Extract the downloaded archive
3. Double-click `EasyKiConverter.app` to run the application

#### Linux

1. Download the latest version from [GitHub Releases](https://github.com/EasyKiconverter/EasyKiConverter/releases)
2. Extract the downloaded archive
3. Run `./EasyKiConverter` to launch the application

## Interface Overview

### Main Window

The main window contains the following main areas:

1. **Component Input Area**
   - Component ID input field
   - Add button
   - Smart extraction button
   - BOM import button

2. **Component List Area**
   - Displays added components
   - Component ID and name
   - Status indicator
   - Delete button

3. **Export Settings Area**
   - Export directory selection
   - Export type selection (symbol, footprint, 3D model)
   - Overwrite files option
   - Debug export option

4. **Progress Display Area**
   - Progress bar
   - Status messages
   - Success/failure count

5. **Results Display Area**
   - Conversion results list
   - Detailed information view
   - File location links

### Theme Switching

Click the theme toggle button in the top right corner to switch between dark and light modes.

## Basic Operations

### Adding Components

#### Method 1: Manual Input

1. Enter the LCSC component ID in the input field (e.g., C123456)
2. Click the "Add" button or press Enter
3. The component will be added to the list

#### Method 2: Smart Extraction

1. Copy component IDs from any source (e.g., BOM, website)
2. Click the "Smart Extraction" button
3. The application will automatically extract valid component IDs
4. Click "Add" to add all extracted components

#### Method 3: BOM Import

1. Click the "BOM Import" button
2. Select your BOM file (CSV or Excel format)
3. The application will parse and import all components

### Configuring Export Settings

1. Click the "Browse" button to select the export directory
2. Check the types to export:
   - Symbols (.kicad_sym)
   - Footprints (.kicad_mod)
   - 3D Models (WRL, STEP, OBJ)
3. Configure other options:
   - Overwrite existing files
   - Enable debug export (developers only)
4. Click "Save" to save settings

### Cache Directory Safety

The cache directory must be empty or already contain an EasyKiConverter ownership marker. Filesystem roots, the user home directory, and symbolic-link directories are rejected. Editing a cache path does not migrate data on every keystroke; the path is applied only after folder selection or editing is completed and validation succeeds.

Cache maintenance follows these boundaries:

- The root and each cache entry must pass ownership-marker and metadata identity checks. Unknown files, directories, symbolic links, and unverifiable legacy entries are preserved.
- Startup self-healing, quota pruning, directory migration, and “Clear cache” process only entries whose ownership can be verified.
- The confirmation dialog shows the number of entries in scope. Confirmed entries are moved to the system trash; if trash is unavailable or an operation fails, the original data is kept and no permanent-delete fallback is used.
- Migration conflicts or failures preserve the source directory and user files; the active configuration is not switched to an incomplete target.

```mermaid
flowchart TD
    Select[Select or edit cache directory] --> Validate{Validate path and ownership}
    Validate -- Reject --> Keep[Keep current configuration and show reason]
    Validate -- Accept --> Apply[Apply after confirmation and migrate verified entries]
    Apply --> Maintain[Self-heal, prune quota, or clear]
    Maintain --> Trash[Move verified entries to system trash only]
    Maintain --> Preserve[Preserve unknown or failed entries]
```

### Starting Conversion

1. Ensure all components are added to the list
2. Configure export settings
3. Click the "Start Conversion" button
4. Wait for conversion to complete
5. Monitor progress in the progress area

### Viewing Results

After conversion completes:

1. View conversion status for each component in the results list
2. Click on a component item to view detailed information
3. Click on file location links to open the export directory
4. Use converted files in KiCad

## Advanced Features

### Layer Mapping

EasyKiConverter automatically maps EasyEDA layers to KiCad layers. Supported layers include:

- Signal layers (Top, Bottom, Inner1-32)
- Silkscreen layers (Top, Bottom)
- Solder mask layers (Top, Bottom)
- Paste layers (Top, Bottom)
- Mechanical layers
- User layers
- Edge cuts
- Fabrication layers

### Polygon Pads

Support for correct export of custom-shaped pads:

- Automatic primitives block generation
- Coordinate transformation
- Size optimization
- KiCad compatibility

### Elliptical Arc Calculation

Precise arc calculation supporting complex geometric shapes:

- Complete algorithm implementation
- SVG arc parameter parsing
- Center and angle calculation
- KiCad format output

### Text Layer Processing

Support for type "N" special processing and mirrored text processing:

- Type "N" text handling
- Mirrored text support
- Layer mapping
- Font effects

### Batch Conversion

Support for simultaneous conversion of multiple components:

- Parallel data collection
- Serial data export
- Progress tracking
- Error isolation

### Smart Extraction

Intelligent extraction of component IDs from clipboard text:

- Automatic component ID format recognition
- Batch extraction
- Format validation
- User-friendly interface

### CLI Mode

In addition to GUI mode, EasyKiConverter also supports a pure command line mode for conversion without launching the GUI:

```bash
# Convert BOM file
EasyKiConverter convert bom --input BOM.csv --output /path/to/export

# Convert single component
EasyKiConverter convert component C12345 --output /path/to/export

# Batch convert
EasyKiConverter convert batch --input components.txt --output /path/to/export

# Generate Shell completion scripts
EasyKiConverter --completion bash
EasyKiConverter --completion zsh
```

CLI mode supports offline batch export, suitable for automated workflows.

For more CLI features, see the [CLI Usage Documentation](../CLI_USAGE.md).

## Configuration Options

### Theme Settings

1. Click the theme toggle button
2. Select theme:
   - Light mode
   - Dark mode
   - System default

### Export Settings

1. Click the "Settings" button
2. Configure default export options:
   - Default export directory
   - Default export types
   - Overwrite behavior
3. Click "Save" to apply settings

### Debug Settings


**Method 1: Command Line Arguments (Recommended)**
```bash
# Enable debug mode
EasyKiConverter --debug

# Set log level
EasyKiConverter --log-level debug

# Specify log file
EasyKiConverter --log-file /path/to/log.txt
```

**Method 2: Environment Variable**
```bash
# Windows
set EASYKICONVERTER_DEBUG_MODE=true

# Linux/macOS
export EASYKICONVERTER_DEBUG_MODE=true
```

**Method 3: UI Settings**
1. Enable debug export in settings
2. Original and exported data will be saved
3. Check debug directory for detailed logs

Note: Command line arguments take precedence over other configuration methods.

For more details, please see the [Debug Mode Documentation](DEBUG_MODE_en.md).

## Using Converted Components in KiCad

### Importing Symbol Library

1. Open KiCad
2. Go to "Preferences" -> "Manage Symbol Libraries"
3. Click "Add Existing Library"
4. Navigate to the export directory
5. Select the `.kicad_sym` file
6. Click "OK"

### Importing Footprint Library

1. Open KiCad
2. Go to "Preferences" -> "Manage Footprint Libraries"
3. Click "Add Existing Library"
4. Navigate to the export directory
5. Select the `.pretty` folder
6. Click "OK"

### Using Components

1. In KiCad Schematic Editor, press "A" to add a component
2. Search for your component by name
3. Place the component in your schematic
4. In PCB Editor, the footprint will be automatically associated

### Viewing 3D Models

1. In KiCad PCB Editor
2. Press "Alt + 3" to view 3D models
3. Rotate and pan to view the model
4. Verify model position and orientation

## Keyboard Shortcuts

- `Enter` - Add component from input field
- `Ctrl+V` - Paste and extract component IDs
- `Ctrl+A` - Select all components
- `Delete` - Remove selected components
- `F5` - Refresh component list
- `Ctrl+E` - Open export settings
- `Ctrl+C` - Start conversion

## Best Practices

### Component Input

- Use smart extraction to quickly add multiple components
- Import BOM files for batch conversion
- Keep a list of commonly used component IDs

### Export Organization

- Create separate folders for different projects
- Use descriptive folder names
- Keep symbol and footprint libraries together

### Performance Optimization

- Convert multiple components at once for better performance
- Use parallel conversion for large batches
- Close other applications to free up resources

### Quality Control

- Verify converted components before using in production
- Keep backups of your component libraries
- Use version control for library files

## Troubleshooting

### Issue: Component Not Found

**Solution:**
- Verify the component ID is correct
- Check if the component exists in the LCSC database
- Ensure you have an internet connection

### Issue: Conversion Failed

**Solution:**
- Check the error message in the results list
- Verify component data is available
- Try converting the component again

### Issue: 3D Model Not Downloaded

**Solution:**
- Ensure "3D Models" is enabled in export settings
- Check if the component has a 3D model
- Verify your internet connection

### Issue: Files Not Found in KiCad

**Solution:**
- Verify the library path in KiCad settings
- Check if files are in the correct directory
- Refresh KiCad's library list

### Issue: Application Crashes

**Solution:**
- Check system requirements
- Update to the latest version
- View error logs
- Report the issue on GitHub

## Additional Resources

- [Getting Started](GETTING_STARTED_en.md) - Quick start guide
- [Features](FEATURES_en.md) - Learn about all features
- [FAQ](FAQ_en.md) - Frequently asked questions
- [GitHub Issues](https://github.com/EasyKiconverter/EasyKiConverter/issues) - Report bugs and request features

## Support

If you need help:

1. Check this documentation
2. Search existing GitHub Issues
3. Create a new issue with detailed information

## License

EasyKiConverter is licensed under GNU General Public License v3.0 (GPL-3.0).

## Acknowledgments

This project references the design and algorithms from [uPesy/easyeda2kicad.py](https://github.com/uPesy/easyeda2kicad.py). Thank you to the original author for providing an excellent foundation framework and core conversion algorithms.
