#include "BomConverter.h"

#include "CliContext.h"
#include "FileReader.h"

#include <QCoreApplication>

namespace EasyKiConverter {

/**
 * @brief 创建 BOM 转换器并绑定 CLI 上下文。
 * @param context CLI 共享上下文。
 * @param parent 父对象。
 */
BomConverter::BomConverter(CliContext* context, QObject* parent) : BaseConverter(context, parent) {}

/**
 * @brief 读取 BOM、预加载组件并执行导出流程。
 * @return 导出成功时返回 true，否则返回 false。
 */
bool BomConverter::execute() {
    printMessage(QCoreApplication::translate("CliConverter", "开始转换 BOM 表..."));

    // 读取 BOM 表文件
    QString readError;
    // 保存从 BOM 表中读取的元器件编号。
    QStringList componentIds = FileReader::readBomFile(context()->parser().inputFile(), readError);

    if (!readError.isEmpty()) {
        setError(readError);
        return false;
    }

    if (componentIds.isEmpty()) {
        setError(QCoreApplication::translate("CliConverter", "BOM 表中没有找到有效的元器件编号"));
        return false;
    }

    return runExport(componentIds);
}

}  // namespace EasyKiConverter
