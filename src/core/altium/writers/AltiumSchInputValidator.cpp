#include "AltiumSchInputValidator.h"

#include "AltiumSchGeometryValidator.h"
#include "AltiumSchLibWriter.h"
#include "AltiumSchOwnershipValidator.h"

#include <QDebug>
#include <QSet>

namespace EasyKiConverter {

namespace {

/**
 * @brief 向写入器诊断列表记录错误并返回失败结果。
 * @param diagnostics 诊断信息列表。
 * @param message 待记录的错误消息。
 * @return 始终返回 false。
 */
bool reject(QStringList& diagnostics, const QString& message) {
    diagnostics.append(message);
    qWarning() << "AltiumSchLibWriter:" << message;
    return false;
}

/**
 * @brief 检查会进入 Altium 参数块的字符串是否包含非法分隔符。
 * @param diagnostics 诊断信息列表。
 * @param component 所属的原理图元件。
 * @param value 待检查的字符串值。
 * @param context 字符串字段的语义上下文。
 * @return 字符串可安全写入时返回 true。
 */
bool validateParameterValue(QStringList& diagnostics,
                            const AltiumSchComponent& component,
                            const QString& value,
                            const QString& context) {
    if (!value.contains(QChar('|')) && !value.contains(QChar::Null)) {
        return true;
    }
    return reject(
        diagnostics,
        QStringLiteral("Altium SchLib 组件 %1 的%2包含参数分隔符或 NUL，已拒绝写入").arg(component.name, context));
}

/**
 * @brief 检查参数名是否非空且不会破坏参数块格式。
 * @param diagnostics 诊断信息列表。
 * @param component 所属的原理图元件。
 * @param name 待检查的参数名。
 * @param context 参数名的语义上下文。
 * @return 参数名有效时返回 true。
 */
bool validateParameterName(QStringList& diagnostics,
                           const AltiumSchComponent& component,
                           const QString& name,
                           const QString& context) {
    if (!name.trimmed().isEmpty() && !name.contains(QChar('|')) && !name.contains(QChar::Null)) {
        return true;
    }
    return reject(
        diagnostics,
        QStringLiteral("Altium SchLib 组件 %1 的%2参数名无效: %3，已拒绝写入").arg(component.name, context, name));
}

}  // namespace

/**
 * @brief 校验 SchLib 组件输入并复用几何及部件归属校验器。
 * @param owner SchLib 写入器，用于复用诊断和几何校验状态。
 * @param components 待写入的组件列表。
 * @return 输入可写入时返回 true。
 */
bool AltiumSchInputValidator::validate(AltiumSchLibWriter& owner, const QList<AltiumSchComponent>& components) {
    QStringList& diagnostics = owner.m_diagnostics;  // 主写入器共享的诊断列表。
    QSet<QString> componentNames;  // 已出现组件名称的大小写折叠集合。
    for (const AltiumSchComponent& component : components) {
        if (component.name.trimmed().isEmpty()) {
            return reject(diagnostics, QStringLiteral("Altium SchLib 组件名称为空，已拒绝写入"));
        }
        if (component.name.toLatin1().size() > 255) {
            return reject(diagnostics,
                          QStringLiteral("Altium SchLib 组件 %1 名称超过 255 字节，已拒绝写入").arg(component.name));
        }
        if (QString::fromLatin1(component.name.toLatin1()) != component.name) {
            return reject(
                diagnostics,
                QStringLiteral("Altium SchLib 组件名称包含无法编码的字符: %1，已拒绝写入").arg(component.name));
        }

        if (!validateParameterValue(diagnostics, component, component.name, QStringLiteral("组件名称")) ||
            !validateParameterValue(diagnostics, component, component.description, QStringLiteral("组件描述")) ||
            !validateParameterValue(diagnostics, component, component.designatorPrefix, QStringLiteral("位号前缀"))) {
            return false;
        }
        for (const QString& alias : component.aliases) {
            if (!validateParameterValue(diagnostics, component, alias, QStringLiteral("组件别名"))) {
                return false;
            }
        }
        for (const AltiumSchPin& pin : component.pins) {
            if (!validateParameterValue(diagnostics, component, pin.name, QStringLiteral("引脚名称")) ||
                !validateParameterValue(diagnostics, component, pin.designator, QStringLiteral("引脚编号"))) {
                return false;
            }
        }
        for (const AltiumSchText& text : component.texts) {
            if (!validateParameterValue(diagnostics, component, text.text, QStringLiteral("文本内容")) ||
                !validateParameterValue(diagnostics, component, text.fontName, QStringLiteral("文本字体名称")) ||
                !validateParameterValue(diagnostics, component, text.anchor, QStringLiteral("文本对齐锚点"))) {
                return false;
            }
        }
        for (const AltiumSchTextFrame& frame : component.textFrames) {
            if (!validateParameterValue(diagnostics, component, frame.text, QStringLiteral("文本框内容")) ||
                !validateParameterValue(diagnostics, component, frame.fontName, QStringLiteral("文本框字体名称"))) {
                return false;
            }
        }
        for (const AltiumSchImage& image : component.images) {
            // 有效的嵌入图片名称由图片 Storage 协调器负责诊断并跳过；只有
            // 外部引用或嵌入数据为空时，文件名才会直接进入参数块。
            if ((!image.embedImage || image.data.isEmpty()) &&
                !validateParameterValue(diagnostics, component, image.fileName, QStringLiteral("图片文件名"))) {
                return false;
            }
        }
        for (const AltiumSchParameter& parameter : component.parameters) {
            if (!validateParameterValue(diagnostics, component, parameter.value, QStringLiteral("参数值"))) {
                return false;
            }
        }
        for (const auto& implementation : component.implementations) {
            if (!validateParameterValue(
                    diagnostics, component, implementation.modelName, QStringLiteral("实现模型名称")) ||
                !validateParameterValue(
                    diagnostics, component, implementation.modelType, QStringLiteral("实现模型类型")) ||
                !validateParameterValue(
                    diagnostics, component, implementation.dataFileKind, QStringLiteral("实现数据文件类型")) ||
                !validateParameterValue(
                    diagnostics, component, implementation.dataFileEntity, QStringLiteral("实现数据文件实体"))) {
                return false;
            }
            for (auto it = implementation.parameters.cbegin(); it != implementation.parameters.cend(); ++it) {
                if (!validateParameterValue(diagnostics, component, it.value(), QStringLiteral("实现参数值"))) {
                    return false;
                }
            }
            for (auto it = implementation.pinMappings.cbegin(); it != implementation.pinMappings.cend(); ++it) {
                if (!validateParameterValue(diagnostics, component, it.value(), QStringLiteral("引脚映射值"))) {
                    return false;
                }
            }
        }

        const QString foldedName = component.name.trimmed().toCaseFolded();  // 用于检测重名的规范化名称。
        if (componentNames.contains(foldedName)) {
            return reject(
                diagnostics,
                QStringLiteral("Altium SchLib 组件名称重复（不区分大小写）: %1，已拒绝写入").arg(component.name));
        }
        componentNames.insert(foldedName);

        if (component.partCount > 32767) {
            return reject(diagnostics,
                          QStringLiteral("Altium SchLib 组件 %1 的 partCount 超出支持范围: %2，已拒绝写入")
                              .arg(component.name)
                              .arg(component.partCount));
        }
        for (const AltiumSchComponent::Implementation& implementation : component.implementations) {
            for (auto it = implementation.parameters.cbegin(); it != implementation.parameters.cend(); ++it) {
                if (it.key().trimmed().isEmpty() || it.key().contains(QChar('|')) || it.key().contains(QChar::Null)) {
                    return reject(diagnostics,
                                  QStringLiteral("Altium SchLib 组件 %1 的实现参数键无效: %2，已拒绝写入")
                                      .arg(component.name, it.key()));
                }
            }
        }
        for (auto it = component.sourceMetadata.cbegin(); it != component.sourceMetadata.cend(); ++it) {
            if (!validateParameterName(diagnostics, component, it.key(), QStringLiteral("源元数据")) ||
                !validateParameterValue(diagnostics, component, it.value(), QStringLiteral("源元数据值"))) {
                return false;
            }
        }
        for (const AltiumSchParameter& parameter : component.parameters) {
            if (!validateParameterName(diagnostics, component, parameter.name, QStringLiteral("参数"))) {
                return false;
            }
        }
        if (!owner.validateGeometry(component) || !owner.validatePartOwnership(component)) {
            return false;
        }
        if (component.partCount <= 0) {
            diagnostics.append(
                QStringLiteral("Altium SchLib 组件 %1 的 partCount 无效，已规范化为 1").arg(component.name));
        }
    }
    return true;
}

}  // namespace EasyKiConverter
