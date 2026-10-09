#include "EasyedaImporter.h"

#include "EasyedaFootprintImporter.h"
#include "EasyedaSymbolImporter.h"

namespace EasyKiConverter {

EasyedaImporter::EasyedaImporter(QObject* parent) : QObject(parent) {}

EasyedaImporter::~EasyedaImporter() {}

/**
 * @brief 将 CAD JSON 导入为符号数据。
 * @param cadData EasyEDA CAD 数据对象。
 * @return 导入得到的符号数据。
 */
QSharedPointer<SymbolData> EasyedaImporter::importSymbolData(const QJsonObject& cadData) {
    EasyedaSymbolImporter importer;  // 符号专用导入器。
    return importer.importSymbolData(cadData);
}

/**
 * @brief 将 CAD JSON 导入为封装数据。
 * @param cadData EasyEDA CAD 数据对象。
 * @return 导入得到的封装数据。
 */
QSharedPointer<FootprintData> EasyedaImporter::importFootprintData(const QJsonObject& cadData) {
    EasyedaFootprintImporter importer;  // 封装专用导入器。
    return importer.importFootprintData(cadData);
}

}  // namespace EasyKiConverter
