#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>

#include <cstdint>

namespace EasyKiConverter {

/**
 * @brief Altium 二进制格式读取器
 * @details 集中实现小端整数、长度前缀块、Pascal 字符串和参数块读取，
 *          所有操作都会检查输入边界，避免损坏库文件导致越界访问。
 */
class AltiumBinaryReader {
public:
    /**
     * @brief 构造读取器
     * @param buffer 待读取的数据
     */
    explicit AltiumBinaryReader(const QByteArray& buffer);

    /**
     * @brief 读取有符号 8 位整数
     * @param value 用于接收整数的输出指针
     * @return 读取成功时返回 true
     */
    bool readInt8(int8_t* value);
    /**
     * @brief 读取无符号 8 位整数
     * @param value 用于接收整数的输出指针
     * @return 读取成功时返回 true
     */
    bool readUInt8(uint8_t* value);
    /**
     * @brief 读取有符号 16 位整数
     * @param value 用于接收整数的输出指针
     * @return 读取成功时返回 true
     */
    bool readInt16(int16_t* value);
    /**
     * @brief 读取无符号 16 位整数
     * @param value 用于接收整数的输出指针
     * @return 读取成功时返回 true
     */
    bool readUInt16(uint16_t* value);
    /**
     * @brief 读取有符号 32 位整数
     * @param value 用于接收整数的输出指针
     * @return 读取成功时返回 true
     */
    bool readInt32(int32_t* value);
    /**
     * @brief 读取无符号 32 位整数
     * @param value 用于接收整数的输出指针
     * @return 读取成功时返回 true
     */
    bool readUInt32(uint32_t* value);
    /**
     * @brief 读取单精度浮点数
     * @param value 用于接收浮点数的输出指针
     * @return 读取成功时返回 true
     */
    bool readFloat(float* value);
    /**
     * @brief 读取双精度浮点数
     * @param value 用于接收浮点数的输出指针
     * @return 读取成功时返回 true
     */
    bool readDouble(double* value);

    /**
     * @brief 读取指定长度的原始字节
     * @param size 字节数
     * @param value 输出数据
     */
    bool readBytes(int size, QByteArray* value);

    /**
     * @brief 读取 Altium 长度前缀块
     * @param payload 输出块内容
     * @param flags 输出高字节标志；可传 nullptr
     */
    bool readBlock(QByteArray* payload, uint8_t* flags = nullptr);

    /**
     * @brief 读取 Pascal 短字符串
     */
    bool readPascalShortString(QString* value);
    /**
     * @brief 读取字符串块
     */
    bool readStringBlock(QString* value);
    /**
     * @brief 读取带额外 NUL 的 Pascal 字符串块
     */
    bool readPascalString(QString* value);

    /**
     * @brief 读取 C 字符串参数块
     * @param params 用于接收参数键值表的输出指针
     * @details 自动应用 `%UTF8%KEY=VALUE` 对对应 ANSI 值的覆盖。
     * @return 读取成功时返回 true
     */
    bool readCStringParameterBlock(QMap<QString, QString>* params);
    /**
     * @brief 解析已经去除长度前缀的 C 字符串参数数据
     * @param data 参数数据，不包含外层长度和标志字段
     * @param params 输出参数
     * @details 自动应用 `%UTF8%KEY=VALUE` 对应的 UTF-8 值覆盖 ANSI 值。
     */
    bool parseCStringParameterData(const QByteArray& data, QMap<QString, QString>* params);

    /**
     * @brief 获取当前读取位置
     */
    int position() const;
    /**
     * @brief 获取剩余字节数
     */
    int remaining() const;
    /**
     * @brief 获取是否发生读取错误
     */
    bool hasError() const;
    /**
     * @brief 获取读取错误说明
     */
    QString errorString() const;

private:
    bool ensure(int size, const QString& operation);
    bool fail(const QString& message);
    bool readUInt32Internal(uint32_t* value);

    const QByteArray& m_buffer;  ///< 待读取的二进制缓冲区引用。
    int m_position = 0;  ///< 当前读取偏移。
    QString m_errorMessage;  ///< 最近一次读取错误说明。
};

}  // namespace EasyKiConverter
