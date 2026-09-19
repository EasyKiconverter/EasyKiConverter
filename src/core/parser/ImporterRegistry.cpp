#include "ImporterRegistry.h"

namespace EasyKiConverter::Parser {

/** @brief 校验并注册导入器，拒绝空 ID、未知格式和重复 ID。 */
bool ImporterRegistry::registerImporter(const ImporterDescriptor& descriptor) {
    if (descriptor.id.trimmed().isEmpty() || descriptor.format == DetectedFormat::Unknown || !descriptor.probe)
        return false;
    for (const ImporterDescriptor& current : m_importers) {
        if (current.id == descriptor.id)
            return false;
    }
    m_importers.append(descriptor);
    return true;
}

/** @brief 注册 Xpedition、Cadstar 和 P-CAD 等当前已有实现的内置导入器。 */
bool ImporterRegistry::registerBuiltInImporters() {
    const QList<ImporterDescriptor> builtIns = {
        {QStringLiteral("xpedition-hkp"),
         QStringLiteral("Xpedition HKP"),
         DetectedFormat::XpeditionHkp,
         {QStringLiteral(".psk.hkp"), QStringLiteral(".cel.hkp"), QStringLiteral(".pdb.hkp")},
         [](const QString& fileName, const QByteArray& content) {
             return FormatDetector::detect(fileName, content) == DetectedFormat::XpeditionHkp;
         }},
        {QStringLiteral("xpedition-symbol"),
         QStringLiteral("Xpedition Symbol"),
         DetectedFormat::XpeditionSymbol,
         {QStringLiteral(".sym.1")},
         [](const QString& fileName, const QByteArray& content) {
             return FormatDetector::detect(fileName, content) == DetectedFormat::XpeditionSymbol;
         }},
        {QStringLiteral("cadstar-ascii"),
         QStringLiteral("Cadstar ASCII"),
         DetectedFormat::CadstarAscii,
         {QStringLiteral(".cpa")},
         [](const QString& fileName, const QByteArray& content) {
             return FormatDetector::detect(fileName, content) == DetectedFormat::CadstarAscii;
         }},
        {QStringLiteral("pcad-ascii-pcb"),
         QStringLiteral("P-CAD ASCII PCB"),
         DetectedFormat::PcadSExpression,
         {QStringLiteral(".pcb")},
         [](const QString& fileName, const QByteArray& content) {
             return FormatDetector::detect(fileName, content) == DetectedFormat::PcadSExpression;
         }},
    };

    bool success = true;
    for (const ImporterDescriptor& builtIn : builtIns) {
        bool alreadyRegistered = false;
        for (const ImporterDescriptor& current : m_importers) {
            if (current.id == builtIn.id) {
                alreadyRegistered = true;
                break;
            }
        }
        if (!alreadyRegistered)
            success = registerImporter(builtIn) && success;
    }
    return success;
}

/** @brief 清空注册表，便于独立任务建立自己的导入器集合。 */
void ImporterRegistry::clear() {
    m_importers.clear();
}

/** @brief 返回注册表的只读描述列表。 */
const QList<ImporterDescriptor>& ImporterRegistry::importers() const {
    return m_importers;
}

/** @brief 按注册顺序执行探测，避免多个格式同时匹配时产生隐式优先级。 */
const ImporterDescriptor* ImporterRegistry::detect(const QString& fileName, const QByteArray& content) const {
    for (const ImporterDescriptor& importer : m_importers) {
        if (importer.probe(fileName, content))
            return &importer;
    }
    return nullptr;
}

}  // namespace EasyKiConverter::Parser
