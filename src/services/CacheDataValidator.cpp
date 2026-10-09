#include "CacheDataValidator.h"

#include "core/kicad/Exporter3DModel.h"

#include <QImage>
#include <QJsonDocument>
#include <QJsonParseError>

namespace EasyKiConverter {

/**
 * @brief 校验 CAD 原始缓存是否为合法 JSON 对象。
 * @param data 待校验的 CAD JSON 数据。
 * @return 数据为合法 JSON 对象时返回 true。
 */
bool CacheDataValidator::isValidCadData(const QByteArray& data) {
    if (data.isEmpty()) {
        return false;
    }

    QJsonParseError parseError;  // CAD JSON 的解析错误信息。
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);  // 解析后的 CAD JSON 文档。
    return parseError.error == QJsonParseError::NoError && document.isObject();
}

/**
 * @brief 使用 Qt 图片解码器校验预览图内容。
 * @param data 待校验的预览图数据。
 * @return 数据能够被 Qt 解码为图像时返回 true。
 */
bool CacheDataValidator::isValidPreviewImage(const QByteArray& data) {
    if (data.isEmpty()) {
        return false;
    }

    QImage image;  // 用于验证预览图编码的临时图像对象。
    return image.loadFromData(data);
}

/**
 * @brief 根据声明格式校验 PDF 签名或 HTML 文档标记。
 * @param data 数据手册内容。
 * @param format 声明的数据手册格式。
 * @return 内容符合声明格式时返回 true。
 */
bool CacheDataValidator::isValidDatasheet(const QByteArray& data, const QString& format) {
    if (data.isEmpty()) {
        return false;
    }
    if (format.compare(QStringLiteral("pdf"), Qt::CaseInsensitive) == 0) {
        return data.startsWith("%PDF-");
    }

    const QByteArray normalized = data.toLower();  // 转为小写后用于匹配 HTML 标记。
    return normalized.contains("<html") || normalized.contains("<!doctype html") || normalized.contains("<body");
}

/**
 * @brief 按模型扩展名调用对应的几何内容校验器。
 * @param data 三维模型文件内容。
 * @param extension 三维模型文件扩展名。
 * @return 内容包含目标格式可用几何数据时返回 true。
 */
bool CacheDataValidator::isUsableModel3D(const QByteArray& data, const QString& extension) {
    const QString normalizedExtension = extension.toLower();  // 规范化后的模型文件扩展名。
    if (normalizedExtension == QStringLiteral("obj")) {
        return Exporter3DModel::hasUsableObjGeometry(data);
    }
    if (normalizedExtension == QStringLiteral("wrl")) {
        return Exporter3DModel::hasUsableWrlGeometry(data);
    }
    if (normalizedExtension == QStringLiteral("step")) {
        return Exporter3DModel::hasUsableStepData(data);
    }
    return false;
}

}  // namespace EasyKiConverter
