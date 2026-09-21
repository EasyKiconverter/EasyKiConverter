# Eagle XML Combined Library Export

The current scope is **EasyEDA/LCSC to an Eagle XML `.lbr` combined library**. The same file contains packages, symbols, DeviceSets, and pin-to-pad connections, so this is not a footprint-only export. 3D files are emitted by the independent model stage; managed Eagle `package3d` associations are not fabricated because they require managed URNs.

```mermaid
flowchart LR
    Source[EasyEDA/LCSC] --> ComponentIR[Unified Component IR]
    ComponentIR --> XML[Eagle XML writer]
    XML --> Package[Package]
    XML --> Symbol[Symbol]
    XML --> DeviceSet[DeviceSet and pin-pad mapping]
    ComponentIR --> ModelStage[Independent 3D model stage]
    ModelStage --> Models[WRL/STEP files]
```

## Implemented

- GUI target `Eagle PCB` and CLI target `--target-format eagle`.
- Basic XML output for SMD, PTH, independent mechanical holes, circles, rectangles, tracks, regions, and text.
- When symbol and footprint export are enabled, symbols, DeviceSets, gates, devices, and pin-to-pad connections are written; multi-part symbols are emitted as separate gates.
- When 3D export is enabled, the independent stage emits WRL/STEP files and reports the result; the `.lbr` does not contain an unverified managed `package3d` URN.
- Conservative mapping for top/bottom copper, silkscreen, mask, paste, assembly, keepout, and mechanical layers.
- Sanitized package-name collisions, empty pin numbers, invalid values, unknown layers, and unsupported primitives produce failure diagnostics.
- A single UTF-8 XML `.lbr` file is emitted and read back with Qt's XML reader in tests.

## Limitations and validation

- Managed Eagle `package3d`, variants, and URN-based 3D associations are not generated.
- Symbol curves, arcs, and geometry that cannot be represented by the supported Eagle XML primitives are rejected instead of silently downgraded.
- Polygon/Trapezoid pads, slots, and arcs are rejected instead of silently converted.
- Eagle is not installed in the current environment; XML structure and reference checks are covered, but no target-software open/save or cross-version validation has been performed.
- Update, append, and retry modes are rejected; an existing file is not replaced when overwrite is disabled.
