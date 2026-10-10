#ifndef WRITEWORKER_H
#define WRITEWORKER_H

#include "core/kicad/Exporter3DModel.h"
#include "core/kicad/ExporterFootprint.h"
#include "core/kicad/ExporterSymbol.h"
#include "models/ComponentExportStatus.h"

#include <QAtomicInt>
#include <QFutureWatcher>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QRunnable>

namespace EasyKiConverter {

class WriteWorkerDebugExporter;

/**
 * @brief 写入工作线程
 *
 * 负责将数据写入文件（磁盘I/O密集型任务）
 */
class WriteWorker : public QObject, public QRunnable {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param status 导出状态（使用 QSharedPointer 避免拷贝）
     * @param outputPath 输出路径
     * @param libName 库名称
     * @param exportSymbol 是否导出符号
     * @param exportFootprint 是否导出封装
     * @param exportModel3D 是否导出三维模型
     * @param exportPreviewImages 是否导出预览图
     * @param exportDatasheet 是否导出数据手册
     * @param debugMode 是否启用调试模式
     * @param tempDir 临时目录路径
     * @param parent Qt 对象父级，可为空
     */
    explicit WriteWorker(QSharedPointer<ComponentExportStatus> status,
                         const QString& outputPath,
                         const QString& libName,
                         bool exportSymbol,
                         bool exportFootprint,
                         bool exportModel3D,
                         bool exportPreviewImages,
                         bool exportDatasheet,
                         bool debugMode,
                         const QString& tempDir,
                         QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~WriteWorker() override;

    /**
     * @brief 执行写入任务
     */
    void run() override;

    /**
     * @brief 取消写入任务 (v3.0.5+)
     */
    void abort();

signals:
    /**
     * @brief 写入完成信号
     * @param status 导出状态（使用 QSharedPointer 避免拷贝）
     */
    void writeCompleted(QSharedPointer<ComponentExportStatus> status);

    /**
     * @brief 单项导出完成信号（用于实时进度更新）
     * @param componentId 元器件ID
     * @param itemType 导出项类型 (ExportItemType)
     * @param success 是否成功
     */
    void itemWriteCompleted(const QString& componentId, int itemType, bool success);

private:
    /**
     * @brief 允许调试数据协作者复用输出目录策略和路径状态。
     */
    friend class WriteWorkerDebugExporter;

    /**
     * @brief 写入符号文件
     * @param status 导出状态
         * @return bool 是否成功
     */
    bool writeSymbolFile(ComponentExportStatus& status);

    /**
     * @brief 写入封装文件
     * @param status 导出状态
         * @return bool 是否成功
     */
    bool writeFootprintFile(ComponentExportStatus& status);

    /**
     * @brief 写入3D模型文件
     * @param status 导出状态
         * @return bool 是否成功
     */
    bool write3DModelFile(ComponentExportStatus& status);

    /**
     * @brief 创建输出目录
     * @param path 路径
     * @return bool 是否成功
     */
    bool createOutputDirectory(const QString& path);

    /**
     * @brief 写入预览图文件
     * @param status 导出状态
     * @return bool 是否成功
     */
    bool writePreviewImageFile(ComponentExportStatus& status);

    /**
     * @brief 写入手册文件
     * @param status 导出状态
     * @return bool 是否成功
     */
    bool writeDatasheetFile(ComponentExportStatus& status);

    /**
     * @brief 导出调试数据
     * @param status 导出状态
     * @return bool 是否成功
     */
    bool exportDebugData(ComponentExportStatus& status);

private:
    QSharedPointer<ComponentExportStatus> m_status;  ///< 线程共享的导出状态。
    QString m_outputPath;  ///< 导出输出目录。
    QString m_libName;  ///< 目标库名称。
    bool m_exportSymbol;  ///< 是否导出符号库。
    bool m_exportFootprint;  ///< 是否导出封装库。
    bool m_exportModel3D;  ///< 是否导出三维模型。
    bool m_exportPreviewImages;  ///< 是否导出预览图。
    bool m_exportDatasheet;  ///< 是否导出数据手册。
    bool m_debugMode;  ///< 是否启用调试数据输出。
    QString m_tempDir;  ///< 导出事务使用的临时目录。

    ExporterSymbol m_symbolExporter;  ///< 符号文件导出器。
    ExporterFootprint m_footprintExporter;  ///< 封装文件导出器。
    Exporter3DModel m_model3DExporter;  ///< 三维模型导出器。
    QAtomicInt m_isAborted;  ///< 取消标志（v3.0.5+）。

    // 导出器互斥锁（确保线程安全）
    QMutex m_symbolMutex;  ///< 保护符号导出器状态的互斥量。
    QMutex m_footprintMutex;  ///< 保护封装导出器状态的互斥量。
    QMutex m_model3DMutex;  ///< 保护三维模型导出器状态的互斥量。
};

}  // namespace EasyKiConverter

#endif  // WRITEWORKER_H
