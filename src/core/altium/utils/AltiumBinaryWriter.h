#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QVector>

namespace EasyKiConverter {

/**
 * @brief Altium 二进制格式写入器
 * @details 集中实现小端整数、长度前缀块及参数块，避免上层写入器自行拼接协议字节。
 *          所有数据以小端序 (Little-Endian) 写入。
 *
 * 块格式：[u32: (flags << 24) | payload_length][payload]
 * 字符串块：[u32 block_size][u8 str_len][bytes]
 * 参数块：[u32 size]["|KEY=VALUE|...\0"]
 */
class AltiumBinaryWriter {
public:
    /**
     * @brief 构造函数
     * @param buffer 输出缓冲区引用
     */
    explicit AltiumBinaryWriter(QByteArray& buffer);

    // ---- 基础类型写入 ----

    void writeInt8(int8_t value);
    void writeUInt8(uint8_t value);
    void writeInt16(int16_t value);
    void writeUInt16(uint16_t value);
    void writeInt32(int32_t value);
    void writeUInt32(uint32_t value);
    void writeFloat(float value);
    void writeDouble(double value);
    void writeBytes(const QByteArray& data);

    // ---- Altium 特定格式 ----

    /**
     * @brief 开始一个大小前缀块
     * @param flags 标志字节（高 8 位），默认 0
     * @details 写入 4 字节占位符，endBlock() 时回填实际大小
     */
    void beginBlock(uint8_t flags = 0);

    /**
     * @brief 结束大小前缀块，回填块大小
     */
    void endBlock();

    /**
     * @brief 写入 Pascal 短字符串
     * @param str 待写入的字符串
     * @details [u8 length][bytes (Windows-1252)]
     */
    void writePascalShortString(const QString& str);

    /**
     * @brief 写入字符串块
     * @param str 待写入的字符串
     * @details [u32 block_size = 1 + len][u8 str_len][bytes]
     */
    void writeStringBlock(const QString& str);

    /**
     * @brief 写入带额外 NUL 的 Pascal 字符串块（SectionKeys 名称格式）
     * @param str 待写入的字符串
     */
    void writePascalString(const QString& str);

    /**
     * @brief 写入 C 字符串参数块
     * @param params 参数键值对（按键排序）
     * @details [u32 size]["|KEY1=VAL1|KEY2=VAL2|...\0"]
     */
    void writeCStringParameterBlock(const QMap<QString, QString>& params);

    /**
     * @brief 写入 C 字符串参数块（自动处理非 ASCII 字符）
     * @param params 参数键值对
     * @details 对包含非 ASCII 字符的值，额外追加 %UTF8%KEY=VALUE 参数，
     *          确保 Altium 打开时能正确还原 Unicode 文本。
     */
    void writeCStringParameterBlockUtf8(const QMap<QString, QString>& params);

    /**
     * @brief 写入原始 C 字符串参数块（保留键的顺序）
     * @param paramString 已格式化的参数字符串（含管道分隔符）
     */
    void writeCStringParameterBlockRaw(const QString& paramString);
    void writeCStringParameterBlockRaw(const QByteArray& parameterBytes);

    /**
     * @brief 写入坐标点（两个 i32）
     * @param x X 坐标
     * @param y Y 坐标
     */
    void writeCoordPoint(int32_t x, int32_t y);

    /**
     * @brief 写入坐标值（一个 i32）
     * @param value 坐标值
     */
    void writeCoord(int32_t value);

    /**
     * @brief 获取当前写入位置
     */
    int position() const;

private:
    QByteArray& m_buffer;  ///< 写入目标缓冲区引用。
    QVector<int> m_blockPositions;  ///< beginBlock 时的缓冲区位置栈
};

}  // namespace EasyKiConverter
