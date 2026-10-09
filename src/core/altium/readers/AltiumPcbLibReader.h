#pragma once

#include "core/altium/compound/OLECompoundReader.h"

#include <QByteArray>
#include <QMap>
#include <QPointF>
#include <QString>
#include <QVector>

namespace EasyKiConverter {

/**
 * @brief Altium PcbLib 库级读取器
 * @details 解析 FileHeader、Library/Data、SectionKeys 以及封装 Storage 映射。
 *          当前只读，不执行增量写回。
 */
class AltiumPcbLibReader {
public:
    /**
     * @brief PcbLib 封装目录项
     */
    struct ComponentInfo {
        QString name;  ///< 封装名称。
        QString sectionKey;  ///< 封装在 OLE 文档中的节名称。
    };

    /**
     * @brief PcbLib Data 中的已知图元块
     */
    struct PrimitiveBlock {
        quint8 flags = 0;  ///< 子块标志位。
        QByteArray payload;  ///< 去除块头后的原始载荷。
        QByteArray encoded;  ///< 包含块头的完整编码数据。
    };

    /**
     * @brief PcbLib 走线公共字段之后的结构化数据
     */
    struct TrackFields {
        qint32 startX = 0;  ///< 起点 X 坐标。
        qint32 startY = 0;  ///< 起点 Y 坐标。
        qint32 endX = 0;  ///< 终点 X 坐标。
        qint32 endY = 0;  ///< 终点 Y 坐标。
        qint32 width = 0;  ///< 走线宽度。
        quint16 netIndex = 0;  ///< 网络索引。
        quint8 componentIndex = 0;  ///< 关联元件索引。
    };

    /**
     * @brief PcbLib 弧线公共字段之后的结构化数据
     */
    struct ArcFields {
        qint32 centerX = 0;  ///< 圆心 X 坐标。
        qint32 centerY = 0;  ///< 圆心 Y 坐标。
        qint32 radius = 0;  ///< 半径。
        double startAngle = 0.0;  ///< 起始角度。
        double endAngle = 0.0;  ///< 结束角度。
        qint32 width = 0;  ///< 弧线宽度。
    };

    /**
     * @brief PcbLib 文本主块的结构化数据
     */
    struct TextFields {
        qint32 locationX = 0;  ///< 文本 X 坐标。
        qint32 locationY = 0;  ///< 文本 Y 坐标。
        qint32 height = 0;  ///< 文本高度。
        qint16 fontId = 0;  ///< 字体索引。
        double rotation = 0.0;  ///< 文本旋转角度。
        quint8 mirrored = 0;  ///< 是否镜像文本。
        qint32 strokeWidth = 0;  ///< 文本笔画宽度。
        quint8 isComment = 0;  ///< 是否为注释文本。
        quint8 isDesignator = 0;  ///< 是否为位号文本。
        quint8 characterSet = 0;  ///< 字符集编码。
        quint8 baseFontType = 0;  ///< 基础字体类型。
        quint32 wideStringIndex = 0;  ///< WideStrings 中的文本索引。
        quint8 kind = 0;  ///< 文本类型编码。
        quint32 v7LayerId = 0;  ///< V7 层标识。
    };

    /**
     * @brief PcbLib 焊盘主块和扩展块的结构化数据
     */
    struct PadFields {
        qint32 locationX = 0;  ///< 焊盘 X 坐标。
        qint32 locationY = 0;  ///< 焊盘 Y 坐标。
        qint32 sizeTopX = 0;  ///< 顶层焊盘 X 尺寸。
        qint32 sizeTopY = 0;  ///< 顶层焊盘 Y 尺寸。
        qint32 sizeMidX = 0;  ///< 中间层焊盘 X 尺寸。
        qint32 sizeMidY = 0;  ///< 中间层焊盘 Y 尺寸。
        qint32 sizeBotX = 0;  ///< 底层焊盘 X 尺寸。
        qint32 sizeBotY = 0;  ///< 底层焊盘 Y 尺寸。
        qint32 holeSize = 0;  ///< 钻孔直径。
        quint8 shapeTop = 0;  ///< 顶层焊盘形状。
        quint8 shapeMid = 0;  ///< 中间层焊盘形状。
        quint8 shapeBot = 0;  ///< 底层焊盘形状。
        double rotation = 0.0;  ///< 焊盘旋转角度。
        quint8 isPlated = 0;  ///< 是否镀铜。
        quint8 stackMode = 0;  ///< 层叠模式。
        quint8 mode = 0;  ///< 焊盘模式。
        quint8 powerPlaneConnectStyle = 0;  ///< 电源层连接方式。
        qint32 reliefAirGapRaw = 0;  ///< Relief 气隙。
        qint32 reliefConductorWidthRaw = 0;  ///< Relief 导体宽度。
        qint16 reliefEntries = 0;  ///< Relief 连接数。
        qint32 powerPlaneClearanceRaw = 0;  ///< 电源层间距。
        qint32 powerPlaneReliefExpansionRaw = 0;  ///< 电源层 Relief 扩展。
        qint32 pasteMaskExpansionRaw = 0;  ///< 锡膏扩展量。
        qint32 solderMaskExpansionRaw = 0;  ///< 阻焊扩展量。
        quint8 pasteMaskExpansionMode = 0;  ///< 锡膏扩展模式。
        quint8 solderMaskExpansionMode = 0;  ///< 阻焊扩展模式。
        quint8 drillType = 0;  ///< 钻孔类型。
        quint8 holeType = 0;  ///< 孔类型。
        qint32 holeSlotLengthRaw = 0;  ///< 槽孔长度。
        double holeRotation = 0.0;  ///< 孔旋转角度。
        quint8 hasRoundedRect = 0;  ///< 是否存在圆角矩形属性。
        quint8 extendedShapeTop = 0;  ///< 顶层扩展形状编码。
        quint8 extendedShapeMid = 0;  ///< 中间层扩展形状编码。
        quint8 extendedShapeBottom = 0;  ///< 底层扩展形状编码。
        quint8 cornerRadiusPercentage = 0;  ///< 圆角半径百分比。
    };

    /**
     * @brief PcbLib 填充图元的结构化数据
     */
    struct FillFields {
        qint32 corner1X = 0;  ///< 第一个角点 X 坐标。
        qint32 corner1Y = 0;  ///< 第一个角点 Y 坐标。
        qint32 corner2X = 0;  ///< 第二个角点 X 坐标。
        qint32 corner2Y = 0;  ///< 第二个角点 Y 坐标。
        double rotation = 0.0;  ///< 填充旋转角度。
        qint32 solderMaskExpansionRaw = 0;  ///< 阻焊扩展量。
        quint8 pasteMaskExpansion = 0;  ///< 锡膏扩展量。
        quint32 v7LayerId = 0;  ///< V7 层标识。
        quint8 keepoutRestrictions = 0;  ///< 禁止区限制标志。
    };

    /**
     * @brief PcbLib 区域图元的结构化数据
     */
    struct RegionFields {
        quint32 reserved = 0;  ///< 协议保留字段。
        quint8 reservedByte = 0;  ///< 协议保留字节。
        QMap<QString, QString> parameters;  ///< 区域参数表。
        QVector<QPointF> vertices;  ///< 区域轮廓顶点。
    };

    /**
     * @brief PcbLib 三维元件体图元的结构化数据
     */
    struct ComponentBodyFields {
        quint32 reserved = 0;  ///< 协议保留字段。
        quint8 reservedByte = 0;  ///< 协议保留字节。
        QMap<QString, QString> parameters;  ///< 元件体参数表。
        QVector<QPointF> outline;  ///< 元件体轮廓顶点。
    };

    /**
     * @brief PcbLib Data 中的图元对象
     * @details 保留对象 ID、所有子块和完整编码；未知对象类型不会被猜测解析。
     */
    struct PrimitiveRecord {
        quint8 objectId = 0;  ///< 图元对象类型编号。
        quint8 layer = 0;  ///< 图元公共头部中的层编号
        quint16 primitiveFlags = 0;  ///< 图元公共头部中的标志位
        bool hasTrackFields = false;  ///< 是否成功解析走线字段。
        TrackFields track;  ///< 走线结构化字段。
        bool hasArcFields = false;  ///< 是否成功解析弧线字段。
        ArcFields arc;  ///< 弧线结构化字段。
        bool hasPadFields = false;  ///< 是否成功解析焊盘字段。
        PadFields pad;  ///< 焊盘结构化字段。
        bool hasFillFields = false;  ///< 是否成功解析填充字段。
        FillFields fill;  ///< 填充结构化字段。
        bool hasRegionFields = false;  ///< 是否成功解析区域字段。
        RegionFields region;  ///< 区域结构化字段。
        bool hasComponentBodyFields = false;  ///< 是否成功解析元件体字段。
        ComponentBodyFields componentBody;  ///< 元件体结构化字段。
        bool hasTextFields = false;  ///< 是否成功解析文本字段。
        TextFields textFields;  ///< 文本结构化字段。
        QString designator;  ///< 图元关联的位号文本。
        QString text;  ///< 图元文本内容。
        QVector<PrimitiveBlock> blocks;  ///< 图元包含的原始子块列表。
        QByteArray encoded;  ///< 图元完整编码数据。
    };

    /**
     * @brief 打开并解析 PcbLib 文件
     * @param filePath 文件路径
     * @return 结构完整且封装映射有效时返回 true
     */
    bool open(const QString& filePath);

    /**
     * @brief 获取封装目录
     * @return 已解析的封装目录项列表
     */
    QVector<ComponentInfo> components() const;
    /**
     * @brief 获取 PcbLib 文件版本文本
     * @return 文件头中的版本字符串
     */
    QString fileVersion() const;
    /**
     * @brief 获取 Library/Data 中的元数据块
     * @return 元数据块的原始字节数据
     */
    QByteArray libraryMetadata() const;
    /**
     * @brief 读取指定封装的指定流
     * @param componentIndex 封装目录项索引
     * @param streamName OLE 流名称
     * @param data 用于接收流数据的输出指针
     * @return 读取成功时返回 true
     */
    bool readFootprintStream(int componentIndex, const QString& streamName, QByteArray* data) const;
    /**
     * @brief 读取指定封装的指定流
     * @param componentName 封装名称
     * @param streamName OLE 流名称
     * @param data 用于接收流数据的输出指针
     * @return 读取成功时返回 true
     */
    bool readFootprintStream(const QString& componentName, const QString& streamName, QByteArray* data) const;
    /**
     * @brief 扫描指定封装 Data 流中的已知图元对象
     * @details Data 流首部的封装名称不会作为对象返回。当前支持项目写入器使用的对象类型；
     *          遇到未知对象 ID 或损坏子块时返回 false，调用方仍可通过 readFootprintStream 获取原始流。
     * @param componentIndex 封装目录项索引
     * @param objects 用于接收图元记录的输出指针
     * @return 扫描成功时返回 true
     */
    bool readFootprintObjects(int componentIndex, QVector<PrimitiveRecord>* objects) const;
    /**
     * @brief 按封装名称扫描已知图元对象
     * @param componentName 封装名称
     * @param objects 用于接收图元记录的输出指针
     * @return 扫描成功时返回 true
     */
    bool readFootprintObjects(const QString& componentName, QVector<PrimitiveRecord>* objects) const;
    /**
     * @brief 获取最近一次错误说明
     * @return 最近一次读取错误；没有错误时为空
     */
    QString errorString() const;
    /**
     * @brief 获取是否发生读取错误
     * @return 已发生读取错误时返回 true
     */
    bool hasError() const;

private:
    bool fail(const QString& message);
    int componentIndex(const QString& componentName) const;

    OLECompoundReader m_oleReader;  ///< OLE 复合文档读取器。
    QVector<ComponentInfo> m_components;  ///< 已解析的封装目录项。
    QString m_fileVersion;  ///< PcbLib 文件版本字符串。
    QByteArray m_libraryMetadata;  ///< Library/Data 元数据块。
    mutable QString m_errorMessage;  ///< 最近一次读取错误说明。
};

}  // namespace EasyKiConverter
