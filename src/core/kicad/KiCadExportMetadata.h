/**
 * @file KiCadExportMetadata.h
 * @brief 提供 KiCad 导出文件的生成器元数据。
 */

#ifndef KICADEXPORTMETADATA_H
#define KICADEXPORTMETADATA_H

#include <QString>

namespace EasyKiConverter::KiCadExportMetadata {

/**
 * @brief 返回导出器名称。
 */
inline QString generatorName() {
    return QStringLiteral("EasyKiConverter");
}

/**
 * @brief 返回生成器写入导出文件的版本号。
 */
inline QString generatorVersion() {
    return QStringLiteral("3.1.8");
}

/**
 * @brief 生成 golden 文件中使用的生成器版本注释。
 */
inline QString goldenMetadataComment() {
    return QStringLiteral("; (generator_version \"%1\")\n").arg(generatorVersion());
}

}  // namespace EasyKiConverter::KiCadExportMetadata

#endif  // KICADEXPORTMETADATA_H
