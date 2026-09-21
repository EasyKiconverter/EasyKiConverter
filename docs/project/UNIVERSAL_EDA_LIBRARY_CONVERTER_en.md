# Universal EDA Library Converter Plan

## Purpose

This document defines EasyKiConverter's long-term direction, scope, and boundaries for EDA library conversion. It is a project planning document; the formats listed here are not necessarily implemented yet.

## Long-term direction

EasyKiConverter aims to provide EDA library conversion through a common intermediate representation (IR):

```mermaid
flowchart LR
    Source[Supported source library] --> Importer[Importer]
    Importer --> IR[EasyKiConverter IR]
    IR --> Exporter[Exporter]
    Exporter --> Target[Supported target library]
```

Once a format has both an Importer and an Exporter, it can in principle interoperate with other integrated formats. Format adapters should handle format-specific syntax and semantics; dedicated pairwise converters should not be added.

The repository already contains the IR directory and foundational types such as `SymbolComponentIR`, `FootprintComponentIR`, and `Model3DIR`. See [ADR 012: Intermediate Representation Refactor](adr/012-intermediate-representation-refactor.md) and [Conversion Mapping](../developer/CONVERSION_MAPPING.md).

## Current export capabilities

The current export pipeline builds symbol, footprint, and 3D data from the component cache and selects either an independent stage or a combined-library writer according to the target format. The table describes the implemented code boundaries; it does not claim complete coverage of each target EDA's native format:

| Target | Symbol library | Footprint library | Device association | 3D output | Native 3D association |
| --- | --- | --- | --- | --- | --- |
| KiCad | `.kicad_sym` | `.kicad_mod` | Component-data association | WRL/STEP/OBJ | Written in KiCad footprint syntax |
| Altium | `.SchLib` | `.PcbLib` | Component and model records | STEP | Embedded in `.PcbLib` |
| Xpedition | ASCII ZIP | ASCII ZIP | Separate symbol and footprint packages | Standalone WRL/STEP | No unverified native association |
| Allegro | Not supported | Import Package | No schematic component library | STEP/model data in the package | Cadence is required to generate `.dra/.psm/.pad` |
| PADS | Schematic Decal `.c` | PCB Decal `.d` | Part Type `.p` | Standalone WRL/STEP | No native association currently written |
| Eagle | Symbols in `.lbr`, including representable arcs | Packages in `.lbr`, including representable arcs | DeviceSets and connections in `.lbr` | Standalone WRL/STEP | No unverified managed `package3d` |
| P-CAD | Schematic `.lia` | PCB `.lia` | `compDef` and Part associations | Standalone WRL/STEP | No native association currently written |
| CADSTAR | Components in `.lib` | Packages/Pads in `.lib` | Parts in `.lib` | Standalone WRL/STEP | No unverified private association |
| OrCAD Capture | XML | No Capture PCB library | `pcbFootprint` name property | Standalone WRL/STEP | XML does not invent a native 3D association |

“Standalone 3D” means that the common `Model3DExportStage` emits the model, or that a target Import Package includes it as a controlled file. It does not mean that the target software has already established a native model reference. Data that the target cannot express must be reported by export diagnostics rather than silently discarded.

The common 3D stage also writes `manifest.json` inside `<library>.3dmodels/`. It records component IDs, symbol names, footprint names, model files, model UUIDs, translations, rotations, STEP offsets, and per-item status so standalone 3D files can be matched to the symbol and footprint outputs from the same export. This is an EasyKiConverter project manifest; it does not claim that the target EDA has created a native 3D association.

Standalone models are written to `<library-name>.3dmodels/` by default and use the model name as the file-name stem. Duplicate model names receive stable suffixes so different components cannot overwrite one another. With “no overwrite” enabled, a model is skipped when all requested output files already exist; if only part of the requested formats exists, that component fails and the existing files remain unchanged.

## Scope and phases

The first phase focuses on library data: symbols, footprints, component associations, 3D models, and common metadata. The intended flow is:

```mermaid
flowchart LR
    Select[Select library] --> Detect[Detect or select source format]
    Detect --> Import[Import to IR]
    Import --> Target[Select target format]
    Target --> Export[Export library]
```

After the library model is stable, a separate project-conversion phase may cover schematics, PCBs, nets, instances, wires, buses, hierarchy, stackups, tracks, vias, zones, and design rules. Project conversion has substantially more format-specific semantics and requires a separate model and compatibility review.

## Quality boundaries

The project can provide conversion paths where both source and target adapters are implemented. It should not promise 100% lossless conversion between every format because EDA data models differ. Multi-unit symbols, Alternate Body/De Morgan data, special pin types, custom pad stacks, regions, variants, embedded models, associations, properties, fonts, and text alignment may not have direct equivalents.

The conversion pipeline should preserve semantic information in IR where possible, check target capabilities, and explicitly report complete conversions, degradations, and unmapped data. Important information must not be silently discarded.

## Conversion reports

Conversion Report or Compatibility Report should record successful, skipped, and failed objects; degradations and their reasons; unmapped properties; missing or unconvertible 3D models; and warnings that identify the affected component, graphic, or field.

## Format-support boundary

The project should aim to support as many mainstream EDA library formats as practical, with documented scope and limitations—not every EDA product, version, or private native file.

Prefer native parsing for stable, readable formats. Where appropriate, support official ASCII, XML, or exchange formats instead. Any external tool dependency must document its version, platform requirements, and known limitations.

## Project definition

> If EasyKiConverter supports an EDA library as an input format and has an Exporter for the target format, the library can be converted through EasyKiConverter IR. Compatibility checks and conversion reports describe complete conversions, degradations, and unmapped data.
