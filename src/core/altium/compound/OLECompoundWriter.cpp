#include "OLECompoundWriter.h"

#include "OLECompoundSerializer.h"

#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace EasyKiConverter {

/**
 * @brief 构造函数
 */
OLECompoundWriter::OLECompoundWriter() = default;

/**
 * @brief 析构函数
 */
OLECompoundWriter::~OLECompoundWriter() = default;

/**
 * @brief 创建新的 OLE 复合文档
 */
bool OLECompoundWriter::create() {
    m_initialized = false;
    m_finalized = false;
    m_errorMessage.clear();
    m_directory.clear();
    m_nodes.clear();
    m_pathToNode.clear();
    m_entryPaths.clear();
    m_streams.clear();
    m_dataSectors.clear();
    m_difatSectors.clear();
    m_miniStream.clear();
    m_fat.clear();
    m_miniFat.clear();

    // 创建 Root Entry 目录条目（索引 0）
    int rootIdx = createDirectoryEntry("Root Entry", ObjectType::Root);
    if (rootIdx != 0) {
        return false;
    }

    // 创建根节点并注册到 m_pathToNode，避免 finalize() 中 prepend 导致索引偏移
    StorageNode rootNode;
    rootNode.name = "Root Entry";
    rootNode.fullPath = "";
    rootNode.dirIndex = 0;
    m_nodes.append(rootNode);
    m_pathToNode[""] = 0;

    m_initialized = true;
    return true;
}

/**
 * @brief 创建目录条目。
 * @param name 目录条目名称。
 * @param type 目录对象类型。
 * @return 新目录条目的索引。
 */
int OLECompoundWriter::createDirectoryEntry(const QString& name, ObjectType type) {
    DirectoryEntry entry;  // 待写入的目录条目。
    entry.objectType = type;

    // 写入 UTF-16LE 名称
    QString utf16Name = name;  // 待编码为 UTF-16LE 的名称。
    int charCount = qMin(utf16Name.length(), 31);  // OLE 目录名称允许的字符数。
    for (int i = 0; i < charCount; ++i) {
        entry.name[i] = static_cast<uint16_t>(utf16Name.at(i).unicode());
    }
    entry.nameSize = static_cast<uint16_t>((charCount + 1) * 2);  // 含 null 终止符

    int index = m_directory.size();  // 新条目在目录数组中的索引。
    m_directory.append(entry);

    return index;
}

/**
 * @brief 在指定存储下创建子存储。
 * @param parentPath 父存储路径。
 * @param name 子存储名称。
 * @return 创建是否成功。
 */
bool OLECompoundWriter::addStorage(const QString& parentPath, const QString& name) {
    if (!m_initialized || m_finalized || !isValidEntryName(name) || !m_pathToNode.contains(parentPath)) {
        setError(QStringLiteral("无法创建存储: %1/%2").arg(parentPath, name));
        return false;
    }

    QString fullPath = parentPath.isEmpty() ? name : parentPath + "/" + name;  // 新存储的完整路径。

    const QString foldedPath = fullPath.toCaseFolded();  // 用于大小写无关冲突检测的路径。
    if (m_entryPaths.contains(foldedPath)) {
        setError(QStringLiteral("存储路径重复: %1").arg(fullPath));
        return false;
    }

    int dirIndex = createDirectoryEntry(name, ObjectType::Storage);  // 新存储对应的目录索引。

    // 创建节点
    StorageNode node;  // 新建的存储节点。
    node.name = name;
    node.fullPath = fullPath;
    node.dirIndex = dirIndex;
    int nodeIndex = m_nodes.size();  // 新节点在节点数组中的索引。
    m_nodes.append(node);
    m_pathToNode[fullPath] = nodeIndex;
    m_entryPaths.insert(foldedPath);

    // 添加到父节点的子列表
    if (parentPath.isEmpty()) {
        // 根存储的子节点
        if (m_pathToNode.contains("")) {
            m_nodes[m_pathToNode[""]].children.append(nodeIndex);
        }
    } else {
        if (m_pathToNode.contains(parentPath)) {
            m_nodes[m_pathToNode[parentPath]].children.append(nodeIndex);
        }
    }

    return true;
}

/**
 * @brief 在根存储下创建子存储。
 * @param name 子存储名称。
 * @return 创建是否成功。
 */
bool OLECompoundWriter::addStorage(const QString& name) {
    return addStorage("", name);
}

/**
 * @brief 在指定存储下写入流数据。
 * @param storagePath 目标存储路径。
 * @param streamName 流名称。
 * @param data 流内容。
 * @return 写入是否成功。
 */
bool OLECompoundWriter::writeStream(const QString& storagePath, const QString& streamName, const QByteArray& data) {
    if (!m_initialized || m_finalized || !isValidEntryName(streamName) || !m_pathToNode.contains(storagePath)) {
        setError(QStringLiteral("无法写入流: %1/%2").arg(storagePath, streamName));
        return false;
    }

    const QString fullPath = storagePath.isEmpty() ? streamName : storagePath + "/" + streamName;  // 流完整路径。
    const QString foldedPath = fullPath.toCaseFolded();  // 用于冲突检测的大小写折叠路径。
    if (m_entryPaths.contains(foldedPath)) {
        setError(QStringLiteral("流路径重复: %1").arg(fullPath));
        return false;
    }

    StreamData stream;  // 待保存的流数据记录。
    stream.storagePath = storagePath;
    stream.streamName = streamName;
    stream.data = data;
    m_streams.append(stream);
    m_entryPaths.insert(foldedPath);

    return true;
}

/**
 * @brief 校验目录或流名称，确保其可安全编码为 OLE 目录条目。
 * @param name 待校验的名称。
 * @return 名称是否符合 OLE 目录条目限制。
 */
bool OLECompoundWriter::isValidEntryName(const QString& name) const {
    return !name.isEmpty() && name.size() <= 31 && !name.contains('/') && !name.contains('\\') && !name.contains(':') &&
           !name.contains('!') && !name.contains(QChar::Null);
}

/**
 * @brief 记录写入过程中的首个结构错误，保留后续诊断的根因。
 * @param message 错误信息。
 */
void OLECompoundWriter::setError(const QString& message) {
    if (m_errorMessage.isEmpty())
        m_errorMessage = message;
}

/**
 * @brief 在根存储下写入流数据。
 * @param name 流名称。
 * @param data 流内容。
 * @return 写入是否成功。
 */
bool OLECompoundWriter::writeStream(const QString& name, const QByteArray& data) {
    return writeStream("", name, data);
}

/**
 * @brief 将目录条目序列化为 128 字节。
 * @param entry 待序列化的目录条目。
 * @param buffer 接收序列化结果的缓冲区。
 */
void OLECompoundWriter::serializeDirectoryEntry(const DirectoryEntry& entry, QByteArray& buffer) const {
    OLECompoundSerializer(*this).serializeDirectoryEntry(entry, buffer);
}

/**
 * @brief 使用递归构建红黑树子树。
 * @param parentIndex 父节点的目录索引
 * @param children 子节点的 m_nodes 索引列表（已排序）
 * @return 子树根节点的目录索引。
 */
uint32_t OLECompoundWriter::buildChildTree(const QVector<int>& children) {
    if (children.isEmpty()) {
        return NOSTREAM;
    }

    // CFB 的 sibling tree 不只是二叉搜索树，还必须满足红黑树不变量。
    // children 已按 CFB 名称顺序排列；逐项插入并进行标准红黑树修复，
    // 可以同时保持目录顺序和所有根到叶路径相同的黑高。
    QVector<int> parent(m_directory.size(), -1);  // 红黑树节点的父节点索引。
    int root = -1;  // 当前红黑树根节点索引。

    auto left = [this](int index) {
        const uint32_t child = m_directory[index].leftChild;  // 左子节点目录索引。
        return child == NOSTREAM ? -1 : static_cast<int>(child);
    };
    auto right = [this](int index) {
        const uint32_t child = m_directory[index].rightChild;  // 右子节点目录索引。
        return child == NOSTREAM ? -1 : static_cast<int>(child);
    };
    auto setLeft = [this, &parent](int index, int child) {
        m_directory[index].leftChild = child < 0 ? NOSTREAM : static_cast<uint32_t>(child);
        if (child >= 0) {
            parent[child] = index;
        }
    };
    auto setRight = [this, &parent](int index, int child) {
        m_directory[index].rightChild = child < 0 ? NOSTREAM : static_cast<uint32_t>(child);
        if (child >= 0) {
            parent[child] = index;
        }
    };

    auto rotateLeft = [&](int pivot) {
        const int promoted = right(pivot);  // 左旋后提升的右子节点。
        const int oldParent = parent[pivot];  // 旋转前枢轴的父节点。
        setRight(pivot, left(promoted));
        parent[promoted] = oldParent;
        if (oldParent < 0) {
            root = promoted;
        } else if (left(oldParent) == pivot) {
            m_directory[oldParent].leftChild = static_cast<uint32_t>(promoted);
        } else {
            m_directory[oldParent].rightChild = static_cast<uint32_t>(promoted);
        }
        setLeft(promoted, pivot);
    };
    auto rotateRight = [&](int pivot) {
        const int promoted = left(pivot);  // 右旋后提升的左子节点。
        const int oldParent = parent[pivot];  // 旋转前枢轴的父节点。
        setLeft(pivot, right(promoted));
        parent[promoted] = oldParent;
        if (oldParent < 0) {
            root = promoted;
        } else if (left(oldParent) == pivot) {
            m_directory[oldParent].leftChild = static_cast<uint32_t>(promoted);
        } else {
            m_directory[oldParent].rightChild = static_cast<uint32_t>(promoted);
        }
        setRight(promoted, pivot);
    };

    for (int nodeIndex : children) {
        const int inserted = m_nodes[nodeIndex].dirIndex;  // 新插入节点的目录索引。
        m_directory[inserted].leftChild = NOSTREAM;
        m_directory[inserted].rightChild = NOSTREAM;
        m_directory[inserted].colorFlag = 0;  // 新节点为红色

        // 排序后的节点总是插入当前最大值的右侧。旋转只改变树形，
        // 不改变中序顺序，因此该性质在后续插入中仍成立。
        int insertionParent = -1;  // 新节点按名称顺序插入的父节点。
        int cursor = root;  // 当前查找位置。
        while (cursor >= 0) {
            insertionParent = cursor;
            cursor = right(cursor);
        }
        parent[inserted] = insertionParent;
        if (insertionParent < 0) {
            root = inserted;
        } else {
            m_directory[insertionParent].rightChild = static_cast<uint32_t>(inserted);
        }

        int current = inserted;  // 红黑树修复当前处理的节点。
        while (parent[current] >= 0 && m_directory[parent[current]].colorFlag == 0) {
            const int currentParent = parent[current];  // 当前节点的父节点。
            const int grandParent = parent[currentParent];  // 当前节点的祖父节点。
            if (currentParent == left(grandParent)) {
                const int uncle = right(grandParent);  // 当前节点的叔节点。
                if (uncle >= 0 && m_directory[uncle].colorFlag == 0) {
                    m_directory[currentParent].colorFlag = 1;
                    m_directory[uncle].colorFlag = 1;
                    m_directory[grandParent].colorFlag = 0;
                    current = grandParent;
                } else {
                    if (current == right(currentParent)) {
                        current = currentParent;
                        rotateLeft(current);
                    }
                    m_directory[parent[current]].colorFlag = 1;
                    m_directory[parent[parent[current]]].colorFlag = 0;
                    rotateRight(parent[parent[current]]);
                }
            } else {
                const int uncle = left(grandParent);  // 当前节点的叔节点。
                if (uncle >= 0 && m_directory[uncle].colorFlag == 0) {
                    m_directory[currentParent].colorFlag = 1;
                    m_directory[uncle].colorFlag = 1;
                    m_directory[grandParent].colorFlag = 0;
                    current = grandParent;
                } else {
                    if (current == left(currentParent)) {
                        current = currentParent;
                        rotateRight(current);
                    }
                    m_directory[parent[current]].colorFlag = 1;
                    m_directory[parent[parent[current]]].colorFlag = 0;
                    rotateLeft(parent[parent[current]]);
                }
            }
        }
        m_directory[root].colorFlag = 1;
    }

    return static_cast<uint32_t>(root);
}

/**
 * @brief 递归为存储节点及其子存储构建红黑树
 * @param nodeIdx 节点在 m_nodes 中的索引
 */
void OLECompoundWriter::buildSubTree(int nodeIdx) {
    StorageNode& node = m_nodes[nodeIdx];  // 当前待构建子树的存储节点。
    if (node.children.isEmpty()) {
        return;
    }

    QVector<int> sorted = node.children;  // 按 CFB 名称规则排序后的子节点索引。
    std::sort(sorted.begin(), sorted.end(), [this](int a, int b) {
        const QString& nameA = m_nodes[a].name;  // 第一个节点名称。
        const QString& nameB = m_nodes[b].name;  // 第二个节点名称。
        if (nameA.size() != nameB.size()) {
            return nameA.size() < nameB.size();
        }
        return nameA.compare(nameB, Qt::CaseInsensitive) < 0;
    });

    m_directory[node.dirIndex].child = buildChildTree(sorted);

    // 递归处理子存储节点
    for (int childIdx : node.children) {
        if (m_nodes[childIdx].dirIndex >= 0 &&
            m_directory[m_nodes[childIdx].dirIndex].objectType == ObjectType::Storage) {
            buildSubTree(childIdx);
        }
    }
}

/**
 * @brief 构建红黑树结构
 * @details 为所有存储节点构建子节点的平衡二叉树。
 *          目录条目已在 addStorage() / finalize() 中创建，无需重复创建。
 */
void OLECompoundWriter::buildRedBlackTree() {
    // 根节点已在 create() 中创建，dirIndex=0
    int rootNodeIdx = m_pathToNode.value("", -1);
    if (rootNodeIdx < 0) {
        return;
    }

    buildSubTree(rootNodeIdx);
}

/**
 * @brief 序列化文件头部（512 字节）。
 * @param header 接收文件头数据的缓冲区。
 */
void OLECompoundWriter::serializeFileHeader(QByteArray& header) const {
    OLECompoundSerializer(*this).serializeFileHeader(header);
}

/**
 * @brief 最终处理：分配扇区、构建目录、准备序列化数据
 */
void OLECompoundWriter::finalize() {
    // 根节点已在 create() 中创建并注册到 m_pathToNode[""]
    // 此处无需再创建，避免 prepend 导致索引偏移

    // 为所有流创建目录条目并分配数据
    for (const StreamData& stream : m_streams) {
        int dirIndex = createDirectoryEntry(stream.streamName, ObjectType::Stream);  // 流目录索引。
        uint64_t dataSize = static_cast<uint64_t>(stream.data.size());  // 流数据字节数。

        if (dataSize == 0) {
            // CFB 零长度流不占用普通扇区或 mini sector。
            m_directory[dirIndex].startSector = ENDOFCHAIN;
            m_directory[dirIndex].streamSize = 0;
        } else if (dataSize < MINI_STREAM_CUTOFF) {
            // 小数据写入 Mini-Stream
            uint32_t miniSectorStart =
                static_cast<uint32_t>(m_miniStream.size() / MINI_SECTOR_SIZE);  // Mini-Stream 起始扇区。
            m_miniStream.append(stream.data);
            // 对齐到 mini 扇区边界
            int remainder = stream.data.size() % MINI_SECTOR_SIZE;  // 未对齐的尾部字节数。
            if (remainder > 0) {
                m_miniStream.append(QByteArray(MINI_SECTOR_SIZE - remainder, 0));
            }
            m_directory[dirIndex].startSector = miniSectorStart;
            m_directory[dirIndex].streamSize = dataSize;

            // 更新 Mini-FAT
            uint32_t miniSectorsNeeded =
                (dataSize + MINI_SECTOR_SIZE - 1) / MINI_SECTOR_SIZE;  // 所需 Mini-FAT 扇区数。
            for (uint32_t i = 0; i < miniSectorsNeeded; ++i) {
                if (i < miniSectorsNeeded - 1) {
                    m_miniFat.append(miniSectorStart + i + 1);
                } else {
                    m_miniFat.append(ENDOFCHAIN);
                }
            }
        } else {
            // 大数据写入普通扇区
            uint32_t sectorStart = static_cast<uint32_t>(m_dataSectors.size());  // 普通数据起始扇区。
            int remaining = stream.data.size();  // 尚未写入的字节数。
            int srcOffset = 0;  // 当前源数据偏移。
            while (remaining > 0) {
                QByteArray sectorData(SECTOR_SIZE, 0);  // 当前普通扇区缓冲区。
                int chunkSize = qMin(remaining, static_cast<int>(SECTOR_SIZE));  // 当前扇区写入字节数。
                memcpy(sectorData.data(), stream.data.constData() + srcOffset, chunkSize);
                m_dataSectors.append(sectorData);
                srcOffset += chunkSize;
                remaining -= chunkSize;
            }
            m_directory[dirIndex].startSector = sectorStart;
            m_directory[dirIndex].streamSize = dataSize;

            // 更新 FAT
            uint32_t sectorsNeeded = (dataSize + SECTOR_SIZE - 1) / SECTOR_SIZE;
            for (uint32_t i = 0; i < sectorsNeeded; ++i) {
                if (i < sectorsNeeded - 1) {
                    m_fat.append(sectorStart + i + 1);
                } else {
                    m_fat.append(ENDOFCHAIN);
                }
            }
        }

        // 创建流节点并添加到正确的父节点
        StorageNode streamNode;  // 当前流对应的节点。
        streamNode.name = stream.streamName;
        streamNode.fullPath =
            stream.storagePath.isEmpty() ? stream.streamName : stream.storagePath + "/" + stream.streamName;
        streamNode.dirIndex = dirIndex;
        int streamNodeIdx = m_nodes.size();  // 流节点在节点数组中的索引。
        m_nodes.append(streamNode);

        if (stream.storagePath.isEmpty()) {
            // 根流：添加到 Root Entry 的子列表
            if (m_pathToNode.contains("")) {
                m_nodes[m_pathToNode[""]].children.append(streamNodeIdx);
            }
        } else {
            // 子流：添加到父 Storage 的子列表
            if (m_pathToNode.contains(stream.storagePath)) {
                m_nodes[m_pathToNode[stream.storagePath]].children.append(streamNodeIdx);
            }
        }
    }

    // 为 Root Entry 分配 Mini-Stream 的存储扇区
    if (!m_miniStream.isEmpty()) {
        // miniStreamSectorStart 是大数据扇区数量（数据区内的相对偏移）
        uint32_t miniStreamSectorStart = static_cast<uint32_t>(m_dataSectors.size());  // Mini-Stream 起始扇区。
        int remaining = m_miniStream.size();  // 尚未写入的 Mini-Stream 字节数。
        int srcOffset = 0;  // 当前 Mini-Stream 源偏移。
        while (remaining > 0) {
            QByteArray sectorData(SECTOR_SIZE, 0);  // 当前 Mini-Stream 扇区缓冲区。
            int chunkSize = qMin(remaining, static_cast<int>(SECTOR_SIZE));  // 当前扇区写入字节数。
            memcpy(sectorData.data(), m_miniStream.constData() + srcOffset, chunkSize);
            m_dataSectors.append(sectorData);
            srcOffset += chunkSize;
            remaining -= chunkSize;
        }
        // startSector 在 post-processing 中会加 dataOffset 转为绝对扇区号
        m_directory[0].startSector = miniStreamSectorStart;
        m_directory[0].streamSize = static_cast<uint64_t>(m_miniStream.size());

        // 更新 FAT（链式指针使用数据区相对值，post-processing 会加 dataOffset）
        uint32_t miniStreamSectors =
            (static_cast<uint32_t>(m_miniStream.size()) + SECTOR_SIZE - 1) / SECTOR_SIZE;  // Mini-Stream 扇区数。
        for (uint32_t i = 0; i < miniStreamSectors; ++i) {
            if (i < miniStreamSectors - 1) {
                m_fat.append(miniStreamSectorStart + i + 1);
            } else {
                m_fat.append(ENDOFCHAIN);
            }
        }
    }

    // 构建红黑树
    buildRedBlackTree();

    // 确保目录大小是扇区对齐的
    uint32_t dirSectors = (static_cast<uint32_t>(m_directory.size()) * DIR_ENTRY_SIZE + SECTOR_SIZE - 1) /
                          SECTOR_SIZE;  // 目录占用的扇区数。
    while (m_directory.size() < static_cast<int>(dirSectors * (SECTOR_SIZE / DIR_ENTRY_SIZE))) {
        DirectoryEntry empty;  // 用于填充目录扇区的空条目。
        empty.objectType = ObjectType::Unknown;
        empty.startSector = FREESECT;
        m_directory.append(empty);
    }

    // 确保 Mini-FAT 大小是扇区对齐的
    while (!m_miniFat.isEmpty() && m_miniFat.size() % (SECTOR_SIZE / 4) != 0) {
        m_miniFat.append(FREESECT);
    }
    uint32_t miniFatSectors =
        static_cast<uint32_t>((m_miniFat.size() * 4 + SECTOR_SIZE - 1) / SECTOR_SIZE);  // Mini-FAT 扇区数。

    // ---- 构建完整的 FAT（包含所有扇区的条目）----
    // 文件布局：FAT | DIFAT | Directory | MiniFAT | Data。
    // m_fat 目前只有数据扇区的链式条目（每个条目=一个数据扇区的后继指针）
    // 需要补充 FAT/dir/MiniFAT 的条目，并为未使用的数据扇区填 FREESECT
    uint32_t dataSectorCount = static_cast<uint32_t>(m_fat.size());  // 当前数据区扇区条目数。
    uint32_t entriesPerSector = SECTOR_SIZE / 4;  // 每个 FAT 扇区可容纳的条目数。

    // 迭代计算 FAT 扇区数（FAT 扇区自身也占条目）
    uint32_t fatSectors = 0;  // FAT 扇区数量。
    uint32_t difatSectors = 0;  // DIFAT 扇区数量。
    uint32_t totalEntries = 0;  // 完整 FAT 所需条目总数。
    for (int iter = 0; iter < 32; ++iter) {
        uint32_t newTotal =
            fatSectors + difatSectors + dirSectors + miniFatSectors + dataSectorCount;  // 本轮估算总条目数。
        newTotal = ((newTotal + entriesPerSector - 1) / entriesPerSector) * entriesPerSector;
        uint32_t newFatSectors = newTotal / entriesPerSector;  // 本轮估算 FAT 扇区数。
        uint32_t newDifatSectors =
            newFatSectors <= 109 ? 0 : (newFatSectors - 109 + (entriesPerSector - 2)) / (entriesPerSector - 1);
        if (newFatSectors == fatSectors && newDifatSectors == difatSectors && newTotal == totalEntries) {
            break;
        }
        fatSectors = newFatSectors;
        difatSectors = newDifatSectors;
        totalEntries = newTotal;
    }

    // 构建完整的 FAT 向量
    QVector<uint32_t> fullFat;
    fullFat.reserve(totalEntries);

    // [0..fatSectors-1] FAT sectors → 标记为 FATSECT
    for (uint32_t i = 0; i < fatSectors; ++i) {
        fullFat.append(FATSECT);
    }

    // DIFAT sectors are allocation-table metadata and have their own marker.
    for (uint32_t i = 0; i < difatSectors; ++i) {
        fullFat.append(DIFSECT);
    }

    const uint32_t directoryOffset = fatSectors + difatSectors;  // 目录区在文件扇区布局中的起始偏移。
    // Directory sectors → 链式
    for (uint32_t i = 0; i < dirSectors; ++i) {
        if (i < dirSectors - 1) {
            fullFat.append(directoryOffset + i + 1);
        } else {
            fullFat.append(ENDOFCHAIN);
        }
    }

    // MiniFAT sectors 也必须通过 FAT 串成链，不能只依赖物理连续性。
    const uint32_t firstMiniFatOffset = directoryOffset + dirSectors;
    for (uint32_t i = 0; i < miniFatSectors; ++i) {
        fullFat.append(i + 1 < miniFatSectors ? firstMiniFatOffset + i + 1 : ENDOFCHAIN);
    }

    // Data sectors → 来自 m_fat，需要加偏移量
    // m_fat 中的链式指针是从 0 开始的数据扇区索引，需要偏移到文件中的实际位置
    uint32_t dataOffset = fatSectors + difatSectors + dirSectors + miniFatSectors;
    for (uint32_t val : m_fat) {
        if (val == ENDOFCHAIN) {
            fullFat.append(ENDOFCHAIN);
        } else {
            fullFat.append(val + dataOffset);
        }
    }

    // 填充 FREESECT 到刚好 fatSectors 个扇区（不多不少，保证 header 和实际一致）
    uint32_t fatCapacity = fatSectors * entriesPerSector;
    while (static_cast<uint32_t>(fullFat.size()) < fatCapacity) {
        fullFat.append(FREESECT);
    }

    m_fat = fullFat;

    // 头部只能容纳 109 个 FAT sector id，其余 id 以每扇区 127 项串成 DIFAT 链。
    m_difatSectors.clear();
    for (uint32_t difatIndex = 0; difatIndex < difatSectors; ++difatIndex) {
        QByteArray sector(SECTOR_SIZE, static_cast<char>(0xFF));  // 当前 DIFAT 扇区缓冲区。
        const uint32_t firstFatIndex =
            109 + difatIndex * (entriesPerSector - 1);  // 当前 DIFAT 扇区引用的第一个 FAT 索引。
        for (uint32_t item = 0; item < entriesPerSector - 1; ++item) {
            const uint32_t fatIndex = firstFatIndex + item;  // 当前 FAT 扇区索引。
            if (fatIndex >= fatSectors) {
                break;
            }
            const int byteOffset = static_cast<int>(item * 4);  // FAT 索引在 DIFAT 扇区中的字节偏移。
            sector[byteOffset] = static_cast<char>(fatIndex & 0xFF);
            sector[byteOffset + 1] = static_cast<char>((fatIndex >> 8) & 0xFF);
            sector[byteOffset + 2] = static_cast<char>((fatIndex >> 16) & 0xFF);
            sector[byteOffset + 3] = static_cast<char>((fatIndex >> 24) & 0xFF);
        }
        const uint32_t next =
            difatIndex + 1 < difatSectors ? fatSectors + difatIndex + 1 : ENDOFCHAIN;  // 下一个 DIFAT 扇区索引。
        const int nextOffset = static_cast<int>((entriesPerSector - 1) * 4);  // 链接字段的字节偏移。
        sector[nextOffset] = static_cast<char>(next & 0xFF);
        sector[nextOffset + 1] = static_cast<char>((next >> 8) & 0xFF);
        sector[nextOffset + 2] = static_cast<char>((next >> 16) & 0xFF);
        sector[nextOffset + 3] = static_cast<char>((next >> 24) & 0xFF);
        m_difatSectors.append(sector);
    }

    // Directory stream start sectors must be absolute CFB sector numbers.
    // Mini-stream entries keep their offset inside the root mini stream.
    for (DirectoryEntry& entry : m_directory) {
        if (entry.objectType == ObjectType::Root && entry.streamSize > 0) {
            entry.startSector += dataOffset;
        } else if (entry.objectType == ObjectType::Stream && entry.streamSize >= MINI_STREAM_CUTOFF &&
                   entry.startSector != ENDOFCHAIN) {
            entry.startSector += dataOffset;
        }
    }
}

/**
 * @brief 保存到文件。
 * @param filePath 输出文件路径。
 * @return 保存是否成功。
 */
bool OLECompoundWriter::saveToFile(const QString& filePath) {
    if (!m_initialized) {
        qWarning() << "OLECompoundWriter::saveToFile: Not initialized";
        return false;
    }

    if (hasError()) {
        qWarning() << "OLECompoundWriter::saveToFile: Aborted due to structure error:" << errorString();
        return false;
    }

    if (!m_finalized) {
        finalize();
        m_finalized = true;
    }

    qDebug() << "OLECompoundWriter::saveToFile: Writing to" << filePath << "streams:" << m_streams.size()
             << "directory:" << m_directory.size() << "fat:" << m_fat.size();

    // 确保父目录存在
    QFileInfo fileInfo(filePath);
    if (!fileInfo.dir().exists()) {
        qDebug() << "OLECompoundWriter::saveToFile: Creating parent directory:" << fileInfo.path();
        QDir().mkpath(fileInfo.path());
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "OLECompoundWriter::saveToFile: Failed to open file:" << filePath
                   << "error:" << file.errorString() << "exists:" << QFile::exists(filePath)
                   << "isDir:" << QFileInfo(filePath).isDir();
        return false;
    }

    // 写入头部（512 字节）
    QByteArray header;
    serializeFileHeader(header);
    auto writeAll = [&file, &filePath](const QByteArray& bytes) {
        if (file.write(bytes) == bytes.size()) {
            return true;
        }
        qWarning() << "OLECompoundWriter::saveToFile: Short write:" << filePath << file.errorString();
        return false;
    };
    if (!writeAll(header)) {
        file.cancelWriting();
        return false;
    }

    // 写入 FAT 扇区（每个扇区 128 个条目）
    uint32_t fatSectorCount = static_cast<uint32_t>(m_fat.size()) / (SECTOR_SIZE / 4);  // FAT 扇区数量。
    uint32_t entriesPerSector = SECTOR_SIZE / 4;  // 每个 FAT 扇区的条目数。
    for (uint32_t i = 0; i < fatSectorCount; ++i) {
        QByteArray sectorData(SECTOR_SIZE, 0);  // 当前 FAT 扇区缓冲区。
        uint32_t startIdx = i * entriesPerSector;  // 当前 FAT 扇区的首条目索引。
        for (uint32_t j = 0; j < entriesPerSector && (startIdx + j) < static_cast<uint32_t>(m_fat.size()); ++j) {
            uint32_t val = m_fat[startIdx + j];  // 当前 FAT 条目值。
            int off = j * 4;  // 条目在扇区中的字节偏移。
            sectorData[off] = static_cast<char>(val & 0xFF);
            sectorData[off + 1] = static_cast<char>((val >> 8) & 0xFF);
            sectorData[off + 2] = static_cast<char>((val >> 16) & 0xFF);
            sectorData[off + 3] = static_cast<char>((val >> 24) & 0xFF);
        }
        if (!writeAll(sectorData)) {
            file.cancelWriting();
            return false;
        }
    }

    for (const QByteArray& sector : m_difatSectors) {
        if (!writeAll(sector)) {
            file.cancelWriting();
            return false;
        }
    }

    // 写入目录扇区
    QByteArray dirData;
    for (const DirectoryEntry& entry : m_directory) {
        QByteArray entryData;  // 当前目录条目的序列化数据。
        serializeDirectoryEntry(entry, entryData);
        dirData.append(entryData);
    }
    if (!writeAll(dirData)) {
        file.cancelWriting();
        return false;
    }

    // 写入 Mini-FAT 扇区
    if (!m_miniFat.isEmpty()) {
        QByteArray miniFatData;  // Mini-FAT 序列化数据。
        for (uint32_t val : m_miniFat) {
            miniFatData.append(static_cast<char>(val & 0xFF));
            miniFatData.append(static_cast<char>((val >> 8) & 0xFF));
            miniFatData.append(static_cast<char>((val >> 16) & 0xFF));
            miniFatData.append(static_cast<char>((val >> 24) & 0xFF));
        }
        if (!writeAll(miniFatData)) {
            file.cancelWriting();
            return false;
        }
    }

    // 写入数据扇区
    for (const QByteArray& sector : m_dataSectors) {
        if (!writeAll(sector)) {
            file.cancelWriting();
            return false;
        }
    }

    if (!file.commit()) {
        qWarning() << "OLECompoundWriter::saveToFile: Commit failed:" << filePath << file.errorString();
        return false;
    }
    return true;
}

// ---- 序列化辅助方法 ----

}  // namespace EasyKiConverter
