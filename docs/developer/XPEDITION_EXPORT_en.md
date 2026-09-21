# Xpedition ASCII Library Export

This document describes the current Xpedition ASCII library export implemented through the unified IR. It generates target text from EasyEDA data after IR normalization; it does not read Xpedition libraries and does not claim complete compatibility without validation in the target software version.

## Current status

| Capability | Status | Notes |
| --- | --- | --- |
| Symbol export | Implemented | Generates Xpedition ASCII symbol text for pins, electrical types, body bounds, rectangles, polylines, polygons, circles, and three-point arcs |
| Footprint export | Implemented | Generates Padstack and Cell HKP text for pad solder paste/mask layers, round holes, slots, outlines, graphics, text, regions, and standalone holes; SMD pads preserve their top or bottom mounting side; standalone-hole references are structurally checked |
| Footprint geometry fidelity | Implemented | Supports rotated rectangles, polyline approximation for arcs, and mapping TopOverlay/BottomOverlay to the corresponding silkscreen side |
| Multi-part symbols | Implemented | Writes one symbol entry per part |
| ZIP packaging | Implemented | Produces separate uncompressed ZIP packages for symbols and footprints |
| 3D model output | Implemented (standalone files) | WRL/STEP files are emitted by the independent stage; native Xpedition 3D association is not written |
| Target-software validation | Not complete | No usable Xpedition installation or command-line reader is available in the current environment; automated tests cover text structure, ZIP integrity, and read-back rules |

## Data flow

```mermaid
flowchart LR
    EasyEDA[EasyEDA component data] --> Importer[EasyEDA Importer]
    Importer --> IR[Unified IR]
    IR --> Symbol[ExporterXpeditionSymbol]
    IR --> Footprint[ExporterXpeditionFootprint]
    IR --> Model3D[Model3DExportStage]
    Symbol --> SymbolZip[lib-name_Symbols.zip]
    Footprint --> FootprintZip[lib-name_Footprints.zip]
    Model3D --> Models[standalone WRL/STEP files]
```

The exporter does not re-parse EasyEDA JSON. Coordinates, pin semantics, pad types, and footprint primitives are normalized by the IR builder first, then converted to Xpedition units and syntax.

## Units and coordinates

- IR geometry uses millimeters.
- Xpedition text currently uses thousandth-inch (TH) units.
- Lengths are converted with `mm / 0.0254`.
- The footprint Cell origin is the center of the current geometry bounds, with Y inverted for output.
- Through-hole round and slot holes receive separate Hole definitions; slot definitions include independent diameter and length dimensions.
- Standalone holes produce a non-plated Padstack and the Cell references the exact same complete name; tests check both the definition and the reference.
- Symbol coordinates preserve the IR orientation and are converted to TH values.

## Output files

When symbols and footprints are exported together, the output contains:

- `<lib-name>_Symbols.zip`: ASCII symbol entries named `<symbol-name>.<part-number>`.
- `<lib-name>_Footprints.zip`: `<name>_Pads.hkp` and `<name>_Cell.hkp` for each footprint.
- Standalone 3D model directory: WRL, STEP, or both are emitted according to the selected format; model names remain associated with the component cache identifiers.

The two stages cannot share one `.zip` path because they run concurrently and would conflict during commit. The exporters therefore return `_Symbols.zip` and `_Footprints.zip` suffixes respectively.

## Degradation and diagnostics

IR data that cannot be safely represented is not silently reported as exported:

- Footprint arcs are approximated as polylines with a maximum 15-degree step and produce an approximation diagnostic; text, filled regions, and standalone holes are written to the Cell.
- Rotated rectangles are serialized from their rotated vertices; TopOverlay/BottomOverlay are mapped to the corresponding silkscreen side.
- SMD and through-hole pads reference solder paste/mask definitions; when IR has no mask expansion field, an explicit 8 TH default expansion is used.
- Text mirroring, text paths, inconsistent KeepOut flags, courtyard generation, and unknown layers produce diagnostics when the target cannot express them completely.
- Xpedition native 3D associations are not written; models are emitted by the independent stage and the result diagnostics explain that manual association in the target tool is required.
- Symbol ellipses, pies, elliptical arcs, paths, Bézier curves, IEEE graphics, ordinary text, text frames, and images still produce unsupported-element diagnostics.
- Duplicate footprint names receive numeric suffixes so ZIP entry names remain unique.
- Cleaned symbol-name collisions receive stable suffixes; quotes, backslashes, and line breaks in REFDES, VALUE, pin names, and pin numbers are escaped.
- Xpedition ZIP output currently supports full replacement only. Existing archives are rejected when overwrite is disabled, and append, update, and retry modes are explicitly rejected rather than treated as safe merges.
- RoundRect, Trapezoid, and Polygon pads are written as CUSTOM only when the IR provides at least three custom vertices. Non-circular Ellipse pads are written as OBLONG; unrepresentable shapes fail with a diagnostic.
- Entry names are restricted to safe relative names; absolute paths, backslashes, and traversal segments are rejected.
- Unrecoverable stage errors are added to export-progress diagnostics; the GUI exposes them in the result state and the CLI prints them as export diagnostics.

## Verification scope

Automated verification currently includes:

- Dedicated Xpedition symbol and footprint unit tests.
- Through-hole Padstack hole-reference tests.
- Diagnostics tests for unsupported footprint primitives.
- The full CTest suite, including standalone-hole, pad-shape, safe-name, existing-library policy, and standalone 3D stage regressions.
- A real export was attempted with the project BOM fixture and existing cache; the EasyEDA component API currently returns HTTP 403, so this external-network run is not counted as passing.
- Repeatable local verification uses fixed IR data, ZIP entry contents, and project cache paths when external data is unavailable.

Do not claim target-software compatibility until the generated libraries have been opened and saved in the intended Xpedition version.

## Code entry points

| Responsibility | File |
| --- | --- |
| ZIP writer | `src/core/xpedition/XpeditionZipWriter.*` |
| Symbol exporter | `src/core/xpedition/ExporterXpeditionSymbol.*` |
| Footprint exporter | `src/core/xpedition/ExporterXpeditionFootprint.*` |
| Exporter registration | `src/core/ExporterFactory.cpp` |
| Dedicated tests | `tests/unit/test_xpedition_exporter.cpp` |
