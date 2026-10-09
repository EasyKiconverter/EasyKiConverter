#ifndef MEMORYMONITOR_H
#define MEMORYMONITOR_H

#include <qglobal.h>

namespace EasyKiConverter {

/**
 * @brief 跨平台内存监控工具类
 *
 * 提供进程内存使用情况的查询功能，支持 Linux、Windows 和 macOS。
 * 所有方法均为静态方法，无需实例化。
 */
class MemoryMonitor {
public:
    MemoryMonitor() = delete;
    ~MemoryMonitor() = delete;
    MemoryMonitor(const MemoryMonitor&) = delete;
    /**
     * @brief 禁止复制赋值。
     * @param other 不使用的复制源对象。
     * @return 不返回，因为该操作被删除。
     */
    MemoryMonitor& operator=(const MemoryMonitor& other) = delete;

    /**
     * @brief 获取当前进程内存使用量（字节）
     * @return qint64 内存使用量，失败返回 0
     */
    static qint64 getCurrentUsage();

    /**
     * @brief 获取峰值内存使用量（字节）
     * @return qint64 峰值内存使用量
     */
    static qint64 getPeakUsage();

    /**
     * @brief 更新峰值内存使用量
     */
    static void updatePeak();

    /**
     * @brief 重置峰值内存使用量
     */
    static void resetPeak();

private:
    static qint64 s_peakUsage;  ///< 进程历史峰值内存使用量（字节）。
};

}  // namespace EasyKiConverter

#endif  // MEMORYMONITOR_H
