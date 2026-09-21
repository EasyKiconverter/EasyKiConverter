# Frequently Asked Questions

This document collects the most frequently asked questions and their solutions.

## General Questions

### What is EasyKiConverter?

EasyKiConverter is a C++ desktop application based on Qt 6 Quick and MVVM architecture for converting LCSC and EasyEDA components to multiple EDA library formats. It converts symbol libraries, footprint libraries, and 3D model files; Xpedition currently emits standalone 3D files without native 3D associations.

### Is EasyKiConverter free?

Yes, EasyKiConverter is licensed under GNU General Public License v3.0 (GPL-3.0) and is completely free and open source.

### Which operating systems are supported?

EasyKiConverter supports:
- Windows 10/11 (recommended)
- macOS 10.15+
- Linux (Ubuntu 20.04+, Debian 10+, Fedora 33+)

### Do I need an EasyEDA account?

No. EasyKiConverter uses the public EasyEDA API and does not require an account. However, if you need to access private components, you will need an EasyEDA account.

## Installation and Setup

### How do I install EasyKiConverter?

Please refer to the [Installation Guide](USER_GUIDE_en.md).

### What are the system requirements?

- Operating System: Windows 10/11, macOS 10.15+, or Linux
- Memory: 4GB RAM minimum, 8GB recommended
- Disk Space: 100MB for application, additional space for converted files
- Network: Internet connection for downloading component data and 3D models

### How do I build from source?

Please refer to the [Build Guide](../developer/BUILD_en.md).

### I get a "Qt not found" error when building. What should I do?

Ensure that:
1. Qt is installed correctly
2. CMAKE_PREFIX_PATH is set correctly
3. Qt version is 6.8 or higher

## Usage Questions

### How do I add components?

There are three methods:
1. **Manual Input**: Enter component ID in the input field and click "Add"
2. **Smart Extraction**: Copy component IDs and click the "Smart Extraction" button
3. **BOM Import**: Click the "BOM Import" button and select a BOM file

For detailed steps, please refer to the [User Guide](USER_GUIDE_en.md).

### What component ID formats are supported?

Standard LCSC component ID formats are supported, for example:
- C123456
- C789012
- C345678

### Can I convert components in batch?

Yes, EasyKiConverter supports batch conversion. You can:
- Use smart extraction to add multiple components
- Import BOM files for batch conversion
- Use parallel conversion for better efficiency

### Where are the converted files?

Converted files are saved in the directory you specified in export settings. You can:
1. Click on file location links in the results list
2. Or manually navigate to the export directory

### How do I use converted components in KiCad?

For detailed steps, please refer to the [User Guide](USER_GUIDE_en.md).

### Which KiCad versions are supported?

KiCad 6.0+, 7.0+, 8.0+, and 9.0+ are supported.

### Which EasyEDA versions are supported?

EasyEDA Standard, EasyEDA Professional, and EasyEDA Pro are supported.

## Conversion Issues

### Why did conversion fail?

Possible reasons:
1. Component ID is incorrect
2. Component does not exist in LCSC database
3. Network connection issues
4. Component data is unavailable

**Solutions:**
- Verify the component ID is correct
- Check network connection
- View error messages in the results list
- Try converting again

### Why was the 3D model not downloaded?

Possible reasons:
1. Component does not have a 3D model
2. "3D Models" option is not enabled
3. Network connection issues

**Solutions:**
- Ensure "3D Models" is enabled in export settings
- Check if the component has a 3D model
- Verify network connection

### What should I do if converted files cannot be opened?

Possible reasons:
1. File is corrupted
2. KiCad version is incompatible
3. File format is incorrect

**Solutions:**
- Convert the component again
- Update KiCad to the latest version
- Check if the file format is correct

### What should I do if conversion is slow?

Optimization suggestions:
- Convert multiple components in batch
- Close other applications
- Use a faster network connection
- Ensure sufficient system resources

### What if conversion results are inconsistent with EasyEDA?

EasyKiConverter strives to maintain consistency with EasyEDA, but minor differences may exist. If you find significant differences:
1. Report the issue on GitHub
2. Provide detailed comparison information
3. Include component ID and screenshots

## Feature Questions

### What is smart extraction?

Smart extraction automatically extracts valid component IDs from clipboard text. It supports:
- Automatic component ID format recognition
- Batch extraction
- Format validation

### What is layer mapping?

Layer mapping is the process of automatically mapping EasyEDA layers to KiCad layers. EasyKiConverter supports mapping of 50+ layers.

### Which 3D model formats are supported?

Supported formats:
- WRL (VRML)
- STEP
- OBJ

### Can I customize layer mapping?

Custom layer mapping is not currently supported. Future versions may add this feature.

### How do I enable debug mode?

**Method 1: Command Line Arguments (Recommended)**
```bash
# Windows
.\build\bin\EasyKiConverter.exe --debug

# Linux/macOS
./build/bin/EasyKiConverter --debug
```

**Method 2: Environment Variable**
```bash
# Windows PowerShell
$env:EASYKICONVERTER_DEBUG_MODE="true"
.\build\bin\EasyKiConverter.exe

# Linux/macOS
export EASYKICONVERTER_DEBUG_MODE=true
./build/bin/EasyKiConverter
```

Please refer to the [Debug Mode Configuration](DEBUG_MODE_en.md) documentation for detailed configuration instructions and all available command line arguments.

### How do I change the theme?

Click the theme toggle button in the top right corner to switch between dark and light modes.

## Performance Issues

### How can I improve conversion speed?

Optimization suggestions:
- Convert multiple components in batch
- Use parallel conversion
- Close unnecessary applications
- Use a faster network connection

### What should I do if memory usage is too high?

Optimization suggestions:
- Reduce the number of components converted in batch
- Close other applications
- Restart the application

### What should I do if the application crashes?

**Solutions:**
1. Check system requirements
2. Update to the latest version
3. View error logs
4. Report the issue on GitHub

## Compatibility Issues

### Which KiCad versions are supported?

KiCad 6.0+, 7.0+, 8.0+, and 9.0+ are supported.

### Which operating systems are supported?

Windows 10/11, macOS 10.15+, and Linux are supported.

### What should I do if I have issues running on Linux?

Ensure that:
1. All dependencies are installed
2. Qt version is correct
3. Compiler version is compatible
4. System libraries are complete

## Error Messages

### "Component not found"

**Cause:** Component ID is incorrect or component does not exist

**Solutions:**
- Verify the component ID
- Check if the component exists in the LCSC database

### "Network connection failed"

**Cause:** Network connection issues

**Solutions:**
- Check network connection
- Try using a VPN
- Check firewall settings

### What should I do if conversions frequently fail under weak network conditions?

If your network environment is unstable (high latency, high packet loss), you may encounter the following issues:
- Many components marked as failed during batch export
- 3D model download timeouts
- Requests hanging with no response for extended periods

**Solutions:**
- Use a more stable network connection (wired preferred over wireless)
- Reduce the number of components exported simultaneously
- Use the "Retry Export" feature for failed components
- If the problem persists, report the issue on GitHub with a description of your network environment

### "File write failed"

**Cause:** Insufficient disk space or permission issues

**Solutions:**
- Check disk space
- Check write permissions
- Select a different export directory

### "Conversion failed"

**Cause:** Component data is unavailable or format is incorrect

**Solutions:**
- View detailed error messages
- Try converting again
- Report the issue on GitHub

## Advanced Questions

### How do I integrate into a build system?

EasyKiConverter supports command-line interface and can be integrated into build systems:

```bash
# Batch conversion
easykiconverter convert batch -i <component_list_file> -o <output_dir>

# Convert single component
easykiconverter convert component -c <lcsc_id> -o <output_dir>

# Convert BOM file
easykiconverter convert bom -i <bom_file> -o <output_dir>
```

For detailed instructions, please refer to [CLI Usage Guide](../CLI_USAGE.md).

### How do I customize export options?

Configure in export settings:
- Export directory
- Export types
- Overwrite behavior
- Debug options

### How do I contribute code?

Please refer to the [Contributing Guide](../developer/CONTRIBUTING_en.md).

### How do I report a bug?

1. Search existing GitHub Issues
2. Create a new Issue with:
   - Problem description
   - Steps to reproduce
   - Expected behavior
   - Actual behavior
   - Environment information
   - Screenshots (if applicable)

### How do I request a new feature?

1. Search existing GitHub Issues
2. Create a new Issue with:
   - Feature description
   - Use case
   - Expected behavior
   - Examples (if applicable)

## Other Questions

### How do I get help?

1. Check documentation
2. Search GitHub Issues
3. Create a new Issue
4. Participate in GitHub Discussions

### How do I contact developers?

Contact project maintainers through GitHub Issues.

### How do I donate?

Donations are not currently accepted. You can support the project by:
- Contributing code
- Reporting bugs
- Requesting features
- Sharing the project

### Are there video tutorials?

There are currently no video tutorials. You can learn by:
- Reading documentation
- Viewing GitHub Issues
- Participating in GitHub Discussions

## Additional Resources

- [User Guide](USER_GUIDE_en.md) - Detailed usage instructions
- [Getting Started](GETTING_STARTED_en.md) - Quick start guide
- [Features](FEATURES_en.md) - Learn about all features
- [Build Guide](../developer/BUILD_en.md) - Build from source
- [Contributing Guide](../developer/CONTRIBUTING_en.md) - Contribute code
- [Developer FAQ](../developer/FAQ_en.md) - Technical issues, root cause analysis, and regression prevention
- [GitHub Issues](https://github.com/EasyKiconverter/EasyKiConverter/issues) - Report issues
- [GitHub Discussions](https://github.com/EasyKiconverter/EasyKiConverter/discussions) - Q&A and discussions

If your question is not listed here, please feel free to create a new Issue on GitHub.
