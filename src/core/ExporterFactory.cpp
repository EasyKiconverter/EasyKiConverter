#include "ExporterFactory.h"

#include "core/allegro/ExporterAllegroFootprint.h"
#include "core/altium/ExporterAltiumFootprint.h"
#include "core/altium/ExporterAltiumSymbol.h"
#include "core/eagle/ExporterEagleFootprint.h"
#include "core/kicad/Exporter3DModel.h"
#include "core/kicad/ExporterFootprint.h"
#include "core/kicad/ExporterSymbol.h"
#include "core/pads/ExporterPadsFootprint.h"
#include "core/pads/ExporterPadsSymbol.h"
#include "core/pcad/ExporterPcadFootprint.h"
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
        default:
            return nullptr;
    }
}

/**
 * @brief 创建 3D 模型导出器
 * @note 使用裸 new 而非 std::make_unique，因为 Exporter3DModel 继承 QObject，
 *       需要将 parent 传递给构造函数以建立 Qt 对象所有权。
 * @note Altium 3D 模型（WRL/STEP）格式与 KiCad 通用，复用 KiCad 实现。
 */
std::unique_ptr<IModel3DExporter> ExporterFactory::createModel3DExporter(TargetEdaFormat format, QObject* parent) {
    // WRL/STEP 的三维模型导出器由支持该格式的目标格式共享。
    switch (format) {
        case TargetEdaFormat::KiCad:
        case TargetEdaFormat::Altium:
            // Altium 同样支持 WRL/STEP 格式，复用 KiCad 的 3D 导出器
            return std::unique_ptr<IModel3DExporter>(new Exporter3DModel(parent));
        case TargetEdaFormat::Allegro:
            return nullptr;
        default:
            return nullptr;
    }
}

}  // namespace EasyKiConverter
