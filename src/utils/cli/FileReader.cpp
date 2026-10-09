#include "FileReader.h"

#include "services/BomParser.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace {
/**
 * @brief 判断文本是否为有效的 LCSC 元件编号。
 * @param id 待检查编号。
 * @return 编号有效时返回 true。
 */
bool isValidComponentId(const QString& id) {
    return EasyKiConverter::BomParser::validateId(id);
}
}  // namespace

namespace EasyKiConverter {

/**
 * @brief 读取 BOM 文件中的元件编号。
 * @param filePath BOM 文件路径。
 * @param errorMessage 接收错误信息的字符串。
 * @return 元件编号列表。
 */
QStringList FileReader::readBomFile(const QString& filePath, QString& errorMessage) {
    QStringList componentIds;  // 解析得到的元件编号。

    if (!QFile::exists(filePath)) {
        errorMessage = QCoreApplication::translate("CliConverter", "输入文件不存在: %1").arg(filePath);
        return componentIds;
    }

    // 使用 BomParser 解析文件
    BomParser parser;
    componentIds = parser.parse(filePath);

    if (componentIds.isEmpty()) {
        errorMessage = QCoreApplication::translate("CliConverter", "BOM 表中没有找到有效的元器件编号");
    }

    return componentIds;
}

/**
 * @brief 读取组件列表文件并校验每行编号。
 * @param filePath 组件列表文件路径。
 * @param errorMessage 接收错误信息的字符串。
 * @return 元件编号列表。
 */
QStringList FileReader::readComponentListFile(const QString& filePath, QString& errorMessage) {
    QStringList componentIds;  // 解析得到的元件编号。

    if (!QFile::exists(filePath)) {
        errorMessage = QCoreApplication::translate("CliConverter", "输入文件不存在: %1").arg(filePath);
        return componentIds;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMessage = QCoreApplication::translate("CliConverter", "无法打开输入文件: %1").arg(filePath);
        return componentIds;
    }

    QTextStream stream(&file);  // 输入文本流。
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();  // 当前编号文本行。
        if (!line.isEmpty()) {
            if (isValidComponentId(line)) {
                componentIds.append(line.toUpper());
            } else {
                qWarning() << "FileReader: skipping invalid component ID:" << line;
            }
        }
    }

    file.close();
    return componentIds;
}

/**
 * @brief 判断文件是否存在。
 * @param filePath 文件路径。
 * @return 文件存在时返回 true。
 */
bool FileReader::fileExists(const QString& filePath) {
    return QFile::exists(filePath);
}

/**
 * @brief 获取文件扩展名。
 * @param filePath 文件路径。
 * @return 小写扩展名。
 */
QString FileReader::getFileExtension(const QString& filePath) {
    QFileInfo fileInfo(filePath);  // 文件信息对象。
    return fileInfo.suffix().toLower();
}

}  // namespace EasyKiConverter
