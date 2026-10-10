#pragma once

#include "models/AltiumSchComponent.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QStringList>

namespace EasyKiConverter {

/**
 * @brief 编码 SchLib 的嵌入图片 Storage 流。
 * @details 负责图片压缩、Storage 条目校验和流内条目拼接，不负责 OLE 存储区写入。
 */
class AltiumSchImageStorageEncoder {
public:
    /**
     * @brief 将组件中的有效嵌入图片编码为 Storage 流数据。
     * @param components 待读取嵌入图片的组件列表
     * @param imageNames 图片对象到 Storage 文件名的映射
     * @param diagnostics 输出非致命编码诊断
     * @return 可直接写入 Storage 流的二进制数据
     */
    static QByteArray encode(const QList<AltiumSchComponent>& components,
                             const QHash<const AltiumSchImage*, QString>& imageNames,
                             QStringList& diagnostics);
};

}  // namespace EasyKiConverter
