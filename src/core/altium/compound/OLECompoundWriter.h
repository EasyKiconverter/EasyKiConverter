#pragma once

#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

namespace EasyKiConverter {

class OLECompoundSerializer;

/**
 * @brief OLE 复合文档 V3 写入器
 * @details 仅支持写入，不支持读取。实现 OLE Structured Storage V3 格式（512 字节扇区）。
 *          用于生成 Altium Designer 可读取的 .SchLib / .PcbLib 文件。
 *
 * 遵循：[MS-CFB] Compound File Binary File Format
 */
class OLECompoundWriter {
public:
    OLECompoundWriter();
    ~OLECompoundWriter();

    /**
     * @brief 创建新的 OLE 复合文档
     * @return 是否成功初始化
     */
    bool create();

    /**
     * @brief 保存到文件
     * @param filePath 输出文件路径
     * @return 是否成功
     */
    bool saveToFile(const QString& filePath);

    /**
     * @brief 在指定存储下创建子存储
     * @param parentPath 父存储路径（空字符串表示根存储）
     * @param name 存储名称
     * @return 是否成功
     */
    bool addStorage(const QString& parentPath, const QString& name);

    /**
     * @brief 在根存储下创建子存储
     * @param name 存储名称
     * @return 是否成功
     */
    bool addStorage(const QString& name);

    /**
     * @brief 在指定存储下写入流数据
     * @param storagePath 存储路径（空字符串表示根存储）
     * @param streamName 流名称
     * @param data 流数据
     * @return 是否成功
     */
    bool writeStream(const QString& storagePath, const QString& streamName, const QByteArray& data);

    /**
     * @brief 在根存储下写入流数据
     * @param name 流名称
     * @param data 流数据
     * @return 是否成功
     */
    bool writeStream(const QString& name, const QByteArray& data);

    /**
     * @brief 获取写入过程中是否发生结构错误
     * @return true 表示此前的存储或流操作失败
     */
    bool hasError() const {
        return !m_errorMessage.isEmpty();
    }

    /**
     * @brief 获取最近一次结构错误说明
     * @return 错误说明；没有错误时为空
     */
    QString errorString() const {
        return m_errorMessage;
    }

private:
    /**
     * @brief 允许序列化协作者读取固定格式字段和当前文件状态。
     */
    friend class OLECompoundSerializer;

    /**
     * @brief OLE 复合文档目录和扇区链使用的固定值
     */
    static constexpr uint32_t ENDOFCHAIN = 0xFFFFFFFE;  ///< 表示扇区链已经结束。
    static constexpr uint32_t FREESECT = 0xFFFFFFFF;  ///< 表示扇区尚未分配。
    static constexpr uint32_t FATSECT = 0xFFFFFFFD;  ///< 表示该扇区属于 FAT。
    static constexpr uint32_t DIFSECT = 0xFFFFFFFC;  ///< 表示该扇区属于 DIFAT。
    static constexpr uint32_t NOSTREAM = 0xFFFFFFFF;  ///< 表示目录条目没有子流或子节点。
    static constexpr uint32_t SECTOR_SIZE = 512;  ///< OLE 普通扇区的字节数。
    static constexpr uint32_t MINI_SECTOR_SIZE = 64;  ///< OLE MiniStream 扇区的字节数。
    static constexpr uint32_t MINI_STREAM_CUTOFF = 4096;  ///< 使用 MiniStream 存储的最大流大小。
    static constexpr uint32_t DIR_ENTRY_SIZE = 128;  ///< OLE 目录条目的字节数。

    /**
     * @brief 目录条目对象类型
     */
    enum class ObjectType : uint8_t { Unknown = 0, Storage = 1, Stream = 2, Root = 5 };

    /**
     * @brief 目录条目结构（128 字节）
     */
    struct DirectoryEntry {
        uint16_t name[32] = {0};  ///< UTF-16LE 名称（最大 31 字符 + null）
        uint16_t nameSize = 0;  ///< 名称字节数（含 null 终止符）
        ObjectType objectType = ObjectType::Unknown;  ///< 目录条目的对象类型。
        uint8_t colorFlag = 1;  ///< 0=Red, 1=Black（红黑树节点颜色）
        uint32_t leftChild = NOSTREAM;  ///< 左子目录条目的索引。
        uint32_t rightChild = NOSTREAM;  ///< 右子目录条目的索引。
        uint32_t child = NOSTREAM;  ///< 子树根目录条目的索引。
        uint8_t clsid[16] = {0};  ///< 目录条目关联的 CLSID 字节序列。
        uint32_t stateBits = 0;  ///< 目录条目的状态标志。
        uint64_t creationTime = 0;  ///< 目录条目的创建时间戳。
        uint64_t modifiedTime = 0;  ///< 目录条目的修改时间戳。
        uint32_t startSector = ENDOFCHAIN;  ///< 流数据起始扇区的索引。
        uint64_t streamSize = 0;  ///< 流数据的字节数。
    };

    /**
     * @brief 内部节点结构，用于跟踪存储树
     */
    struct StorageNode {
        QString name;  ///< 节点名称
        QString fullPath;  ///< 完整路径
        int dirIndex = -1;  ///< 目录条目索引
        QVector<int> children;  ///< 子节点索引列表
    };

    // ---- 目录操作 ----
    int createDirectoryEntry(const QString& name, ObjectType type);
    void buildRedBlackTree();
    void buildSubTree(int nodeIdx);
    uint32_t buildChildTree(const QVector<int>& children);

    // ---- 数据写入 ----
    void serializeDirectoryEntry(const DirectoryEntry& entry, QByteArray& buffer) const;
    void serializeFileHeader(QByteArray& header) const;
    void finalize();
    bool isValidEntryName(const QString& name) const;
    void setError(const QString& message);

    // ---- 内部状态 ----
    bool m_initialized = false;  ///< 是否已经成功初始化输出文件。
    bool m_finalized = false;  ///< 是否已经完成目录和扇区数据收尾。
    QString m_errorMessage;  ///< 最近一次结构或写入错误说明。
    QVector<DirectoryEntry> m_directory;  ///< 目录条目列表
    QVector<StorageNode> m_nodes;  ///< 存储节点树
    QHash<QString, int> m_pathToNode;  ///< 路径 → 节点索引映射
    QSet<QString> m_entryPaths;  ///< CFB 大小写不敏感的完整路径，用于阻止同级重名

    // 流数据（按存储路径+流名索引）
    struct StreamData {
        QString storagePath;  ///< 流所属存储对象的路径。
        QString streamName;  ///< 流在所属存储对象中的名称。
        QByteArray data;  ///< 流的原始字节数据。
    };

    QVector<StreamData> m_streams;  ///< 待写入 OLE 文件的流数据列表。

    // 扇区数据
    QVector<QByteArray> m_dataSectors;  ///< 普通数据扇区
    QVector<QByteArray> m_difatSectors;  ///< 扩展 DIFAT 扇区（FAT 超过头部 109 项时使用）
    QByteArray m_miniStream;  ///< Mini-Stream 数据
    QVector<uint32_t> m_fat;  ///< FAT 表
    QVector<uint32_t> m_miniFat;  ///< Mini-FAT 表
};

}  // namespace EasyKiConverter
