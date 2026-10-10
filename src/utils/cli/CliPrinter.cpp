#include "CliPrinter.h"

#include <QTextStream>

namespace EasyKiConverter {

/**
 * @brief 创建 CLI 输出器。
 * @param quiet 是否抑制普通输出。
 */
CliPrinter::CliPrinter(bool quiet) : m_quiet(quiet) {}

/**
 * @brief 输出不追加换行的消息。
 * @param message 待输出消息。
 */
void CliPrinter::print(const QString& message) const {
    if (!m_quiet) {
        QTextStream(stdout) << message;
    }
}

/**
 * @brief 输出并追加换行的消息。
 * @param message 待输出消息。
 */
void CliPrinter::println(const QString& message) const {
    if (!m_quiet) {
        QTextStream(stdout) << message << Qt::endl;
    }
}

/**
 * @brief 输出错误消息。
 * @param message 错误消息。
 */
void CliPrinter::printError(const QString& message) const {
    QTextStream(stderr) << message << Qt::endl;
}

/**
 * @brief 输出进度条。
 * @param progress 当前进度百分比。
 */
void CliPrinter::printProgressBar(int progress) const {
    if (m_quiet) {
        return;
    }

    const int barWidth = 50;  // 进度条总宽度。
    int pos = barWidth * progress / 100;  // 已完成位置。

    QTextStream stream(stdout);
    stream << "\r[";
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) {
            stream << "=";
        } else if (i == pos) {
            stream << ">";
        } else {
            stream << " ";
        }
    }
    stream << "] " << progress << "%";
    stream.flush();
}

/**
 * @brief 清除当前控制台行。
 */
void CliPrinter::clearLine() const {
    if (!m_quiet) {
        QTextStream(stdout) << "\r\033[K";
    }
}

}  // namespace EasyKiConverter
