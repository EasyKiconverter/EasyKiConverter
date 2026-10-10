#include "ComponentListPreviewUpdateBuffer.h"

namespace EasyKiConverter {

/**
 * @brief 在锁内保存一个组件的完整预览图列表。
 * @param componentId 元器件编号。
 * @param encodedImages 编码后的预览图列表。
 */
void ComponentListPreviewUpdateBuffer::addComplete(const QString& componentId, const QStringList& encodedImages) {
    QMutexLocker locker(&m_mutex);
    m_completeImages.insert(componentId, encodedImages);
}

/**
 * @brief 在锁内保存一个组件的单张预览图并覆盖同索引旧值。
 * @param componentId 元器件编号。
 * @param imageIndex 预览图索引。
 * @param encodedImage 编码后的预览图。
 */
void ComponentListPreviewUpdateBuffer::addIncremental(const QString& componentId,
                                                      int imageIndex,
                                                      const QString& encodedImage) {
    QMutexLocker locker(&m_mutex);
    m_incrementalImages[componentId][imageIndex] = encodedImage;
}

/**
 * @brief 在同一锁区间内转移两个映射，避免 UI 更新期间丢失后台回调。
 * @return 当前缓冲区的更新快照。
 */
ComponentListPreviewUpdateBuffer::Snapshot ComponentListPreviewUpdateBuffer::take() {
    QMutexLocker locker(&m_mutex);
    // 保存从两个更新映射转移出的快照。
    Snapshot snapshot;
    snapshot.completeImages = std::move(m_completeImages);
    snapshot.incrementalImages = std::move(m_incrementalImages);
    return snapshot;
}

/**
 * @brief 在锁内清空所有完整和增量预览图更新。
 */
void ComponentListPreviewUpdateBuffer::clear() {
    QMutexLocker locker(&m_mutex);
    m_completeImages.clear();
    m_incrementalImages.clear();
}

}  // namespace EasyKiConverter
