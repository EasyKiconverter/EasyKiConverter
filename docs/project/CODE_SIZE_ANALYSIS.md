# 代码文件规模分析报告

> 分析日期：2026-09-18 | 工具：`python3 tools/python/analyze_project.py`

## 概述

本报告基于行数分析项目源码文件的规模分布，识别需要拆分或重构的过长文件。为后续 [ADR 012: IR 架构重构](adr/012-intermediate-representation-refactor.md) 和代码质量改进提供依据。

| 类型 | 文件数 | 过长 | 偏长 | 健康率 |
|------|--------|------|------|--------|
| 产品源码与资源 | 569 | 47 | 46 | -- |
| Python 工具 | 13 | 5 | 4 | -- |
| **合计** | **582** | **52** | **50** | -- |

当前统计工具按文件总行数使用统一阈值：高风险 >500 行，中风险 300-500 行，低风险 200-300 行。类型分项和代码/注释行数需要额外脚本才能精确拆分，因此本报告不再保留旧的推算健康率。

当前基线：`src` 509 个文件、78,488 行；`tests` 58 个文件、18,782 行；翻译资源 2 个文件、3,303 行；`tools/python` 13 个文件、6,642 行。项目工具的 `--all` 统计覆盖产品源码、测试和翻译资源，工具目录单独统计后合计 582 个文件、107,215 行。产品源码与资源的高风险文件 47 个、中风险文件 46 个、低风险文件 51 个；Python 工具分别为 5、4、1 个。

本轮重构已将批量转换器和 BOM 转换器重复的导出服务配置、信号连接、预加载等待、导出等待和结果汇总流程下沉到 `BaseConverter`，两个具体转换器仅保留输入文件解析和场景提示。该调整不改变 CLI 入口、选项配置、等待边界或导出结果语义。

已清理 `src/ui/qml/components/deprecated/ExportSettingsCard.qml`。核对结果显示运行时使用 `ExportSettingsBaseCard`，测试和 `qmldir` 仅依赖根目录下保留的 7 行兼容包装 `ExportSettingsCard.qml`，废弃目录文件无源码、构建或资源引用。

本轮还将 `NetworkClient`、`FetchWorker` 和 `NetworkWorker` 中同构的 `BlockingRequestContext` 合并为共享实现，保留请求方原有的取消、超时、回调线程和请求对象回收边界。

导出阶段审查暂不合并 `FootprintExportStage` 与 `SymbolExportStage`：前者包含缓存代次、嵌入 STEP、重复封装重命名、已有库合并和回滚，后者包含符号兼容转换与调试导出；两者的生命周期相似但业务分支不同，继续按行数抽象会扩大行为回归风险。

本轮将 `ComponentListItem.qml` 和 `ResultListItem.qml` 的复制、外链点击和悬停判定提取到 `ListItemInteraction.qml`；预览图、导出徽标、验证状态、提示动画和按钮布局仍由各自列表项维护。

Worker 审计结论：`DatasheetExportWorker` 和 `PreviewImagesExportWorker` 主要执行同步文件写入，`Model3DExportWorker` 还负责异步模型下载、网络错误回调、WRL/STEP 双路径和取消收敛；三者仅共享现有 `IExportWorker` 接口，不新增公共基类。

---

## 一、头文件问题清单

### 过长（>500 行）

| 文件 | 总行数 | 代码行 | 注释行 | 问题分析 |
|------|--------|--------|--------|---------|
| `src/services/ComponentCacheService.h` | 584 | 122 | 366 | 注释占比 63%，Doxygen 文档过重或接口声明过多 |
| `src/services/ComponentService.h` | 571 | -- | -- | 服务接口和异步请求状态定义仍较集中 |
| `src/core/network/INetworkClient.h` | 523 | 386 | 90 | 纯代码行过多，接口定义臃肿 |

### 偏长（300-500 行）

| 文件 | 总行数 | 代码行 | 说明 |
|------|--------|--------|------|
| `src/models/SymbolData.h` | 491 | 323 | 结构体过多，IR 重构后自动拆分 |
| `src/services/ConfigService.h` | 406 | 77 | 注释 266 行，文档比代码重 |
| `src/models/FootprintData.h` | 403 | 278 | IR 重构后自动拆分 |
| `src/utils/CommandLineParser.h` | 390 | 101 | 注释 228 行 |
| `src/services/export/ExportProgress.h` | 320 | 171 | 含 ExportOptions 等多个结构体 |

### 处理建议

- `SymbolData.h`、`FootprintData.h`：ADR 012 IR 重构时自然解决，无需单独处理
- `ComponentCacheService.h`、`ComponentService.h`：拆分接口，提取子服务或使用 Pimpl 隐藏实现细节
- `INetworkClient.h`：接口过大，考虑按功能分组（同步/异步/资源类型）拆分为多个接口
- 注释过重的文件：审查 Doxygen 注释是否重复了代码本身已表达的信息

---

## 二、源文件问题清单

### 服务与格式写入拆分追踪

| 文件 | 总行数 | 代码行 | 注释行 | 问题分析 |
|------|--------|--------|--------|---------|
| `src/services/ComponentCacheService.cpp` | 565 | -- | -- | 缓存生命周期和二级缓存接口仍集中，缓存路径、目录切换、读取校验、文件读取、预览图下载、元数据、3D 文件、CAD 数据、预览图写入、数据手册文件存储、数据手册下载、缓存维护、配额清理和一级内存缓存语义已提取 |
| `src/services/ComponentCacheFileReadCoordinator.cpp` | 70 | -- | -- | 独立协调符号、封装、CAD JSON、预览图和元数据的二级文件读取，保持一级缓存优先级及目录迁移锁边界 |
| `src/services/ComponentCacheReadCoordinator.cpp` | 105 | -- | -- | 独立协调完整缓存校验、元器件数据读取和符号封装 CAD 缓存验证，保持目录迁移锁与 L1 锁顺序 |
| `src/services/CacheDirectoryCoordinator.cpp` | 71 | -- | -- | 独立协调缓存目录切换、旧目录迁移、缓存代次失效、目录初始化和自修复，保持 ComponentCacheService 的公开接口与锁顺序 |
| `src/services/CachePathResolver.cpp` | 66 | -- | -- | 独立解析组件目录、元数据、预览图、数据手册和三维模型路径，并执行输入校验 |
| `src/services/PreviewImageDownloadService.cpp` | 119 | -- | -- | 独立协调预览图缓存命中、网络请求、取消、诊断和代次保护写入 |
| `src/services/ComponentCacheMetadataWriter.cpp` | 51 | -- | -- | 独立协调元数据快照合并、代次校验和 L1/L2 提交 |
| `src/services/ComponentCacheCadDataWriter.cpp` | 86 | -- | -- | 独立协调符号、封装和 CAD JSON 的校验、原子写入、配额维护及 L1 同步 |
| `src/services/ComponentCachePreviewImageWriter.cpp` | 52 | -- | -- | 独立协调预览图校验、代次/tombstone 检查、原子写入和配额维护 |
| `src/services/DatasheetCacheFileStore.cpp` | 88 | -- | -- | 独立承载数据手册缓存读取、格式校验、格式切换、旧文件清理和原子写入 |
| `src/services/ComponentCacheMaintenance.cpp` | 130 | -- | -- | 独立协调缓存删除、全量清理、L1 清理、有效组件枚举和磁盘大小统计 |
| `src/services/ComponentCacheQuotaEnforcer.cpp` | 40 | -- | -- | 独立协调磁盘缓存限制、冷却策略、目录清理和缓存大小信号 |
| `src/services/ComponentCacheMemoryStore.cpp` | 92 | -- | -- | 独立承载一级缓存复合键、元数据 JSON、CAD 数据校验、LRU 数据访问和容量统计 |
| `src/services/ComponentCacheModel3DCoordinator.cpp` | 91 | -- | -- | 独立协调三维模型缓存的锁边界、代次校验、文件读写、导出复制和磁盘配额触发 |
| `src/services/LcscImageService.cpp` | 560 | -- | -- | LCSC 预览图和数据手册请求服务；产品搜索响应解析、精确匹配和媒体字段提取已提取 |
| `src/services/LcscProductParser.cpp` | 115 | -- | -- | 独立解析 LCSC 产品搜索响应，执行元件精确匹配、图片地址规范化和媒体字段提取 |
| `src/ui/viewmodels/ComponentListViewModel.cpp` | 445 | -- | -- | ViewModel 仍包含列表模型接口、搜索选择、状态统计、导出状态和公开槽转发；批量导入、列表生命周期、剪贴板处理、失败项重试、列表数据回调、预览图协调、预览更新缓冲、列表项更新缓冲、验证队列状态、验证错误分类、定时器初始化和服务信号连接已提取 |
| `src/ui/viewmodels/ComponentListMutationCoordinator.cpp` | 185 | -- | -- | 独立协调列表添加、删除、清空、请求取消、验证队列移除和模型状态重置 |
| `src/ui/viewmodels/ComponentListClipboardCoordinator.cpp` | 64 | -- | -- | 独立协调剪贴板文本读取、元件编号提取、列表去重、批量添加和全量编号复制 |
| `src/ui/viewmodels/ComponentListRetryCoordinator.cpp` | 46 | -- | -- | 独立协调可重试失败项筛选、状态重置、服务请求和验证计数更新 |
| `src/ui/viewmodels/ComponentListPreviewCoordinator.cpp` | 117 | -- | -- | 独立协调预览图缓存更新、批量请求、完成状态和界面提示，保持公开槽接口及异步信号顺序 |
| `src/ui/viewmodels/ComponentListTimerCoordinator.cpp` | 73 | -- | -- | 独立创建并连接预览图、批处理、列表统计和延迟获取定时器，保持防抖窗口及原有回调顺序 |
| `src/ui/viewmodels/ComponentListServiceConnectionCoordinator.cpp` | 91 | -- | -- | 独立连接验证完成、组件数据、预览图成功/失败和批量预览完成信号，保持图片编码、缓存更新和验证状态推进顺序 |
| `src/ui/viewmodels/ComponentValidationCoordinator.cpp` | 81 | -- | -- | 独立协调验证队列的启动、并发调度、完成回调和 BOM 导入状态推进 |
| `src/ui/viewmodels/ComponentListDataCoordinator.cpp` | 166 | -- | -- | 独立协调组件基础信息、CAD、LCSC、数据手册和错误回调到列表项状态的转换 |
| `src/ui/viewmodels/ComponentListBatchCoordinator.cpp` | 189 | -- | -- | 独立协调批量添加、模型行插入、BOM 解析回调和验证启动时序 |
| `src/services/ComponentService.cpp` | 399 | -- | -- | 数据获取和服务编排仍在服务内，基础信息字段解析、BOM 文本编号提取、缓存加载任务、预览图文件编码、CAD 结果收敛、媒体回调、API 回调、队列初始化、并行批量协调、CAD 响应协调、请求取消和单请求启动已提取 |
| `src/services/ComponentRequestCoordinator.cpp` | 109 | -- | -- | 独立协调单请求去重、缓存命中、缓存代次读取和 CAD 后台获取，保持 ComponentService 的公开请求接口与旧回调链路不变 |
| `src/services/ComponentRequestCancellationCoordinator.cpp` | 58 | -- | -- | 独立协调全量请求和单器件请求的 API、网络客户端、图片服务取消及状态清理顺序 |
| `src/services/ComponentMediaCallbackCoordinator.cpp` | 274 | -- | -- | 独立协调预览图、LCSC 数据和数据手册回调，保持缓存代次校验、异步写入、并行队列完成通知和服务信号转发顺序 |
| `src/services/ComponentApiCallbackCoordinator.cpp` | 85 | -- | -- | 独立协调 EasyEDA API 基础信息和错误回调，保持缓存代次校验、失败状态清理、并行错误通知和服务信号转发顺序 |
| `src/services/ComponentCacheLoadCoordinator.cpp` | 145 | -- | -- | 独立协调后台缓存读取、缓存代次校验、网络回退、结果合并和缓存结果信号转发 |
| `src/services/ComponentCadFetchCoordinator.cpp` | 93 | -- | -- | 独立协调 CAD 响应的后台解析、缓存代次校验、结果提交和并行批处理通知 |

### 当前仍偏长的文件与已提取职责

| 文件 | 行数 | 说明 |
|------|------|------|
| `src/ui/viewmodels/ExportProgressViewModel.cpp` | 685 | 导出进度管理；结果列表统计、失败重试和阶段进度计算已提取 |
| `src/ui/viewmodels/ExportProgressStageCoordinator.cpp` | 135 | 独立承载预加载进度、阶段权重和三段式进度依赖计算 |
| `src/ui/viewmodels/ExportProgressStageCoordinator.h` | 33 | 阶段进度协调接口 |
| `src/ui/viewmodels/ExportProgressResultsCoordinator.cpp` | 179 | 独立协调导出结果列表的状态计算、过滤、类型成功统计和重试重置 |
| `src/ui/viewmodels/ExportProgressRetryCoordinator.cpp` | 84 | 独立协调单个组件和批量失败结果的重试状态重置、请求启动和刷新节流 |
| `src/services/export/ParallelExportService.cpp` | 561 | 并行导出生命周期协调；导出计划、进度状态聚合、导出阶段启动和预加载批次处理已提取 |
| `src/services/export/ParallelExportPreloadCoordinator.cpp` | 147 | 独立协调磁盘缓存回退、网络批量结果合并、严格数据校验和预加载进度收敛 |
| `src/services/export/ParallelExportPreloadCoordinator.h` | 29 | 预加载批次和网络结果合并协调接口 |
| `src/services/export/ExportStageLaunchCoordinator.cpp` | 164 | 独立协调导出阶段进度初始化、缺失数据标记、目标格式分支和阶段启动 |
| `src/services/export/ExportRunPlan.cpp` | 57 | 独立计算导出类型、可导出元件和缺失缓存数据，保持服务启动阶段无副作用 |
| `src/services/export/ExportProgressAggregator.cpp` | 65 | 独立合并阶段状态、重算阶段计数和汇总最终元件结果 |
| `src/ui/viewmodels/ExportSettingsViewModel.cpp` | 617 | 导出设置状态管理；选项组装和输出路径解析已提取 |
| `src/ui/viewmodels/ExportOptionsBuilder.cpp` | 81 | 独立承载导出选项组装、默认目录解析、目标格式映射和诊断日志 |
| `src/ui/viewmodels/ExportOptionsBuilder.h` | 22 | 导出选项构建接口 |
| `src/main.cpp` | 859 | 入口文件混入了 CLI/GUI 切换逻辑 |
| `src/workers/WriteWorker.cpp` | 612 | 文件写入工作线程；调试数据导出已提取 |
| `src/workers/WriteWorkerDebugExporter.cpp` | 267 | 独立承载原始响应、模型数据和结构化调试摘要的文件写入 |
| `src/core/altium/writers/AltiumPcbLibWriter.cpp` | 610 | PcbLib 文件级写入与元件记录编排；图元记录写入和输入校验已提取 |
| `src/core/altium/writers/AltiumPcbPrimitiveWriter.cpp` | 384 | 独立承载 PcbLib 焊盘、走线、弧线、文本、填充、区域和组件实体图元记录写入 |
| `src/core/altium/writers/AltiumPcbInputValidator.cpp` | 243 | 独立承载 PcbLib 封装名称、图元属性、三维模型和扩展记录的写入前校验 |
| `src/core/altium/ExporterAltiumFootprint.cpp` | 425 | Altium 封装 IR 转换和导出编排；几何包围盒与原点平移已提取 |
| `src/core/altium/AltiumFootprintGeometryNormalizer.cpp` | 110 | 独立承载 PcbLib 图元包围盒计算、区域坐标边界处理和统一平移 |
| `src/core/altium/compound/OLECompoundWriter.cpp` | 736 | OLE 存储树、流数据和扇区分配；目录条目与文件头序列化已提取 |
| `src/core/altium/compound/OLECompoundSerializer.cpp` | 153 | 独立承载 OLE 目录条目和 512 字节文件头的 CFB V3 字节序列化 |
| `src/core/altium/ExporterAltiumSymbol.cpp` | 501 | SchLib 符号转换；引脚、参数、引脚文本、路径与曲线图元、基础图元、标注图元、实现关系、几何归一化、部件 ID/方向/颜色/线型和坐标量化共享规则已提取 |
| `src/core/altium/AltiumSchSymbolGeometryNormalizer.cpp` | 348 | 独立承载符号包围盒归一化、引脚连接端网格量化、主体边界投影和重合文本布局 |
| `src/core/altium/AltiumSymbolPinConverter.cpp` | 190 | 独立承载 IR 引脚到 Altium 引脚记录的电气类型、显示标志、IEEE 装饰和电源引脚识别 |
| `src/core/altium/AltiumSymbolCurveConverter.cpp` | 155 | 独立承载圆弧、多边形、折线、路径、Bezier、椭圆、扇形和椭圆弧图元转换 |
| `src/core/altium/AltiumSymbolAnnotationConverter.cpp` | 91 | 独立承载符号文本、文本框和图片的字体、样式、嵌入数据及部件归属转换 |
| `src/core/altium/AltiumSymbolPrimitiveConverter.cpp` | 84 | 独立承载矩形、圆角矩形、圆和 IEEE 图元的几何、样式、颜色及部件归属转换 |
| `src/core/altium/AltiumSymbolImplementationConverter.cpp` | 65 | 独立承载候选封装、显式模型和来源元数据模型的实现关系转换与封装去重 |
| `src/core/altium/AltiumSymbolParameterConverter.cpp` | 24 | 独立承载已校验符号参数的字段、坐标、颜色、方向和部件归属转换 |
| `src/core/altium/AltiumSymbolPinTextConverter.cpp` | 105 | 独立承载引脚名称/编号文本校验后的记录转换、锚点回退诊断和部件归属映射 |
| `src/core/altium/AltiumSymbolPathConverter.cpp` | 199 | 独立承载路径分段校验、线段与曲线映射、填充路径转换、来源索引和诊断顺序维护 |
| `src/core/altium/utils/AltiumSymbolConversionUtils.h` | 71 | 独立承载符号部件 ID、方向、颜色、线型和坐标量化等共享转换规则 |
| `src/core/altium/writers/AltiumSchLibWriter.cpp` | 437 | SchLib 主记录写入；文本记录、图片记录、引脚记录、几何图元、文件级头部、字体表、图片 Storage、组件 Data 流、输入校验、几何校验、部件归属校验和来源顺序校验已提取 |
| `src/core/altium/writers/AltiumSchInputValidator.cpp` | 177 | 独立协调组件名称、参数字段、字符串编码、重复名称、实现映射、几何和部件归属的写入前校验 |
| `src/core/altium/writers/AltiumSchGraphicOrderValidator.cpp` | 203 | 独立校验来源图元顺序、引脚映射、路径分段、重复引用和未解析来源索引 |
| `src/core/altium/writers/AltiumSchPinRecordWriter.cpp` | 61 | 独立承载 SchLib `RECORD=2` 二进制引脚记录编码，并复用主写入器的 Owner 校验与内容序号状态 |
| `src/core/altium/writers/AltiumSchPrimitiveRecordWriter.cpp` | 273 | 独立承载矩形、弧线、多边形、折线、Bezier 和 IEEE 等文本参数图元记录编码 |
| `src/core/altium/writers/AltiumSchComponentRecordWriter.cpp` | 265 | 独立承载 Designator、参数字段、实现关系、引脚映射和实现参数记录编码 |
| `src/core/altium/writers/AltiumSchTextRecordWriter.cpp` | 142 | 独立承载 SchLib 文本与文本框记录编码 |
| `src/core/altium/writers/AltiumSchImageRecordWriter.cpp` | 54 | 独立承载 SchLib 图片记录编码 |
| `src/core/altium/writers/AltiumSchLibraryHeaderWriter.cpp` | 105 | 独立承载 SchLib FileHeader 和 SectionKeys 文件级流编码 |
| `src/core/altium/writers/AltiumSchImageStorageWriter.cpp` | 101 | 独立承载嵌入图片名称校验、重复命名消解和 Storage 流编码 |
| `src/core/altium/writers/AltiumSchComponentStorageWriter.cpp` | 145 | 独立承载单个组件 Data 流的记录顺序协调和 OLE 存储写入 |
| `src/core/altium/writers/AltiumSchOwnershipValidator.cpp` | 48 | 独立承载 SchLib 图元和参数记录的 OWNERPARTID 范围校验与诊断 |
| `src/core/altium/writers/AltiumSchGeometryValidator.cpp` | 192 | 独立承载 SchLib 图元的坐标、尺寸、方向、字符串和控制点约束校验 |
| `src/models/SymbolDataSerializer.cpp` | 742 | 图形类型兼容转发已提取到独立实现，剩余主体序列化逻辑待 IR 重构收敛 |
| `src/models/SymbolDataShapeCompatibility.cpp` | 106 | 独立承载旧 SymbolDataSerializer 图形类型接口到 SymbolShapeSerializer 的兼容转发 |
| `src/services/export/TempFileManager.cpp` | 632 | 临时文件管理；备份事务路径操作已提取，保留共享目录引用保护 |
| `src/services/export/TempFileTransactionUtils.cpp` | 204 | 独立承载备份事务路径移动、清单序列化和恢复数据解析 |
| `src/services/export/TempFileTransactionUtils.h` | 48 | 备份事务工具的数据结构和路径操作接口 |
| `src/core/kicad/SymbolGraphicsGenerator.cpp` | 443 | KiCad 符号图形生成 |
| `src/core/kicad/ExporterSymbol.cpp` | 768 | KiCad 符号导出 |
| `src/core/kicad/Exporter3DModel.cpp` | 702 | 3D 模型导出 |
| `src/models/FootprintDataSerializer.cpp` | 379 | 封装基本信息和顶层数组聚合；图形、层定义和对象可见性序列化已提取 |
| `src/models/FootprintShapeSerializer.cpp` | 284 | 独立承载封装边界框、焊盘、走线、孔、图形、文本、区域、轮廓、层定义和对象可见性的 JSON 映射 |
| `src/models/FootprintDataShapeCompatibility.cpp` | 126 | 独立承载旧 `FootprintDataSerializer` 图形类型接口到 `FootprintShapeSerializer` 的兼容转发 |
| `src/models/FootprintShapeSerializer.h` | 53 | 封装图形序列化接口 |
| `src/core/kicad/FootprintGraphicsGenerator.cpp` | 543 | KiCad 封装图形生成 |
| `src/ui/viewmodels/ExportSettingsViewModel.cpp` | 617 | 导出设置状态管理；选项组装和输出路径解析已提取 |
| `src/core/altium/compound/OLECompoundWriter.cpp` | 736 | OLE 存储树、流数据、扇区分配和文件落盘；固定结构序列化已提取 |
| `src/core/altium/compound/OLECompoundSerializer.cpp` | 153 | 独立承载 OLE 目录条目和文件头的固定字节布局编码 |
| `src/core/network/AsyncNetworkRequest.cpp` | 669 | 异步网络请求 |
| `src/services/export/FootprintExportStage.cpp` | 437 | 封装导出阶段；3D 模型缓存准备和坐标偏移已提取 |
| `src/services/export/FootprintModel3DPreparation.cpp` | 291 | 独立承载 STEP、OBJ、WRL 缓存读取、校验、回写和坐标偏移准备 |
| `src/services/export/FootprintModel3DPreparation.h` | 31 | 封装 3D 模型准备接口 |
| 其余高风险文件 | -- | 请以 `analyze_project.py --all --json` 的当前输出为准 |

### 处理建议

**ADR 012 IR 重构自动解决的**（6 个文件）：
- `SymbolDataSerializer.cpp`、`FootprintDataSerializer.cpp` -- 序列化逻辑合并到 IR 层
- `SymbolData.h` 相关的模型文件 -- 拆分为 IR + Importer

**需要独立拆分的**：
- `ComponentCacheService.cpp`（565 行）：已提取缓存文件布局、目录迁移、目录切换、元数据存储、组件数据读取、完整缓存校验、符号封装 CAD 缓存验证、二级文件读取、CAD 数据写入、预览图下载/写入、数据手册文件存储、数据手册下载、缓存维护、磁盘配额清理、写入代次策略、数据校验、tombstone、L1 内存缓存语义、元数据写入协调和三维模型缓存协调职责，后续继续按锁边界拆分管理流程
- `ComponentCacheFileReadCoordinator.cpp`（70 行）：独立承载符号、封装、CAD JSON、预览图和元数据的二级文件读取，保留一级缓存优先级、文件校验、目录迁移锁和 ComponentCacheService 的公开接口
- `ComponentCacheReadCoordinator.cpp`（105 行）：独立承载完整缓存校验、磁盘元器件数据读取和符号封装 CAD 缓存验证，保持目录迁移锁覆盖路径快照与文件读取，并复用缓存服务的 L1 锁边界
- `CacheDirectoryCoordinator.cpp`（71 行）：独立承载缓存目录切换、旧目录迁移、缓存代次失效、目录初始化和自修复，ComponentCacheService 继续负责公开接口、信号转发和协调器生命周期
- `PreviewImageDownloadService.cpp`（119 行）：独立承载预览图缓存命中、网络重试、取消检查、诊断回写和代次保护写入，ComponentCacheService 继续保留公开接口
- `CachePathResolver.cpp`（66 行）：独立承载缓存路径构造、旧目录大小写兼容和模型路径安全校验，ComponentCacheService 继续负责锁和目录生命周期
- `ComponentCacheCadDataWriter.cpp`（86 行）：统一承载符号、封装和 CAD JSON 的磁盘写入流程，保持目录迁移锁、代次校验、配额维护和内存缓存同步顺序
- `Model3DCacheFileStore.cpp`（64 行）：独立承载三维模型文件的校验读取、原子写入和导出复制，ComponentCacheService 继续负责锁、路径、代次和配额策略
- `ComponentCacheModel3DCoordinator.cpp`（91 行）：独立协调三维模型缓存的锁边界、代次校验、文件读写、导出复制和磁盘配额触发，ComponentCacheService 继续保留公开接口和路径策略
- `ComponentListViewModel.cpp`（445 行）：已提取列表状态统计、编号索引、批量导入、列表生命周期、剪贴板处理、失败项重试、组件服务数据回调、预览图协调、预览更新缓冲、列表项更新缓冲、验证队列状态、验证错误分类、定时器初始化、服务信号连接和验证队列协调职责，并移除失效的预览图编码线程池，后续继续拆分搜索和选择
- `ComponentListMutationCoordinator.cpp`（185 行）：独立承载列表添加、删除、清空及其模型行、编号索引、请求取消、验证队列和状态计数同步，ComponentListViewModel 继续保留公开槽接口和信号
- `ComponentListClipboardCoordinator.cpp`（64 行）：独立承载剪贴板文本读取、元件编号提取、列表去重、批量添加和全量编号复制，ComponentListViewModel 继续保留公开槽接口和信号
- `ComponentListRetryCoordinator.cpp`（46 行）：独立承载可重试失败项筛选、列表项验证状态重置、服务请求和验证计数更新，ComponentListViewModel 继续保留公开槽接口和信号
- `ComponentListPreviewCoordinator.cpp`（117 行）：独立承载预览图缓存批量更新、有效元件筛选、批量请求、完成状态和验证/预览提示清理，ComponentListViewModel 继续保留公开槽接口、定时器入口和服务信号边界
- `LcscImageService.cpp`（560 行）：已提取 LCSC 产品搜索响应解析和媒体字段提取，服务继续负责网络请求、缓存读取、下载状态、取消和代次保护
- `LcscProductParser.cpp`（115 行）：独立承载产品搜索 JSON 解析、元件编号精确匹配、预览图 URL 规范化以及制造商和数据手册字段提取
- `ExporterAltiumSymbol.cpp`（501 行）：已提取符号引脚、参数、引脚名称/编号文本、路径与曲线图元、矩形/圆/IEEE 基础图元、文本/文本框/图片图元、实现关系、几何归一化、部件 ID/方向/颜色/线型和坐标量化共享规则，主文件继续负责符号记录编排、参数诊断和模型关联编排
- `AltiumSchSymbolGeometryNormalizer.cpp`（348 行）：独立承载符号原点归一化、引脚连接端吸附、主体边界投影和重合文本布局，保持 Altium 符号输出坐标规则不变
- `AltiumSymbolPinConverter.cpp`（190 行）：独立承载 IR 引脚到 Altium 引脚记录的电气类型、显示标志、IEEE 装饰和电源引脚识别，保持主导出器的公开接口不变
- `AltiumSymbolCurveConverter.cpp`（155 行）：独立承载圆弧、多边形、折线、路径、Bezier、椭圆、扇形和椭圆弧转换，保持坐标单位、线型、颜色和部件归属映射不变
- `AltiumSymbolAnnotationConverter.cpp`（91 行）：独立承载符号文本、文本框和图片转换，保持字体、颜色、方向、边框、图片嵌入和部件归属映射不变
- `AltiumSymbolPrimitiveConverter.cpp`（84 行）：独立承载矩形、圆角矩形、圆和 IEEE 图元转换，保持边界、半径归一化、颜色、线型和部件归属映射不变
- `AltiumSymbolImplementationConverter.cpp`（65 行）：独立承载候选封装去重、显式模型和来源元数据模型的实现记录转换，保持模型类型、文件字段、参数及引脚映射不变
- `AltiumSymbolParameterConverter.cpp`（24 行）：独立承载已通过校验的符号参数字段映射，保持坐标、字体大小、可见性、只读标志、方向、颜色和部件归属不变
- `AltiumSymbolPinTextConverter.cpp`（105 行）：独立承载引脚名称和编号文本的有效性判断、锚点规范化、诊断回退及部件归属映射，保持公共部件和显示标志行为不变
- `AltiumSymbolPathConverter.cpp`（199 行）：独立承载路径分段参数校验、线段、二次与三次 Bézier、圆弧、椭圆弧和填充路径转换，保持来源索引、诊断顺序和目标记录语义不变
- `AltiumSymbolConversionUtils.h`（71 行）：集中维护 Altium 符号转换中跨参数、引脚和图元复用的部件 ID、方向、颜色、线型及坐标量化规则
- `ComponentListTimerCoordinator.cpp`（73 行）：独立创建并连接预览图、批处理、列表统计和延迟获取定时器，保持防抖窗口及原有回调顺序
- `ComponentListServiceConnectionCoordinator.cpp`（91 行）：独立连接验证完成、组件数据、预览图成功/失败和批量预览完成信号，保持图片编码、缓存更新和验证状态推进顺序
- `ComponentValidationCoordinator.cpp`（81 行）：独立协调验证队列启动、并发调度、完成回调和 BOM 导入状态推进，保持 ViewModel 的状态更新与服务请求顺序
- `ComponentListDataCoordinator.cpp`（166 行）：独立协调基础信息、CAD、LCSC、数据手册和错误回调，保持列表项状态更新、验证完成通知和批量刷新顺序
- `ComponentListBatchCoordinator.cpp`（189 行）：独立协调批量编号去重、分批插入、BOM 异步解析和验证启动，保持模型通知与状态计数顺序
- `ComponentListItemUpdateBuffer.cpp`（约 31 行）：独立收集异步基础信息更新涉及的列表项，负责去重、线程安全转移和清理
- `ComponentCachePreviewImageWriter.cpp`（52 行）：独立承载预览图二级缓存写入，保持校验、目录迁移锁、代次策略、原子替换和配额维护顺序
- `DatasheetCacheFileStore.cpp`（88 行）：独立承载数据手册缓存读取、格式校验、格式切换、旧文件清理和原子写入
- `DatasheetDownloadService.cpp`（约 150 行）：独立承载数据手册缓存命中、同步网络下载、取消处理、网络诊断和代次保护写入
- `ComponentCacheMaintenance.cpp`（130 行）：独立承载缓存删除、全量清理、L1 清理、有效组件枚举和磁盘大小统计，保持代次、tombstone、锁和信号顺序

**CLI 重复逻辑收敛**：
- `CliExportWaiter.cpp`：统一 BatchConverter 和 BomConverter 的预加载/导出事件循环等待逻辑，转换器自身继续负责输入读取、结果统计和错误处理
- `ComponentCacheQuotaEnforcer.cpp`（40 行）：独立承载磁盘缓存配额的冷却判断、目标容量计算、清理执行和大小变化通知
- `ComponentCacheMemoryStore.cpp`（92 行）：独立承载一级缓存复合键、JSON 编解码、CAD 数据校验、LRU 访问和容量统计；ComponentCacheService 继续负责信号、代次和磁盘流程
- `ComponentValidationErrorPolicy.cpp`（65 行）：集中维护 CAD 数据失败、不可重试错误和预览图错误分类规则，保持 ViewModel 的验证状态与界面提示行为一致
- `ComponentService.cpp`（399 行）：已提取组件数据内存缓存、基础信息字段解析、BOM 文本编号提取、后台缓存加载、预览图文件编码、CAD 结果收敛、媒体回调、API 回调、队列初始化、并行批量协调、CAD 响应协调、请求取消和单请求启动职责，后续继续按异步请求状态边界拆分获取与错误处理
- `ComponentRequestCoordinator.cpp`（109 行）：独立承载单请求去重、可复用数据判断、符号封装缓存命中、缓存代次读取和 CAD 后台获取，ComponentService 继续保留公开请求接口与回调信号
- `ComponentRequestCancellationCoordinator.cpp`（58 行）：独立协调全量请求和单器件请求的 API、网络客户端、图片服务取消及状态清理顺序，保持 ComponentService 的公开取消接口不变
- `ComponentMediaCallbackCoordinator.cpp`（274 行）：独立协调预览图、LCSC 数据和数据手册的异步回调，保持缓存代次校验、异步写入、并行队列完成通知和服务信号转发顺序
- `ComponentApiCallbackCoordinator.cpp`（85 行）：独立协调 EasyEDA API 基础信息和错误回调，保持缓存代次校验、失败状态清理、并行错误通知和服务信号转发顺序
- `ComponentCacheLoadCoordinator.cpp`（145 行）：独立协调后台缓存读取、缓存代次校验、网络回退、结果合并和缓存结果信号转发
- `ComponentParallelFetchCoordinator.cpp`（约 100 行）：独立协调并行上下文完成/失败回调、队列槽位释放、超时通知和批量状态重置
- `ComponentValidationCoordinator.cpp`（81 行）：独立协调列表验证队列的并发调度、完成状态、BOM 导入更新和后续队列推进
- `ComponentCadFetchCoordinator.cpp`（93 行）：独立协调 CAD 响应解析、请求代次校验、缓存提交和并行批处理通知
- `PreviewImageDataEncoder.cpp`（47 行）：独立承载预览图文件序号解析、文件读取和 Base64 编码，避免 ComponentService 同时承担文件处理细节
- `main.cpp`（859 行）：CLI 入口逻辑已迁移到 `CliConverter`，剩余 GUI 初始化可提取为 `ApplicationSetup` 类

**可接受但需关注的**（导出器和写入器）：
- `AltiumSchLibWriter.cpp`（437 行）：二进制格式写入天然较长，文本记录、图片记录、引脚记录、几何图元、文件级头部、字体表、图片 Storage、组件 Data 流、输入校验、几何校验、部件归属校验和来源顺序校验已提取，后续可按组件级文件流程继续拆分
- `AltiumSchInputValidator.cpp`（177 行）：独立承载 SchLib 组件文本、参数、名称、编码、实现映射、几何和部件归属的写入前校验，保持主写入器的诊断信息和拒绝时序
- `AltiumSchGraphicOrderValidator.cpp`（203 行）：独立承载来源图元顺序的完整性、重复引用、引脚覆盖、路径分段和未解析索引校验，保持组件存储写入器的默认顺序回退行为
- `AltiumSchPinRecordWriter.cpp`（61 行）：独立编码二进制引脚记录，保持引脚方向、可见性、名称/编号和连接属性的原有写入顺序
- `AltiumSchPrimitiveRecordWriter.cpp`（273 行）：独立编码几何图元文本参数，复用主写入器的坐标、Owner、唯一标识和诊断策略
- `AltiumSchComponentRecordWriter.cpp`（265 行）：独立编码参数和实现关系记录，复用主写入器的字体、编号、库名和诊断状态
- `AltiumSchLibraryHeaderWriter.cpp`（105 行）：独立承载 SchLib FileHeader 和 SectionKeys 流，保持字体表、组件计数、记录权重和存储键映射的一致性
- `AltiumSchImageStorageWriter.cpp`（101 行）：独立承载嵌入图片名称校验、大小写不敏感的重复命名消解、255 字节限制和 OLE Storage 流编码
- `AltiumSchComponentStorageWriter.cpp`（145 行）：独立承载单个组件 Data 流的默认/来源顺序选择、补充图元以及参数和实现记录的协调
- `AltiumSchOwnershipValidator.cpp`（48 行）：独立校验所有 SchLib 记录的 OWNERPARTID 范围并复用主写入器诊断通道，保持拒绝写入行为不变
- `AltiumSchGeometryValidator.cpp`（192 行）：独立校验图元几何边界、编码字符串和枚举范围，并复用主写入器诊断通道，保持拒绝写入行为不变
- `AltiumPcbLibWriter.cpp`（610 行）：已提取焊盘、走线、弧线、文本、填充、区域和组件实体图元记录以及输入校验，主写入器继续负责文件级状态、封装数据、唯一标识表和元件记录编排
- `AltiumPcbPrimitiveWriter.cpp`（384 行）：独立承载 PcbLib 图元记录编码，复用主写入器的层映射、标志编码、有限值归一化、广字符串和公共图元头部规则
- `AltiumPcbInputValidator.cpp`（243 行）：独立承载封装名称、焊盘/走线/文本/区域属性、三维模型、三维实体和扩展记录的输入校验，复用主写入器诊断通道并保持拒绝写入行为不变
- `ExporterAltiumFootprint.cpp`（425 行）：已提取封装图元包围盒、区域坐标边界处理和原点平移，主导出器继续负责 IR 转换、3D 模型关联和 ComponentBody 生成
- `AltiumFootprintGeometryNormalizer.cpp`（110 行）：独立承载 PcbLib 图元包围盒计算和统一平移，分别保留原点归一化与三维轮廓生成所需的区域坐标策略
- `OLECompoundWriter.cpp`（736 行）：继续负责 OLE 存储树、流登记、mini stream、FAT/DIFAT 扇区分配和文件落盘，固定结构编码已移出
- `OLECompoundSerializer.cpp`（153 行）：独立承载目录条目、版本字段、FAT/DIFAT 索引和扇区布局的 CFB V3 序列化，保持 OLE 写入器的只读状态边界
- `WriteWorker.cpp`（612 行）：继续负责写入任务调度、符号/封装/三维模型/预览图/数据手册写入和状态汇总，调试数据输出已提取
- `WriteWorkerDebugExporter.cpp`（267 行）：独立承载调试原始文件、结构化 JSON 摘要和调试目录创建，复用 WriteWorker 的路径安全边界
- KiCad 导出器系列（各 660-690 行）：与 IR 重构后的导出器接口调整一并处理

---

## 三、QML 文件问题清单（问题最集中）

### 已完成拆分的列表组件

| 文件 | 总行数 | 代码行 | 问题分析 |
|------|--------|--------|---------|
| `src/ui/qml/components/ComponentListCard.qml` | 371 | -- | 卡片状态协调、过滤触发和弹窗定位；预览弹窗、工具栏和列表视图已提取 |
| `src/ui/qml/components/ComponentToolbar.qml` | 427 | -- | 独立承载数量、筛选、搜索和批量操作 |
| `src/ui/qml/components/ComponentListView.qml` | 187 | -- | 独立承载 DelegateModel、网格委托和可见区域预取 |
| `src/ui/qml/components/ComponentPreviewPopup.qml` | 320 | -- | 独立承载预览图展示、缩略图切换和延迟隐藏 |
| `src/ui/qml/MainWindow.qml` | 911 | -- | 主窗口布局+状态管理+对话框逻辑 |
| `src/ui/qml/components/SidebarSettingsView.qml` | 724 | -- | 侧边栏设置面板 |
| `src/ui/qml/components/ComponentListItem.qml` | 615 | -- | 已提取复制、外链和悬停交互，预览图与验证状态仍集中 |
| `src/ui/qml/components/SliderDialogBase.qml` | 608 | 530 | 滑动对话框基类 |
| `src/ui/qml/components/ExportSettingsBaseCard.qml` | 533 | 483 | 导出设置卡片基类 |

### 偏长（300-500 行）

| 文件 | 行数 | 说明 |
|------|------|------|
| `HeaderSection.qml` | 402 | |
| `ExitDialog.qml` | 400 | |
| `ResultListItem.qml` | 370 | |
| `tst_ExportFlow.qml` | 341 | 测试文件 |
| `ExportResultsCard.qml` | 306 | |

### 处理建议

QML 文件过长是当前最严重的问题区域，建议按以下优先级处理：

1. **`ComponentListCard.qml`（371 行）**：已提取预览弹窗、工具栏和列表视图，后续重点转向 ViewModel 与服务层拆分
2. **`MainWindow.qml`（911 行）**：提取 `MenuBar`、`StatusBar`、`DialogManager` 等独立 QML 组件
3. **`ComponentListItem.qml`（615 行）**：已提取共享交互层，后续仅在预览和验证职责出现明确边界时继续拆分
4. **`SidebarSettingsView.qml`（724 行）**：各设置区块提取为独立组件

---

## 四、工具脚本和 CMake

### Python 工具脚本

| 文件 | 行数 | 优先级 |
|------|------|--------|
| `tools/python/manage_version.py` | 1,190 | 低（开发工具，不影响产品） |
| `tools/python/build_project.py` | 1,092 | 低 |
| `tools/python/format_code.py` | 926 | 低 |

工具脚本过长但不影响产品质量，可在空闲时逐步拆分为子模块。

### CMake 文件

| 文件 | 行数 | 说明 |
|------|------|------|
| `tests/unit/CMakeLists.txt` | 536 | 测试目标过多，可按模块拆分为子 CMakeLists |
| `CMakeLists.txt` (根) | 424 | 项目配置复杂度所致，可接受 |

---

## 五、与 ADR 012 的关联

以下文件在 IR 重构中会被自然处理，无需单独拆分：

| 文件 | 行数 | IR 重构后的去向 |
|------|------|----------------|
| `src/models/SymbolData.h` | 491 | 拆分到 `ir/SymbolIR.h` + `importers/easyeda/EasyedaMetadata.h` |
| `src/models/FootprintData.h` | 403 | 拆分到 `ir/FootprintIR.h` + `importers/easyeda/EasyedaMetadata.h` |
| `src/models/SymbolDataSerializer.cpp` | 742 | 逻辑合并到 IR 层或删除 |
| `src/models/FootprintDataSerializer.cpp` | 677 | 同上 |
| `src/models/ComponentData.h` | 234 | 合并到 `ir/ComponentIR.h` |
| `src/models/Model3DData.h` | 124 | 合并到 `ir/Model3DIR.h` |

---

## 六、推荐实施顺序

| 阶段 | 内容 | 文件数 | 预估工时 |
|------|------|--------|---------|
| 1 | ADR 012 IR 重构（解决 6 个模型文件） | ~20 | 3 周 |
| 2 | QML 组件拆分（解决 7 个过长 QML） | ~15 | 1-2 周 |
| 3 | 服务层拆分（CacheService/ComponentService/ViewModel） | ~6 | 1 周 |
| 4 | main.cpp 瘦身 | 1 | 0.5 周 |
| 5 | 工具脚本模块化（可选） | 3 | 空闲时 |

---

## 附录：目录行数分布

| 目录 | 总行数 | 文件数 |
|------|--------|--------|
| `src` | 78,488 | 509 |
| `tests` | 18,782 | 58 |
| `resources/translations` | 3,303 | 2 |
| `tools/python` | 6,642 | 13 |

> 目录级统计以上述项目工具的当前输出为准；旧版按子目录手工估算的数据已移除，避免与总量不一致。产品目录数据由 `python3 tools/python/analyze_project.py --all --analyze --top 100` 生成，工具目录数据由 `python3 tools/python/analyze_project.py tools/python --analyze --top 30` 生成。
