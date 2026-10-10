#ifndef CLICONTEXT_H
#define CLICONTEXT_H

#include "services/export/ExportProgress.h"
#include "utils/CommandLineParser.h"

#include <QString>

namespace EasyKiConverter {

class ComponentService;
class ParallelExportService;

/**
 * @brief CLI 上下文
 *
 * 保存 CLI 模式下的共享状态和服务实例。
 */
class CliContext {
public:
    /**
     * @brief 构造函数
     * @param parser 命令行解析器
     */
    explicit CliContext(const CommandLineParser& parser);

    /**
     * @brief 析构函数
     */
    ~CliContext();

    // 禁用拷贝
    CliContext(const CliContext&) = delete;
    /// 禁止通过赋值复制 CLI 上下文。
    CliContext& operator=(const CliContext&) = delete;

    /**
     * @brief 获取命令行解析器
     * @return 命令行解析器引用
     */
    const CommandLineParser& parser() const {
        return m_parser;
    }

    /**
     * @brief 获取元器件服务
     * @return 元器件服务指针
     */
    ComponentService* componentService() const {
        return m_componentService;
    }

    /**
     * @brief 获取导出服务
     * @return 导出服务指针
     */
    ParallelExportService* exportService() const {
        return m_exportService;
    }

    /**
     * @brief 创建导出选项
     * @return 导出选项配置
     */
    ExportOptions createExportOptions() const;

    /**
     * @brief 获取错误信息
     * @return 错误信息字符串
     */
    QString errorMessage() const {
        return m_errorMessage;
    }

    /**
     * @brief 设置错误信息
     * @param message 错误信息
     */
    void setErrorMessage(const QString& message) {
        m_errorMessage = message;
    }

private:
    /// 保存外部提供的命令行解析器引用。
    const CommandLineParser& m_parser;
    /// 管理元器件数据获取的服务。
    ComponentService* m_componentService{nullptr};
    /// 管理预加载和导出任务的服务。
    ParallelExportService* m_exportService{nullptr};
    QString m_errorMessage;  ///< 保存 CLI 执行过程中最近一次错误信息。
};

}  // namespace EasyKiConverter

#endif  // CLICONTEXT_H
