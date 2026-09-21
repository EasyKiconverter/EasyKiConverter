#include "ExporterFactory.h"

#include "core/allegro/ExporterAllegroFootprint.h"
#include "core/altium/ExporterAltiumFootprint.h"
#include "core/altium/ExporterAltiumSymbol.h"
#include "core/cadstar/ExporterCadstarLibrary.h"
#include "core/eagle/ExporterEagleFootprint.h"
#include "core/kicad/Exporter3DModel.h"
#include "core/kicad/ExporterFootprint.h"
#include "core/kicad/ExporterSymbol.h"
#include "core/orcad/ExporterOrcadSymbol.h"
#include "core/pads/ExporterPadsFootprint.h"
#include "core/pads/ExporterPadsSymbol.h"
#include "core/pcad/ExporterPcadFootprint.h"
#include "core/pcad/ExporterPcadSymbol.h"
#include "core/xpedition/ExporterXpeditionFootprint.h"
#include "core/xpedition/ExporterXpeditionSymbol.h"

namespace EasyKiConverter {

/**
 * @brief 创建符号导出器
 */
std::unique_ptr<ISymbolExporter> ExporterFactory::createSymbolExporter(TargetEdaFormat format) {
    // 根据目标格式选择符号导出器实现，未知格式不创建实例。
    switch (format) {
        case TargetEdaFormat::KiCad:
            return std::make_unique<ExporterSymbol>();
        case TargetEdaFormat::Altium:
            return std::make_unique<ExporterAltiumSymbol>();
        case TargetEdaFormat::Xpedition:
            return std::make_unique<ExporterXpeditionSymbol>();
        case TargetEdaFormat::Pads:
            return std::make_unique<ExporterPadsSymbol>();
        case TargetEdaFormat::Pcad:
            return std::make_unique<ExporterPcadSymbol>();
        case TargetEdaFormat::Orcad:
            return std::make_unique<ExporterOrcadSymbol>();
        case TargetEdaFormat::Allegro:
            return nullptr;
        default:
            return nullptr;
    }
}

/**
 * @brief 创建封装导出器
 */
std::unique_ptr<IFootprintExporter> ExporterFactory::createFootprintExporter(TargetEdaFormat format) {
    // 根据目标格式选择封装导出器实现，未知格式不创建实例。
    switch (format) {
        case TargetEdaFormat::KiCad:
            return std::make_unique<ExporterFootprint>();
        case TargetEdaFormat::Altium:
            return std::make_unique<ExporterAltiumFootprint>();
        case TargetEdaFormat::Xpedition:
            return std::make_unique<ExporterXpeditionFootprint>();
        case TargetEdaFormat::Allegro:
            return std::make_unique<ExporterAllegroFootprint>();
        case TargetEdaFormat::Pads:
            return std::make_unique<ExporterPadsFootprint>();
        case TargetEdaFormat::Eagle:
            return std::make_unique<ExporterEagleFootprint>();
        case TargetEdaFormat::Pcad:
            return std::make_unique<ExporterPcadFootprint>();
        case TargetEdaFormat::Cadstar:
            return std::make_unique<ExporterCadstarLibrary>();
        default:
            return nullptr;
    }
}

/**
 * @brief 创建 3D 模型导出器
 * @note 使用裸 new 而非 std::make_unique，因为 Exporter3DModel 继承 QObject，
 *       需要将 parent 传递给构造函数以建立 Qt 对象所有权。
 * @note WRL/STEP 是独立模型产物，所有目标格式都可以复用该转换器；目标格式的原生模型关联仍由各自封装导出器决定。
 */
std::unique_ptr<IModel3DExporter> ExporterFactory::createModel3DExporter(TargetEdaFormat format, QObject* parent) {
    // 独立 WRL/STEP 模型不依赖目标库的私有关联语法，因此统一复用可靠的公共实现。
    switch (format) {
        case TargetEdaFormat::KiCad:
        case TargetEdaFormat::Altium:
        case TargetEdaFormat::Allegro:
        case TargetEdaFormat::Xpedition:
        case TargetEdaFormat::Pads:
        case TargetEdaFormat::Eagle:
        case TargetEdaFormat::Pcad:
        case TargetEdaFormat::Cadstar:
        case TargetEdaFormat::Orcad:
            return std::unique_ptr<IModel3DExporter>(new Exporter3DModel(parent));
        default:
            return nullptr;
    }
}

}  // namespace EasyKiConverter
