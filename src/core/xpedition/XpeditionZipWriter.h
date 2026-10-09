#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

namespace EasyKiConverter {

/**
 * @brief 写入不压缩的 ZIP 文件
 *
 * Xpedition 库由多个文本文件组成，使用不压缩 ZIP 保留文件边界并避免
 * 引入额外的运行时依赖。写入器只接受相对文件名，拒绝目录穿越路径。
 */
class XpeditionZipWriter {
public:
    /**
     * @brief 添加一个文本或二进制文件条目。
     * @param name ZIP 内的相对条目名称。
     * @param data 条目内容。
     * @return 名称安全且未重复时返回 true。
     */
    bool addFile(const QString& name, const QByteArray& data);

    /**
     * @brief 将当前条目写入 ZIP 文件。
     * @param filePath 输出文件路径。
     * @return 文件创建并写入成功时返回 true。
     */
    bool write(const QString& filePath) const;

private:
    struct Entry {
        QString name;  ///< ZIP 内的相对条目名称。
        QByteArray data;  ///< 条目内容。
    };

    QVector<Entry> m_entries;  ///< 待写入 ZIP 的条目列表。
};

}  // namespace EasyKiConverter
