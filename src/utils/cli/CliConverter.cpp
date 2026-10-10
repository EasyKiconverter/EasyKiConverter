#include "CliConverter.h"

#include "BaseConverter.h"
#include "BatchConverter.h"
#include "BomConverter.h"
#include "CliContext.h"
#include "ComponentConverter.h"

#include <QCoreApplication>

namespace EasyKiConverter {

/**
 * @brief 创建 CLI 转换器。
 * @param parser 命令行参数解析器。
 * @param parent Qt 对象父级。
 */
CliConverter::CliConverter(const CommandLineParser& parser, QObject* parent) : QObject(parent) {
    m_context = new CliContext(parser);
}

/**
 * @brief 销毁 CLI 转换器及其上下文。
 */
CliConverter::~CliConverter() {
    if (m_converter) {
        delete m_converter;
    }
    if (m_context) {
        delete m_context;
    }
}

/**
 * @brief 执行当前 CLI 模式。
 * @return 转换成功时返回 true。
 */
bool CliConverter::execute() {
    CommandLineParser::CliMode mode = m_context->parser().cliMode();  // 当前 CLI 模式。

    m_converter = createConverter(mode);
    if (!m_converter) {
        m_context->setErrorMessage(QCoreApplication::translate("CliConverter", "未知的 CLI 模式"));
        return false;
    }

    return m_converter->execute();
}

/**
 * @brief 获取最近一次转换错误信息。
 * @return 错误信息文本。
 */
QString CliConverter::errorMessage() const {
    if (m_converter) {
        return m_converter->errorMessage();
    }
    if (m_context) {
        return m_context->errorMessage();
    }
    return QCoreApplication::translate("CliConverter", "未知错误");
}

/**
 * @brief 根据 CLI 模式创建具体转换器。
 * @param mode CLI 模式。
 * @return 对应转换器；模式未知时返回 nullptr。
 */
BaseConverter* CliConverter::createConverter(CommandLineParser::CliMode mode) {
    /**
     * @brief 按 CLI 模式选择具体转换器。
     */
    switch (mode) {
        case CommandLineParser::CliMode::ConvertBom:
            return new BomConverter(m_context, this);
        case CommandLineParser::CliMode::ConvertComponent:
            return new ComponentConverter(m_context, this);
        case CommandLineParser::CliMode::ConvertBatch:
            return new BatchConverter(m_context, this);
        default:
            return nullptr;
    }
}

}  // namespace EasyKiConverter
