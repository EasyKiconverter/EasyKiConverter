#include "KiCadLibraryTableManager.h"

#include "core/utils/KiCadTableUtils.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>

namespace EasyKiConverter {

/**
 * @brief 将符号库登记到目标 KiCad 工程，或生成手动导入指南。
 * @param outputDir 导出输出目录。
 * @param libName 库名称。
 * @param libFilePath 符号库文件路径。
 * @param libraryDescription 库描述。
 * @return 登记结果。
 */
KiCadLibraryTableManager::RegistrationResult KiCadLibraryTableManager::registerSymbolLibrary(
    const QString& outputDir,
    const QString& libName,
    const QString& libFilePath,
    const QString& libraryDescription) {
    const QString projectRoot = findKiCadProjectRoot(outputDir);  ///< 检测到的 KiCad 工程根目录。
    if (projectRoot.isEmpty()) {
        return writeImportGuide(outputDir) ? RegistrationResult::ImportGuideWritten : RegistrationResult::Failed;
    }

    const QString uri = makeProjectUri(projectRoot, libFilePath);  ///< 相对于工程根目录的库 URI。
    return updateProjectLibraryTable(projectRoot,
                                     QStringLiteral("sym-lib-table"),
                                     QStringLiteral("sym_lib_table"),
                                     libName,
                                     uri,
                                     libraryDescription)
               ? RegistrationResult::Registered
               : RegistrationResult::Failed;
}

/**
 * @brief 将封装库登记到目标 KiCad 工程，或生成手动导入指南。
 * @param outputDir 导出输出目录。
 * @param libName 库名称。
 * @param libDirPath 封装库目录路径。
 * @param libraryDescription 库描述。
 * @return 登记结果。
 */
KiCadLibraryTableManager::RegistrationResult KiCadLibraryTableManager::registerFootprintLibrary(
    const QString& outputDir,
    const QString& libName,
    const QString& libDirPath,
    const QString& libraryDescription) {
    const QString projectRoot = findKiCadProjectRoot(outputDir);  ///< 检测到的 KiCad 工程根目录。
    if (projectRoot.isEmpty()) {
        return writeImportGuide(outputDir) ? RegistrationResult::ImportGuideWritten : RegistrationResult::Failed;
    }

    const QString uri = makeProjectUri(projectRoot, libDirPath);  ///< 相对于工程根目录的库 URI。
    return updateProjectLibraryTable(projectRoot,
                                     QStringLiteral("fp-lib-table"),
                                     QStringLiteral("fp_lib_table"),
                                     libName,
                                     uri,
                                     libraryDescription)
               ? RegistrationResult::Registered
               : RegistrationResult::Failed;
}

/**
 * @brief 在未检测到 KiCad 工程时写入库导入指南。
 * @param outputDir 导出输出目录。
 * @return 导入指南写入成功时返回 true。
 */
bool KiCadLibraryTableManager::writeImportGuide(const QString& outputDir) {
    QDir dir(outputDir);  ///< 导入指南所在的输出目录。
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        qWarning() << "KiCadLibraryTableManager: Failed to create output directory for import guide:" << outputDir;
        return false;
    }

    const QString guidePath = dir.filePath(QStringLiteral("KiCad_IMPORT.md"));  ///< 导入指南文件路径。
    QSaveFile file(guidePath);  ///< 原子写入导入指南的文件对象。
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "KiCadLibraryTableManager: Failed to open import guide:" << guidePath;
        return false;
    }

    QTextStream out(&file);  ///< 导入指南文本输出流。
    out.setEncoding(QStringConverter::Utf8);

    out << "# KiCad Library Import\n\n";
    out << "EasyKiConverter did not find a KiCad project (`*.kicad_pro`) in this output path or its parent "
           "directories, so it did not modify any project library table.\n\n";
    out << "To show library descriptions in KiCad, add the exported libraries to the target project's library "
           "tables or to KiCad's global library tables.\n\n";

    const QString symTablePath = dir.filePath(QStringLiteral("sym-lib-table"));  ///< 符号库表路径。
    if (QFileInfo::exists(symTablePath)) {
        out << "## Symbol Library Entry\n\n";
        out << "Use the entry from:\n\n";
        out << "```text\n" << symTablePath << "\n```\n\n";
    }

    const QString fpTablePath = dir.filePath(QStringLiteral("fp-lib-table"));  ///< 封装库表路径。
    if (QFileInfo::exists(fpTablePath)) {
        out << "## Footprint Library Entry\n\n";
        out << "Use the entry from:\n\n";
        out << "```text\n" << fpTablePath << "\n```\n\n";
    }

    out << "Project-specific KiCad library tables are named `sym-lib-table` and `fp-lib-table` and live next to the "
           "project's `.kicad_pro` file.\n";

    if (!file.commit()) {
        qWarning() << "KiCadLibraryTableManager: Failed to commit import guide:" << guidePath;
        return false;
    }

    return true;
}

/**
 * @brief 从输出目录向上查找最近的 KiCad 工程根目录。
 * @param outputDir 输出目录。
 * @return 最近的 KiCad 工程根目录，未找到时返回空字符串。
 */
QString KiCadLibraryTableManager::findKiCadProjectRoot(const QString& outputDir) {
    QDir dir(QFileInfo(outputDir).absoluteFilePath());  ///< 当前向上搜索的目录。
    if (!dir.exists()) {
        return QString();
    }

    while (true) {
        const QStringList projectFiles =
            dir.entryList(QStringList(QStringLiteral("*.kicad_pro")), QDir::Files);  ///< 当前目录中的 KiCad 工程文件。
        if (!projectFiles.isEmpty()) {
            return dir.absolutePath();
        }

        if (!dir.cdUp()) {
            break;
        }
    }

    return QString();
}

/**
 * @brief 更新工程库表并替换同名库条目。
 * @param projectRoot KiCad 工程根目录。
 * @param tableFileName 库表文件名。
 * @param tableRootName 库表根节点名称。
 * @param libName 库名称。
 * @param uri 库 URI。
 * @param libraryDescription 库描述。
 * @return 更新成功时返回 true。
 */
bool KiCadLibraryTableManager::updateProjectLibraryTable(const QString& projectRoot,
                                                         const QString& tableFileName,
                                                         const QString& tableRootName,
                                                         const QString& libName,
                                                         const QString& uri,
                                                         const QString& libraryDescription) {
    QDir projectDir(projectRoot);  ///< KiCad 工程根目录对象。
    const QString tablePath = projectDir.filePath(tableFileName);  ///< 目标库表文件路径。

    QString content;  ///< 现有库表文本内容。
    {
        QFile existingFile(tablePath);  ///< 现有库表文件对象。
        if (existingFile.exists()) {
            if (!existingFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                qWarning() << "KiCadLibraryTableManager: Failed to read library table:" << tablePath;
                return false;
            }
            QTextStream in(&existingFile);  ///< 库表读取流。
            in.setEncoding(QStringConverter::Utf8);
            content = in.readAll();
        }
    }

    if (content.trimmed().isEmpty()) {
        content = QStringLiteral("(%1\n  (version 7)\n)\n").arg(tableRootName);
    }

    QStringList lines = content.split('\n');  ///< 按行拆分后的库表内容。
    const QRegularExpression entryRegex(  // 匹配同名库条目的正则表达式。
        QStringLiteral(R"(^\s*\(lib\s+\(name\s+"?%1"?\))").arg(QRegularExpression::escape(libName)));

    QStringList filteredLines;  ///< 删除旧同名条目后的库表行。
    filteredLines.reserve(lines.size() + 1);
    for (const QString& line : lines) {  // 当前库表文本行。
        if (!entryRegex.match(line).hasMatch()) {
            filteredLines.append(line);
        }
    }

    int insertIndex = filteredLines.size();  ///< 新库条目的插入位置。
    while (insertIndex > 0 && filteredLines.at(insertIndex - 1).trimmed().isEmpty()) {
        --insertIndex;
    }
    if (insertIndex > 0 && filteredLines.at(insertIndex - 1).trimmed() == QStringLiteral(")")) {
        --insertIndex;
    }

    filteredLines.insert(insertIndex, makeLibraryEntry(libName, uri, libraryDescription));
    QString updatedContent = filteredLines.join('\n');  ///< 更新后的库表文本。
    if (!updatedContent.endsWith('\n')) {
        updatedContent += '\n';
    }

    QSaveFile file(tablePath);  ///< 原子写入库表的文件对象。
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "KiCadLibraryTableManager: Failed to write library table:" << tablePath;
        return false;
    }

    QTextStream out(&file);  ///< 库表文本输出流。
    out.setEncoding(QStringConverter::Utf8);
    out << updatedContent;
    if (!file.commit()) {
        qWarning() << "KiCadLibraryTableManager: Failed to commit library table:" << tablePath;
        return false;
    }

    qDebug() << "KiCadLibraryTableManager: Updated project library table:" << tablePath;
    return true;
}

/**
 * @brief 生成 KiCad 库表中的单条库记录。
 * @param libName 库名称。
 * @param uri 库 URI。
 * @param libraryDescription 库描述。
 * @return 库表记录文本。
 */
QString KiCadLibraryTableManager::makeLibraryEntry(const QString& libName,
                                                   const QString& uri,
                                                   const QString& libraryDescription) {
    // 库表中待写入的单条记录文本。
    QString entry = QStringLiteral("  (lib (name \"%1\")(type \"KiCad\")(uri \"%2\")(options \"\")")
                        .arg(KiCadTableUtils::escapeTableValue(libName), KiCadTableUtils::escapeTableValue(uri));
    if (!libraryDescription.isEmpty()) {
        entry += QStringLiteral("(descr \"%1\")").arg(KiCadTableUtils::escapeTableValue(libraryDescription));
    } else {
        entry += QStringLiteral("(descr \"\")");
    }
    entry += QStringLiteral(")");
    return entry;
}

/**
 * @brief 生成相对于工程根目录的 KiCad 库 URI。
 * @param projectRoot KiCad 工程根目录。
 * @param libraryPath 库文件或目录路径。
 * @return 相对于工程根目录的 KiCad 库 URI。
 */
QString KiCadLibraryTableManager::makeProjectUri(const QString& projectRoot, const QString& libraryPath) {
    QString relativePath =
        QDir(projectRoot).relativeFilePath(QFileInfo(libraryPath).absoluteFilePath());  ///< 库相对于工程根目录的路径。
    relativePath.replace(QDir::separator(), QLatin1Char('/'));
    if (relativePath == QStringLiteral(".")) {
        return QStringLiteral("${KIPRJMOD}");
    }
    return QStringLiteral("${KIPRJMOD}/%1").arg(relativePath);
}

}  // namespace EasyKiConverter
