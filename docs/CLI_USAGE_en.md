# CLI Mode Usage

EasyKiConverter supports command-line mode, allowing conversion operations without opening the GUI.

## Subcommands

```bash
# Convert BOM file
easykiconverter convert bom -i <bom_file> -o <output_dir> [options]

# Convert single component
easykiconverter convert component -c <lcsc_id> -o <output_dir> [options]

# Batch conversion
easykiconverter convert batch -i <component_list_file> -o <output_dir> [options]
```

## Common Arguments

| Argument | Short | Description | Default |
|----------|-------|-------------|---------|
| `--input` | `-i` | Input file path | - |
| `--output` | `-o` | Output directory path | - |
| `--lib-name` | | Export library name | EasyKiConverter |
| `--component` | `-c` | LCSC component ID | - |
| `--target-format` | | Target format (kicad/altium/xpedition/allegro/pads/eagle) | kicad |
| `--symbol` | | Export symbol library | true |
| `--footprint` | | Export footprint library | true |
| `--3d-model` | | Export 3D models (default WRL format) | false |
| `--3d-model-format` | | 3D model format (wrl/step/both) | wrl |
| `--datasheet` | | Export datasheets | false |
| `--preview` | | Export preview images | false |
| `--3d-path-mode` | | 3D model path mode (relative/absolute) | relative |
| `--symbol-description` | | Symbol library description text | - |
| `--footprint-description` | | Footprint library description text | - |
| `--weak-network` | | Weak network mode (double timeout, more retries, lower concurrency) | false |
| `--update-mode` | | Update mode (only export missing or changed files) | false |
| `--no-overwrite` | | Do not overwrite existing files | false |
| `--cache-dir` | | Set disk cache directory | Cache directory from config |
| `--cache-size-mb` | | Set disk cache size limit (MB) | Cache size limit from config |
| `--progress` | | Show progress bar | false |
| `--quiet` | `-q` | Quiet mode | false |
| `--debug` | `-d` | Debug mode (generate detailed reports) | false |
| `--completion` | | Generate shell completion script | - |

## Default Export Options

CLI mode exports the following by default:
- Symbol library (Symbol)
- Footprint library (Footprint)

**Note**:
- 3D models, preview images, and datasheets are not exported by default
- Use `--3d-model` when needed; KiCad defaults to WRL, while Altium automatically converges to STEP and embeds it in PcbLib
- Xpedition currently exports symbol and footprint ZIP packages only and does not support 3D model association; `--3d-model` records a warning and skips the 3D stage
- PADS currently exports ASCII PCB Decal footprints only and does not support symbols or 3D model associations; `--3d-model` records a warning and skips the 3D stage, while update, append, and retry modes are rejected
- Eagle exports XML `.lbr` libraries containing symbols, packages, DeviceSets, and pin-to-pad connections; `--3d-model` emits WRL/STEP files through the independent model stage but does not fabricate managed `package3d` associations, while update, append, and retry modes are rejected
- P-CAD currently exports Pad Styles and Patterns in an ASCII PCB Library (`.lia`) only and does not support symbols, Component/Part associations, or 3D models; `--3d-model` records a warning and skips the 3D stage, while update, append, and retry modes are rejected
- Use `--datasheet` when datasheets are needed
- Normal mode does not generate detailed reports; only in debug mode (`--debug`)

## Cache Configuration

CLI mode reuses the same component disk cache as GUI. You can override the cache directory and size limit for the current run:

```bash
easykiconverter convert component -c C12345 -o ./output \
  --cache-dir /tmp/easykiconverter-cache \
  --cache-size-mb 2048
```

Notes:
- `--cache-dir` specifies the cache root directory for this run
- Valid range for `--cache-size-mb` is `1` to `1048576`
- When modifying the cache directory in GUI, the app attempts to migrate old cache data to the new directory to reuse already-downloaded data; existing files with the same name are not overwritten
- 3D model cache is stored in the `model3d` subdirectory of the cache directory, excluded from LRU eviction by default to avoid frequent re-downloading of large files

## Shell Completion

EasyKiConverter supports shell completion for bash, zsh, and fish.

### Bash Completion

```bash
# Temporary (current session)
eval "$(easykiconverter --completion bash)"

# Permanent
easykiconverter --completion bash >> ~/.bash_completion
echo 'source ~/.bash_completion' >> ~/.bashrc
```

### Zsh Completion

```bash
# Temporary (current session)
eval "$(easykiconverter --completion zsh)"

# Permanent
mkdir -p ~/.zsh
easykiconverter --completion zsh > ~/.zsh/_easykiconverter
echo 'fpath=(~/.zsh $fpath)' >> ~/.zshrc
echo 'autoload -Uz compinit && compinit' >> ~/.zshrc
```

### Fish Completion

```bash
# Permanent
easykiconverter --completion fish > ~/.config/fish/completions/easykiconverter.fish
```

### Completion Features

- Argument option completion: auto-completes `--help`, `--input`, `--output`, etc.
- Subcommand completion: auto-completes `convert`, `bom`, `component`, `batch`, etc.
- File completion: after entering `-i`, auto-completes `.xlsx`, `.csv`, etc.
- Directory completion: after entering `-o`, auto-completes directory paths
- **Dynamic LCSC ID completion**: after entering `-c`, auto-completes cached component IDs

## Usage Examples

```bash
# Default export (symbol + footprint library)
easykiconverter convert bom -i my_project.xlsx -o ./kicad_libs

# Export KiCad symbol, footprint, and 3D models (default WRL format)
easykiconverter convert bom -i my_project.xlsx -o ./output --3d-model

# Export Altium SchLib/PcbLib (3D models are embedded as STEP)
easykiconverter convert bom -i my_project.xlsx -o ./output --target-format altium --3d-model

# Export everything including preview images
easykiconverter convert bom -i my_project.xlsx -o ./output --3d-model --preview

# Debug mode (generate detailed report)
easykiconverter convert bom -i my_project.xlsx -o ./output --debug

# With progress bar
easykiconverter convert bom -i bom.xlsx -o ./output --progress

# Custom cache directory with 2GB disk cache limit
easykiconverter convert component -c C12345 -o ./output --cache-dir /tmp/ekc-cache --cache-size-mb 2048

# Convert single component
easykiconverter convert component -c C12345 -o ./output

# Batch conversion
easykiconverter convert batch -i components.txt -o ./output

# Export STEP format 3D models
easykiconverter convert bom -i my_project.xlsx -o ./output --3d-model --3d-model-format step

# Export datasheets
easykiconverter convert bom -i my_project.xlsx -o ./output --datasheet

# Use absolute paths for 3D model references
easykiconverter convert bom -i my_project.xlsx -o ./output --3d-model --3d-path-mode absolute

# Do not overwrite existing files
easykiconverter convert bom -i my_project.xlsx -o ./output --no-overwrite

# Only export missing or changed files (update mode)
easykiconverter convert bom -i my_project.xlsx -o ./output --update-mode

# Weak network mode (double timeout, more retries, lower concurrency)
easykiconverter convert bom -i my_project.xlsx -o ./output --weak-network

# Specify library description text
easykiconverter convert bom -i my_project.xlsx -o ./output --symbol-description "My Symbol Lib" --footprint-description "My Footprint Lib"

# Specify library name
easykiconverter convert bom -i my_project.xlsx -o ./output --lib-name my_lib
```

## Architecture

The CLI module uses a loosely coupled design with the following components:

- `CommandLineParser` - Command-line argument parser
- `CliContext` - CLI context holding shared state
- `CliPrinter` - CLI output utility
- `FileReader` - File reading utility
- `BaseConverter` - Base converter class
- `BomConverter` - BOM file converter
- `ComponentConverter` - Single component converter
- `BatchConverter` - Batch converter
- `CliConverter` - Main coordinator
- `CompletionGenerator` - Shell completion script generator
