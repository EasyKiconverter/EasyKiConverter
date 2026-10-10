#ifndef CLIPRINTER_H
#define CLIPRINTER_H

#include <QString>

namespace EasyKiConverter {

/**
 * @brief CLI 输出工具
 *
 * 提供命令行模式下的输出功能。
 */
class CliPrinter {
public:
    /**
     * @brief 构造函数
     * @param quiet 是否为安静模式
     */
    explicit CliPrinter(bool quiet = false);

    /**
     * @brief 输出消息
     * @param message 消息内容
     */
    void print(const QString& message) const;

    /**
     * @brief 输出消息并换行
     * @param message 消息内容
     */
    void println(const QString& message) const;

    /**
     * @brief 输出错误消息
     * @param message 错误消息内容
     */
    void printError(const QString& message) const;

    /**
     * @brief 输出进度条
     * @param progress 进度百分比 (0-100)
     */
    void printProgressBar(int progress) const;

    /**
     * @brief 清除当前行
     */
    void clearLine() const;

    /**
     * @brief 是否为安静模式
     * @return 安静模式返回 true
     */
    bool isQuiet() const {
        return m_quiet;
    }

private:
    bool m_quiet;  ///< 标记是否抑制普通 CLI 输出。
};

}  // namespace EasyKiConverter

#endif  // CLIPRINTER_H
