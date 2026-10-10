#pragma once

#include "models/ComponentExportStatus.h"

#include <QAtomicInt>
#include <QByteArray>
#include <QString>

#include <cstdint>

namespace EasyKiConverter {

class ComponentCacheService;

/**
 * @brief 负责预览图缓存命中、网络下载和诊断回写。
 *
 * 预览图写入仍由 ComponentCachePreviewImageWriter 负责，本类只协调下载
 * 流程和缓存读取，避免 ComponentCacheService 同时承担网络与文件流程。
 */
class PreviewImageDownloadService final {
public:
    /**
     * @brief 创建绑定到指定缓存服务的预览图下载器。
     * @param owner 所属缓存服务，不由本对象管理其生命周期
     */
    explicit PreviewImageDownloadService(ComponentCacheService& owner);

    /**
     * @brief 下载指定预览图，优先返回有效的磁盘缓存。
     * @param componentId 元件编号
     * @param imageUrl 预览图地址
     * @param imageIndex 预览图序号
     * @param diag 网络诊断输出
     * @param cancelled 可选取消标志
     * @param weakNetwork 是否启用弱网络策略
     * @param expectedGeneration 请求创建时捕获的缓存代次
     * @return 有效图片数据，失败或取消时为空
     */
    QByteArray download(const QString& componentId,
                        const QString& imageUrl,
                        int imageIndex,
                        ComponentExportStatus::NetworkDiagnostics* diag,
                        QAtomicInt* cancelled,
                        bool weakNetwork,
                        uint64_t expectedGeneration);

private:
    /**
     * @brief 保存缓存服务引用，不负责其生命周期。
     */
    ComponentCacheService& m_owner;
};

}  // namespace EasyKiConverter
