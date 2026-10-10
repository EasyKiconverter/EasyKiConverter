# ADR-004: 符号库更新导出修复

## 状态
已接受 (Accepted)

## 日期
2026-01-21

## 上下文

在 v3.0.0 版本中，我们实现了符号库的追加和更新导出功能。然而，在实际使用中发现了一个严重的 bug：当使用更新导出功能时，分体式符号（multi-part symbols）的子符号会被错误地作为独立的顶层符号存在，导致符号库格式破坏。

### 发现的问题

#### 问题 1：更新模式参数未传递 [严重]

**问题描述**：
- `ExportService_Pipeline::mergeSymbolLibrary()` 方法中，`updateMode` 参数没有被正确传递给 `exportSymbolLibrary` 方法
- 原代码只使用了 `overwriteExistingFiles` 来设置 `appendMode`，忽略了 `updateMode`
- 导致更新导出时，所有已存在的符号都被跳过，而不是被覆盖

**影响范围**：
- 所有使用更新模式的符号库导出
- 用户无法更新已存在的符号

**代码位置**：
- `src/services/ExportService_Pipeline.cpp:395-397`

**示例**：
```cpp
// 原代码（错误）
bool appendMode = !m_options.overwriteExistingFiles;
bool success = exporter.exportSymbolLibrary(m_symbols, m_options.libName, libraryPath, appendMode);

// 修复后（正确）
bool appendMode = !m_options.overwriteExistingFiles;
bool updateMode = m_options.updateMode;
bool success = exporter.exportSymbolLibrary(m_symbols, m_options.libName, libraryPath, appendMode, updateMode);
```

---

#### 问题 2：子符号识别不准确 [严重]

**问题描述**：
- 原代码使用简单的字符串匹配 `startsWith(parentSymbolName + "_")` 来识别子符号
- 这种方法无法准确区分分体式符号和单体符号的子符号
- 可能误删其他符号的子符号

**影响范围**：
- 混合符号库（同时包含分体式符号和单体符号）
- 符号名称包含下划线的符号

**代码位置**：
- `src/core/kicad/ExporterSymbol.cpp:195-206`

**示例场景**：
```
假设符号库中包含：
- 分体式符号 A：MCU_A，有子符号 MCU_A_1_1, MCU_A_2_1
- 单体符号 B：Resistor_B，有子符号 Resistor_B_0_1

当更新 MCU_A 时，原代码会：
1. 识别 MCU_A 为要覆盖的符号
2. 查找以 MCU_A_ 开头的子符号并标记删除
3. 但如果 Resistor_B_0_1 被错误识别，可能导致格式破坏
```

---

#### 问题 3：孤离子符号未删除 [严重]

**问题描述**：
- 在某些情况下，分体式符号的子符号可能被错误地作为顶层符号导出
- 原代码只删除了父符号内部的子符号，没有删除作为顶层符号存在的孤离子符号
- 导致更新后，旧的孤离子符号仍然存在

**影响范围**：
- 之前使用错误方式导出的符号库
- 符号库结构不正确的情况

**代码位置**：
- `src/core/kicad/ExporterSymbol.cpp:310-360`

**示例场景**：
```
假设符号库中包含：
[正确] 正确的结构：
- 父符号：MCIMX6Y2CVM08AB
  - 子符号：MCIMX6Y2CVM08AB_1_1
  - 子符号：MCIMX6Y2CVM08AB_2_1

[错误] 实际存在的错误结构：
- 父符号：MCIMX6Y2CVM08AB
  - 子符号：MCIMX6Y2CVM08AB_1_1
- 顶层符号：MCIMX6Y2CVM08AB_2_1（错误！）

当更新 MCIMX6Y2CVM08AB 时：
1. MCIMX6Y2CVM08AB 被正确覆盖
2. MCIMX6Y2CVM08AB_1_1 作为子符号被正确删除和重新生成
3. MCIMX6Y2CVM08AB_2_1 作为顶层符号被保留（错误！）
4. 新的 MCIMX6Y2CVM08AB_2_1 作为子符号被生成
5. 结果：符号库中存在两个 MCIMX6Y2CVM08AB_2_1，格式破坏
```

## 决策

我们实施了以下修复方案来解决上述问题：

### 修复 1：传递 updateMode 参数

**问题**：`updateMode` 参数没有被正确传递给 `exportSymbolLibrary` 方法

**解决方案**：
```cpp
// 修复前
bool appendMode = !m_options.overwriteExistingFiles;
bool success = exporter.exportSymbolLibrary(m_symbols, m_options.libName, libraryPath, appendMode);

// 修复后
bool appendMode = !m_options.overwriteExistingFiles;
bool updateMode = m_options.updateMode;
qDebug() << "Merge settings - Append mode:" << appendMode << "Update mode:" << updateMode;
bool success = exporter.exportSymbolLibrary(m_symbols, m_options.libName, libraryPath, appendMode, updateMode);
```

**影响**：
- 更新模式现在能够正确覆盖已存在的符号
- 追加模式仍然保留已存在的符号

---

### 修复 2：改进子符号识别逻辑

**问题**：简单的字符串匹配无法准确区分分体式符号和单体符号的子符号

**解决方案**：
```cpp
// 首先分析现有符号，确定哪些是分体式符号（有多个子符号）
QMap<QString, QStringList> parentToSubSymbols; // 父符号名 -> 子符号列表
for (const QString &subSymbolName : subSymbolNames)
{
    // 从子符号名称中提取父符号名
    // 子符号格式：{parentName}_{unitNumber}_1
    int lastUnderscore = subSymbolName.lastIndexOf('_');
    if (lastUnderscore > 0)
    {
        int secondLastUnderscore = subSymbolName.lastIndexOf('_', lastUnderscore - 1);
        if (secondLastUnderscore > 0)
        {
            QString parentName = subSymbolName.left(secondLastUnderscore);
            parentToSubSymbols[parentName].append(subSymbolName);
        }
    }
}

// 对于每个被覆盖的父符号，收集其所有子符号
for (const QString &parentSymbolName : overwrittenSymbolNames)
{
    if (parentToSubSymbols.contains(parentSymbolName))
    {
        // 这是一个分体式符号，删除所有子符号
        for (const QString &subSymbolName : parentToSubSymbols[parentSymbolName])
        {
            subSymbolsToDelete.insert(subSymbolName);
            qDebug() << "Marking sub-symbol for deletion (multipart):" << subSymbolName << "(parent:" << parentSymbolName << ")";
        }
    }
    else
    {
        // 这是一个单体符号，查找其子符号（格式：{parentName}_0_1）
        QString expectedSubSymbolName = parentSymbolName + "_0_1";
        if (subSymbolNames.contains(expectedSubSymbolName))
        {
            subSymbolsToDelete.insert(expectedSubSymbolName);
            qDebug() << "Marking sub-symbol for deletion (single part):" << expectedSubSymbolName << "(parent:" << parentSymbolName << ")";
        }
    }
}
```

**影响**：
- 准确区分分体式符号和单体符号
- 避免误删其他符号的子符号
- 支持混合符号库的正确处理

---

### 修复 3：删除孤立的顶层子符号

**问题**：作为顶层符号存在的孤离子符号没有被删除

**解决方案**：
```cpp
// 检查是否是以被覆盖符号名开头的顶层符号（可能是之前错误导出的子符号）
bool isOrphanedSubSymbol = false;
if (!isOverwritten)
{
    for (const QString &parentSymbolName : overwrittenSymbolNames)
    {
        if (symbolName.startsWith(parentSymbolName + "_"))
        {
            isOrphanedSubSymbol = true;
            qDebug() << "Skipping orphaned sub-symbol (top-level):" << symbolName << "(parent:" << parentSymbolName << ")";
            break;
        }
    }
}

// 只导出未被覆盖且不是孤离子符号的符号
if (!isOverwritten && !isOrphanedSubSymbol)
{
    // 导出符号内容
    out << filteredContent;
    qDebug() << "Keeping existing symbol:" << symbolName;
}
```

**影响**：
- 删除所有以被覆盖父符号名开头的顶层符号
- 确保符号库结构的正确性
- 避免重复的子符号

## 后果

### 正面影响

1. **修复了更新导出功能**
   - 更新模式现在能够正确覆盖已存在的符号
   - 用户可以更新符号库中的符号

2. **准确的子符号处理**
   - 正确区分分体式符号和单体符号
   - 分体式符号：删除所有子符号（`_1_1`, `_2_1`, ...）
   - 单体符号：只删除 `_0_1` 子符号

3. **符号库结构正确性**
   - 删除孤立的顶层子符号
   - 确保子符号只在父符号内部存在
   - 避免符号库格式破坏

4. **支持混合符号库**
   - 正确处理同时包含分体式符号和单体符号的符号库
   - 避免误删其他符号的子符号

### 负面影响

1. **复杂度增加**
   - 子符号识别逻辑更加复杂
   - 需要分析符号库结构

2. **性能影响**
   - 需要额外的符号分析步骤
   - 可能略微增加导出时间

3. **向后兼容性**
   - 可能需要清理之前导出的错误符号库
   - 用户可能需要重新导出符号库

## 替代方案

### 方案 A：使用正则表达式匹配子符号

**描述**：使用正则表达式来匹配子符号名称

**优点**：
- 更灵活的模式匹配
- 可以处理更复杂的命名规则

**缺点**：
- 正则表达式性能较差
- 代码可读性降低

**选择**：未采用，当前方案更简单高效

### 方案 B：重新导出整个符号库

**描述**：在更新模式下，完全重新导出符号库

**优点**：
- 实现简单
- 确保符号库结构正确

**缺点**：
- 性能较差
- 无法保留用户修改

**选择**：未采用，当前方案更高效且保留用户修改

## 实施细节

### 修改文件

**修复 1**：
1. `src/services/ExportService_Pipeline.cpp` - 传递 `updateMode` 参数

**修复 2**：
1. `src/core/kicad/ExporterSymbol.cpp` - 改进子符号识别逻辑

**修复 3**：
1. `src/core/kicad/ExporterSymbol.cpp` - 删除孤立的顶层子符号

### 测试验证

**测试场景**：
1. 更新分体式符号
2. 更新单体符号
3. 混合符号库更新
4. 符号库包含孤离子符号的情况

**预期结果**：
- 分体式符号被正确覆盖，所有子符号被正确更新
- 单体符号被正确覆盖，子符号被正确更新
- 混合符号库中，其他符号不受影响
- 孤离子符号被正确删除

## 当前 Altium 库增量导出限制

当前追加和更新逻辑仅适用于支持文本合并的 KiCad 符号库。Altium SchLib/PcbLib 是 OLE 二进制库，项目虽然已经具备只读结构解析能力，但尚未实现已有库的增量写回，因此在追加或更新模式下检测到已有 Altium 库时仍会拒绝导出，并保留原文件，避免静默覆盖用户数据。

后续如果实现 Altium 库读取器和增量合并，应同时补充 OLE 流、FileHeader、SectionKeys、多部件符号及图片 Storage 的回归测试，并移除当前保护性拒绝逻辑。

当前已新增 `OLECompoundReader` 作为只读 CFB/OLE 基础层，能够校验 V3 文件、解析 FAT/DIFAT、枚举目录流，并读取普通流和迷你流。该读取器只提供原始流访问，不负责解释 SchLib/PcbLib 的业务记录，也不会改变“已有 Altium 库禁止追加或更新”的保护策略。后续增量合并必须在此基础上增加保留未知流和元数据的写回能力，并以真实库样本补充回归测试。

当前已进一步增加 `AltiumSchLibReader`，可解析 SchLib 的 FileHeader、SectionKeys、组件名称及组件 Data 流映射，并校验目录中声明的组件是否实际存在。读取器会从 FileHeader 暴露可见部件数量和按 FontID 排列的字体表；组件 Data 还可以按长度块拆分为记录，并保留 flags、payload 及完整编码。对 C 字符串参数记录会额外提供 `RECORD`、`OWNERPARTID`、`IndexInSheet` 和 `FONTID` 等结构化字段，对二进制引脚记录也会读取记录类型和所属部件，以便验证多部件归属、字体引用与图元顺序；这些数值字段无法解析、字体引用越界、所属部件越界或 `IndexInSheet` 重复倒序时会明确失败，不再静默回退。根 `/Storage` 现在还可以被独立校验，读取图片条目的标志、文件名和原始压缩数据，并拒绝重复名称、路径穿越名称、长度不一致或声明数量不一致的损坏条目。未知记录仍保持原始形式，以便后续合并时无损转发；截断记录或参数格式错误会通过 `errorString()` 报告偏移量和底层原因。它仍是只读的库级读取能力，尚未解释全部业务字段和提供写回逻辑。

同时已增加 `AltiumPcbLibReader`，可解析 PcbLib 的 FileHeader、Library/Data、SectionKeys 和封装 Storage 映射，并提供封装 Header、Parameters、WideStrings、Data 等流的读取入口。PcbLib 的 Data 流由对象 ID 和对象类型专属子记录组成，不能直接复用 SchLib 的长度块拆分策略。读取器现在可以安全扫描项目已知对象类型并保留对象及其子块的完整编码，同时校验 Pad/Text 的字符串子块格式和图元主块的 13 字节公共头部，并提供 Layer、Flags 字段；Pad 还会解析主块尺寸、形状、孔参数及扩展块圆角信息，Track、Arc、Text 会解析基础字段，Fill 会解析矩形、旋转和层信息，Region 与 ComponentBody 会解析参数块及轮廓顶点，文本字符串同时从独立字符串块恢复。遇到未知对象 ID、损坏子块或缺少公共头部时会失败，`errorString()` 会返回具体对象和子块诊断，并要求调用方回退到原始流。两种库的读取器都只做结构校验和原始流访问，尚未承担增量写回。

## 相关文档

- [ADR-001: MVVM 架构](001-mvvm-architecture.md)
- [ADR-002: 流水线并行架构](002-pipeline-parallelism-for-export.md)
- [ADR-003: 流水线性能优化](003-pipeline-performance-optimization.md)
- [符号导出器文档](../../developer/ARCHITECTURE.md)

## 参考资料

- [KiCad 符号库格式规范](https://docs.kicad.org/7.0/en/eeschema/eeschema_libs/)
- [符号类型说明](https://docs.kicad.org/7.0/en/eeschema/eeschema_symbols/)
