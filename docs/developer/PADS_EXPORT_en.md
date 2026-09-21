# PADS PCB Footprint Export

The current feature supports **EasyEDA/LCSC footprint to PADS Parts Library ASCII PCB Decal** export only. It is not a PADS native binary library, schematic library, or complete component-library exporter.

## Export boundary

```mermaid
flowchart LR
    EasyEDA[EasyEDA/LCSC data] --> IR[Shared Footprint IR]
    IR --> Model[PADS Decal text model]
    Model --> Decal[Independent .d PCB Decal files]
    Decal --> PADS[User import/library workflow in PADS]
```

The exporter consumes the shared IR and does not parse EasyEDA JSON again or reuse KiCad, Altium, or Xpedition final writers. Coordinates are emitted in imperial mils, with `I` as the file-header unit marker.

## Implemented

- GUI target: `PADS PCB`.
- CLI target: `--target-format pads`.
- One sanitized `<name>.d` file per footprint in the output directory.
- Circle, square, rectangle, oval, and basic track/rectangle/region primitives.
- SMD and PTH pad stacks; PTH pads contain top and bottom records with drill and plating information.
- Sanitized-name collisions, empty pin numbers, invalid coordinates, and non-ASCII pin/text values stop export with diagnostics.
- Each Decal contains a header, timestamp, primitives, text, terminals, and pad-stack records.

## Explicit limitations

- Independent mechanical holes, arcs, RoundRect, Trapezoid, and custom Polygon pads are not emitted losslessly. Export fails with a diagnostic instead of silently degrading them.
- 3D model associations are not written. Models present in the IR produce a visible diagnostic. PADS 3D association requires a target-version-specific implementation and samples.
- PADS schematic/CAE libraries, complete Part/Logic libraries, multi-file indexes, and updates/appends to existing PADS libraries are not implemented.
- `no-overwrite`, `update-mode`, and `retry-mode` are rejected by the export stage rather than being presented as safe merges.
- PADS is not installed in the current environment. Automated validation covers text structure and file generation only; no PADS open/save/read-back validation has been performed.

## Format basis and usage

The implementation follows the [public PADS Parts Library ASCII specification](https://www.freecalypso.org/pub/CAD/PADS/pdfdocs/Plib_ASCII.pdf), including the PCB Decal header, `CIRCLE`/`OPEN`/`CLOSED` primitives, `T` terminals, and `PAD` pad-stack records. No third-party project source code was copied.

Select `PADS PCB` in the GUI or use:

```text
easykiconverter --target-format pads ...
```

Import the generated directory through the library workflow of the target PADS version. Confirm layer semantics, text encoding, manufacturing attributes, and library indexing in that software; this project does not claim cross-version PADS native compatibility.

## Tests

`tests/unit/test_pads_exporter.cpp` uses a local IR fixture to verify the Decal structure, mil unit header, SMD/PTH pad stacks, sanitized-name collisions, and failure diagnostics for data that cannot be represented losslessly. The tests do not access the network or require a local PADS installation.
