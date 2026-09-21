# P-CAD ASCII footprint library export

The current feature supports **EasyEDA/LCSC footprint to P-CAD ASCII PCB Library (`.lia`)** export. It is not a complete P-CAD component library: this stage writes Pad Styles and Patterns only, without schematic symbols, Component/Part associations, or 3D model links.

```mermaid
flowchart LR
    IR[FootprintIR] --> M[P-CAD Pad Style and Pattern mapping]
    M --> L[.lia ASCII Library]
    L --> P[Import in P-CAD or a compatible tool]
```

## Implemented

- SMD and through-hole pads.
- Ellipse, rounded rectangle, rectangle, and oval Pad Styles.
- Through-hole diameter and plating flag.
- Common graphical layer mappings using the default P-CAD layers 1 through 11.
- Lines, circles, rectangles, polygon regions, and text graphics.
- Pad Style deduplication using geometry, drill, plating, and layer semantics.
- Local read-back validation with the project P-CAD parser.

## Output and limitations

Select `P-CAD PCB` in the GUI or use `--target-format pcad` in the CLI. The output is a single `<library>.lia` file.

The exporter rejects independent unnumbered mounting holes, slots, polygon/trapezoid pads, unmappable layers, non-ASCII names or text, and update, append, or retry modes for an existing library. 3D models are skipped with a visible diagnostic.

The file declares `fileUnits MM`, writes coordinates using P-CAD's upward-positive Y convention, and stores rotations in tenths of a degree.

`tests/unit/test_pcad_exporter.cpp` validates the header, Pad Style deduplication, Pattern and graphics output, name collisions, and independent-hole diagnostics, then reads the generated file through the project P-CAD parser. P-CAD or another official compatible tool is not installed in the current environment, so official open/save/read-back validation has not been performed.

The format research used public P-CAD ASCII structure documentation and KiCad format research. The implementation is independently written against EasyKiConverter's FootprintIR; no third-party source code was copied.
