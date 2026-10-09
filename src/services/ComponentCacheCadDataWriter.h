#pragma once

#include "models/ComponentData.h"

#include <QByteArray>
#include <QString>

#include <cstdint>

namespace EasyKiConverter {

class ComponentCacheService;

/**
 * @brief 协调符号、封装和 CAD JSON 的二级缓存写入。
 * @details 统一处理数据校验、代次/tombstone 检查、原子文件写入、配额维护和 L1 同步。
 */
class ComponentCacheCadDataWriter final {
public:
    /**
     * @brief 创建绑定到元器件缓存服务的 CAD 数据写入器。
     * @param owner 所属缓存服务。
     */
    explicit ComponentCacheCadDataWriter(ComponentCacheService& owner);

    /**
     * @brief 写入符号 CAD 数据并同步 L1 缓存。
     * @param componentId 元器件编号。
     * @param data 符号 CAD 数据。
     * @param expectedGeneration 调用方捕获的缓存代次。
     */
    void writeSymbol(const QString& componentId, const QByteArray& data, uint64_t expectedGeneration);
    /**
     * @brief 写入封装 CAD 数据并同步 L1 缓存。
     * @param componentId 元器件编号。
     * @param data 封装 CAD 数据。
     * @param expectedGeneration 调用方捕获的缓存代次。
     */
    void writeFootprint(const QString& componentId, const QByteArray& data, uint64_t expectedGeneration);
    /**
     * @brief 写入原始 CAD JSON 数据。
     * @param componentId 元器件编号。
     * @param data CAD JSON 数据。
     * @param expectedGeneration 调用方捕获的缓存代次。
     */
    void writeCadJson(const QString& componentId, const QByteArray& data, uint64_t expectedGeneration);

private:
    enum class DataKind { Symbol, Footprint, CadJson };

    /**
     * @brief 执行三类 CAD 文件共用的校验、加锁和原子写入流程。
     * @param componentId 元器件编号。
     * @param data CAD 数据。
     * @param expectedGeneration 调用方捕获的缓存代次。
     * @param kind 数据类型。
     */
    void writeCadFile(const QString& componentId, const QByteArray& data, uint64_t expectedGeneration, DataKind kind);

    ComponentCacheService& m_owner;  ///< 所属缓存服务。
};

}  // namespace EasyKiConverter
