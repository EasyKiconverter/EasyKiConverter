#ifndef COMPONENTEXPORTSTATUS_H
#define COMPONENTEXPORTSTATUS_H

#include "models/ComponentData.h"
#include "models/FootprintData.h"
#include "models/Model3DData.h"
#include "models/SymbolData.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QSharedPointer>
#include <QString>

namespace EasyKiConverter {

struct MemorySnapshot {
    qint64 rawDataSize = 0;  ///< 原始数据占用字节数。
    qint64 parsedModelSize = 0;  ///< 解析后模型占用的估算字节数。
    qint64 totalSize = 0;  ///< 原始数据与解析模型的总估算字节数。
};

/**
 * @brief 导出项类型枚举
 */
enum class ExportItemType { Symbol = 0, Footprint = 1, Model3D = 2, PreviewImage = 3, Datasheet = 4 };

/**
 * @brief 元件导出状态
 *
 * 跟踪元件在流水线各阶段的状态
 */
struct ComponentExportStatus {
    QString componentId;  ///< 元器件编号。

    // 线程安全：保护可变成员的互斥锁
    // 注意：此互斥锁仅用于保护 ComponentExportStatus 自身的可变成员
    // 不应替代流水线各阶段之间的同步机制
    mutable QMutex m_mutex;  ///< 保护状态成员的互斥锁。

    // 原始数据
    QByteArray componentInfoRaw;  ///< 元器件基础信息原始数据。
    QByteArray cinfoJsonRaw;  ///< 元器件 JSON 原始数据。
    QByteArray cadDataRaw;  ///< CAD 数据原始文本。
    QByteArray cadJsonRaw;  ///< CAD JSON 原始数据。
    QByteArray advJsonRaw;  ///< 高级属性 JSON 原始数据。
    QByteArray model3DObjRaw;  ///< OBJ 模型原始数据。
    QByteArray model3DStepRaw;  ///< STEP 模型原始数据。

    // 预览图和手册数据
    QList<QByteArray> previewImageDataList;  ///< 预览图数据列表。
    QByteArray datasheetData;  ///< 数据手册原始数据。

    // 解析后的数据
    QSharedPointer<ComponentData> componentData;  ///< 解析后的元器件数据。
    QSharedPointer<SymbolData> symbolData;  ///< 解析后的符号数据。
    QSharedPointer<FootprintData> footprintData;  ///< 解析后的封装数据。
    QSharedPointer<Model3DData> model3DData;  ///< 解析后的三维模型数据。

    // 状态标志
    bool fetchSuccess = false;  ///< 获取阶段是否成功。
    QString fetchMessage;  ///< 获取阶段错误或状态信息。
    qint64 fetchDurationMs = 0;  ///< 获取阶段耗时毫秒数。

    bool processSuccess = false;  ///< 解析处理阶段是否成功。
    QString processMessage;  ///< 处理阶段错误或状态信息。
    qint64 processDurationMs = 0;  ///< 处理阶段耗时毫秒数。

    bool writeSuccess = false;  ///< 写入阶段是否成功。
    QString writeMessage;  ///< 写入阶段错误或状态信息。
    qint64 writeDurationMs = 0;  ///< 写入阶段耗时毫秒数。

    // 取消标志 (v3.0.5+)
    bool isCancelled = false;  ///< 是否被用户或流水线取消。

    // 详细成果标志位 (v3.0.4)
    bool symbolWritten = false;  ///< 符号是否已写出。
    bool footprintWritten = false;  ///< 封装是否已写出。
    bool model3DWritten = false;  ///< 三维模型是否已写出。
    bool previewImageWritten = false;  ///< 预览图是否已写出。
    bool datasheetWritten = false;  ///< 数据手册是否已写出。

    QStringList debugLog;  ///< 调试日志条目。
    bool need3DModel = false;  ///< 是否需要导出三维模型。
    bool fetch3DOnly = false;  ///< 是否只获取三维数据并复用预加载的符号、封装。
    bool needPreviewImages = false;  ///< 是否需要获取预览图。
    bool needDatasheet = false;  ///< 是否需要获取数据手册。

    // 缓存文件路径（用于直接拷贝，避免大文件经过内存）
    // 当使用缓存导出3D模型时，这些路径指向缓存中的文件
    QString cachedModel3DWrlPath;  ///< 缓存的 WRL 文件路径，可直接拷贝。
    QString cachedModel3DStepPath;  ///< 缓存的 STEP 文件路径，可直接拷贝。

    struct NetworkDiagnostics {
        QString url;  ///< 请求 URL。
        int statusCode = 0;  ///< HTTP 状态码。
        QString errorString;  ///< 请求错误文本。
        QString responseContentType;  ///< 错误响应的内容类型。
        QString retryAfter;  ///< 服务端建议的重试等待时间。
        QString rateLimitRemaining;  ///< 服务端返回的剩余请求数提示。
        QString rateLimitReset;  ///< 服务端返回的限流重置时间提示。
        QString responseSummary;  ///< 限长后的文本响应摘要。
        int retryCount = 0;  ///< 已重试次数。
        qint64 latencyMs = 0;  ///< 请求延迟毫秒数。
        bool wasRateLimited = false;  ///< 是否被明确识别为限流响应。
        bool hasRateLimitHint = false;  ///< 是否出现限流相关响应头。
    };

    QList<NetworkDiagnostics> networkDiagnostics;  ///< 网络请求诊断列表。

    /**
     * @brief 判断获取、解析和写入三个阶段是否全部成功。
     * @return 三个阶段均成功时返回 true。
     */
    bool isCompleteSuccess() const {
        return fetchSuccess && processSuccess && writeSuccess;
    }

    /**
     * @brief 返回第一个失败的流水线阶段名称。
     * @return 阶段名称；全部成功时返回空字符串。
     */
    QString getFailedStage() const {
        if (!fetchSuccess)
            return "Fetch";
        if (!processSuccess)
            return "Process";
        if (!writeSuccess)
            return "Write";
        return "";
    }

    /**
     * @brief 返回第一个失败阶段的错误原因。
     * @return 第一条失败消息；全部成功时返回空字符串。
     */
    QString getFailureReason() const {
        if (!fetchSuccess)
            return fetchMessage;
        if (!processSuccess)
            return processMessage;
        if (!writeSuccess)
            return writeMessage;
        return "";
    }

    /**
     * @brief 汇总获取、解析和写入阶段耗时。
     * @return 三个阶段耗时之和，单位为毫秒。
     */
    qint64 getTotalDurationMs() const {
        return fetchDurationMs + processDurationMs + writeDurationMs;
    }

    /**
     * @brief 在线程安全的条件下追加调试日志。
     * @param message 待追加的日志消息。
     */
    void addDebugLog(const QString& message) {
        QMutexLocker locker(&m_mutex);
        debugLog.append(message);
    }

    // 内存管理增强 (v3.0.5)

    /**
     * @brief 清理中间数据。
     * @param log 是否保留清理日志行为。
     * @details model3DStepRaw 会保留给 WriteWorker，另由 clearStepData() 清理。
     */
    void clearIntermediateData(bool log = true) {
        // 注意：由于 mutex 可能被并发访问，不在这里锁定
        // clearIntermediateData 通常在 ProcessWorker 中调用，
        // 而 QtConcurrent 任务还在运行并可能调用 addDebugLog()
        qint64 freed = 0;  // 已释放的中间数据容量。

        auto clearAndCount = [&](QByteArray& ba) {
            qint64 size = ba.capacity();  // 当前字节数组的容量。
            ba.clear();
            ba.squeeze();
            return size;
        };

        freed += clearAndCount(componentInfoRaw);
        freed += clearAndCount(cinfoJsonRaw);
        freed += clearAndCount(cadDataRaw);
        freed += clearAndCount(cadJsonRaw);
        freed += clearAndCount(advJsonRaw);

        // model3DObjRaw 已经被解析到 Model3DData 中（作为 QString），可以清理
        freed += clearAndCount(model3DObjRaw);

        if (log && freed > 0) {
            // qDebug() << "Freed" << freed << "bytes of intermediate data for" << componentId;
        }
    }

    /**
     * @brief 清理 STEP 数据。
     */
    void clearStepData() {
        QMutexLocker locker(&m_mutex);
        model3DStepRaw.clear();
        model3DStepRaw.squeeze();
    }

    /**
     * @brief 获取内存快照
     * @return MemorySnapshot 内存使用估算
     * @note 此方法返回的是估算值，实际内存使用可能因 Qt 内部优化而不同
     */
    MemorySnapshot getMemorySnapshot() const {
        MemorySnapshot snapshot;  // 当前内存占用快照。
        snapshot.rawDataSize = componentInfoRaw.capacity() + cinfoJsonRaw.capacity() + cadDataRaw.capacity() +
                               cadJsonRaw.capacity() + advJsonRaw.capacity() + model3DObjRaw.capacity() +
                               model3DStepRaw.capacity();

        // 估算解析后的对象大小
        if (model3DData) {
            // QString 大小 = 字符数 * 2 + Qt 元数据开销（估算32字节）
            snapshot.parsedModelSize += model3DData->rawObj().length() * 2 + sizeof(Model3DData) + 32;
        }

        if (symbolData) {
            snapshot.parsedModelSize += sizeof(SymbolData);
            snapshot.parsedModelSize += symbolData->pins().size() * sizeof(SymbolPin);
            snapshot.parsedModelSize += symbolData->rectangles().size() * sizeof(SymbolRectangle);
            snapshot.parsedModelSize += symbolData->circles().size() * sizeof(SymbolCircle);
            snapshot.parsedModelSize += symbolData->arcs().size() * sizeof(SymbolArc);
            snapshot.parsedModelSize += symbolData->polylines().size() * sizeof(SymbolPolyline);
        }

        if (footprintData) {
            snapshot.parsedModelSize += sizeof(FootprintData);
            snapshot.parsedModelSize += footprintData->pads().size() * sizeof(FootprintPad);
            snapshot.parsedModelSize += footprintData->tracks().size() * sizeof(FootprintTrack);
            snapshot.parsedModelSize += footprintData->circles().size() * sizeof(FootprintCircle);
            snapshot.parsedModelSize += footprintData->arcs().size() * sizeof(FootprintArc);
            snapshot.parsedModelSize += footprintData->texts().size() * sizeof(FootprintText);
        }

        snapshot.totalSize = snapshot.rawDataSize + snapshot.parsedModelSize;
        return snapshot;
    }
};

/**
 * @brief RAII 辅助类：确保状态对象在析构时（或显式调用时）清理数据
 * 
 * 使用场景：
 * - 在 Worker 析构时自动清理
 * - 在异常处理中确保清理
 * 
 * 注意：此 RAII 类会在析构时清理数据，不要在数据仍需使用时析构此对象
 */
class ScopedDataClearer {
public:
    /**
     * @brief 创建数据清理辅助对象。
     * @param status 待清理状态的共享引用。
     */
    explicit ScopedDataClearer(QSharedPointer<ComponentExportStatus> status) : m_status(status) {}

    /**
     * @brief 析构时清理仍然存活的状态对象中间数据。
     */
    ~ScopedDataClearer() {
        if (auto status = m_status.toStrongRef()) {  // 尝试获取强引用
            status->clearIntermediateData(false);
        }
    }

    /**
     * @brief 释放清理责任并保留状态对象中的数据。
     */
    void release() {
        m_status.clear();
    }

private:
    QWeakPointer<ComponentExportStatus> m_status;  ///< 使用弱引用，避免延长生命周期。
};

struct ExportStatistics {
    int total = 0;  ///< 导出项总数。
    int success = 0;  ///< 成功导出项数量。
    int failed = 0;  ///< 失败导出项数量。

    // 详细分项统计 (v3.0.4)
    int successSymbol = 0;  ///< 成功导出的符号数量。
    int successFootprint = 0;  ///< 成功导出的封装数量。
    int successModel3D = 0;  ///< 成功导出的三维模型数量。
    int successPreviewImage = 0;  ///< 成功导出的预览图数量。
    int successDatasheet = 0;  ///< 成功导出的数据手册数量。

    QMap<QString, int> failureReasons;  ///< 失败原因计数。
    QMap<QString, int> stageFailures;  ///< 阶段失败计数。

    qint64 totalDurationMs = 0;  ///< 总导出耗时。
    qint64 avgFetchTimeMs = 0;  ///< 平均获取耗时。
    qint64 avgProcessTimeMs = 0;  ///< 平均处理耗时。
    qint64 avgWriteTimeMs = 0;  ///< 平均写入耗时。

    int totalNetworkRequests = 0;  ///< 网络请求总数。
    int totalRetries = 0;  ///< 重试总次数。
    qint64 avgNetworkLatencyMs = 0;  ///< 平均网络延迟。
    int rateLimitHitCount = 0;  ///< 命中限流次数。
    QMap<int, int> statusCodeDistribution;  ///< HTTP 状态码分布。

    // 内存统计 (v3.0.5)
    qint64 peakMemoryUsage = 0;  ///< 峰值内存占用。

    /**
     * @brief 计算所有导出项的成功百分比。
     * @return 成功率，范围为 0 到 100。
     */
    double getSuccessRate() const {
        return total > 0 ? (success * 100.0 / total) : 0.0;
    }

    /**
     * @brief 生成统计摘要文本供日志和界面展示。
     * @return 格式化后的统计摘要。
     */
    QString getSummary() const {
        return QString(
                   "Total: %1, Success: %2, Failed: %3, Symbol: %4, Footprint: %5, 3D: %6, Preview: %7, Datasheet: %8")
            .arg(total)
            .arg(success)
            .arg(failed)
            .arg(successSymbol)
            .arg(successFootprint)
            .arg(successModel3D)
            .arg(successPreviewImage)
            .arg(successDatasheet);
    }
};

}  // namespace EasyKiConverter

#endif  // COMPONENTEXPORTSTATUS_H
