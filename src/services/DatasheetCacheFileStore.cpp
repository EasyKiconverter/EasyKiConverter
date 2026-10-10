#include "DatasheetCacheFileStore.h"

#include "CacheDataValidator.h"
#include "CacheMetadataStore.h"
#include "CacheSafety.h"
#include "ComponentCacheService.h"
#include "utils/logging/LogMacros.h"

#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>

namespace EasyKiConverter {

/**
 * @brief 保存文件存储器所属的缓存服务。
 * @param owner 所属的组件缓存服务。
 */
DatasheetCacheFileStore::DatasheetCacheFileStore(ComponentCacheService& owner) : m_owner(owner) {}

/**
 * @brief 从二级磁盘缓存读取数据手册。
 * @details 数据手册路径、读取、校验和无效文件清理均在磁盘写锁内完成。
 * @param owner 所属的组件缓存服务。
 * @param componentId 元器件编号。
 * @return 有效的数据手册内容，无效或缺失时返回空数据。
 */
QByteArray DatasheetCacheFileStore::load(const ComponentCacheService& owner, const QString& componentId) {
    QMutexLocker diskLocker(&owner.m_diskWriteMutex);
    const QString preferredFormat =
        CacheMetadataStore::read(owner.metadataPath(componentId)).value(QStringLiteral("datasheetFormat")).toString();
    const QString datasheetFilePath =
        owner.resolveDatasheetPath(componentId, preferredFormat, false);  // 数据手册缓存路径。
    if (!QFileInfo::exists(datasheetFilePath))
        return QByteArray();

    QFile file(datasheetFilePath);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();

    const QByteArray data = file.readAll();  // 数据手册原始字节。
    file.close();
    // 根据文件扩展名推断实际数据手册格式。
    const QString format = datasheetFilePath.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)
                               ? QStringLiteral("pdf")
                               : QStringLiteral("html");
    if (CacheDataValidator::isValidDatasheet(data, format))
        return data;

    QString trashError;  // 回收站操作失败时的诊断信息。
    if (CacheSafety::isOwnedComponentFile(owner.cacheDir(), datasheetFilePath))
        CacheSafety::moveToTrash(datasheetFilePath, &trashError);
    return QByteArray();
}

/**
 * @brief 校验并写入数据手册，同时清理另一种格式的旧文件。
 * @details 代次和 tombstone 校验、目录准备、格式切换以及原子替换保持在同一写锁边界内。
 * @param componentId 元器件编号。
 * @param data 待缓存的数据手册内容。
 * @param format 数据手册声明格式。
 * @param expectedGeneration 请求创建时捕获的缓存代次。
 */
void DatasheetCacheFileStore::save(const QString& componentId,
                                   const QByteArray& data,
                                   const QString& format,
                                   uint64_t expectedGeneration) {
    QString effectiveFormat = format.toLower();  // 规范化后的数据手册格式。
    if (effectiveFormat != QStringLiteral("pdf") && effectiveFormat != QStringLiteral("html"))
        return;
    if (effectiveFormat == QStringLiteral("pdf") && !data.startsWith("%PDF-"))
        effectiveFormat = QStringLiteral("html");
    if (!CacheDataValidator::isValidDatasheet(data, effectiveFormat))
        return;

    QMutexLocker diskLocker(&m_owner.m_diskWriteMutex);
    if (expectedGeneration != 0 &&
        (m_owner.m_cacheGeneration.load() != expectedGeneration || m_owner.isTombstoned(componentId)))
        return;

    QString actualPath;  // 当前格式对应的数据手册缓存路径。
    QString trashError;  // 回收站操作失败时的诊断信息。
    {
        QMutexLocker locker(&m_owner.m_mutex);
        if (m_owner.ensureComponentDir(componentId).isEmpty())
            return;
        actualPath = m_owner.resolveDatasheetPath(componentId, effectiveFormat, true);
        const QString alternatePath = actualPath.endsWith(QStringLiteral(".pdf"))  // 另一种格式的旧缓存路径。
                                          ? m_owner.resolveDatasheetPath(componentId, QStringLiteral("html"), true)
                                          : m_owner.resolveDatasheetPath(componentId, QStringLiteral("pdf"), true);
        if (alternatePath != actualPath && QFile::exists(alternatePath) &&
            CacheSafety::isOwnedComponentFile(m_owner.cacheDir(), alternatePath))
            CacheSafety::moveToTrash(alternatePath, &trashError);
    }

    if (CacheMetadataStore::writeAtomically(actualPath, data)) {
        LOG_DEBUG(LogModule::Core, "Saved datasheet to disk: {}", actualPath);
        m_owner.enforceDiskCacheLimit();
    } else {
        LOG_WARN(LogModule::Core, "Failed to write datasheet: {}", actualPath);
    }
    // 数据手册体积较大，只保留二级磁盘缓存，不写入一级内存缓存。
}

}  // namespace EasyKiConverter
