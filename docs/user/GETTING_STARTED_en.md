# Getting Started

This guide will help you get started with EasyKiConverter quickly.

## Introduction

EasyKiConverter is a C++ desktop application for converting LCSC and EasyEDA component data into multiple EDA library outputs. The current export pipeline covers symbols, footprints, device associations, and 3D model files for KiCad, Altium, Xpedition, PADS, Eagle, P-CAD, CADSTAR, and OrCAD Capture, with target-specific scope and diagnostics. Read the [format capability matrix](../project/UNIVERSAL_EDA_LIBRARY_CONVERTER_en.md) before choosing a target because native-format coverage differs between tools.

## Prerequisites

Before using EasyKiConverter, ensure you have:

### System Requirements

- **Operating System**: Windows 10/11 (recommended), macOS 10.15+, or Linux
- **Memory**: 4GB RAM minimum, 8GB recommended
- **Disk Space**: 100MB for application, additional space for converted files
- **Network**: Internet connection for downloading component data and 3D models

### Software Requirements

- **KiCad**: KiCad 6.0 or higher (to use converted components)
- **EasyEDA Account**: Optional, for accessing private components

## Installation

### Option 1: Download Pre-built Binary

1. Go to the [GitHub Releases](https://github.com/EasyKiconverter/EasyKiConverter/releases) page.
2. On Windows, choose `windows-x86_64` for Intel/AMD 64-bit devices or `windows-arm64` for Windows on Arm, then choose the installer, portable ZIP, or MSIX package.
3. On macOS, choose `macos-x86_64.dmg` or `macos-arm64.dmg`; on Linux, choose `linux-x86_64.AppImage` or `linux-aarch64.AppImage`.
4. Follow the platform-specific installation or launch instructions in the [User Guide](USER_GUIDE_en.md).

### Option 2: Build from Source

If you prefer to build from source, follow the [Build Guide](../developer/BUILD_en.md).

## Quick Start

### Step 1: Launch the Application

Run EasyKiConverter from your applications menu or by double-clicking the executable.

#### Using Command Line Arguments

EasyKiConverter supports configuring startup behavior via command line arguments:

```bash
# Enable debug mode
./EasyKiConverter.exe --debug

# Set log level
./EasyKiConverter.exe --log-level debug

# Set language to English
./EasyKiConverter.exe --language en

# Set theme to light
./EasyKiConverter.exe --theme light

# Display help information
./EasyKiConverter.exe --help

# Display version information
./EasyKiConverter.exe --version
```

For more command line argument information, please see the [CLI Usage Documentation](../CLI_USAGE.md) or run `--help`.

### Using CLI Mode

EasyKiConverter supports a pure command line mode for conversion without GUI:

```bash
# Convert BOM file
./EasyKiConverter convert bom --input BOM.csv --output /path/to/export

# Convert single component
./EasyKiConverter convert component C12345 --output /path/to/export

# Batch convert
./EasyKiConverter convert batch --input components.txt --output /path/to/export

# Generate Shell completion scripts
./EasyKiConverter --completion bash
./EasyKiConverter --completion zsh
```

### Step 2: Add Components

There are three ways to add components:

#### Method 1: Manual Input

1. Enter the LCSC component number in the input field
2. Click the "Add" button or press Enter
3. The component will be added to the list

#### Method 2: Smart Extraction

1. Copy component numbers from any source (e.g., BOM, website)
2. Click the "Paste" button
3. The application will automatically extract valid component numbers
4. Click "Add" to add all extracted components

#### Method 3: BOM Import

1. Click the "Import BOM" button
2. Select your BOM file (CSV or Excel format)
3. The application will parse and import all components

### Step 3: Configure Export Settings

1. Select the export directory
2. Choose what to export:
   - Symbols
   - Footprints
   - 3D Models
3. Configure additional options:
   - Overwrite existing files
   - Enable debug export (for developers)

### Step 4: Start Conversion

1. Click the "Convert" button
2. Wait for the conversion to complete
3. Monitor progress in the progress section

### Step 5: Review Results

After conversion completes:

1. Check the results list for success/failure status
2. Click on items to view details
3. Navigate to the export directory to view generated files

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

## Common Use Cases

### Converting a Single Component

1. Enter the component number (e.g., `C123456`)
2. Click "Add"
3. Configure export settings
4. Click "Convert"
5. Use the component in KiCad

### Batch Converting Components

1. Copy multiple component numbers from a BOM
2. Click "Paste" to extract them
3. Click "Add" to add all components
4. Configure export settings
5. Click "Convert"
6. Wait for all components to be converted

### Converting with 3D Models

1. Add components as usual
2. In export settings, enable "3D Models"
3. Click "Convert"
4. The application will automatically download and convert 3D models
5. In KiCad PCB Editor, view the 3D model by pressing "Alt + 3"

## Tips and Tricks

### Efficient Component Input

- Use smart extraction to quickly add multiple components
- Import BOM files for batch conversion
- Keep a list of commonly used component numbers

### Export Organization

- Create separate folders for different projects
- Use descriptive folder names
- Keep symbol and footprint libraries together

### Performance Optimization

- Convert multiple components at once for better performance
- Use parallel conversion for large batches
- Close other applications to free up resources

### Troubleshooting

- If a component fails to convert, check the component number
- Ensure you have an internet connection for downloading data
- Verify the export directory has write permissions
- Check the results list for error messages

## Keyboard Shortcuts

- `Enter` - Add component from input field
- `Ctrl+V` - Paste and extract component numbers
- `Ctrl+A` - Select all components
- `Delete` - Remove selected components
- `F5` - Refresh component list
- `Ctrl+E` - Open export settings
- `Ctrl+C` - Start conversion

## Configuration

### Theme Settings

1. Click the theme toggle button (light/dark icon)
2. Choose your preferred theme:
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

For developers:

1. Enable debug export in settings
2. Original and exported data will be saved
3. Check the debug directory for detailed logs

## Common Issues

### Issue: Component Not Found

**Solution:**
- Verify the component number is correct
- Check if the component exists in LCSC database
- Ensure you have an internet connection

### Issue: Conversion Failed

**Solution:**
- Check the error message in the results list
- Verify the component data is available
- Try converting the component again

### Issue: 3D Model Not Downloaded

**Solution:**
- Ensure "3D Models" is enabled in export settings
- Check if the component has a 3D model
- Verify your internet connection

### Issue: Files Not Found in KiCad

**Solution:**
- Verify the library path in KiCad settings
- Check if the files are in the correct directory
- Refresh KiCad's library list

## Advanced Usage

### Custom Layer Mapping

For advanced users, you can customize layer mapping:

1. Edit the layer mapping configuration
2. Map EasyEDA layers to KiCad layers
3. Save your custom mapping

### Batch Processing Scripts

For automation, you can use the command-line interface:

```bash
EasyKiConverter --input components.txt --output ./export --batch
```

### Integration with Build Systems

Integrate EasyKiConverter into your build workflow:

1. Export component libraries as part of build process
2. Use version control for library files
3. Automate conversion with scripts

## Getting Help

### Documentation

- [Features Documentation](FEATURES_en.md) - Learn about all features
- [Architecture Documentation](../developer/ARCHITECTURE_en.md) - Understand the architecture
- [Build Guide](../developer/BUILD_en.md) - Build from source
- [Contributing Guide](../developer/CONTRIBUTING_en.md) - Contribute to the project

### Community

- [GitHub Issues](https://github.com/EasyKiconverter/EasyKiConverter/issues) - Report bugs and request features
- [GitHub Discussions](https://github.com/EasyKiconverter/EasyKiConverter/discussions) - Ask questions and share ideas

### Support

If you need help:
1. Check this documentation
2. Search existing GitHub Issues
3. Create a new issue with detailed information

## Next Steps

Now that you're familiar with the basics:

1. Explore advanced features in the [Features Documentation](FEATURES_en.md)
2. Learn about the project [Architecture](../developer/ARCHITECTURE_en.md)
3. Consider [Contributing](../developer/CONTRIBUTING_en.md) to the project
4. Customize the application for your workflow

## Best Practices

1. **Always verify converted components** before using them in production
2. **Keep backups** of your component libraries
3. **Use version control** for your library files
4. **Document custom settings** for your team
5. **Test conversions** with a few components before batch processing

## Performance Tips

1. **Batch convert** multiple components at once
2. **Use parallel conversion** for better performance
3. **Close unnecessary applications** to free up resources
4. **Use a fast internet connection** for downloading data
5. **Enable debug export** only when needed

## Security Considerations

1. Only download components from trusted sources
2. Verify component data before use
3. Keep your application updated
4. Use secure network connections
5. Review converted files before using in production

## License

EasyKiConverter is licensed under GNU General Public License v3.0 (GPL-3.0).

## Acknowledgments

This project references the design and algorithms from [uPesy/easyeda2kicad.py](https://github.com/uPesy/easyeda2kicad.py). Thank you to the original author for providing an excellent foundation framework and core conversion algorithms.

## Feedback

We welcome your feedback! Please:
- Report bugs via GitHub Issues
- Suggest features via GitHub Issues
- Share your experience in GitHub Discussions
- Contribute code improvements

Thank you for using EasyKiConverter!
