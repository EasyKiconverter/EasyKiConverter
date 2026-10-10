#include "AltiumSchImageStorageWriter.h"

#include "AltiumSchImageStorageEncoder.h"
#include "AltiumSchLibWriter.h"
#include "utils/AltiumWriterUtils.h"

#include <QFileInfo>
#include <QSet>

namespace EasyKiConverter {

/**
 * @brief 创建 SchLib 图片 Storage 写入器。
 * @param owner 所属的 SchLib 主写入器。
 */
AltiumSchImageStorageWriter::AltiumSchImageStorageWriter(AltiumSchLibWriter& owner) : m_owner(owner) {}

/**
 * @brief 校验嵌入图片名称并生成不冲突的 Storage 文件名。
 * @details 无效图片不会进入 Storage，但仍通过主写入器记录诊断信息。
 * @param components 待处理的原理图组件列表。
 */
void AltiumSchImageStorageWriter::prepareNames(const QList<AltiumSchComponent>& components) {
    QSet<QString> usedNames;  // 已分配的 Storage 文件名集合。
    for (const AltiumSchComponent& component : components) {  // 遍历所有组件图片。
        for (int imageIndex = 0; imageIndex < component.images.size(); ++imageIndex) {
            const AltiumSchImage& image = component.images.at(imageIndex);  // 当前待处理的图片。
            if (!image.embedImage) {
                if (image.fileName.trimmed().isEmpty()) {
                    const QString diagnostic =  // 外部图片引用缺少文件名时的诊断文本。
                        QStringLiteral("组件 %1 图片 %2 的外部文件名为空，已跳过文件引用")
                            .arg(component.name)
                            .arg(imageIndex);
                    m_owner.m_diagnostics.append(diagnostic);
                    qWarning() << "AltiumSchLibWriter:" << diagnostic;
                }
                continue;
            }

            QString sourceName = image.fileName;  // 图片来源文件名副本。
            sourceName.replace('\\', '/');
            const QString embeddedName = QFileInfo(sourceName).fileName();  // 去除目录后的嵌入文件名。
            if (image.data.isEmpty()) {
                const QString diagnostic =  // 嵌入数据缺失时的诊断文本。
                    QStringLiteral("组件 %1 图片 %2 的嵌入数据为空，已跳过 Storage")
                        .arg(component.name)
                        .arg(imageIndex);
                m_owner.m_diagnostics.append(diagnostic);
                qWarning() << "AltiumSchLibWriter:" << diagnostic;
                continue;
            }
            if (embeddedName.toLocal8Bit().size() > 255) {
                const QString diagnostic =  // 嵌入文件名过长时的诊断文本。
                    QStringLiteral("组件 %1 图片 %2 的嵌入文件名超过 255 字节，已跳过 Storage")
                        .arg(component.name)
                        .arg(imageIndex);
                m_owner.m_diagnostics.append(diagnostic);
                qWarning() << "AltiumSchLibWriter:" << diagnostic;
                continue;
            }
            if (!AltiumWriterUtils::isValidImageStorageName(embeddedName)) {
                const QString diagnostic =  // 嵌入文件名不符合格式时的诊断文本。
                    QStringLiteral("组件 %1 图片 %2 的嵌入文件名无效: %3，已跳过 Storage")
                        .arg(component.name)
                        .arg(imageIndex)
                        .arg(image.fileName);
                m_owner.m_diagnostics.append(diagnostic);
                qWarning() << "AltiumSchLibWriter:" << diagnostic;
                continue;
            }

            const QFileInfo fileInfo(embeddedName);  // 当前嵌入文件名的信息对象。
            const QString suffix = fileInfo.suffix();  // 文件扩展名。
            const QString baseName =  // 不含扩展名的基础文件名。
                suffix.isEmpty() ? embeddedName : embeddedName.left(embeddedName.size() - suffix.size() - 1);
            QString candidate = embeddedName;  // 当前候选的唯一 Storage 文件名。
            int duplicateIndex = 1;  // 重名文件的递增后缀。
            while (usedNames.contains(candidate.toCaseFolded())) {
                ++duplicateIndex;
                const QString suffixText = suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix;  // 扩展名文本。
                const QString marker = QStringLiteral("_%1").arg(duplicateIndex);  // 重名后缀。
                QString trimmedBase = baseName;  // 为长度限制准备的基础文件名副本。
                while (!trimmedBase.isEmpty() && (trimmedBase + marker + suffixText).toLocal8Bit().size() > 255)
                    trimmedBase.chop(1);
                candidate = trimmedBase + marker + suffixText;
            }
            usedNames.insert(candidate.toCaseFolded());
            m_owner.m_embeddedImageNames.insert(&image, candidate);
            if (candidate != embeddedName) {
                const QString diagnostic =  // 重名文件改名时的诊断文本。
                    QStringLiteral("组件 %1 图片 %2 的嵌入文件名 %3 重复，已改为 %4")
                        .arg(component.name)
                        .arg(imageIndex)
                        .arg(embeddedName)
                        .arg(candidate);
                m_owner.m_diagnostics.append(diagnostic);
                qWarning() << "AltiumSchLibWriter:" << diagnostic;
            }
        }
    }
}

/**
 * @brief 使用统一编码器写入根 Storage 图片流。
 * @param ole SchLib 根 OLE 写入器。
 * @param components 待写入嵌入图片的组件列表。
 */
void AltiumSchImageStorageWriter::write(OLECompoundWriter& ole, const QList<AltiumSchComponent>& components) {
    const QByteArray storageData =  // 统一编码器生成的 Storage 流数据。
        AltiumSchImageStorageEncoder::encode(components, m_owner.m_embeddedImageNames, m_owner.m_diagnostics);
    ole.writeStream("Storage", storageData);
}

}  // namespace EasyKiConverter
