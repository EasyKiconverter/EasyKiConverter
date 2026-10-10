# ADR-004: Symbol Library Update/Overwrite Export Fix

## Status
Accepted

## Date
2026-01-21

## Context

In v3.0.0, we implemented symbol library append and update export functionality. However, a serious bug was discovered during actual use: when using update export, sub-symbols of multi-part symbols were incorrectly exported as independent top-level symbols, corrupting the library format.

### Issues Found

#### Issue 1: updateMode Parameter Not Passed [Critical]

**Description**:
- In `ExportService_Pipeline::mergeSymbolLibrary()`, the `updateMode` parameter was not correctly passed to the `exportSymbolLibrary` method
- The original code only used `overwriteExistingFiles` to set `appendMode`, ignoring `updateMode`
- As a result, during update export, all existing symbols were skipped instead of being overwritten

**Scope**:
- All symbol library exports using update mode
- Users could not update existing symbols

**Code Location**:
- `src/services/ExportService_Pipeline.cpp:395-397`

**Example**:
```cpp
// Original code (incorrect)
bool appendMode = !m_options.overwriteExistingFiles;
bool success = exporter.exportSymbolLibrary(m_symbols, m_options.libName, libraryPath, appendMode);

// Fixed (correct)
bool appendMode = !m_options.overwriteExistingFiles;
bool updateMode = m_options.updateMode;
bool success = exporter.exportSymbolLibrary(m_symbols, m_options.libName, libraryPath, appendMode, updateMode);
```

---

#### Issue 2: Sub-Symbol Identification Inaccurate [Critical]

**Description**:
- The original code used simple string matching `startsWith(parentSymbolName + "_")` to identify sub-symbols
- This approach cannot accurately distinguish between multi-part symbol sub-symbols and single-part symbol sub-symbols
- May accidentally delete other symbols' sub-symbols

**Scope**:
- Mixed symbol libraries (containing both multi-part and single-part symbols)
- Symbols with underscores in their names

**Code Location**:
- `src/core/kicad/ExporterSymbol.cpp:195-206`

**Example Scenario**:
```
Assume the library contains:
- Multi-part symbol A: MCU_A, with sub-symbols MCU_A_1_1, MCU_A_2_1
- Single-part symbol B: Resistor_B, with sub-symbol Resistor_B_0_1

When updating MCU_A, the original code would:
1. Identify MCU_A as the symbol to overwrite
2. Find sub-symbols starting with MCU_A_ and mark them for deletion
3. But if Resistor_B_0_1 is incorrectly identified, it may cause format corruption
```

---

#### Issue 3: Orphan Top-Level Sub-Symbols Not Deleted [Critical]

**Description**:
- In some cases, sub-symbols of multi-part symbols may have been incorrectly exported as top-level symbols
- The original code only deleted sub-symbols within the parent symbol, not orphan top-level sub-symbols
- Result: old orphan sub-symbols remain after update

**Scope**:
- Libraries previously exported with the incorrect method
- Incorrect library structure

**Code Location**:
- `src/core/kicad/ExporterSymbol.cpp:310-360`

**Example Scenario**:
```
Assume the library contains:
[Correct] Correct structure:
- Parent symbol: MCIMX6Y2CVM08AB
  - Sub-symbol: MCIMX6Y2CVM08AB_1_1
  - Sub-symbol: MCIMX6Y2CVM08AB_2_1

[Incorrect] Actual incorrect structure:
- Parent symbol: MCIMX6Y2CVM08AB
  - Sub-symbol: MCIMX6Y2CVM08AB_1_1
- Top-level symbol: MCIMX6Y2CVM08AB_2_1 (INCORRECT!)

When updating MCIMX6Y2CVM08AB:
1. MCIMX6Y2CVM08AB is correctly overwritten
2. MCIMX6Y2CVM08AB_1_1 is correctly deleted and regenerated as sub-symbol
3. MCIMX6Y2CVM08AB_2_1 is preserved as top-level symbol (INCORRECT!)
4. New MCIMX6Y2CVM08AB_2_1 is generated as sub-symbol
5. Result: Library contains two MCIMX6Y2CVM08AB_2_1 entries, format corrupted
```

## Decision

We implemented the following fixes:

### Fix 1: Pass updateMode Parameter

**Problem**: `updateMode` parameter was not correctly passed to `exportSymbolLibrary`

**Solution**:
```cpp
// Before
bool appendMode = !m_options.overwriteExistingFiles;
bool success = exporter.exportSymbolLibrary(m_symbols, m_options.libName, libraryPath, appendMode);

// After
bool appendMode = !m_options.overwriteExistingFiles;
bool updateMode = m_options.updateMode;
qDebug() << "Merge settings - Append mode:" << appendMode << "Update mode:" << updateMode;
bool success = exporter.exportSymbolLibrary(m_symbols, m_options.libName, libraryPath, appendMode, updateMode);
```

**Impact**:
- Update mode now correctly overwrites existing symbols
- Append mode still preserves existing symbols

---

### Fix 2: Improved Sub-Symbol Identification Logic

**Problem**: Simple string matching cannot accurately distinguish multi-part vs single-part sub-symbols

**Solution**:
```cpp
// First analyze existing symbols to determine which are multi-part (have multiple sub-symbols)
QMap<QString, QStringList> parentToSubSymbols; // parent name -> sub-symbol list
for (const QString &subSymbolName : subSymbolNames)
{
    // Extract parent name from sub-symbol name
    // Sub-symbol format: {parentName}_{unitNumber}_1
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

// For each overwritten parent symbol, collect all its sub-symbols
for (const QString &parentSymbolName : overwrittenSymbolNames)
{
    if (parentToSubSymbols.contains(parentSymbolName))
    {
        // This is a multi-part symbol, delete all sub-symbols
        for (const QString &subSymbolName : parentToSubSymbols[parentSymbolName])
        {
            subSymbolsToDelete.insert(subSymbolName);
            qDebug() << "Marking sub-symbol for deletion (multipart):" << subSymbolName << "(parent:" << parentSymbolName << ")";
        }
    }
    else
    {
        // This is a single-part symbol, find its sub-symbol (format: {parentName}_0_1)
        QString expectedSubSymbolName = parentSymbolName + "_0_1";
        if (subSymbolNames.contains(expectedSubSymbolName))
        {
            subSymbolsToDelete.insert(expectedSubSymbolName);
            qDebug() << "Marking sub-symbol for deletion (single part):" << expectedSubSymbolName << "(parent:" << parentSymbolName << ")";
        }
    }
}
```

**Impact**:
- Accurately distinguishes multi-part and single-part symbols
- Avoids accidentally deleting other symbols' sub-symbols
- Supports correct handling of mixed symbol libraries

---

### Fix 3: Delete Orphan Top-Level Sub-Symbols

**Problem**: Orphan sub-symbols existing as top-level symbols were not deleted

**Solution**:
```cpp
// Check if this is a top-level symbol that starts with an overwritten symbol name (may be a previously incorrectly exported sub-symbol)
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

// Only export symbols that are not overwritten and not orphan sub-symbols
if (!isOverwritten && !isOrphanedSubSymbol)
{
    // Export symbol content
    out << filteredContent;
    qDebug() << "Keeping existing symbol:" << symbolName;
}
```

**Impact**:
- Deletes all top-level symbols starting with overwritten parent symbol names
- Ensures correct library structure
- Avoids duplicate sub-symbols

## Consequences

### Positive

1. **Fixed update export functionality**
   - Update mode now correctly overwrites existing symbols
   - Users can update symbols in the library

2. **Accurate sub-symbol handling**
   - Correctly distinguishes multi-part and single-part symbols
   - Multi-part symbols: delete all sub-symbols (`_1_1`, `_2_1`, ...)
   - Single-part symbols: only delete `_0_1` sub-symbol

3. **Library structure integrity**
   - Deletes orphan top-level sub-symbols
   - Ensures sub-symbols only exist within parent symbols
   - Avoids library format corruption

4. **Support for mixed symbol libraries**
   - Correctly handles libraries with both multi-part and single-part symbols
   - Avoids accidentally deleting other symbols' sub-symbols

### Negative

1. **Increased complexity**
   - Sub-symbol identification logic is more complex
   - Requires analyzing library structure

2. **Performance impact**
   - Additional symbol analysis step required
   - May slightly increase export time

3. **Backward compatibility**
   - May require cleaning up previously incorrectly exported libraries
   - Users may need to re-export symbol libraries

## Alternatives Considered

### Option A: Regex Matching for Sub-Symbols

**Description**: Use regex to match sub-symbol names

**Pros**:
- More flexible pattern matching
- Can handle more complex naming rules

**Cons**:
- Regex performance is poor
- Reduced code readability

**Decision**: Not adopted; current solution is simpler and more efficient

### Option B: Re-Export Entire Library

**Description**: In update mode, completely re-export the entire library

**Pros**:
- Simple implementation
- Ensures correct library structure

**Cons**:
- Poor performance
- Cannot preserve user modifications

**Decision**: Not adopted; current solution is more efficient and preserves user modifications

## Implementation Details

### Modified Files

**Fix 1**:
1. `src/services/ExportService_Pipeline.cpp` - Pass `updateMode` parameter

**Fix 2**:
1. `src/core/kicad/ExporterSymbol.cpp` - Improved sub-symbol identification logic

**Fix 3**:
1. `src/core/kicad/ExporterSymbol.cpp` - Delete orphan top-level sub-symbols

### Testing

**Test Scenarios**:
1. Update multi-part symbol
2. Update single-part symbol
3. Mixed library update
4. Library containing orphan sub-symbols

**Expected Results**:
- Multi-part symbols correctly overwritten, all sub-symbols correctly updated
- Single-part symbols correctly overwritten, sub-symbols correctly updated
- Other symbols in mixed library unaffected
- Orphan sub-symbols correctly deleted

## Current Altium Library Merge Limitation

Append and update behavior currently applies only to KiCad symbol libraries, which support textual merging. Altium SchLib/PcbLib files are OLE-based binary libraries, and this project does not yet implement reading and incremental merging of existing libraries. Therefore, append or update requests are rejected when an existing Altium library is detected, preserving the original file instead of silently overwriting user data.

If Altium library reading and incremental merging are implemented later, add regression coverage for OLE streams, FileHeader, SectionKeys, multi-part symbols, and image Storage before removing this protective rejection.

The project now provides `OLECompoundReader` as a read-only CFB/OLE foundation. It validates V3 files, parses FAT/DIFAT, enumerates directory streams, and reads both regular and mini streams. It intentionally exposes raw stream data only; it does not yet interpret SchLib/PcbLib records or enable incremental merging, so the protective rejection for existing Altium libraries remains in place. Future merging must add Altium structure parsing, preservation of unknown streams and metadata, write-back support, and regression coverage using real library samples.

The project now also provides `AltiumSchLibReader`, which parses SchLib FileHeader and SectionKeys data, resolves component names to storage keys, and validates that each declared component has a Data stream. It exposes the visible part count and the FontID-ordered font table from FileHeader. Component Data can also be split into length-prefixed records while retaining flags, payloads, and complete encodings; C-string parameter records additionally expose structured `RECORD`, `OWNERPARTID`, `IndexInSheet`, and `FONTID` fields, while binary pin records expose their record type and owning part for multi-part ownership and graphic-order validation. Invalid numeric fields, out-of-range font references or owning parts, and non-monotonic `IndexInSheet` values now fail explicitly instead of silently falling back. The root `/Storage` stream can now be validated independently, exposing image flags, file names, and original compressed data while rejecting duplicate names, traversal-style names, inconsistent lengths, or mismatched declared counts. Unknown records remain available in their original form for lossless forwarding during future merging, while truncated records or malformed parameter data report the offset and underlying reason through `errorString()`. It remains read-only and does not yet interpret every business field or provide write-back support.

The project also provides `AltiumPcbLibReader`, which parses PcbLib FileHeader, Library/Data, SectionKeys, and footprint storage mappings, with access to footprint Header, Parameters, WideStrings, and Data streams. PcbLib Data consists of object IDs followed by object-type-specific subrecords, so it must not reuse SchLib's length-prefixed record splitter. The reader can now safely scan the object types known to this project while retaining complete encodings for objects and subblocks, and it validates string subblocks used by Pad and Text objects plus the 13-byte common header of each object's main block, exposing its Layer and Flags fields. Pad objects also expose main-block dimensions, shapes, hole data, and extension-block corner information; Track, Arc, and Text objects expose safely parsed base fields; Fill objects expose rectangle, rotation, and layer fields; Region and ComponentBody objects expose parameter blocks and outline vertices; and Text content is restored from its separate string block. Unknown object IDs, malformed subblocks, or missing common headers fail safely, and `errorString()` reports the affected object and subblock so callers can fall back to the raw stream. Both readers remain limited to structural validation and raw stream access; incremental write-back is not enabled yet.

## Related Documents

- [ADR-001: MVVM Architecture](001-mvvm-architecture.md)
- [ADR-002: Pipeline Parallelism for Export](002-pipeline-parallelism-for-export.md)
- [ADR-003: Pipeline Performance Optimization](003-pipeline-performance-optimization.md)
- [Symbol Exporter Documentation](../../developer/ARCHITECTURE.md)

## References

- [KiCad Symbol Library Format Specification](https://docs.kicad.org/7.0/en/eeschema/eeschema_libs/)
- [Symbol Type Documentation](https://docs.kicad.org/7.0/en/eeschema/eeschema_symbols/)
