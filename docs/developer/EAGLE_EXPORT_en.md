# Eagle XML Footprint Export

The current scope is **EasyEDA/LCSC footprint to an Eagle XML `.lbr` package library**. This is not a complete Eagle component library: symbols, devices, variants, and 3D associations are not generated in the first phase.

```mermaid
flowchart LR
    Source[EasyEDA/LCSC] --> IR[Footprint IR]
    IR --> Writer[Eagle XML package writer]
    Writer --> Library[.lbr library]
```

## Implemented

- GUI target `Eagle PCB` and CLI target `--target-format eagle`.
- Basic XML output for SMD, PTH, independent mechanical holes, circles, rectangles, tracks, regions, and text.
- Conservative mapping for top/bottom copper, silkscreen, mask, paste, assembly, keepout, and mechanical layers.
- Sanitized package-name collisions, empty pin numbers, invalid values, unknown layers, and unsupported primitives produce failure diagnostics.
- A single UTF-8 XML `.lbr` file is emitted and read back with Qt's XML reader in tests.

## Limitations and validation

- Eagle symbols, devices, complete component libraries, and 3D associations are not generated.
- Polygon/Trapezoid pads, slots, and arcs are rejected instead of silently converted.
- Eagle is not installed in the current environment; XML structure and reference checks are covered, but no target-software open/save or cross-version validation has been performed.
- Update, append, and retry modes are rejected; an existing file is not replaced when overwrite is disabled.

