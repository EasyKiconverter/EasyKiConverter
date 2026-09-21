#pragma once

#include <QDateTime>
#include <QMap>
#include <QMetaType>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

/**
 * @brief 支持的目标 EDA 格式枚举
 * @details 用于指定导出文件的目标 EDA 工具格式。
 *          枚举值用于持久化存储和 UI 下拉索引，从 0 递增。
 */
enum class TargetEdaFormat {
    KiCad = 0, /**< KiCad 格式（默认） */
    Altium = 1, /**< Altium Designer 格式 */
    Xpedition = 2, /**< Xpedition ASCII 库格式 */
    Allegro = 3, /**< Allegro PCB 封装 Import Package */
    Pads = 4, /**< PADS Parts Library ASCII 符号、器件关联和 PCB Decal */
    Eagle = 5, /**< Eagle XML 组合库（符号、封装和器件关联） */
    Pcad = 6, /**< P-CAD ASCII 符号库、器件库和 PCB 库 */
    Cadstar = 7 /**< CADSTAR ASCII combined symbol and package library */
};

/**
 * @brief 导出选项配置
 * @details 用户在导出对话框中选择的导出选项，决定哪些类型的资源需要导出
 *
 * 使用示例:
 * @code
 * ExportOptions options;
 * options.exportSymbol = true;
 * options.exportFootprint = true;
 * options.outputPath = "/path/to/output";
 * @endcode
 */
struct ExportOptions {
    // 3D模型格式常量 (位掩码)
    static constexpr int MODEL_3D_FORMAT_NONE = 0;  // 无格式
    static constexpr int MODEL_3D_FORMAT_WRL = 1;  // WRL (bit 0)
    static constexpr int MODEL_3D_FORMAT_STEP = 2;  // STEP (bit 1)
    static constexpr int MODEL_3D_FORMAT_BOTH = 3;  // Both (WRL + STEP)
    static constexpr int MODEL_3D_PATH_RELATIVE = 0;  // Footprint model path uses ../<lib>.3dmodels/...
    static constexpr int MODEL_3D_PATH_ABSOLUTE = 1;  // Footprint model path uses an absolute filesystem path

    // 3D模型格式判断辅助方法
    constexpr bool needsModel3DWrl() const {
        return (exportModel3DFormat & MODEL_3D_FORMAT_WRL) != 0;
    }

    constexpr bool needsModel3DStep() const {
        return (exportModel3DFormat & MODEL_3D_FORMAT_STEP) != 0;
    }

    /**
     * @brief 判断当前目标是否必须嵌入 STEP 三维模型
     *
     * Altium PcbLib 不使用 KiCad 的外部 WRL/STEP 文件引用，三维模型必须
     * 以 STEP 形式嵌入封装库。因此即使调用方传入了 WRL-only 配置，也必须
     * 为 Altium 获取并嵌入 STEP。
     */
    constexpr bool needsEmbeddedModel3DStep() const {
        if (!exportModel3D || targetFormat == TargetEdaFormat::Xpedition || targetFormat == TargetEdaFormat::Pads ||
            targetFormat == TargetEdaFormat::Eagle || targetFormat == TargetEdaFormat::Pcad)
            return false;
        return needsModel3DStep() || targetFormat == TargetEdaFormat::Altium ||
               targetFormat == TargetEdaFormat::Allegro;
    }

    static constexpr int normalizePathMode(int mode) {
        return mode == MODEL_3D_PATH_ABSOLUTE ? MODEL_3D_PATH_ABSOLUTE : MODEL_3D_PATH_RELATIVE;
    }

    QString outputPath;  ///< 导出文件输出目录路径
    QString libName;  ///< 库名称（用于构建子目录路径）
    TargetEdaFormat targetFormat = TargetEdaFormat::KiCad;  ///< 目标 EDA 格式
    bool exportSymbol = true;  ///< 是否导出符号 (Symbol)
    bool exportFootprint = true;  ///< 是否导出封装 (Footprint)
    bool exportModel3D = false;  ///< 是否导出3D模型 (3D Model)
    int exportModel3DFormat = MODEL_3D_FORMAT_BOTH;  ///< 3D模型格式
    int exportModel3DPathMode = MODEL_3D_PATH_RELATIVE;  ///< 3D模型在封装中的路径模式
    bool exportPreviewImages = false;  ///< 是否导出预览图 (Preview Images)
    bool exportDatasheet = false;  ///< 是否导出数据手册 (Datasheet)
    bool overwriteExistingFiles = false;  ///< 是否覆盖已存在的文件
    bool retryMode = false;  ///< 重试模式：保留已有库内容，仅更新重试的组件
    bool weakNetworkSupport = false;  ///< 是否启用客户端弱网络适配
    bool updateMode = false;  ///< 更新模式：仅导出缺失或已更改的文件
    bool debugMode = false;  ///< 调试模式：输出详细调试信息
    bool exportSymbolDescription = true;  ///< 是否导出符号库描述 (ki_description)
    bool exportFootprintDescription = true;  ///< 是否导出封装库描述 (descr)
    QString symbolLibraryDescription;  ///< 用户输入的符号库描述文本
    QString footprintLibraryDescription;  ///< 用户输入的封装库描述文本
    QString footprintLibraryKeywords;  ///< 用户输入的封装库关键词文本
};

/**
 * @brief 单个导出项的状态信息
 * @details 跟踪单个元器件的导出状态，包括进度、错误信息、文件路径等
 *
 * 状态流转: Pending -> InProgress -> Success/Failed/Skipped
 *
 * 使用示例:
 * @code
 * ExportItemStatus status;
 * status.status = ExportItemStatus::Status::InProgress;
 * status.startTime = QDateTime::currentDateTime();
 * @endcode
 */
struct ExportItemStatus {
    /** @brief 导出项状态枚举 */
    enum class Status {
        Pending,  ///< 等待中 - 尚未开始处理
        InProgress,  ///< 处理中 - 正在导出
        Success,  ///< 成功 - 导出完成且成功
        Failed,  ///< 失败 - 导出过程中发生错误
        Skipped  ///< 跳过 - 因条件不满足而跳过（如文件已存在且未启用覆盖）
    };

    Status status = Status::Pending;  ///< 当前状态
    QString errorMessage;  ///< 错误信息（当status为Failed时有效）
    QStringList diagnostics;  ///< 非致命输入诊断（不阻断导出）
    QString filePath;  ///< 导出文件路径（当status为Success时有效）
    qint64 bytesProcessed = 0;  ///< 已处理的字节数（用于大文件导出进度）
    qint64 totalBytes = 0;  ///< 总字节数（用于计算进度百分比）
    QDateTime startTime;  ///< 开始时间
    QDateTime endTime;  ///< 结束时间
    int retryCount = 0;  ///< 已重试次数

    /**
     * @brief 判断导出项是否已完成（无论成功或失败）
     * @return true 表示已完成（Success/Failed/Skipped之一）
     */
    bool isComplete() const {
        return status == Status::Success || status == Status::Failed || status == Status::Skipped;
    }

    /** @brief 判断导出是否成功 */
    bool isSuccess() const {
        return status == Status::Success;
    }

    /**
     * @brief 计算导出耗时
     * @return 毫秒单位的导出耗时，startTime或endTime无效时返回0
     */
    qint64 durationMs() const {
        return startTime.isValid() && endTime.isValid() ? startTime.msecsTo(endTime) : 0;
    }

    /**
     * @brief 计算导出进度百分比
     * @return 0-100的整数百分比，totalBytes为0时返回0
     */
    int percentage() const {
        return totalBytes > 0 ? static_cast<int>((bytesProcessed * 100) / totalBytes) : 0;
    }
};

/**
 * @brief 单个导出类型的总体进度
 * @details 跟踪一种导出类型（如符号导出）的整体进度，包含该类型下所有元器件的状态
 *
 * 使用示例:
 * @code
 * ExportTypeProgress progress;
 * progress.typeName = "Symbol";
 * progress.totalCount = 100;
 * progress.completedCount = 50;
 * @endcode
 */
struct ExportTypeProgress {
    QString typeName;  ///< 导出类型名称（如 "Symbol", "Footprint", "Model3D"）
    int totalCount = 0;  ///< 需要导出的元器件总数
    int completedCount = 0;  ///< 已完成的元器件数量
    int successCount = 0;  ///< 成功导出的元器件数量
    int failedCount = 0;  ///< 导出失败的元器件数量
    int skippedCount = 0;  ///< 被跳过的元器件数量
    int inProgressCount = 0;  ///< 当前正在导出的元器件数量
    QMap<QString, ExportItemStatus> itemStatus;  ///< 所有元器件的导出状态映射
    QStringList diagnostics;  ///< 导出器产生的非致命诊断
    qint64 totalBytes = 0;  ///< 该类型所有文件的总字节数
    qint64 processedBytes = 0;  ///< 已处理的字节数
    qint64 totalTimeMs = 0;  ///< 该类型导出的总耗时（毫秒）

    /**
     * @brief 计算导出进度百分比
     * @return 0-100的整数百分比
     */
    int percentage() const {
        return totalCount > 0 ? (completedCount * 100 / totalCount) : 0;
    }

    /**
     * @brief 判断该类型的导出是否全部完成
     * @return true 表示所有元器件都已处理完成
     */
    bool isComplete() const {
        return completedCount >= totalCount;
    }
};

/**
 * @brief 预加载阶段进度
 * @details 跟踪元器件数据预加载阶段的进度，包括从网络获取元器件信息
 *
 * 预加载阶段发生在用户点击导出之前，用于获取并缓存元器件的可复用数据
 */
struct PreloadProgress {
    int totalCount = 0;  ///< 需要预加载的元器件总数
    int completedCount = 0;  ///< 已完成的预加载数量
    int successCount = 0;  ///< 预加载成功的元器件数量
    int failedCount = 0;  ///< 预加载失败的元器件数量
    int inProgressCount = 0;  ///< 当前正在预加载的元器件数量
    QString currentComponentId;  ///< 当前正在预加载的元器件ID
    QMap<QString, QString> failedComponents;  ///< 预加载失败的元器件ID及错误原因

    /**
     * @brief 计算预加载进度百分比
     * @return 0-100的整数百分比
     */
    int percentage() const {
        return totalCount > 0 ? (completedCount * 100 / totalCount) : 0;
    }

    /**
     * @brief 判断预加载是否全部完成
     * @return true 表示所有元器件都已处理
     */
    bool isComplete() const {
        return completedCount >= totalCount;
    }
};

/**
 * @brief 整体导出进度
 * @details 跟踪整个导出任务的整体进度，包含预加载进度和各导出类型的进度
 *
 * 使用示例:
 * @code
 * ExportOverallProgress progress;
 * progress.currentStage = ExportOverallProgress::Stage::Preloading;
 * progress.totalComponents = 100;
 * @endcode
 */
struct ExportOverallProgress {
    /** @brief 导出任务阶段枚举 */
    enum class Stage {
        Idle,  ///< 空闲 - 导出任务未开始
        Preloading,  ///< 预加载 - 正在预加载元器件数据
        Exporting,  ///< 导出 - 正在导出各类型文件
        Completed,  ///< 已完成 - 导出任务成功完成
        Cancelled,  ///< 已取消 - 用户取消了导出任务
        Failed  ///< 失败 - 导出过程中发生严重错误
    };

    Stage currentStage = Stage::Idle;  ///< 当前所处阶段
    int totalComponents = 0;  ///< 需要导出的元器件总数
    PreloadProgress preloadProgress;  ///< 预加载阶段进度
    QMap<QString, ExportTypeProgress> exportTypeProgress;  ///< 各导出类型的进度（key为类型名）
    QDateTime startTime;  ///< 导出任务开始时间
    QDateTime endTime;  ///< 导出任务结束时间

    /**
     * @brief 计算所有导出类型的成功总数
     * @return 成功导出的元器件总数
     */
    int totalSuccessCount() const {
        int count = 0;
        for (const auto& p : exportTypeProgress) {
            count += p.successCount;
        }
        return count;
    }

    /**
     * @brief 计算所有导出类型的失败总数
     * @return 导出失败的元器件总数
     */
    int totalFailedCount() const {
        int count = 0;
        for (const auto& p : exportTypeProgress) {
            count += p.failedCount;
        }
        return count;
    }

    /**
     * @brief 判断导出任务是否全部完成
     * @return true 表示导出任务已结束（完成/取消/失败之一）
     */
    bool isComplete() const {
        if (currentStage != Stage::Exporting && currentStage != Stage::Preloading) {
            return currentStage == Stage::Completed || currentStage == Stage::Cancelled ||
                   currentStage == Stage::Failed;
        }
        if (currentStage == Stage::Preloading) {
            return preloadProgress.isComplete();
        }
        for (const auto& p : exportTypeProgress) {
            if (!p.isComplete())
                return false;
        }
        return true;
    }

    /**
     * @brief 计算整体导出进度百分比
     * @return 0-100的整数百分比
     *
     * 计算逻辑:
     * - Idle: 0%
     * - Preloading: 返回预加载进度百分比
     * - Exporting: 返回所有导出类型的综合百分比
     * - Completed: 100%
     * - 其他: 0%
     */
    int overallPercentage() const {
        if (currentStage == Stage::Idle)
            return 0;
        if (currentStage == Stage::Preloading)
            return preloadProgress.percentage();
        if (currentStage == Stage::Exporting) {
            int total = 0, completed = 0;
            for (const auto& p : exportTypeProgress) {
                total += p.totalCount;
                completed += p.completedCount;
            }
            return total > 0 ? (completed * 100 / total) : 0;
        }
        if (currentStage == Stage::Completed)
            return 100;
        return 0;
    }
};

}  // namespace EasyKiConverter

Q_DECLARE_METATYPE(EasyKiConverter::ExportItemStatus)
Q_DECLARE_METATYPE(EasyKiConverter::ExportTypeProgress)
Q_DECLARE_METATYPE(EasyKiConverter::PreloadProgress)
Q_DECLARE_METATYPE(EasyKiConverter::ExportOverallProgress)
