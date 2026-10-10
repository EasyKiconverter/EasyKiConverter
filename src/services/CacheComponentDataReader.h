#ifndef CACHECOMPONENTDATAREADER_H
#define CACHECOMPONENTDATAREADER_H

#include "models/ComponentData.h"

#include <QJsonObject>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 将磁盘缓存元数据还原为组件数据对象。
 *
 * 该类只负责 JSON 字段映射和模型变换，不负责文件访问、缓存锁或缓存代次判断。
 */
class CacheComponentDataReader final {
public:
    /**
     * @brief 从元数据对象构造组件数据。
     * @param componentId 元件编号。
     * @param metadata 已解析并通过基础校验的元数据。
     * @return 构造成功的组件数据；三维变换字段无效时返回空指针。
     */
    static QSharedPointer<ComponentData> read(const QString& componentId, const QJsonObject& metadata);
};

}  // namespace EasyKiConverter

#endif  // CACHECOMPONENTDATAREADER_H
