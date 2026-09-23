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

void ComponentDataCache::setCacheDir(const QString& path) {
    QMutexLocker locker(&m_cacheMutex);
    QString error;
    if (!CacheSafety::ensureOwnedRoot(path, &error)) {
        qWarning() << "ComponentDataCache: refusing unowned cache directory" << error;
        return;
    }
    m_cacheDir = path;

    // 创建子目录结构
    QDir dir(path);
    QStringList subDirs = {QStringLiteral("symbols"),
                           QStringLiteral("footprints"),
                           QStringLiteral("3dmodels"),
                           QStringLiteral("datasheets"),
                           QStringLiteral("previews")};

    for (const QString& subDir : subDirs) {
        dir.mkpath(subDir);
    }
}

bool ComponentDataCache::has(const QString& componentId) const {
    QMutexLocker locker(&m_cacheMutex);
    return m_cache.contains(componentId);
}

QSharedPointer<ComponentData> ComponentDataCache::get(const QString& componentId) const {
    QMutexLocker locker(&m_cacheMutex);
    return m_cache.value(componentId, nullptr);
}

void ComponentDataCache::put(const QString& componentId, const QSharedPointer<ComponentData>& data) {
    {
        QMutexLocker locker(&m_cacheMutex);
        m_cache[componentId] = data;
    }
    emit cacheUpdated(componentId);
}

void ComponentDataCache::remove(const QString& componentId) {
    QMutexLocker locker(&m_cacheMutex);
    m_cache.remove(componentId);
}

void ComponentDataCache::clear() {
    {
        QMutexLocker locker(&m_cacheMutex);
        m_cache.clear();
    }
    emit cacheCleared();
}

QStringList ComponentDataCache::cachedComponentIds() const {
    QMutexLocker locker(&m_cacheMutex);
    return m_cache.keys();
}

int ComponentDataCache::size() const {
    QMutexLocker locker(&m_cacheMutex);
    return m_cache.size();
}

void ComponentDataCache::flushToDisk(const QString& componentId) {
    QMutexLocker locker(&m_cacheMutex);

    if (m_cacheDir.isEmpty()) {
        qWarning() << "ComponentDataCache: Cache dir not set";
        return;
    }

    QStringList idsToFlush;
    if (componentId.isEmpty()) {
        idsToFlush = m_cache.keys();
    } else if (m_cache.contains(componentId)) {
        idsToFlush.append(componentId);
    } else {
        return;
    }

    for (const QString& id : idsToFlush) {
        const auto& data = m_cache.value(id);
        if (!data)
            continue;

        // 构建元器件数据文件路径
        QString dataFilePath = m_cacheDir + QStringLiteral("/") + id + QStringLiteral(".json");

        QJsonObject json;
        json[QStringLiteral("componentId")] = id;

        // 序列化符号数据
        if (data->symbolData()) {
            QJsonObject symbolJson = SymbolDataSerializer::toJson(*data->symbolData());
            json[QStringLiteral("symbolData")] = symbolJson;
        }

        // 序列化封装数据
        if (data->footprintData()) {
            QJsonObject footprintJson = FootprintDataSerializer::toJson(*data->footprintData());
            json[QStringLiteral("footprintData")] = footprintJson;
        }

        // 写入JSON文件
        QFile file(dataFilePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(json).toJson());
            file.close();
        } else {
            qWarning() << "ComponentDataCache: Failed to write cache file" << dataFilePath;
        }
    }
}

bool ComponentDataCache::loadFromDisk(const QString& componentId) {
    if (m_cacheDir.isEmpty()) {
        return false;
    }

    QString dataFilePath = m_cacheDir + QStringLiteral("/") + componentId + QStringLiteral(".json");
    QFile file(dataFilePath);

    if (!file.exists()) {
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "ComponentDataCache: Failed to parse cache file" << error.errorString();
        return false;
    }

    // 反序列化为ComponentData
    QJsonObject json = doc.object();
    auto data = QSharedPointer<ComponentData>::create();

    if (json.contains(QStringLiteral("symbolData"))) {
        QJsonObject symbolJson = json[QStringLiteral("symbolData")].toObject();
        SymbolData symbolData;
        if (SymbolDataSerializer::fromJson(symbolData, symbolJson)) {
            data->setSymbolData(QSharedPointer<SymbolData>::create(symbolData));
        }
    }

    if (json.contains(QStringLiteral("footprintData"))) {
        QJsonObject footprintJson = json[QStringLiteral("footprintData")].toObject();
        FootprintData footprintData;
        if (FootprintDataSerializer::fromJson(footprintData, footprintJson)) {
            data->setFootprintData(QSharedPointer<FootprintData>::create(footprintData));
        }
    }

    put(componentId, data);
    return true;
}

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
