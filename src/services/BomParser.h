#ifndef BOMPARSER_H
#define BOMPARSER_H

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

/**
 * @brief BOM 解析器类
 *
 * 专门负责从不同格式的文件中提取 LCSC 元件 ID
 */
class BomParser : public QObject {
    Q_OBJECT

public:
    explicit BomParser(QObject* parent = nullptr);

    /**
     * @brief 解析 BOM 文件并提取元件 ID
     *
     * @param filePath 文件物理路径
     * @return QStringList 提取到的元件 ID 列表（已去重且转大写）
     */
    QStringList parse(const QString& filePath);

    /**
     * @brief 验证元件 ID 格式
     *
     * @param componentId 元件 ID
     * @return bool 是否有效
     */
    static bool validateId(const QString& componentId);

    /**
     * @brief 从任意文本中提取 LCSC 元件编号
     *
     * @param text 输入文本
     * @return QStringList 去重并转换为大写后的元件编号
     */
    static QStringList extractIdsFromText(const QString& text);

    /**
     * @brief 获取排除的元件 ID 列表（用于过滤常见的非 LCSC 元件）
     */
    static const QSet<QString>& getExcludedIds();

private:
    /**
     * @brief 解析 CSV/TXT 格式
     */
    QStringList parseCsv(const QString& filePath);

    /**
     * @brief 解析 Excel (xlsx/xls) 格式
     */
    QStringList parseExcel(const QString& filePath);

    /**
     * @brief 处理单元格文本并提取 ID
     * @param text 单元格文本。
     * @param result 输出的元件编号列表。
     */
    void processCellText(const QString& text, QStringList& result);
};

}  // namespace EasyKiConverter

#endif  // BOMPARSER_H
