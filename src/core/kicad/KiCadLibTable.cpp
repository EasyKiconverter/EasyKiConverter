#include "KiCadLibTable.h"

#include "core/utils/KiCadTableUtils.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QTextStream>

namespace EasyKiConverter {

/**
 * @brief 生成 KiCad 符号库或封装库表文件。
 * @param tableType 表类型。
 * @param fileName 输出文件名。
 * @param libName 库名称。
 * @param libPath 库目录路径。
 * @param outputDir 表文件输出目录。
 * @param libraryDescription 库描述。
 * @return 文件生成成功返回 true。
 */
bool generateKiCadLibTable(const QString& tableType,
                           const QString& fileName,
                           const QString& libName,
                           const QString& libPath,
                           const QString& outputDir,
                           const QString& libraryDescription) {
    QString filePath = QDir(outputDir).filePath(fileName);  // 表文件的完整输出路径。

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to create" << fileName << ":" << filePath;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    QFileInfo libInfo(libPath);
    QString relativePath = libInfo.fileName();  // 写入表文件的库相对路径。

    out << "(" << tableType << "\n";
    out << "  (version 7)\n";
    out << QString("  (lib (name \"%1\")(type \"KiCad\")(uri \"%2\")(options \"\")")
               .arg(KiCadTableUtils::escapeTableValue(libName), KiCadTableUtils::escapeTableValue(relativePath));
    if (!libraryDescription.isEmpty()) {
        out << QString("(descr \"%1\")").arg(KiCadTableUtils::escapeTableValue(libraryDescription));
    }
    out << ")\n";
    out << ")\n";

    file.close();
    qDebug() << "Generated" << fileName << ":" << filePath;
    return true;
}

}  // namespace EasyKiConverter
