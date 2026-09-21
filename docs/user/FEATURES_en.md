# Features

This document provides a detailed description of the features available in EasyKiConverter.

## Core Functions

### Symbol Conversion

Convert EasyEDA symbols to KiCad (.kicad_sym), Altium (.SchLib), or Xpedition ASCII symbol libraries.

**Features:**
- Complete symbol geometry conversion
- Pin information preservation
- Symbol properties transfer
- Multiple symbol support per component
- Symbol library organization

**Supported Elements:**
- Pins (number, name, type, orientation)
- Lines, rectangles, circles, arcs
- Text labels
- Symbol properties (name, value, footprint)

### Footprint Generation

Create KiCad (.kicad_mod), Altium (.PcbLib), Xpedition ASCII, PADS PCB Decal, P-CAD ASCII (.lia), or Eagle XML (.lbr) libraries from EasyEDA data. Eagle can write symbols, packages, and device associations together; 3D models are emitted by an independent stage.

**Features:**
- Complete footprint geometry conversion
- Pad information preservation
- Layer mapping
- Custom-shaped pad support
- 3D model integration

**Supported Elements:**
- Pads (SMD, THT, custom shapes)
- Lines, rectangles, circles, arcs, polygons
- Text on silkscreen layers
- Reference designator and value
- Courtyard and assembly layers

### 3D Model Support

Automatically download and convert 3D models.

**Supported Formats:**
- WRL (VRML)
- STEP
- OBJ

**Features:**
- Automatic download from EasyEDA
- Format conversion
- Position and rotation adjustment
- Model scaling
- Model verification
- Altium embeds STEP models in PcbLib instead of creating external WRL references
- Xpedition currently exports symbols and footprints without 3D model associations
- PADS currently exports ASCII PCB Decals only; it does not export symbols, native binary libraries, or 3D associations, and rejects geometry that cannot be represented losslessly
- Eagle exports XML symbols, packages, DeviceSets, and pin associations; 3D files are emitted by the independent stage, but unverified managed `package3d` associations are not written, and unsupported geometry is rejected
- P-CAD currently exports Pad Styles and Patterns in an ASCII PCB Library only; it does not export symbols, Component/Part associations, or 3D models, and rejects geometry that cannot be represented losslessly

## Performance Optimization

### Parallel Conversion

Support multi-threaded parallel processing to fully utilize multi-core CPUs.

**Features:**
- QThreadPool for thread management
- QRunnable for task execution
- QMutex for thread safety
- Progress tracking
- Error isolation

**Benefits:**
- Faster conversion speed
- Better resource utilization
- Improved user experience

### Two-Stage Pipeline Parallel Architecture

Optimize batch conversion performance with a two-stage pipeline strategy.

**Stage 1: Data Fetch (Fetch, concurrent network tasks)**
- Parallel data fetching from network
- Asynchronous network requests
- Timeout and retry support

**Stage 2: Export (Process/Write, parallel export tasks)**
- Build IR, convert to the target format, and write files in isolated tasks
- Altium STEP models are embedded into PcbLib during footprint writing
- Aggregate success and failure states for symbols, footprints, and 3D models

**Progress weights**: dynamically aggregated by `ExportProgress` from task counts and stage state

**Benefits:**
- Optimal performance
- Thread-safe file operations
- Reliable data export

### State Machine Pattern

Async data collection for improved response speed.

**States:**
- Idle - Initial state
- Collecting - Data collection in progress
- Completed - Collection completed
- Error - Error occurred

**Benefits:**
- Clear state management
- Better error handling
- Improved user feedback

## User Interface

### Modern Interface

Fluent user interface based on Qt 6 Quick.

**Features:**
- Card-based layout
- Smooth animations
- Responsive design
- Intuitive navigation

**UI Components:**
- Card container
- Modern buttons
- Icon components
- List items
- Progress indicators

### Dark Mode

Support dark/light theme switching.

**Features:**
- Global style system
- Dynamic theme switching
- Component adaptation
- Smooth transitions
- Configuration persistence

**Theme Options:**
- Light mode
- Dark mode
- System default

### Real-time Progress

Real-time display of conversion progress and status.

**Features:**
- Progress bar
- Status messages
- Component count
- Success/failure indicators
- Elapsed time

## Advanced Features

### Layer Mapping System

Complete EasyEDA to KiCad layer mapping (50+ layers).

**Supported Layers:**
- Signal layers (Top, Bottom, Inner1-32)
- Silkscreen layers (Top, Bottom)
- Solder mask layers (Top, Bottom)
- Paste layers (Top, Bottom)
- Mechanical layers
- User layers
- Edge cuts
- Fabrication layers

**Features:**
- Automatic layer mapping
- Unit conversion (mil to mm)
- Layer type detection
- Custom layer support

### Polygon Pad Support

Correct export of custom-shaped pads.

**Features:**
- Custom shape pad support
- Primitives block generation
- Coordinate transformation
- Size optimization
- KiCad compatibility

### Elliptical Arc Calculation

Precise arc calculation supporting complex geometric shapes.

**Features:**
- Complete algorithm implementation
- SVG arc parameter parsing
- Center and angle calculation
- KiCad format output
- High precision

### Text Layer Processing

Support for type "N" special processing and mirrored text processing.

**Features:**
- Type "N" text handling
- Mirrored text support
- Layer mapping
- Font effects
- Display control

### Overwrite File Function

Support overwriting existing KiCad V9 format files.

**Features:**
- File overwrite control
- KiCad V9 format support
- Configuration persistence
- UI integration
- User confirmation

### Smart Extraction

Support intelligent extraction of component numbers from clipboard text.

**Features:**
- Clipboard support
- Intelligent recognition
- Batch addition
- Format validation
- User-friendly interface

### BOM Import

Support importing BOM files for batch component conversion.

**Supported Formats:**
- CSV
- Excel (XLSX)

**Features:**
- File import
- Data parsing
- Component validation
- Batch processing
- Error reporting

## Network Optimization

### Retry Mechanism

Automatic retry on network request failures to improve conversion success rate.

**Features:**
- Configurable retry count (default: 3)
- Exponential backoff algorithm
- Smart delay calculation
- Error detection
- Retry logging

**Benefits:**
- Higher success rate
- Better reliability
- Improved user experience

### GZIP Decompression

Automatically decompress GZIP-encoded response data.

**Features:**
- Automatic decompression
- Reduced data transfer
- Faster response times
- Transparent handling
- zlib integration

**Benefits:**
- Reduced bandwidth usage
- Faster downloads
- Better performance

## Debug Features

### Debug Export Mode

Support saving original and exported data for debugging.

**Features:**
- Original data saving
- Exported data saving
- Detailed logging
- Error diagnosis
- Developer tools

**How to Enable:**

Build with debug export enabled:
```bash
cmake .. -DENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT=ON
```

**Output:**
- Original EasyEDA data (JSON)
- Exported KiCad data
- Conversion logs
- Error reports

## Data Validation

### Component ID Validation

Validate component IDs before processing.

**Features:**
- Format validation
- Length check
- Character check
- Error reporting
- User feedback

### File Path Validation

Validate file paths before export.

**Features:**
- Path format check
- Directory existence check
- Write permission check
- File overwrite protection
- User confirmation

## Configuration Management

### Command Line Arguments

Support configuring application startup behavior via command line arguments.

**Features:**
- Debug mode control
- Log level settings
- Log file path specification
- Config file path specification
- Interface language settings
- Interface theme settings
- Portable mode enablement
- Help and version information display

Command line arguments take precedence over other configuration methods, suitable for automation scripts and quick configuration.

### User Preferences

Save and load user preferences.

**Features:**
- Theme preference
- Export settings
- File paths
- Window position and size
- Other user settings

**Persistence:**
- Automatic save
- Application startup load
- Configuration file management
- Default values

## Error Handling

### Comprehensive Error Handling

Robust error handling throughout the application.

**Features:**
- Exception catching
- Error logging
- User-friendly error messages
- Error recovery
- Error reporting

**Error Types:**
- Network errors
- File I/O errors
- Data parsing errors
- Validation errors
- Conversion errors

## Accessibility

### Keyboard Navigation

Full keyboard navigation support.

**Features:**
- Tab navigation
- Keyboard shortcuts
- Focus management
- Screen reader support

### High Contrast

High contrast mode support.

**Features:**
- Improved visibility
- Better readability
- Accessibility compliance

## Internationalization

### Multi-language Support

Support for multiple languages.

**Current Languages:**
- English
- Chinese (Simplified)

**Features:**
- Language switching
- Translation files
- Unicode support
- Date/time formatting

## Security

### Data Privacy

Protect user data privacy.

**Features:**
- No data collection
- Local processing only
- No third-party tracking
- Secure network connections

### Input Sanitization

Sanitize user inputs to prevent security issues.

**Features:**
- Input validation
- Path traversal prevention
- SQL injection prevention
- XSS prevention

## Future Features

Planned features for future releases:

- [ ] More 3D model formats support
- [ ] Advanced search functionality
- [ ] Component library management
- [ ] Batch export optimization
- [ ] Custom template support
- [ ] Plugin system
- [ ] Cloud sync support
- [ ] Mobile version

## Feature Requests

To request new features, please:
1. Check existing GitHub Issues
2. Create a new issue with:
   - Feature description
   - Use case
   - Expected behavior
   - Examples (if applicable)

## Known Limitations

Current limitations:

- Limited 3D model format support
- No component library management
- No cloud sync
- Limited batch export options

## Performance Metrics

Typical performance metrics:

- Single component conversion: 2-5 seconds
- Batch conversion (10 components): 10-20 seconds
- Parallel conversion speedup: 2-4x (depending on CPU cores)

## Compatibility

### KiCad Versions

- KiCad 6.0+
- KiCad 7.0+
- KiCad 8.0+
- KiCad 9.0+

### EasyEDA Versions

- EasyEDA Standard
- EasyEDA Professional
- EasyEDA Pro

### Operating Systems

- Windows 10/11
- macOS 10.15+
- Linux (Ubuntu 20.04+, Debian 10+, Fedora 33+)

## Related Documentation

- [Getting Started Guide](GETTING_STARTED_en.md) - Learn how to use features
- [Architecture Documentation](../developer/ARCHITECTURE_en.md) - Understand feature implementation
- [Build Guide](../developer/BUILD_en.md) - Build the application with specific features
