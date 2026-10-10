#include "BatchConverter.h"

#include "CliContext.h"
#include "FileReader.h"

#include <QCoreApplication>

namespace EasyKiConverter {

/**
 * @brief 创建批量转换器并绑定 CLI 上下文。
 * @param context CLI 共享上下文。
 * @param parent 父对象。
 */
BatchConverter::BatchConverter(CliContext* context, QObject* parent) : BaseConverter(context, parent) {}

/**
 * @brief 读取元器件列表、预加载组件并执行导出流程。
 * @return 导出成功时返回 true，否则返回 false。
 */
bool BatchConverter::execute() {
    printMessage(QCoreApplication::translate("CliConverter", "开始批量转换..."));

    // 读取元器件列表文件
    QString readError;
    // 保存从列表文件中读取的元器件编号。
    QStringList componentIds = FileReader::readComponentListFile(context()->parser().inputFile(), readError);

    if (!readError.isEmpty()) {
        setError(readError);
        return false;
    }

    if (componentIds.isEmpty()) {
        setError(QCoreApplication::translate("CliConverter", "元器件列表文件为空"));
        return false;
    }

    return runExport(componentIds);
}

}  // namespace EasyKiConverter
