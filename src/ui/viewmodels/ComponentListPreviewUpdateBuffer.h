#pragma once

#include <QMap>
#include <QMutex>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

/**
 * @brief 线程安全地收集组件预览图的批量更新。
 * @details 将完整预览图列表和单张增量图片分开保存，供 UI 定时器在主线程统一取出。
 */
class ComponentListPreviewUpdateBuffer {
public:
    /**
     * @brief 待处理预览图快照。
     */
    struct Snapshot {
        QMap<QString, QStringList> completeImages;  ///< 保存完整预览图更新。
        QMap<QString, QMap<int, QString>> incrementalImages;  ///< 保存增量预览图更新。

        /**
         * @brief 判断快照是否不包含任何更新。
         * @return 没有完整或增量更新时返回 true。
         */
        bool isEmpty() const {
            return completeImages.isEmpty() && incrementalImages.isEmpty();
        }
    };

    /**
     * @brief 添加一个组件的完整预览图列表。
     * @param componentId 元器件编号。
     * @param encodedImages 编码后的预览图列表。
     */
    void addComplete(const QString& componentId, const QStringList& encodedImages);

    /**
     * @brief 添加一个组件的单张增量预览图。
     * @param componentId 元器件编号。
     * @param imageIndex 预览图索引。
     * @param encodedImage 编码后的预览图。
     */
    void addIncremental(const QString& componentId, int imageIndex, const QString& encodedImage);

    /**
     * @brief 原子取出并清空全部待处理更新。
     */
    Snapshot take();

    /**
     * @brief 丢弃全部待处理更新。
     */
    void clear();

private:
    /**
     * @brief 保护两个待处理映射的互斥锁。
     */
    mutable QMutex m_mutex;
    /**
     * @brief 等待整组替换的预览图列表。
     */
    QMap<QString, QStringList> m_completeImages;
    /**
     * @brief 等待按索引增量更新的预览图。
     */
    QMap<QString, QMap<int, QString>> m_incrementalImages;
};

}  // namespace EasyKiConverter
