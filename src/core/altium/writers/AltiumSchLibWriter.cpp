#include "AltiumSchLibWriter.h"

#include "AltiumSchComponentRecordWriter.h"
#include "AltiumSchComponentStorageWriter.h"
#include "AltiumSchGeometryValidator.h"
#include "AltiumSchGraphicOrderValidator.h"
#include "AltiumSchImageRecordWriter.h"
#include "AltiumSchImageStorageWriter.h"
#include "AltiumSchInputValidator.h"
#include "AltiumSchLibraryHeaderWriter.h"
#include "AltiumSchOwnershipValidator.h"
#include "AltiumSchPinRecordWriter.h"
#include "AltiumSchPrimitiveRecordWriter.h"
#include "AltiumSchTextRecordWriter.h"
#include "utils/AltiumConstants.h"
#include "utils/AltiumCoord.h"
#include "utils/AltiumWriterUtils.h"

#include <QDebug>
#include <QIODevice>

#include <algorithm>
#include <cmath>

namespace EasyKiConverter {

/**
 * @brief 获取元件的 Section Key（存储键）
 * @details 委托给 AltiumWriterUtils::getSectionKey()
 * @param name 元件名称。
 * @return 可用作 OLE 存储键的名称。
 */
QString AltiumSchLibWriter::getSectionKey(const QString& name) const {
    return AltiumWriterUtils::getSectionKey(name);
}

/**
 * @brief 添加坐标参数（DXP 单位 + 小数部分）
 * @param params 参数映射（输出）。
 * @param key 参数键名。
 * @param raw IR 原始坐标值。
 */
void AltiumSchLibWriter::addCoordParam(QMap<QString, QString>& params, const QString& key, int raw) {
    int16_t dxp = AltiumCoord::toDxpInt(raw);  // DXP 整数坐标部分。
    int32_t frac = AltiumCoord::toDxpFrac(raw);  // DXP 小数坐标部分。
    params[key] = QString::number(dxp);
    if (frac != 0) {
        params[key + "_Frac"] = QString::number(frac);
    }
}

/**
 * @brief 添加颜色参数
 * @param params 参数映射（输出）。
 * @param key 参数键名。
 * @param color 颜色整数值。
 */
void AltiumSchLibWriter::addColorParam(QMap<QString, QString>& params, const QString& key, uint32_t color) {
    if (color != 0) {
        params[key] = QString::number(color);
    }
}

/**
 * @brief 生成并添加 UniqueID
 * @param params 参数映射（输出）。
 */
void AltiumSchLibWriter::addUniqueID(QMap<QString, QString>& params) {
    m_uniqueIdCounter++;  // 递增当前库文件的记录唯一标识计数器。
    QString id = QString("%1").arg(m_uniqueIdCounter, 8, 10, QChar('0'));  // 格式化后的八位唯一标识。
    params["UniqueID"] = id;
}

/**
 * @brief 写入 SchLib 文件
 * @param components 元件列表。
 * @param filePath 输出文件路径。
 * @param libraryName 输出库名称。
 * @return 写入成功时返回 true。
 */
bool AltiumSchLibWriter::write(const QList<AltiumSchComponent>& components,
                               const QString& filePath,
                               const QString& libraryName) {
    m_diagnostics.clear();
    if (components.isEmpty()) {
        m_diagnostics.append(QStringLiteral("Altium SchLib 输入组件为空，已拒绝写入"));
        qWarning() << "AltiumSchLibWriter: Refusing to write an empty library";
        return false;
    }
    if (filePath.trimmed().isEmpty()) {
        m_diagnostics.append(QStringLiteral("Altium SchLib 输出路径为空，已拒绝写入"));
        qWarning() << "AltiumSchLibWriter: Refusing to write without an output path";
        return false;
    }
    if (!AltiumSchInputValidator::validate(*this, components)) {
        return false;
    }
    m_fontRegistry.clear();
    m_embeddedImageNames.clear();
    m_uniqueIdCounter = 0;
    m_nextIndexInSheet = 0;
    m_libraryName = libraryName;  // 当前输出库名称，用于生成实现记录中的 PcbLib 引用。

    prepareImageStorageNames(components);

    // 确保有默认字体
    m_fontRegistry.getOrAdd(QStringLiteral("Times New Roman"), 10);
    m_fontRegistry.registerTextFonts(components);

    OLECompoundWriter ole;  // 当前 SchLib 的 OLE 复合文档写入器。
    if (!ole.create()) {
        qWarning() << "AltiumSchLibWriter: Failed to create OLE compound document for" << filePath;
        return false;
    }

    QStringList names;  // 按输入顺序收集的组件名称。
    for (const AltiumSchComponent& component : components) {
        names.append(component.name);
    }
    const QStringList sectionKeys = AltiumWriterUtils::makeUniqueSectionKeys(names);  // 组件唯一 OLE 存储键。

    AltiumSchLibraryHeaderWriter headerWriter(*this);
    headerWriter.writeFileHeader(ole, components);

    headerWriter.writeSectionKeys(ole, components, sectionKeys);

    // 写入每个元件的存储
    for (int i = 0; i < components.size(); ++i) {
        writeComponentStorage(ole, components[i], sectionKeys[i]);
    }

    writeImageStorage(ole, components);

    if (ole.hasError()) {
        qWarning() << "AltiumSchLibWriter: Failed to construct OLE document:" << ole.errorString();
        return false;
    }

    return ole.saveToFile(filePath);
}

/**
 * @brief 兼容旧内部调用入口，并委托文件级头部写入器。
 * @param ole SchLib 根 OLE 写入器。
 * @param components 待写入的组件列表。
 */
void AltiumSchLibWriter::writeFileHeader(OLECompoundWriter& ole, const QList<AltiumSchComponent>& components) {
    AltiumSchLibraryHeaderWriter headerWriter(*this);
    headerWriter.writeFileHeader(ole, components);
}

/**
 * @brief 兼容旧内部调用入口，并委托 SectionKeys 写入器。
 * @param ole SchLib 根 OLE 写入器。
 * @param components 待写入的组件列表。
 * @param sectionKeys 组件对应的 OLE 存储键列表。
 */
void AltiumSchLibWriter::writeSectionKeys(OLECompoundWriter& ole,
                                          const QList<AltiumSchComponent>& components,
                                          const QStringList& sectionKeys) {
    AltiumSchLibraryHeaderWriter headerWriter(*this);
    headerWriter.writeSectionKeys(ole, components, sectionKeys);
}

/**
 * @brief 写入元件存储
 * @param ole SchLib 根 OLE 写入器。
 * @param component 待写入的原理图元件。
 * @param sectionKey 组件对应的 OLE 存储键。
 */
void AltiumSchLibWriter::writeComponentStorage(OLECompoundWriter& ole,
                                               const AltiumSchComponent& component,
                                               const QString& sectionKey) {
    AltiumSchComponentStorageWriter componentStorageWriter(*this);
    componentStorageWriter.write(ole, component, sectionKey);
}

/**
 * @brief 委托独立校验器检查组件的几何字段和图元编码约束。
 * @param component 待校验的原理图元件。
 * @return 几何数据有效时返回 true。
 */
bool AltiumSchLibWriter::validateGeometry(const AltiumSchComponent& component) {
    return AltiumSchGeometryValidator::validate(*this, component);
}

/**
 * @brief 校验所有图元和参数记录的 OWNERPARTID 是否落在组件部件范围内。
 * @param component 待校验的原理图元件。
 * @return 部件归属有效时返回 true。
 */
bool AltiumSchLibWriter::validatePartOwnership(const AltiumSchComponent& component) {
    return AltiumSchOwnershipValidator::validate(*this, component);
}

/**
 * @brief 写入元件记录 (RECORD=1)
 * @param writer 目标二进制写入器。
 * @param component 原理图元件模型。
 */
void AltiumSchLibWriter::writeComponentRecord(AltiumBinaryWriter& writer, const AltiumSchComponent& component) {
    QMap<QString, QString> params;  // 当前组件的 RECORD=1 参数。
    params["RECORD"] = "1";
    params["LibReference"] = component.name;

    if (!component.description.isEmpty()) {
        params["ComponentDescription"] = component.description;
    }

    params["PartCount"] = QString::number(qMax(1, component.partCount) + 1);
    params["DisplayModeCount"] = "1";
    params["IndexInSheet"] = "-1";
    params["OwnerPartId"] = "-1";
    params["CurrentPartId"] = "1";
    params["LibraryPath"] = "*";
    params["SourceLibraryName"] = "*";
    params["SheetPartFileName"] = "*";
    params["TargetFileName"] = "*";
    params["ALLPINCOUNT"] = QString::number(component.pins.size());
    if (!component.aliases.isEmpty())
        params["Aliases"] = component.aliases.join(",");

    addUniqueID(params);
    writer.writeCStringParameterBlockUtf8(params);
}

/**
 * @brief 写入引脚记录 (RECORD=2, 二进制格式)
 * @param writer 目标二进制写入器。
 * @param pin 引脚模型。
 */
void AltiumSchLibWriter::writePinRecord(AltiumBinaryWriter& writer, const AltiumSchPin& pin) {
    AltiumSchPinRecordWriter pinWriter(*this);
    pinWriter.write(writer, pin);
}

/**
 * @brief 写入矩形记录 (RECORD=14)
 * @param writer 目标二进制写入器。
 * @param rect 矩形模型。
 */
void AltiumSchLibWriter::writeRectangleRecord(AltiumBinaryWriter& writer, const AltiumSchRectangle& rect) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writeRectangle(writer, rect);
}

/**
 * @brief 写入圆角矩形记录 (RECORD=10)
 * @param writer 目标二进制写入器。
 * @param rect 圆角矩形模型。
 */
void AltiumSchLibWriter::writeRoundRectangleRecord(AltiumBinaryWriter& writer, const AltiumSchRoundRectangle& rect) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writeRoundRectangle(writer, rect);
}

/**
 * @brief 写入线段记录 (RECORD=13)
 * @param writer 目标二进制写入器。
 * @param line 线段模型。
 */
void AltiumSchLibWriter::writeLineRecord(AltiumBinaryWriter& writer, const AltiumSchLine& line) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writeLine(writer, line);
}

/**
 * @brief 写入弧线记录 (RECORD=12)
 * @param writer 目标二进制写入器。
 * @param arc 圆弧模型。
 */
void AltiumSchLibWriter::writeArcRecord(AltiumBinaryWriter& writer, const AltiumSchArc& arc) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writeArc(writer, arc);
}

/**
 * @brief 写入多边形记录 (RECORD=7)
 * @param writer 目标二进制写入器。
 * @param polygon 多边形模型。
 */
void AltiumSchLibWriter::writePolygonRecord(AltiumBinaryWriter& writer, const AltiumSchPolygon& polygon) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writePolygon(writer, polygon);
}

/**
 * @brief 写入椭圆记录 (RECORD=8)
 * @param writer 目标二进制写入器。
 * @param ellipse 椭圆模型。
 */
void AltiumSchLibWriter::writeEllipseRecord(AltiumBinaryWriter& writer, const AltiumSchEllipse& ellipse) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writeEllipse(writer, ellipse);
}

/**
 * @brief 写入扇形记录 (RECORD=9)
 * @param writer 目标二进制写入器。
 * @param pie 扇形模型。
 */
void AltiumSchLibWriter::writePieRecord(AltiumBinaryWriter& writer, const AltiumSchPie& pie) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writePie(writer, pie);
}

/**
 * @brief 写入椭圆弧记录 (RECORD=11)
 * @param writer 目标二进制写入器。
 * @param arc 椭圆弧模型。
 */
void AltiumSchLibWriter::writeEllipticalArcRecord(AltiumBinaryWriter& writer, const AltiumSchEllipticalArc& arc) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writeEllipticalArc(writer, arc);
}

/**
 * @brief 写入折线记录 (RECORD=6)
 * @param writer 目标二进制写入器。
 * @param polyline 折线模型。
 */
void AltiumSchLibWriter::writePolylineRecord(AltiumBinaryWriter& writer, const AltiumSchPolyline& polyline) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writePolyline(writer, polyline);
}

/**
 * @brief 写入路径记录（委托给折线记录）
 * @param writer 二进制写入器。
 * @param path 路径数据。
 */
void AltiumSchLibWriter::writePathRecord(AltiumBinaryWriter& writer, const AltiumSchPath& path) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writePath(writer, path);
}

/**
 * @brief 写入三次 Bézier 曲线记录 (RECORD=5)
 * @param writer 目标二进制写入器。
 * @param bezier Bézier 曲线模型。
 */
void AltiumSchLibWriter::writeBezierRecord(AltiumBinaryWriter& writer, const AltiumSchBezier& bezier) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writeBezier(writer, bezier);
}

/**
 * @brief 写入 IEEE 图形记录 (RECORD=3)
 * @param writer 目标二进制写入器。
 * @param ieee IEEE 图形模型。
 */
void AltiumSchLibWriter::writeIeeeRecord(AltiumBinaryWriter& writer, const AltiumSchIeee& ieee) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writeIeee(writer, ieee);
}

/**
 * @brief 将文本记录委托给专用协作者，保持写入器对外行为不变。
 * @param writer 目标二进制写入器。
 * @param text 文本模型。
 */
void AltiumSchLibWriter::writeTextRecord(AltiumBinaryWriter& writer, const AltiumSchText& text) {
    AltiumSchTextRecordWriter textWriter(*this);
    textWriter.writeText(writer, text);
}

/**
 * @brief 将文本框记录委托给专用协作者，保持写入器对外行为不变。
 * @param writer 目标二进制写入器。
 * @param frame 文本框模型。
 */
void AltiumSchLibWriter::writeTextFrameRecord(AltiumBinaryWriter& writer, const AltiumSchTextFrame& frame) {
    AltiumSchTextRecordWriter textWriter(*this);
    textWriter.writeTextFrame(writer, frame);
}

/**
 * @brief 将图片记录委托给专用协作者，保持主写入器的状态边界不变。
 * @param writer 目标二进制写入器。
 * @param image 图片模型。
 */
void AltiumSchLibWriter::writeImageRecord(AltiumBinaryWriter& writer, const AltiumSchImage& image) {
    AltiumSchImageRecordWriter imageWriter(*this);
    imageWriter.write(writer, image);
}

/**
 * @brief 为嵌入图片生成稳定且唯一的 Storage 文件名。
 * @details Altium 的 Storage 以文件名关联图片记录；重复名称会导致多个图元指向同一条目。
 * @param components 待处理的组件列表。
 */
void AltiumSchLibWriter::prepareImageStorageNames(const QList<AltiumSchComponent>& components) {
    AltiumSchImageStorageWriter imageStorageWriter(*this);
    imageStorageWriter.prepareNames(components);
}

/**
 * @brief 写入 SchLib 根 /Storage 图片流
 * @details 图片内容使用 Qt zlib 压缩结果（去除 qCompress 的四字节长度头），
 *          并按 Altium 的 D0 标记和 Pascal 文件名组织条目。
 * @param ole SchLib 根 OLE 写入器。
 * @param components 待写入嵌入图片的组件列表。
 */
void AltiumSchLibWriter::writeImageStorage(OLECompoundWriter& ole, const QList<AltiumSchComponent>& components) {
    AltiumSchImageStorageWriter imageStorageWriter(*this);
    imageStorageWriter.write(ole, components);
}

/**
 * @brief 写入元件参数记录（Designator、Value 和自定义参数）
 * @param writer 二进制写入器
 * @param component 元件数据
 */
void AltiumSchLibWriter::writeComponentParameterRecords(AltiumBinaryWriter& writer,
                                                        const AltiumSchComponent& component) {
    AltiumSchComponentRecordWriter recordWriter(*this);
    recordWriter.writeParameters(writer, component);
}

/**
 * @brief 写入实现记录 (RECORD=44-48)
 * @param writer 目标二进制写入器。
 * @param component 原理图元件模型。
 */
void AltiumSchLibWriter::writeImplementationRecords(AltiumBinaryWriter& writer, const AltiumSchComponent& component) {
    AltiumSchComponentRecordWriter recordWriter(*this);
    recordWriter.writeImplementations(writer, component);
}

/**
 * @brief 计算元件的记录总数（用于 FileHeader WEIGHT 字段）
 * @param component 元件数据
 * @return 记录数（含元件记录、图元、参数和实现记录）
 */
int AltiumSchLibWriter::componentRecordCount(const AltiumSchComponent& component) const {
    int validBezierCount = 0;  // 具有四个控制点、可写出的 Bezier 数量。
    for (const AltiumSchBezier& bezier : component.beziers) {
        if (bezier.controlPoints.size() == 4)
            ++validBezierCount;
    }
    const int graphics =  // 所有图元记录数量。
        component.pins.size() + component.rectangles.size() + component.roundRectangles.size() +  // 所有图元记录数量。
        component.lines.size() + component.arcs.size() + component.polygons.size() + component.ellipses.size() +
        component.pies.size() + component.ellipticalArcs.size() + component.polylines.size() + component.paths.size() +
        validBezierCount + component.ieeeSymbols.size() + component.texts.size() + component.textFrames.size() +
        component.images.size();
    // Component + graphics + 参数字段 + ImplementationList + implementation records.
    // 每个实现包含 RECORD=45、46、48，以及每个符号引脚对应的 RECORD=47。
    return 1 + graphics + componentParameterRecordCount(component) + 1 +
           component.implementations.size() * (3 + component.pins.size());
}

/**
 * @brief 计算元件参数记录数。
 * @param component 元件数据
 * @return Designator、Value 和自定义参数记录总数
 */
int AltiumSchLibWriter::componentParameterRecordCount(const AltiumSchComponent& component) const {
    return AltiumSchComponentRecordWriter::parameterRecordCount(component);
}

/**
 * @brief 向内容记录添加共享的 IndexInSheet 序号
 * @param params 参数映射（输出）
 * @details 首条内容记录的索引 0 由 Altium 隐含表示，因此不写出字段。
 */
void AltiumSchLibWriter::addContentIndex(QMap<QString, QString>& params) {
    if (m_nextIndexInSheet != 0)
        params["IndexInSheet"] = QString::number(m_nextIndexInSheet);
    ++m_nextIndexInSheet;
}

/**
 * @brief 向参数映射中添加 OwnerPartId 和 ISNOTACCESIBLE 字段
 * @param params 参数映射（输出）。
 * @param ownerPartId 所属部件 ID。
 * @param context 部件编号的语义上下文。
 */
int AltiumSchLibWriter::normalizeOwnerPartId(int ownerPartId, const QString& context) {
    if (ownerPartId == 0 || ownerPartId < -1 || ownerPartId > 32767) {
        const int normalized = ownerPartId < 0 ? -1 : qBound(1, ownerPartId, 32767);  // 规范化后的部件编号。
        const QString diagnostic =
            QStringLiteral("Altium SchLib %1 OWNERPARTID=%2 无效，已规范化为 %3")  // 部件编号修正诊断。
                .arg(context)
                .arg(ownerPartId)
                .arg(normalized);
        m_diagnostics.append(diagnostic);
        qWarning() << "AltiumSchLibWriter:" << diagnostic;
        return normalized;
    }
    return ownerPartId;
}

/**
 * @brief 将非法角度归一化为回退值，并向调用方报告诊断信息。
 * @param angle 待校验的角度。
 * @param fallback 非法角度使用的回退值。
 * @param context 角度字段的语义上下文。
 * @return 有限角度原值，或非法时的回退值。
 */
double AltiumSchLibWriter::normalizeFiniteAngle(double angle, double fallback, const QString& context) {
    if (std::isfinite(angle))
        return angle;

    const QString diagnostic =  // 非法角度的规范化诊断。
        QStringLiteral("Altium SchLib %1无效，已规范化为 %2 度").arg(context).arg(fallback, 0, 'f', 3);
    m_diagnostics.append(diagnostic);
    qWarning() << "AltiumSchLibWriter:" << diagnostic;
    return fallback;
}

/**
 * @brief 写入图元通用的可见性、内容索引、显示模式和部件归属参数。
 * @param params 参数映射（输出）。
 * @param ownerPartId 所属部件 ID。
 */
void AltiumSchLibWriter::addOwnerParams(QMap<QString, QString>& params, int ownerPartId) {
    params["ISNOTACCESIBLE"] = "T";
    addContentIndex(params);
    // 当前每个符号只有一个显示模式；真实 SchLib 样本使用从 1 开始的显示模式编号。
    params["OWNERPARTDISPLAYMODE"] = "1";
    params["OWNERPARTID"] = QString::number(normalizeOwnerPartId(ownerPartId, QStringLiteral("图元")));
}

}  // namespace EasyKiConverter
