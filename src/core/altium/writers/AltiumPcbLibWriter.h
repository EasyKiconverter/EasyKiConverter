#pragma once

#include "compound/OLECompoundWriter.h"
#include "models/AltiumPcbComponent.h"
#include "utils/AltiumBinaryWriter.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

class AltiumPcbPrimitiveWriter;
class AltiumPcbInputValidator;

/**
 * @brief Altium PcbLib 文件写入器
 * @details 分层生成 CFB 容器、库级配置、封装目录和强类型图元记录；每一层均保持
 *          自己的长度与索引不变量，避免依赖不可审查的二进制模板。
 */
class AltiumPcbLibWriter {
public:
    /**
     * @brief 写入 PcbLib 文件
     * @param components 封装列表
     * @param filePath 输出文件路径
     * @param libraryName 库名称
     * @return 是否成功
     */
    bool write(const QList<AltiumPcbComponent>& components,
               const QString& filePath,
               const QString& libraryName = QString());

    /**
     * @brief 获取最近一次写入产生的非致命诊断
     * @return 被修正的非有限浮点字段诊断列表
     */
    QStringList diagnostics() const {
        return m_diagnostics;
    }

private:
    // ---- 文件级写入 ----
    void writeFileHeader(OLECompoundWriter& ole);
    void writeSectionKeys(OLECompoundWriter& ole,
                          const QList<AltiumPcbComponent>& components,
                          const QStringList& sectionKeys);
    void writeLibraryStorage(OLECompoundWriter& ole,
                             const QList<AltiumPcbComponent>& components,
                             const QString& filePath);
    void writeLibraryData(QByteArray& buffer, const QList<AltiumPcbComponent>& components, const QString& filePath);
    void writeModelsStorage(OLECompoundWriter& ole, const QList<AltiumPcbComponent>& components);

    // ---- 封装级写入 ----
    void writeFootprintStorage(OLECompoundWriter& ole, const AltiumPcbComponent& component, const QString& sectionKey);
    void writeFootprintParameters(QByteArray& buffer, const AltiumPcbComponent& component);
    void writeFootprintData(QByteArray& buffer, const AltiumPcbComponent& component);
    void writeWideStrings(QByteArray& buffer, const AltiumPcbComponent& component);

    // ---- 辅助 ----
    void writeCommonPrimitiveHeader(AltiumBinaryWriter& writer, uint8_t layer, uint16_t flags);
    static uint16_t encodePrimitiveFlags(bool isLocked, bool isTentingTop, bool isTentingBottom, bool isKeepout);
    void writeUniqueIdPrimitiveInformation(QByteArray& buffer, const AltiumPcbComponent& component);
    void writeExtendedPrimitiveInformation(QByteArray& buffer, const AltiumPcbComponent& component);
    uint32_t toV7LayerId(uint8_t layer) const;
    double normalizeFiniteValue(double value, double fallback, const QString& context);
    bool validateComponents(const QList<AltiumPcbComponent>& components, const QString& filePath);
    int countPrimitives(const AltiumPcbComponent& component) const;
    QString buildLibraryMetadata(const QString& filePath) const;

    /**
     * @brief 允许图元写入器复用文件级校验、层映射和字符串状态。
     */
    friend class AltiumPcbPrimitiveWriter;
    friend class AltiumPcbInputValidator;

    // 广字符串管理
    int addWideString(const QString& text);
    QStringList m_wideStrings;  ///< 当前库待写入的宽字符串列表。
    QStringList m_diagnostics;  ///< 最近一次写入产生的非致命诊断列表。
};

}  // namespace EasyKiConverter
