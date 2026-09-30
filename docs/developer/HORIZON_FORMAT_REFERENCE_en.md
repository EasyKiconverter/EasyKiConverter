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

## Layout and implementation boundary

The official Pool uses `units/`, `symbols/`, `entities/`, `padstacks/`, `packages/`, `parts/`, and `3d_models/`. A Package is directory-based; official samples place its main object at `packages/<path>/package.json` and may store padstacks beside it.

UUIDs, JSON ordering, paths, and diagnostics must be deterministic within EasyKiConverter. This document records format behaviour only; the exporter is an independent implementation and does not copy Horizon GPLv3 source.

## Current export boundary

- Circles, ellipses, arcs, pies, and common package graphics are converted to Horizon polylines or polygons with approximation diagnostics.
- Custom pad polygons are written to Padstack `polygons`; a rounded rectangle is degraded to a rectangle when no corner radius is available.
- STEP/OBJ data is written under `3d_models/`; Package model placement combines `translation` and `stepOffsetMm`.
- Pictures and mechanical mounting holes are currently omitted with explicit diagnostics; Bézier and some curves use polyline approximation.

## Validation status

Qt host builds, deterministic output, reference integrity, pin-to-pad mapping, multipart data, degraded geometry, and 3D model tests are covered. The upstream `horizon-pr-review` was built at commit `ac014d2` and successfully ran `--pool-update` against a generated fixture Pool without file errors.
