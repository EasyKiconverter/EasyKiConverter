#include "BomParser.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

#include <xlsxdocument.h>

namespace EasyKiConverter {

namespace {

/**
 * @brief 按 CSV 引号规则拆分一行。
 * @param line 待拆分的 CSV 记录。
 * @return 保留引号内逗号和转义引号语义的单元格列表。
 */
QStringList splitCsvLine(const QString& line) {
    QStringList cells;  // 拆分后的单元格列表。
    QString cell;  // 当前正在构造的单元格。
    bool inQuotes = false;  // 当前是否位于引号包围区域。

    for (int i = 0; i < line.size(); ++i) {
        const QChar character = line.at(i);  // 当前 CSV 字符。
        if (character == QChar('"')) {
            if (inQuotes && i + 1 < line.size() && line.at(i + 1) == QChar('"')) {
                cell += QChar('"');
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (character == QChar(',') && !inQuotes) {
            cells.append(cell);
            cell.clear();
        } else {
            cell += character;
        }
    }
    cells.append(cell);
    return cells;
}

/**
 * @brief 判断跨行 CSV 记录是否仍处于引号包围状态。
 * @param record 待检查的 CSV 记录。
 * @return 记录仍缺少结束引号时返回 true。
 */
bool csvRecordHasOpenQuote(const QString& record) {
    bool inQuotes = false;  // 当前是否位于引号包围区域。
    for (int i = 0; i < record.size(); ++i) {
        if (record.at(i) != QChar('"'))
            continue;
        if (inQuotes && i + 1 < record.size() && record.at(i + 1) == QChar('"')) {
            ++i;
        } else {
            inQuotes = !inQuotes;
        }
    }
    return inQuotes;
}

}  // namespace

/**
 * @brief 创建 BOM 解析器。
 * @param parent Qt 对象父级。
 */
BomParser::BomParser(QObject* parent) : QObject(parent) {}

/**
 * @brief 获取需要排除的预设元件编号集合。
 * @return 排除编号集合。
 */
const QSet<QString>& BomParser::getExcludedIds() {
    static const QSet<QString> excluded = {"C0402", "C0603", "C0805", "C1206"};  // 预设排除的元件编号。
    return excluded;
}

/**
 * @brief 校验 LCSC 编号格式并过滤预设的非元件编号。
 * @param componentId 待校验的元件编号。
 * @return 编号格式有效且不在排除列表中时返回 true。
 */
bool BomParser::validateId(const QString& componentId) {
    // LCSC 元件ID格式：以 'C' 或 'c' 开头，后面跟至少4位数字
    static const QRegularExpression re("^[Cc]\\d{4,}$");  // LCSC 编号格式。
    bool matches = re.match(componentId).hasMatch();  // 格式匹配结果。

    if (matches) {
        return !getExcludedIds().contains(componentId.toUpper());
    }
    return false;
}

/**
 * @brief 从文本中提取有效、去重并规范化的 LCSC 元件编号。
 * @param text 输入文本。
 * @return 规范化后的去重编号列表。
 */
QStringList BomParser::extractIdsFromText(const QString& text) {
    QStringList extractedIds;  // 提取结果列表。
    static const QRegularExpression re(QStringLiteral("[Cc]\\d{4,}"));  // 编号匹配表达式。
    QRegularExpressionMatchIterator it = re.globalMatch(text);  // 全局匹配迭代器。

    while (it.hasNext()) {
        const QString id = it.next().captured().toUpper();  // 当前规范化编号。
        if (validateId(id) && !extractedIds.contains(id)) {
            extractedIds.append(id);
        }
    }
    return extractedIds;
}

/**
 * @brief 根据文件扩展名解析 CSV、TXT 或 Excel BOM 文件。
 * @param filePath BOM 文件路径。
 * @return 去重后的元件编号列表。
 */
QStringList BomParser::parse(const QString& filePath) {
    qDebug() << "BomParser: Parsing file:" << filePath;

    QStringList componentIds;  // 解析得到的元件编号。
    QFileInfo fileInfo(filePath);  // 输入文件信息。

    if (!fileInfo.exists()) {
        qWarning() << "BomParser: File does not exist:" << filePath;
        return componentIds;
    }

    QString suffix = fileInfo.suffix().toLower();  // 小写文件扩展名。

    if (suffix == "csv" || suffix == "txt") {
        componentIds = parseCsv(filePath);
    } else if (suffix == "xlsx" || suffix == "xls") {
        componentIds = parseExcel(filePath);
    } else {
        qWarning() << "BomParser: Unsupported file format:" << suffix;
    }

    qDebug() << "BomParser: Extracted" << componentIds.size() << "IDs";
    return componentIds;
}

/**
 * @brief 读取 UTF-8 CSV/TXT 文件并提取元件编号。
 * @param filePath CSV 或 TXT 文件路径。
 * @return 去重后的元件编号列表。
 */
QStringList BomParser::parseCsv(const QString& filePath) {
    QStringList componentIds;  // 解析得到的元件编号。
    QFile file(filePath);  // 输入文本文件。

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "BomParser: Failed to open CSV:" << file.errorString();
        return componentIds;
    }

    QTextStream in(&file);  // UTF-8 文本流。
    in.setEncoding(QStringConverter::Utf8);

    QString record;  // 当前可能跨行的 CSV 记录。
    while (!in.atEnd()) {
        QString line = in.readLine();  // 当前输入行。
        if (!record.isEmpty())
            record.append('\n');
        record.append(line);

        if (csvRecordHasOpenQuote(record))
            continue;

        const QStringList cells = splitCsvLine(record);  // 当前记录的单元格。
        for (const QString& cell : cells) {
            processCellText(cell, componentIds);
        }
        record.clear();
    }

    if (!record.isEmpty()) {
        const QStringList cells = splitCsvLine(record);  // 文件尾部记录的单元格。
        for (const QString& cell : cells)
            processCellText(cell, componentIds);
    }

    file.close();
    return componentIds;
}

/**
 * @brief 遍历 Excel 文件的所有工作表和单元格并提取元件编号。
 * @param filePath Excel 文件路径。
 * @return 去重后的元件编号列表。
 */
QStringList BomParser::parseExcel(const QString& filePath) {
    QStringList componentIds;  // 解析得到的元件编号。
    QXlsx::Document xlsx(filePath);  // Excel 文档对象。

    if (!xlsx.load()) {
        qWarning() << "BomParser: Failed to load Excel file";
        return componentIds;
    }

    for (const QString& sheetName : xlsx.sheetNames()) {  // 遍历工作表。
        xlsx.selectSheet(sheetName);
        QXlsx::CellRange range = xlsx.dimension();  // 当前工作表有效单元格范围。

        for (int row = range.firstRow(); row <= range.lastRow(); ++row) {
            for (int col = range.firstColumn(); col <= range.lastColumn(); ++col) {
                if (auto cell = xlsx.cellAt(row, col)) {
                    processCellText(cell->readValue().toString(), componentIds);
                }
            }
        }
    }

    return componentIds;
}

/**
 * @brief 清理单元格文本、校验编号并追加规范化结果。
 * @param text 单元格文本。
 * @param result 接收元件编号的输出列表。
 */
void BomParser::processCellText(const QString& text, QStringList& result) {
    QString trimmedCell = text.trimmed();  // 去除空白后的单元格文本。

    // 去除可能的引号
    if (trimmedCell.startsWith('"') && trimmedCell.endsWith('"')) {
        trimmedCell = trimmedCell.mid(1, trimmedCell.length() - 2);
    }

    if (validateId(trimmedCell)) {
        QString id = trimmedCell.toUpper();  // 规范化后的元件编号。
        if (!result.contains(id)) {
            result.append(id);
        }
    }
}

}  // namespace EasyKiConverter
