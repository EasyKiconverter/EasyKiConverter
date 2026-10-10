#ifndef DATASHEETDOWNLOADSERVICE_H
#define DATASHEETDOWNLOADSERVICE_H

#include "models/ComponentExportStatus.h"

#include <QAtomicInt>
#include <QByteArray>
#include <QString>

#include <cstdint>

namespace EasyKiConverter {

class ComponentCacheService;

/**
 * @brief 负责同步下载数据手册并协调缓存读写。
 *
 * 将缓存命中、网络请求、取消处理、诊断信息和有效数据落盘集中在一个
 * 独立协作者中，避免缓存服务同时承担缓存管理与下载流程。
 */
class DatasheetDownloadService final {
public:
    /**
     * @brief 创建绑定到指定缓存服务的数据手册下载器。
     * @param owner 所属缓存服务
     */
    explicit DatasheetDownloadService(ComponentCacheService& owner);

    /**
     * @brief 下载数据手册，优先复用有效的磁盘缓存。
     * @param componentId 元器件编号
     * @param datasheetUrl 数据手册 URL
     * @param format 输出格式
     * @param diag 网络诊断信息
     * @param cancelled 可选取消标志
     * @param weakNetwork 是否启用弱网络策略
     * @param expectedGeneration 请求创建时的缓存代次
     * @return 有效数据手册内容，失败或取消时为空
     */
    QByteArray download(const QString& componentId,
                        const QString& datasheetUrl,
                        QString* format,
                        ComponentExportStatus::NetworkDiagnostics* diag,
                        QAtomicInt* cancelled,
                        bool weakNetwork,
                        uint64_t expectedGeneration);

private:
    /**
     * @brief 保存所属缓存服务引用，不负责其生命周期。
     */
    ComponentCacheService& m_owner;
};

}  // namespace EasyKiConverter

#endif  // DATASHEETDOWNLOADSERVICE_H
