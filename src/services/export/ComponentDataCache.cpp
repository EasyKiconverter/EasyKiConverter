#include "ComponentDataCache.h"

#include "../../models/ComponentData.h"
#include "../../models/FootprintDataSerializer.h"
#include "../../models/SymbolDataSerializer.h"
#include "../CacheSafety.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace EasyKiConverter {

ComponentDataCache::ComponentDataCache(QObject* parent) : QObject(parent) {}

ComponentDataCache::~ComponentDataCache() {
    // 析构前同步到磁盘
    flushToDisk();
}

/**
 * @brief 设置并初始化缓存目录。
 * @param path 缓存根目录路径。
 */
void ComponentDataCache::setCacheDir(const QString& path) {
    QMutexLocker locker(&m_cacheMutex);  ///< 保护缓存目录和内存缓存状态的锁。
    QString error;  ///< 缓存目录所有权校验失败原因。
    if (!CacheSafety::ensureOwnedRoot(path, &error)) {
        qWarning() << "ComponentDataCache: refusing unowned cache directory" << error;
        return;
    }
    m_cacheDir = path;

    // 创建子目录结构
    QDir dir(path);  ///< 缓存根目录对象。
    // 缓存目录需要初始化的子目录名称。
    QStringList subDirs = {QStringLiteral("symbols"),
                           QStringLiteral("footprints"),
                           QStringLiteral("3dmodels"),
                           QStringLiteral("datasheets"),
                           QStringLiteral("previews")};  ///< 需要初始化的缓存子目录名称。

    for (const QString& subDir : subDirs) {  ///< 当前待创建的缓存子目录。
        dir.mkpath(subDir);
    }
}

/**
 * @brief 检查元器件数据是否存在于内存缓存中。
 * @param componentId 元器件编号。
 * @return 缓存中存在该元器件时返回 true。
 */
bool ComponentDataCache::has(const QString& componentId) const {
    QMutexLocker locker(&m_cacheMutex);  ///< 保护内存缓存的锁。
    return m_cache.contains(componentId);
}

/**
 * @brief 获取内存缓存中的元器件数据。
 * @param componentId 元器件编号。
 * @return 对应的元器件数据，不存在时返回空指针。
 */
QSharedPointer<ComponentData> ComponentDataCache::get(const QString& componentId) const {
    QMutexLocker locker(&m_cacheMutex);  ///< 保护内存缓存的锁。
    return m_cache.value(componentId, nullptr);
}

/**
 * @brief 写入一个元器件的内存缓存并发送更新信号。
 * @param componentId 元器件编号。
 * @param data 待缓存的元器件数据。
 */
void ComponentDataCache::put(const QString& componentId, const QSharedPointer<ComponentData>& data) {
    {
        QMutexLocker locker(&m_cacheMutex);  ///< 保护内存缓存的锁。
        m_cache[componentId] = data;
    }
    emit cacheUpdated(componentId);
}

/**
 * @brief 移除一个元器件的内存缓存。
 * @param componentId 元器件编号。
 */
void ComponentDataCache::remove(const QString& componentId) {
    QMutexLocker locker(&m_cacheMutex);  ///< 保护内存缓存的锁。
    m_cache.remove(componentId);
}

/**
 * @brief 清空全部内存缓存并发送清除信号。
 */
void ComponentDataCache::clear() {
    {
        QMutexLocker locker(&m_cacheMutex);  ///< 保护内存缓存的锁。
        m_cache.clear();
    }
    emit cacheCleared();
}

/**
 * @brief 返回当前内存缓存中的元器件编号。
 * @return 当前内存缓存中的元器件编号列表。
 */
QStringList ComponentDataCache::cachedComponentIds() const {
    QMutexLocker locker(&m_cacheMutex);  ///< 保护内存缓存的锁。
    return m_cache.keys();
}

/**
 * @brief 返回当前内存缓存中的元器件数量。
 * @return 当前内存缓存中的元器件数量。
 */
int ComponentDataCache::size() const {
    QMutexLocker locker(&m_cacheMutex);  ///< 保护内存缓存的锁。
    return m_cache.size();
}

/**
 * @brief 将内存缓存中的数据序列化到磁盘。
 * @param componentId 指定刷新的元器件编号；为空时刷新全部缓存。
 */
void ComponentDataCache::flushToDisk(const QString& componentId) {
    QMutexLocker locker(&m_cacheMutex);  ///< 保护内存缓存和磁盘同步过程的锁。

    if (m_cacheDir.isEmpty()) {
        qWarning() << "ComponentDataCache: Cache dir not set";
        return;
    }

    QStringList idsToFlush;  ///< 本次需要写入磁盘的元器件编号。
    if (componentId.isEmpty()) {
        idsToFlush = m_cache.keys();
    } else if (m_cache.contains(componentId)) {
        idsToFlush.append(componentId);
    } else {
        return;
    }

    for (const QString& id : idsToFlush) {  ///< 当前待写入的元器件编号。
        const auto& data = m_cache.value(id);  ///< 当前待写入的元器件数据。
        if (!data)
            continue;

        // 构建元器件数据文件路径
        QString dataFilePath =
            m_cacheDir + QStringLiteral("/") + id + QStringLiteral(".json");  ///< 元器件缓存文件路径。

        QJsonObject json;  ///< 元器件缓存 JSON 根对象。
        json[QStringLiteral("componentId")] = id;

        // 序列化符号数据
        if (data->symbolData()) {
            QJsonObject symbolJson = SymbolDataSerializer::toJson(*data->symbolData());  ///< 序列化后的符号 JSON。
            json[QStringLiteral("symbolData")] = symbolJson;
        }

        // 序列化封装数据
        if (data->footprintData()) {
            QJsonObject footprintJson =
                FootprintDataSerializer::toJson(*data->footprintData());  ///< 序列化后的封装 JSON。
            json[QStringLiteral("footprintData")] = footprintJson;
        }

        // 写入JSON文件
        QFile file(dataFilePath);  ///< 当前元器件缓存输出文件。
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(json).toJson());
            file.close();
        } else {
            qWarning() << "ComponentDataCache: Failed to write cache file" << dataFilePath;
        }
    }
}

/**
 * @brief 从磁盘反序列化一个元器件缓存到内存。
 * @param componentId 元器件编号。
 * @return 读取并解析成功时返回 true。
 */
bool ComponentDataCache::loadFromDisk(const QString& componentId) {
    if (m_cacheDir.isEmpty()) {
        return false;
    }

    QString dataFilePath =
        m_cacheDir + QStringLiteral("/") + componentId + QStringLiteral(".json");  ///< 元器件缓存文件路径。
    QFile file(dataFilePath);  ///< 当前元器件缓存输入文件。

    if (!file.exists()) {
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray jsonData = file.readAll();  ///< 读取到的缓存 JSON 原始数据。
    file.close();

    QJsonParseError error;  ///< 缓存 JSON 解析错误信息。
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &error);  ///< 解析后的缓存 JSON 文档。
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "ComponentDataCache: Failed to parse cache file" << error.errorString();
        return false;
    }

    // 反序列化为ComponentData
    QJsonObject json = doc.object();  ///< 元器件缓存 JSON 根对象。
    auto data = QSharedPointer<ComponentData>::create();  ///< 反序列化得到的元器件数据。

    if (json.contains(QStringLiteral("symbolData"))) {
        QJsonObject symbolJson = json[QStringLiteral("symbolData")].toObject();  ///< 缓存中的符号 JSON。
        SymbolData symbolData;  ///< 反序列化得到的符号数据。
        if (SymbolDataSerializer::fromJson(symbolData, symbolJson)) {
            data->setSymbolData(QSharedPointer<SymbolData>::create(symbolData));
        }
    }

    if (json.contains(QStringLiteral("footprintData"))) {
        QJsonObject footprintJson = json[QStringLiteral("footprintData")].toObject();  ///< 缓存中的封装 JSON。
        FootprintData footprintData;  ///< 反序列化得到的封装数据。
        if (FootprintDataSerializer::fromJson(footprintData, footprintJson)) {
            data->setFootprintData(QSharedPointer<FootprintData>::create(footprintData));
        }
    }

    put(componentId, data);
    return true;
}

/**
 * @brief 拒绝清理无法验证所有权的旧磁盘缓存。
 * @param componentId 元器件编号。
 */
void ComponentDataCache::clearDiskCache(const QString& componentId) {
    if (m_cacheDir.isEmpty()) {
        return;
    }

    // 该旧缓存格式没有逐条所有权清单，无法证明目录中的文件全部由应用创建。
    // 因此宁可拒绝清理，也不递归删除或永久删除可能属于用户的数据。
    Q_UNUSED(componentId);
    qWarning() << "ComponentDataCache: refusing disk cleanup because entry ownership is unverifiable";
}

}  // namespace EasyKiConverter
