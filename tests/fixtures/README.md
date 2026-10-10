# Test Fixtures

本目录存放自动化测试使用的稳定输入数据；[fixture provenance](../../docs/developer/ai-development/fixture-provenance.json) 记录哈希、用途和未知来源。来源信息未知时必须保持 `unknown`，不得从文件内容猜造。

This directory stores stable inputs for automated tests. The [fixture provenance manifest](../../docs/developer/ai-development/fixture-provenance.json) records hashes, purposes, and unknown provenance. Unknown source facts must remain `unknown` and must not be inferred from file contents.

This directory stores stable input data for automated tests.

- `easyeda/`: EasyEDA API responses, CAD JSON payloads, and malformed response samples.
- `bom/`: CSV/XLSX BOM files covering header variants, blank rows, duplicates, invalid LCSC IDs, and package placeholder filtering.
- `altium/`: Altium library samples; provenance and redistribution status are not verified by this repository.
- `allegro/`: Allegro Import Package fixture data.
- `cadstar/`, `pcad/`, `xpedition/`: local parser/exporter samples; their original source and software version remain unknown unless the manifest says otherwise.

Fixtures must be deterministic and must not require network access.
