#include "MemoryMonitor.h"

#include <QByteArray>
#include <QDebug>
#include <QFile>

#if defined(Q_OS_LINUX)
#    include <unistd.h>
#elif defined(Q_OS_WIN) || defined(_WIN32)
// clang-format off
// Order matters: windows.h must be included before other Windows API headers
#    if defined(__MINGW32__) || defined(__MINGW64__)
#        include <qt_windows.h>

#        include <psapi.h>
#        pragma comment(lib, "psapi")
#    else
#        include <windows.h>
#        include <psapi.h>
#        pragma comment(lib, "psapi")
#    endif
// clang-format on
#elif defined(Q_OS_MAC)
#    include <mach/mach.h>
#    include <mach/task_info.h>
#endif

namespace EasyKiConverter {

qint64 MemoryMonitor::s_peakUsage = 0;  ///< 进程峰值内存使用量（字节）。

/**
 * @brief 获取当前进程内存使用量。
 * @return 当前使用量（字节）；平台不支持或读取失败时返回 0。
 */
qint64 MemoryMonitor::getCurrentUsage() {
#if defined(Q_OS_LINUX)
    QFile file("/proc/self/status");  // Linux 进程状态文件。
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open /proc/self/status:" << file.errorString();
        return 0;
    }
    QByteArray content = file.readAll();  // 状态文件内容。
    file.close();

    QList<QByteArray> lines = content.split('\n');  // 状态文件行列表。
    for (const QByteArray& line : lines) {
        if (line.startsWith("VmRSS:")) {
            QByteArray value = line.mid(6).trimmed();  // VmRSS 数值文本。
            if (value.endsWith("kB") || value.endsWith("kb")) {
                value = value.left(value.length() - 2);
            } else if (value.endsWith("k")) {
                value = value.left(value.length() - 1);
            }
            value = value.trimmed();
            bool ok = false;  // 数值解析是否成功。
            qint64 rssKb = value.toLongLong(&ok);  // 常驻内存大小（KiB）。
            if (ok) {
                return rssKb * 1024;
            }
        }
    }
    return 0;

#elif defined(Q_OS_WIN) || defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc;  // Windows 进程内存计数器。
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<qint64>(pmc.WorkingSetSize);
    }
    return 0;

#elif defined(Q_OS_MAC)
    struct task_basic_info info;  // macOS 进程内存信息。
    mach_msg_type_number_t size = TASK_BASIC_INFO_COUNT;  // 信息结构大小。
    if (task_info(mach_task_self(), TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &size) == KERN_SUCCESS) {
        return static_cast<qint64>(info.resident_size);
    }
    return 0;

#else
    return 0;
#endif
}

/**
 * @brief 获取历史峰值内存使用量。
 * @return 峰值使用量（字节）。
 */
qint64 MemoryMonitor::getPeakUsage() {
    return s_peakUsage;
}

/**
 * @brief 使用当前内存使用量更新峰值。
 */
void MemoryMonitor::updatePeak() {
    qint64 currentMemory = getCurrentUsage();  // 当前内存使用量。
    if (currentMemory > s_peakUsage) {
        s_peakUsage = currentMemory;
    }
}

/**
 * @brief 重置历史峰值内存使用量。
 */
void MemoryMonitor::resetPeak() {
    s_peakUsage = 0;
}

}  // namespace EasyKiConverter
