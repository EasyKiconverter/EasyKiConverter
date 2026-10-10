#ifndef BASECONVERTER_H
#define BASECONVERTER_H

#include "services/export/ExportProgress.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

class CliContext;
class CliPrinter;
class ParallelExportService;

/**
 * @brief 转换器基类
 *
 * 所有 CLI 转换器的基类，提供通用的转换逻辑。
 */
class BaseConverter : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param context CLI 上下文
     * @param parent 父对象指针
     */
    explicit BaseConverter(CliContext* context, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~BaseConverter() override = default;

    /**
     * @brief 执行转换
     * @return 成功返回 true，失败返回 false
     */
    virtual bool execute() = 0;

    /**
     * @brief 获取错误信息
     * @return 错误信息字符串
     */
    QString errorMessage() const {
        return m_errorMessage;
    }

signals:
    /**
     * @brief 进度更新
     * @param progress 进度百分比
     * @param message 进度消息
     */
    void progressUpdated(int progress, const QString& message);

    /**
     * @brief 转换完成
     * @param success 是否成功
     * @param message 完成消息
     */
    void conversionFinished(bool success, const QString& message);

protected:
    /**
     * @brief 执行 CLI 共用的预加载和导出流程。
     * @param componentIds 待导出的元器件编号
     * @return 所有元器件导出成功返回 true，否则返回 false
     */
    bool runExport(const QStringList& componentIds);

    /**
     * @brief 获取 CLI 上下文
     * @return CLI 上下文指针
     */
    CliContext* context() const {
        return m_context;
    }

    /**
     * @brief 获取打印机
     * @return 打印机指针
     */
    const CliPrinter* printer() const {
        return m_printer;
    }

    /**
     * @brief 设置错误信息
     * @param message 错误信息
     */
    void setError(const QString& message) {
        m_errorMessage = message;
    }

    /**
     * @brief 输出消息
     * @param message 消息内容
     */
    void printMessage(const QString& message) const;

    /**
     * @brief 输出进度条
     * @param progress 进度百分比
     */
    void printProgressBar(int progress) const;

private slots:
    /**
     * @brief 输出预加载阶段的成功和失败数量。
     * @param successCount 成功预加载的元器件数量。
     * @param failedCount 预加载失败的元器件数量。
     */
    void onPreloadCompleted(int successCount, int failedCount);

    /**
     * @brief 根据 CLI 选项输出导出进度。
     * @param progress 当前导出进度。
     */
    void onProgressChanged(const ExportOverallProgress& progress);

    /**
     * @brief 保存导出完成统计并更新最终成功状态。
     * @param successCount 成功导出的元器件数量。
     * @param failedCount 导出失败的元器件数量。
     */
    void onExportCompleted(int successCount, int failedCount);

    /**
     * @brief 保存导出失败信息并终止成功状态。
     * @param error 导出失败信息。
     */
    void onExportFailed(const QString& error);

private:
    CliContext* m_context;  ///< 保存 CLI 共享上下文。
    CliPrinter* m_printer;  ///< 保存 CLI 输出打印器。
    QString m_errorMessage;  ///< 保存最近一次导出错误。
    bool m_exportSuccess{false};  ///< 标记最近一次导出是否全部成功。
    int m_successCount{0};  ///< 保存成功导出的元器件数量。
    int m_failedCount{0};  ///< 保存导出失败的元器件数量。
    QStringList m_reportedDiagnostics;  ///< 保存已经输出过的诊断消息，避免重复显示。
};

}  // namespace EasyKiConverter

#endif  // BASECONVERTER_H
