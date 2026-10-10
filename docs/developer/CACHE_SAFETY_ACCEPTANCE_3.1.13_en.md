# v3.1.13 Cache Safety Acceptance Record

## Scope

This record verifies the Issue #210 cache-deletion boundary in v3.1.13 source code: directory selection, legacy-cache migration, self-healing, quota pruning, cache clearing, and asynchronous-write boundaries. It covers cache safety only and is not a complete desktop release acceptance record.

The baseline is the `v3.1.13` version-branch commit `c5e14f92801e09ce6eb143acdb857db6745fdc48`; the `v3.1.13` tag has not been created in the current repository. The acceptance work used branch HEAD `cabf64f014aad355c267f78a011c9b8af846066a`. The repository contains the `v3.1.12` source tag, but no safely runnable v3.1.12 binary was found, so no claim is made for a real old-binary upgrade test.

## Isolation and sample provenance

- Every test uses QtTest `QTemporaryDir` for cache roots, migration targets, and configuration files.
- Migration tests construct a v3.1.12-equivalent legacy layout by removing current ownership fields and the root marker while retaining identifiable component metadata. The samples are cleaned with the temporary directories after each run.
- Real user homes, real cache directories, and user document directories are never used as cleanup targets.
- `CacheSafety::TrashFunction` is injected to verify successful trash calls, failure behavior, and the absence of permanent-delete fallback.

## Scenario results

| Scenario | Result | Evidence |
| --- | --- | --- |
| Empty custom cache directory | Passed | `test_cache_safety::acceptsEmptyCustomChildDirectory` |
| Home/root paths and non-empty unowned directories | Passed | `test_cache_safety`, `test_export_settings_viewmodel` |
| Symlinked parent or cache entries | Passed | `rejectsSymlinkParentDirectory`, `rejectsSymlinkedModel3DDirectory`, unknown-entry tests |
| Recognizable legacy-cache migration | Passed | `testLegacyCacheDirMigrationPreservesExistingCache` |
| Target conflicts and migration failure | Passed | `testCacheDirMigrationConflictPreservesSourceEntries`, ViewModel migration-failure tests |
| Unknown files and directories | Passed | `testClearCachePreservesUnknownComponentFiles`, unverifiable-entry migration test |
| Cache clearing and quota pruning | Passed | `test_cache_pruner`, `test_component_cache_service` |
| Trash failure | Passed | `preservesEntryWhenTrashFails`, `testPrunePreservesEntryWhenTrashFails` |
| Generation, cancellation, and tombstone boundaries | Passed | stale-write, tombstone, and generation tests |

The safety policy remains conservative: maintenance is allowed only when the root, component directory, metadata, and individual cache file ownership can be verified. Unknown content is preserved, and trash failure never falls back to permanent deletion. A symlinked 3D-model directory is rejected, so the application does not follow a link to write or clean an external directory.

## Checks executed

The following commands ran with the project `.venv` and Qt 6.10.2 GCC environment, all with exit status 0:

```bash
QT_QPA_PLATFORM=offscreen ./.venv/bin/python tools/python/build_project.py --test -j 2
./.venv/bin/python tools/python/format_code.py --check --cpp --dirs src tests
./.venv/bin/python tools/python/check_comment_rate.py --threshold 100 --check-doxygen --check-variables --check-params --forbid-generic-comments --forbid-external-references tests/unit/test_export_settings_viewmodel.cpp
./.venv/bin/python tools/python/validate_ai_development.py
./.venv/bin/python tools/python/build_docs.py --mkdocs
git diff --check
```

The project build tool reported that the full test suite passed. The cache-focused tests were also reviewed individually: `test_cache_safety`, `test_cache_pruner`, `test_component_cache_service`, and `test_export_settings_viewmodel`.

## Unverified items and release conclusion

- A real v3.1.12-binary-to-v3.1.13 startup migration was not run; the evidence is based on the v3.1.12 source tag and an equivalent isolated legacy layout.
- The system trash was not observed in a real desktop session. The injected implementation proves business-level invocation, preservation on failure, and the absence of permanent-delete fallback, but not the availability of the current desktop trash implementation.
- No Niri/Wayland file-manager or real-user-directory cleanup test was run.

The cache-safety code and automated boundaries therefore have reproducible evidence. If release policy requires a real old-binary upgrade or real system-trash verification, those manual acceptance items remain open; this record must not be presented as complete hardware or desktop acceptance.

[中文版](CACHE_SAFETY_ACCEPTANCE_3.1.13.md)
