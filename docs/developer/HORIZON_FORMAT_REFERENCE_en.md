# Horizon EDA Format Reference

This document records format facts verified while developing the Horizon exporter. Implementations must remain grounded in the upstream source; this is not a source-free field guess.

## Verification sources

- Horizon: `https://github.com/horizon-eda/horizon`
- Verified commit: `ac014d28cf651fa05cef9f54f5b493053de9852b`
- Horizon Pool: `https://github.com/horizon-eda/horizon-pool`
- Pool verified commit: `0696685973dee6c9d7202704d23510768e212744`
- Verification date: 2026-10-01

The primary evidence is `src/pool/{unit,symbol,entity,package,padstack,part,pool_info}.{cpp,hpp}`, `src/package/{pad,hole,shape}.{cpp,hpp}`, `src/common/common.hpp`, and the readers and `serialize()` methods in `src/pool-update/`.

## Confirmed facts

- Horizon coordinates are integer nanometres; `common.hpp` states that one unit is 1 nm, so 1 mm is 1,000,000 units.
- `pool.json` must contain `uuid`, `default_via`, `name`, and `type: "pool"`; `pools_included` and `default_frame` are optional, and `default_frame` requires file version 1.
- A Unit file has `type: "unit"`, `name`, `manufacturer`, `uuid`, and UUID-keyed `pins`. Each Pin uses `primary_name`, `direction`, and `swap_group`, with optional alternate names.
- A Symbol references a Unit UUID and UUID-keys `junctions`, `pins`, `lines`, `arcs`, `polygons`, `texts`, and `text_placements`.
- An Entity indexes `gates` by Gate UUID; each Gate includes `name`, `suffix`, `swap_group`, and a Unit UUID.
- A Part references `entity` and `package`, and its `pad_map` maps each Package Pad UUID to a Gate UUID and Unit Pin UUID.
- A Package must include `default_model` even when it has no 3D model; use the all-zero UUID in that case, or point to one UUID in `models` when models exist.
- PoolUpdater owns the SQLite `pool.db` index; EasyKiConverter must write source files and must not copy its database schema writer.
- The export stage first invokes the official Python binding on a temporary Pool with `horizon.Pool.update(path)`, verifies a non-empty `pool.db`, and only then commits the source files. After the commit it checks `PoolManager.get_pools()` for UUID conflicts, calls `horizon.PoolManager.add_pool(path)`, and reads the registry back. Append, update, and retry copy the existing Pool into the temporary directory before writing the selected components, preserving entries not included in the current export; overwrite rebuilds the Pool. The interpreter can be selected with `EASYKICONVERTER_HORIZON_PYTHON`, and the module directory with `EASYKICONVERTER_HORIZON_PYTHONPATH`; a PoolUpdater failure leaves the source directory untouched, while a registration failure fails the export and retains the generated source directory with diagnostics.

## Layout and implementation boundary

The official Pool uses `units/`, `symbols/`, `entities/`, `padstacks/`, `packages/`, `parts/`, and `3d_models/`. A Package is directory-based; official samples place its main object at `packages/<path>/package.json` and may store padstacks beside it.

UUIDs, JSON ordering, paths, and diagnostics must be deterministic within EasyKiConverter. This document records format behaviour only; the exporter is an independent implementation and does not copy Horizon GPLv3 source.

## Current export boundary

- Package circles use four native Horizon arcs, and rectangles/poly-lines use native Lines with their stroke width. Ellipses, pies, Bézier curves, and other primitives without an equivalent native object are converted to polylines or polygons with approximation diagnostics.
- SMD Padstacks write copper, solder-mask, and paste shapes and use the official `parameter_program` for solder-mask expansion and paste contraction; custom pad polygons are written to the corresponding Padstack `polygons`, with rotated manufacturing outlines expanded through `expand-polygon`.
- Through-hole Padstacks write copper shapes on the supported copper layers, top/bottom mask shapes, plating state, and round/slot holes; standalone mounting holes remain mechanical Padstacks and never become electrical pins.
- Non-plated through holes (NPTH) use the `plated: false` mechanical semantics; even when the source has no number, they produce only a Package Pad and Padstack and are omitted from the Part `pad_map`, so no electrical pin is fabricated.
- Standalone mounting holes are written as mechanical Padstacks referenced by Package Pads. The current IR expresses standalone holes as round holes; slot length still requires source data that provides it.
- KeepOut uses Package `polygons` plus `keepouts` usage objects and is not mapped to the courtyard layer.
- A round rectangle first preserves an IR-provided custom outline; when both the radius and outline points are unavailable, it is downgraded to a rectangle and reported in diagnostics.
- When 3D export is enabled, STEP data is written under `3d_models/` and Package model placement combines `translation` and `stepOffsetMm`; the fixed Horizon version's desktop 3D loader supports STEP only, so OBJ-only models are rejected with a diagnostic. When 3D export is disabled, no model files or references are written.
- Pictures are currently omitted with an explicit diagnostic; Bézier and some curves use polyline approximation.
- The exporter uses the official binding to create `pool.db` and register the Pool. This repository does not copy the official SQLite schema. If the official binding is unavailable, export fails with an explicit diagnostic instead of claiming automatic registration.
- The pinned upstream has no public runtime-refresh API for external applications; its `reload-pools` notification uses Horizon-internal IPC and a cookie. Therefore a closed Horizon instance sees the registration on its next start, while a running instance must be restarted or manually reloaded as indicated. EasyKiConverter does not inject into the process or simulate GUI actions.

## Validation status

Qt host builds, deterministic output, reference integrity, pin-to-pad mapping, multipart data, paste, mounting holes, KeepOut, degraded geometry, and 3D model tests are covered. The upstream `horizon-pr-review` was built at commit `ac014d2` and successfully ran `--pool-update` against an EasyKiConverter-generated fixture Pool without file errors; `pool.db` was read back for entity, symbol, unit, package, padstack, and part indexes. The same upstream commit was then built as `horizon.so` using a Python 3.12 development package unpacked into a repository-local temporary directory. `horizon.Pool.update()` and `horizon.PoolManager.add_pool()` succeeded, and `pools.json` was read back under an isolated `XDG_CONFIG_HOME`. On 2026-10-02, the real C2040 component was exported to a Pool, and the same pinned Horizon desktop build was launched in the Niri/Wayland session. The registered Pool opened `RP2040` in the Unit editor; a manufacturer field was edited and saved, then the application was restarted and the same Unit was reopened with the edited value intact. Runtime refresh remains unverified; a running instance still requires restart or manual reload under the pinned upstream behavior.

The repository integration validator can repeat the official-tool check without modifying the input Pool. It creates and removes an isolated copy beside the Pool:

```bash
.venv/bin/python tests/integration/validate_horizon_pool.py \
  --pool <generated-pool> \
  --horizon-review <path-to-horizon-pr-review>
```

The command runs the fixed-version `horizon-pr-review --pool-update`, requires a non-empty `pool.db`, and checks both the official indexes and source-JSON references for Unit, Symbol, Entity, Package, Padstack, Part, Pad Maps, and 3D files; model paths that escape the Pool root are rejected. To validate the official Python binding update and registration path, replace the last argument with:

```bash
EASYKICONVERTER_HORIZON_PYTHONPATH=<directory-containing-horizon.so> \
.venv/bin/python tests/integration/validate_horizon_pool.py \
  --pool <generated-pool> \
  --horizon-python <python-interpreter>
```

Both modes operate on an isolated copy and leave the input Pool unchanged; Python mode also verifies `PoolManager.add_pool()` in a temporary `XDG_CONFIG_HOME`. C2040 desktop editing, saving, and reopening after a restart were completed in the Niri/Wayland session; command-line and desktop checks still do not constitute validation of runtime refresh in an already-running Horizon process.
