#pragma once

/**
 * @file ImporterRegistry.h
 * @brief 格式导入器描述和检测注册表。
 */

#include "TextParsers.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

namespace EasyKiConverter::Parser {

/** @brief 一个格式导入器的能力描述和保守探测函数。 */
struct ImporterDescriptor {
    QString id;
    QString displayName;
    DetectedFormat format = DetectedFormat::Unknown;
    QStringList extensions;
    std::function<bool(const QString&, const QByteArray&)> probe;
};

/**
 * @brief 管理格式导入器注册和输入格式探测。
 * @details 注册表只负责选择解析器，不负责持有格式专用模型或执行转换。
 */
class ImporterRegistry {
public:
    /** @brief 注册一个唯一 ID 的导入器描述。 */
    bool registerImporter(const ImporterDescriptor& descriptor);

    /**
     * @brief 注册当前已实现解析器的内置描述。
     * @details 方法可重复调用；只会补充缺失的内置 ID，不会覆盖调用方注册的描述。
     * @return 所有内置描述均注册成功或已存在时返回 true。
     */
    bool registerBuiltInImporters();

    /** @brief 清空当前注册的导入器。 */
    void clear();

    /** @brief 返回全部注册描述，顺序与注册顺序一致。 */
    const QList<ImporterDescriptor>& importers() const;

    /**
     * @brief 按文件名和内容选择第一个可处理的导入器。
     * @return 找到时返回描述指针，否则返回空指针。
     */
    const ImporterDescriptor* detect(const QString& fileName, const QByteArray& content) const;

private:
    QList<ImporterDescriptor> m_importers;
};

}  // namespace EasyKiConverter::Parser
