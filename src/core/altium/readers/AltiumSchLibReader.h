#pragma once

#include "core/altium/compound/OLECompoundReader.h"

#include <QMap>
#include <QVector>

namespace EasyKiConverter {

/**
 * @brief Altium SchLib 库级读取器
 * @details 在 OLE 和二进制读取基础上解析 FileHeader、SectionKeys 以及组件 Data 流映射。
 *          当前只读，不执行增量写回。
 */
class AltiumSchLibReader {
public:
    /** @brief SchLib 组件目录项 */
    struct ComponentInfo {
        QString name;  ///< 组件名称。
        QString sectionKey;  ///< 组件在 OLE 文档中的节名称。
        int partCount = 1;  ///< 可见部件数量，不包含公共 Part 0
    };

    /** @brief SchLib Data 中的原始记录块 */
    struct Record {
        quint8 flags = 0;  ///< 记录块标志位。
        QByteArray payload;  ///< 去除块头后的原始载荷。
        QByteArray encoded;  ///< 包含块头的完整编码数据。
        bool hasParameters = false;  ///< 是否已解析参数记录。
        QMap<QString, QString> parameters;  ///< 参数记录键值表。
        int recordType = -1;  ///< RECORD 字段或二进制记录类型
        int ownerPartId = -1;  ///< OWNERPARTID，公共 Part 使用 -1
        int ownerPartDisplayMode = -1;  ///< OWNERPARTDISPLAYMODE，未提供时为 -1
        int indexInSheet = -1;  ///< 内容记录序号，首条隐含序号为 0
        int fontId = -1;  ///< FONTID，按 FileHeader 中的 FontID 顺序，从 1 开始
    };

    /** @brief SchLib /Storage 中的嵌入图片条目 */
    struct ImageStorageEntry {
        quint8 flags = 0;  ///< 图片条目标志位。
        QString name;  ///< 图片文件名。
        QByteArray compressedData;  ///< 图片压缩数据。
    };

    /** @brief SchLib FileHeader 中的字体表条目 */
    struct FontInfo {
        QString name;  ///< 字体名称。
        int size = 0;  ///< 字体大小。
        bool bold = false;  ///< 是否使用粗体。
        bool italic = false;  ///< 是否使用斜体。
        bool underline = false;  ///< 是否添加下划线。
    };

    /**
     * @brief 打开并解析 SchLib 文件
     * @param filePath 文件路径
     * @return 结构完整且组件映射有效时返回 true
     */
    bool open(const QString& filePath);

    /**
     * @brief 获取组件目录
     * @return 组件目录项列表
     */
    QVector<ComponentInfo> components() const;
    /**
     * @brief 获取 FileHeader 参数
     * @return 文件头参数键值表
     */
    QMap<QString, QString> headerParameters() const;
    /**
     * @brief 获取按 FontID 顺序排列的字体表
     * @return 字体表条目列表
     */
    QVector<FontInfo> fonts() const;
    /**
     * @brief 获取指定组件的 Data 流
     * @param componentIndex 组件目录项索引
     * @param data 用于接收流数据的输出指针
     * @return 读取成功时返回 true
     */
    bool readComponentData(int componentIndex, QByteArray* data) const;
    /**
     * @brief 获取指定组件的 Data 流
     * @param componentName 组件名称
     * @param data 用于接收流数据的输出指针
     * @return 读取成功时返回 true
     */
    bool readComponentData(const QString& componentName, QByteArray* data) const;
    /**
     * @brief 拆分指定组件的 Data 记录
     * @details 不解释记录业务字段，保留每条记录的完整编码，确保未知记录可无损转发。
     * @param componentIndex 组件目录项索引
     * @param records 用于接收记录列表的输出指针
     * @return 解析成功时返回 true
     */
    bool readComponentRecords(int componentIndex, QVector<Record>* records) const;
    /**
     * @brief 按组件名称拆分 Data 记录
     * @param componentName 组件名称
     * @param records 用于接收记录列表的输出指针
     * @return 解析成功时返回 true
     */
    bool readComponentRecords(const QString& componentName, QVector<Record>* records) const;
    /**
     * @brief 读取并校验根 /Storage 中的嵌入图片
     * @details 保留去除 Qt 长度前缀后的原始压缩数据，不在读取阶段解码图片。
     * @param entries 用于接收图片条目的输出指针
     * @return 读取成功时返回 true
     */
    bool readImageStorage(QVector<ImageStorageEntry>* entries) const;
    /**
     * @brief 获取最近一次错误说明
     * @return 最近一次错误说明；没有错误时为空
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
    QVector<ComponentInfo> m_components;  ///< 已解析的组件目录项。
    QMap<QString, QString> m_headerParameters;  ///< FileHeader 参数表。
    QVector<FontInfo> m_fonts;  ///< FileHeader 字体表。
    mutable QString m_errorMessage;  ///< 最近一次读取错误说明。
};

}  // namespace EasyKiConverter
