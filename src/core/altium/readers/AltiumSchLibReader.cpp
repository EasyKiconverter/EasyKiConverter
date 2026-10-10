#include "AltiumSchLibReader.h"

#include "core/altium/utils/AltiumBinaryReader.h"
#include "core/altium/utils/AltiumWriterUtils.h"

#include <QSet>

#include <cmath>
#include <zlib.h>

namespace EasyKiConverter {

namespace {

/**
 * @brief 读取整数参数，缺失或格式错误时返回调用方提供的回退值
 * @param parameters 参数键值表
 * @param name 参数名称
 * @param fallback 参数缺失或无效时的回退值
 * @return 参数值或回退值
 */
int parameterInt(const QMap<QString, QString>& parameters, const QString& name, int fallback) {
    bool ok = false;  // 参数转换是否成功。
    const int value = parameters.value(name).toInt(&ok);  // 转换后的整数值。
    return ok ? value : fallback;
}

/**
 * @brief 读取可选整数参数，并区分缺失字段与格式错误
 * @param parameters 参数键值表
 * @param name 参数名称
 * @param fallback 参数缺失时的回退值
 * @param value 用于接收结果的输出指针
 * @return 参数读取成功时返回 true
 */
bool readOptionalParameterInt(const QMap<QString, QString>& parameters, const QString& name, int fallback, int* value) {
    if (value == nullptr)
        return false;
    if (!parameters.contains(name)) {
        *value = fallback;
        return true;
    }
    bool ok = false;  // 参数转换是否成功。
    const int parsed = parameters.value(name).toInt(&ok);  // 转换后的整数值。
    if (!ok)
        return false;
    *value = parsed;
    return true;
}

/**
 * @brief 按 SchLib 记录类型校验图元几何参数、边界和枚举范围
 * @param parameters 图元参数键值表
 * @param recordType SchLib 记录类型
 * @param error 用于接收具体错误说明的输出指针
 * @return 参数有效时返回 true
 */
bool validateGraphicParameters(const QMap<QString, QString>& parameters, int recordType, QString* error) {
    const auto readInt = [&parameters](const QString& name, bool required, int fallback, int* value) {
        if (value == nullptr)
            return false;
        if (!parameters.contains(name)) {
            if (required)
                return false;
            *value = fallback;
            return true;
        }
        bool ok = false;  // 参数转换是否成功。
        const int parsed = parameters.value(name).toInt(&ok);  // 转换后的整数值。
        if (!ok)
            return false;
        *value = parsed;
        return true;
    };
    const auto failValidation = [error](const QString& message) {
        if (error != nullptr)
            *error = message;
        return false;
    };
    const auto readCoordinate = [&readInt](const QString& name, qint64* value) {
        if (value == nullptr)
            return false;
        int integer = 0;  // 坐标整数部分。
        int fraction = 0;  // 坐标小数部分。
        if (!readInt(name, true, 0, &integer) || !readInt(name + QStringLiteral("_Frac"), false, 0, &fraction) ||
            fraction <= -100000 || fraction >= 100000)
            return false;
        *value = static_cast<qint64>(integer) * 100000 + fraction;
        return true;
    };
    const auto readFiniteDouble = [&parameters](const QString& name) {
        if (!parameters.contains(name))
            return true;
        bool ok = false;  // 浮点转换是否成功。
        const double value = parameters.value(name).toDouble(&ok);  // 转换后的浮点值。
        return ok && std::isfinite(value);
    };
    const auto validateBounds = [&]() {
        const QStringList coordinateNames = {QStringLiteral("Location.X"),  // 边界坐标字段名称。
                                             QStringLiteral("Location.Y"),
                                             QStringLiteral("Corner.X"),
                                             QStringLiteral("Corner.Y")};
        bool hasCoordinate = false;  // 是否存在任一边界坐标字段。
        for (const QString& name : coordinateNames)
            hasCoordinate = hasCoordinate || parameters.contains(name);
        if (!hasCoordinate)
            return true;

        qint64 locationX = 0;  // 左上角 X 坐标。
        qint64 locationY = 0;  // 左上角 Y 坐标。
        qint64 cornerX = 0;  // 右下角 X 坐标。
        qint64 cornerY = 0;  // 右下角 Y 坐标。
        if (!readCoordinate(QStringLiteral("Location.X"), &locationX) ||
            !readCoordinate(QStringLiteral("Location.Y"), &locationY) ||
            !readCoordinate(QStringLiteral("Corner.X"), &cornerX) ||
            !readCoordinate(QStringLiteral("Corner.Y"), &cornerY))
            return false;
        return locationX != cornerX && locationY != cornerY;
    };

    if (recordType == 5 || recordType == 6 || recordType == 7) {
        const int minimumCount = recordType == 5 ? 4 : (recordType == 7 ? 3 : 2);  // 该图元的最少顶点数。
        int locationCount = 0;  // 图元顶点数量。
        if (!readInt(QStringLiteral("LocationCount"), true, 0, &locationCount) || locationCount < minimumCount ||
            locationCount > 100000) {
            return failValidation(QStringLiteral("LocationCount 无效"));
        }
        for (int i = 1; i <= locationCount; ++i) {
            for (const QString& axis : {QStringLiteral("X"), QStringLiteral("Y")}) {
                const QString name = QStringLiteral("%1%2").arg(axis).arg(i);  // 坐标字段名称。
                int coordinate = 0;  // 当前坐标值。
                if (!readInt(name, false, 0, &coordinate))
                    return failValidation(QStringLiteral("%1 坐标无效").arg(name));
            }
        }
    }

    if (recordType == 8 || recordType == 9 || recordType == 11 || recordType == 12) {
        int radius = 0;  // 主半径。
        if (!readInt(QStringLiteral("Radius"), true, 0, &radius) || radius <= 0)
            return failValidation(QStringLiteral("Radius 无效"));
        if (recordType == 8 || recordType == 11) {
            int secondaryRadius = 0;  // 第二半径。
            const bool required = recordType == 11;  // 是否必须存在第二半径。
            if (!readInt(QStringLiteral("SecondaryRadius"), required, 0, &secondaryRadius) ||
                (parameters.contains(QStringLiteral("SecondaryRadius")) && secondaryRadius <= 0)) {
                return failValidation(QStringLiteral("SecondaryRadius 无效"));
            }
        }
    }

    if ((recordType == 9 || recordType == 11 || recordType == 12) &&
        (!readFiniteDouble(QStringLiteral("StartAngle")) || !readFiniteDouble(QStringLiteral("EndAngle"))))
        return failValidation(QStringLiteral("圆弧角度无效"));

    if (recordType == 3) {
        int symbol = 0;  // IEEE 符号编号。
        if (!readInt(QStringLiteral("Symbol"), false, 0, &symbol) || symbol < 0 || symbol > 34)
            return failValidation(QStringLiteral("IEEE 符号编号无效"));
        int scaleFactor = 0;  // IEEE 缩放因子。
        if (!readInt(QStringLiteral("ScaleFactor"), false, 1, &scaleFactor) || scaleFactor < 1)
            return failValidation(QStringLiteral("IEEE 缩放因子无效"));
        int lineWidth = 0;  // IEEE 线宽索引。
        if (!readInt(QStringLiteral("LineWidth"), false, 0, &lineWidth) || lineWidth < 0 || lineWidth > 3)
            return failValidation(QStringLiteral("IEEE 线宽索引无效"));
        int orientation = 0;  // IEEE 方向编码。
        if (!readInt(QStringLiteral("Orientation"), false, 0, &orientation) || orientation < 0 || orientation > 3)
            return failValidation(QStringLiteral("IEEE 方向无效"));
    }

    if (recordType == 10) {
        if (!validateBounds())
            return failValidation(QStringLiteral("圆角矩形边界尺寸无效"));
        for (const QString& name : {QStringLiteral("CornerXRadius"), QStringLiteral("CornerYRadius")}) {
            int radius = 0;  // 圆角半径。
            if (!readInt(name, false, 0, &radius) || radius < 0)
                return failValidation(QStringLiteral("%1 无效").arg(name));
        }
    }
    if (recordType == 13 && !validateBounds())
        return failValidation(QStringLiteral("线段边界尺寸无效"));
    if (recordType == 30 && !validateBounds())
        return failValidation(QStringLiteral("图片边界尺寸无效"));
    if (recordType == 30 && !readFiniteDouble(QStringLiteral("Rotation")))
        return failValidation(QStringLiteral("图片旋转角度无效"));
    if (recordType == 14 && !validateBounds())
        return failValidation(QStringLiteral("矩形边界尺寸无效"));
    if (recordType == 28) {
        if (!validateBounds())
            return failValidation(QStringLiteral("文本框边界尺寸无效"));
        int textMargin = 0;  // 文本框整数边距。
        int textMarginFraction = 0;  // 文本框小数边距。
        if (!readInt(QStringLiteral("TextMargin"), false, 0, &textMargin) ||
            !readInt(QStringLiteral("TextMargin_Frac"), false, 0, &textMarginFraction) ||
            textMarginFraction <= -100000 || textMarginFraction >= 100000 ||
            (textMargin < 0 || (textMargin == 0 && textMarginFraction < 0)))
            return failValidation(QStringLiteral("TextMargin 无效"));
    }
    return true;
}

/**
 * @brief 从二进制引脚或图元记录头部读取类型、部件归属和显示模式
 * @param payload 记录头部载荷
 * @param recordType 用于接收记录类型的输出指针
 * @param ownerPartId 用于接收部件编号的输出指针
 * @param ownerPartDisplayMode 用于接收部件显示模式的可选输出指针
 * @return 记录头部读取成功时返回 true
 */
bool readBinaryRecordMetadata(const QByteArray& payload,
                              int* recordType,
                              int* ownerPartId,
                              int* ownerPartDisplayMode = nullptr) {
    if (recordType == nullptr || ownerPartId == nullptr || payload.size() < 7)
        return false;
    AltiumBinaryReader reader(payload);
    qint32 type = -1;  // 记录类型。
    quint8 unknown = 0;  // 记录头部保留字节。
    qint16 owner = -1;  // 所属部件编号。
    if (!reader.readInt32(&type) || !reader.readUInt8(&unknown) || !reader.readInt16(&owner))
        return false;
    *recordType = type;
    *ownerPartId = owner;
    if (ownerPartDisplayMode != nullptr)
        *ownerPartDisplayMode = -1;
    if (ownerPartDisplayMode != nullptr && type == 2 && payload.size() >= 8) {
        AltiumBinaryReader displayReader(payload);
        qint32 ignoredType = -1;  // 显示模式读取时忽略的记录类型。
        quint8 ignoredUnknown = 0;  // 显示模式读取时忽略的保留字节。
        qint16 ignoredOwner = -1;  // 显示模式读取时忽略的部件编号。
        quint8 displayMode = 0;  // 部件显示模式。
        if (!displayReader.readInt32(&ignoredType) || !displayReader.readUInt8(&ignoredUnknown) ||
            !displayReader.readInt16(&ignoredOwner) || !displayReader.readUInt8(&displayMode))
            return false;
        *ownerPartDisplayMode = displayMode;
    }
    return true;
}

constexpr qint64 kMaxImageDecompressedSize = 512LL * 1024LL * 1024LL;  ///< 图片允许的最大解压字节数。

/**
 * @brief 校验图片 Storage 的 zlib 数据，并限制解压后的最大尺寸
 * @param compressedData 压缩图片数据
 * @return 数据可解压且解压大小在限制内时返回 true
 */
bool isValidZlibPayload(const QByteArray& compressedData) {
    if (compressedData.isEmpty())
        return false;

    z_stream stream{};  // zlib 解压状态。
    if (inflateInit(&stream) != Z_OK)
        return false;

    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressedData.constData()));
    stream.avail_in = static_cast<uInt>(compressedData.size());
    QByteArray buffer(8192, Qt::Uninitialized);  // 解压输出缓冲区。
    qint64 decompressedSize = 0;  // 已解压字节数。
    int result = Z_OK;  // 最近一次 zlib 操作结果。
    while (result == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());
        result = inflate(&stream, Z_NO_FLUSH);
        decompressedSize += buffer.size() - stream.avail_out;
        if (decompressedSize > kMaxImageDecompressedSize) {
            inflateEnd(&stream);
            return false;
        }
    }

    const bool valid =
        result == Z_STREAM_END && stream.avail_in == 0 && decompressedSize > 0;  // 解压结果是否完整有效。
    inflateEnd(&stream);
    return valid;
}

}  // namespace

/**
 * @brief 清理已读取内容并记录 SchLib 解析失败原因
 * @param message 解析失败说明
 * @return 始终返回 false，便于错误路径直接返回
 */
bool AltiumSchLibReader::fail(const QString& message) {
    m_components.clear();
    m_headerParameters.clear();
    m_fonts.clear();
    m_errorMessage = message;
    return false;
}

/**
 * @brief 打开 SchLib 容器，解析文件头、字体表、组件目录和存储键
 * @param filePath SchLib 文件路径
 * @return 文件结构和组件映射有效时返回 true
 */
bool AltiumSchLibReader::open(const QString& filePath) {
    m_components.clear();
    m_headerParameters.clear();
    m_fonts.clear();
    m_errorMessage.clear();

    if (!m_oleReader.open(filePath))
        return fail(m_oleReader.errorString());

    QByteArray headerData;  // FileHeader 流数据。
    if (!m_oleReader.readStream(QStringLiteral("FileHeader"), &headerData))
        return fail(QStringLiteral("SchLib 缺少 FileHeader 流"));

    AltiumBinaryReader headerReader(headerData);
    if (!headerReader.readCStringParameterBlock(&m_headerParameters))
        return fail(QStringLiteral("SchLib FileHeader 参数块无效: %1").arg(headerReader.errorString()));

    bool fontCountOk = true;  // 字体数量转换是否成功。
    const int fontCount = m_headerParameters.contains(QStringLiteral("FontIdCount"))  // 字体表条目数量。
                              ? m_headerParameters.value(QStringLiteral("FontIdCount")).toInt(&fontCountOk)
                              : 0;
    if (!fontCountOk || fontCount < 0)
        return fail(QStringLiteral("SchLib FileHeader 的 FontIdCount 无效"));
    m_fonts.reserve(fontCount);
    for (int i = 1; i <= fontCount; ++i) {
        FontInfo font;  // 当前字体表条目。
        font.name = m_headerParameters.value(QStringLiteral("FontName%1").arg(i));
        font.size = parameterInt(m_headerParameters, QStringLiteral("Size%1").arg(i), 0);
        font.bold = m_headerParameters.value(QStringLiteral("Bold%1").arg(i)) == QStringLiteral("T");
        font.italic = m_headerParameters.value(QStringLiteral("Italic%1").arg(i)) == QStringLiteral("T");
        font.underline = m_headerParameters.value(QStringLiteral("Underline%1").arg(i)) == QStringLiteral("T");
        if (font.name.isEmpty() || font.size <= 0)
            return fail(QStringLiteral("SchLib FileHeader 的字体 %1 无效").arg(i));
        m_fonts.append(font);
    }

    bool componentCountOk = false;  // 组件数量转换是否成功。
    const int componentCount =
        m_headerParameters.value(QStringLiteral("COMPCOUNT")).toInt(&componentCountOk);  // 组件数量。
    if (!componentCountOk || componentCount < 0)
        return fail(QStringLiteral("SchLib FileHeader 的 COMPCOUNT 无效"));

    int32_t serializedCount = 0;  // FileHeader 序列化的组件数量。
    if (!headerReader.readInt32(&serializedCount) || serializedCount != componentCount)
        return fail(QStringLiteral("SchLib FileHeader 的组件数量不一致"));

    QVector<QString> names;  // 组件名称列表。
    names.reserve(componentCount);
    QSet<QString> componentNames;  // 大小写折叠后的组件名称集合。
    for (int i = 0; i < componentCount; ++i) {
        QString name;  // 当前组件名称。
        if (!headerReader.readStringBlock(&name) || name.isEmpty())
            return fail(QStringLiteral("SchLib FileHeader 的组件名称无效"));
        const QString foldedName = name.toCaseFolded();  // 用于重复检查的折叠名称。
        if (componentNames.contains(foldedName))
            return fail(QStringLiteral("SchLib FileHeader 的组件名称重复: %1").arg(name));
        componentNames.insert(foldedName);
        names.append(name);
    }
    if (headerReader.hasError() || headerReader.remaining() != 0)
        return fail(QStringLiteral("SchLib FileHeader 末尾包含无效数据"));

    QMap<QString, QString> sectionKeys;  // 组件名称到 OLE 节名称的映射。
    if (m_oleReader.containsStream(QStringLiteral("SectionKeys"))) {
        QByteArray sectionKeyData;  // SectionKeys 流数据。
        if (!m_oleReader.readStream(QStringLiteral("SectionKeys"), &sectionKeyData))
            return fail(QStringLiteral("无法读取 SchLib SectionKeys 流"));
        AltiumBinaryReader sectionKeyReader(sectionKeyData);
        QMap<QString, QString> parameters;  // SectionKeys 参数表。
        if (!sectionKeyReader.readCStringParameterBlock(&parameters))
            return fail(QStringLiteral("SchLib SectionKeys 参数块无效: %1").arg(sectionKeyReader.errorString()));
        bool keyCountOk = false;  // 映射数量转换是否成功。
        const int keyCount = parameters.value(QStringLiteral("KeyCount")).toInt(&keyCountOk);  // 映射数量。
        if (!keyCountOk || keyCount < 0)
            return fail(QStringLiteral("SchLib SectionKeys 的 KeyCount 无效"));
        QSet<QString> mappedComponents;  // 已映射的组件名称集合。
        QSet<QString> mappedSectionKeys;  // 已使用的节名称集合。
        for (int i = 0; i < keyCount; ++i) {
            const QString libRef = parameters.value(QStringLiteral("LibRef%1").arg(i));  // 组件名称。
            const QString sectionKey = parameters.value(QStringLiteral("SectionKey%1").arg(i));  // OLE 节名称。
            if (libRef.isEmpty() || sectionKey.isEmpty())
                return fail(QStringLiteral("SchLib SectionKeys 映射不完整"));
            if (!names.contains(libRef))
                return fail(QStringLiteral("SchLib SectionKeys 包含未知组件: %1").arg(libRef));
            if (mappedComponents.contains(libRef.toCaseFolded()) ||
                mappedSectionKeys.contains(sectionKey.toCaseFolded()))
                return fail(QStringLiteral("SchLib SectionKeys 映射重复: %1").arg(libRef));
            mappedComponents.insert(libRef.toCaseFolded());
            mappedSectionKeys.insert(sectionKey.toCaseFolded());
            sectionKeys.insert(libRef, sectionKey);
        }
        if (sectionKeyReader.hasError() || sectionKeyReader.remaining() != 0)
            return fail(QStringLiteral("SchLib SectionKeys 末尾包含无效数据"));
    }

    QSet<QString> resolvedSectionKeys;  // 已解析的组件存储键集合。
    for (int i = 0; i < names.size(); ++i) {
        const QString& name = names.at(i);  // 当前组件名称。
        const QString sectionKey = sectionKeys.value(name, name);  // 当前组件存储键。
        const QString foldedSectionKey = sectionKey.toCaseFolded();  // 用于重复检查的折叠存储键。
        if (resolvedSectionKeys.contains(foldedSectionKey))
            return fail(QStringLiteral("SchLib 组件存储键重复: %1").arg(sectionKey));
        resolvedSectionKeys.insert(foldedSectionKey);
        const QString dataPath = sectionKey + QStringLiteral("/Data");  // 组件 Data 流路径。
        if (!m_oleReader.containsStream(dataPath))
            return fail(QStringLiteral("SchLib 缺少组件 Data 流: %1").arg(dataPath));
        const int serializedPartCount =
            parameterInt(m_headerParameters, QStringLiteral("PARTCOUNT%1").arg(i), 2);  // 序列化部件数量。
        if (serializedPartCount < 1)
            return fail(QStringLiteral("SchLib 组件 %1 的 PARTCOUNT 无效").arg(name));
        m_components.append({name, sectionKey, qMax(1, serializedPartCount - 1)});
    }
    return true;
}

/**
 * @brief 返回当前库中解析出的组件目录信息
 * @return 组件目录项列表
 */
QVector<AltiumSchLibReader::ComponentInfo> AltiumSchLibReader::components() const {
    return m_components;
}

/**
 * @brief 返回 FileHeader 参数表
 * @return 文件头参数键值表
 */
QMap<QString, QString> AltiumSchLibReader::headerParameters() const {
    return m_headerParameters;
}

/**
 * @brief 返回 FileHeader 声明的字体表
 * @return 字体表条目列表
 */
QVector<AltiumSchLibReader::FontInfo> AltiumSchLibReader::fonts() const {
    return m_fonts;
}

/**
 * @brief 按组件名称查找其在库目录中的索引
 * @param componentName 组件名称
 * @return 组件索引；未找到时返回 -1
 */
int AltiumSchLibReader::componentIndex(const QString& componentName) const {
    for (int i = 0; i < m_components.size(); ++i) {
        if (m_components.at(i).name == componentName)
            return i;
    }
    return -1;
}

/**
 * @brief 按组件索引读取其 Data 流的原始内容
 * @param componentIndexValue 组件目录项索引
 * @param data 用于接收流数据的输出指针
 * @return 读取成功时返回 true
 */
bool AltiumSchLibReader::readComponentData(int componentIndexValue, QByteArray* data) const {
    if (data == nullptr || componentIndexValue < 0 || componentIndexValue >= m_components.size())
        return false;
    return m_oleReader.readStream(m_components.at(componentIndexValue).sectionKey + QStringLiteral("/Data"), data);
}

/**
 * @brief 按组件名称读取其 Data 流的原始内容
 * @param componentName 组件名称
 * @param data 用于接收流数据的输出指针
 * @return 读取成功时返回 true
 */
bool AltiumSchLibReader::readComponentData(const QString& componentName, QByteArray* data) const {
    return readComponentData(componentIndex(componentName), data);
}

/**
 * @brief 解析组件 Data 流中的参数记录和二进制记录，并校验索引连续性
 * @param componentIndexValue 组件目录项索引
 * @param records 用于接收记录列表的输出指针
 * @return 解析成功时返回 true
 */
bool AltiumSchLibReader::readComponentRecords(int componentIndexValue, QVector<Record>* records) const {
    if (records == nullptr) {
        m_errorMessage = QStringLiteral("读取 SchLib 记录时输出容器为空");
        return false;
    }
    records->clear();
    m_errorMessage.clear();

    const auto failRead = [this, records](const QString& message) {
        records->clear();
        m_errorMessage = message;
        return false;
    };

    QByteArray data;  // 组件 Data 流数据。
    if (!readComponentData(componentIndexValue, &data))
        return failRead(QStringLiteral("无法读取 SchLib 组件 Data 流"));
    AltiumBinaryReader reader(data);
    int lastIndexInSheet = -1;  // 最近一次记录的图纸索引。
    int nextContentIndex = -1;  // 期望的下一个内容索引。
    while (reader.remaining() > 0) {
        const int startPosition = reader.position();  // 当前记录起始偏移。
        QByteArray payload;  // 当前记录载荷。
        uint8_t flags = 0;  // 当前记录标志位。
        if (!reader.readBlock(&payload, &flags))
            return failRead(QStringLiteral("读取 SchLib 组件记录失败: %1").arg(reader.errorString()));
        if (payload.isEmpty())
            return failRead(QStringLiteral("SchLib 组件记录为空，偏移量 %1").arg(startPosition));
        Record record;  // 当前组件记录。
        record.flags = flags;
        record.payload = payload;
        record.encoded = data.mid(startPosition, reader.position() - startPosition);
        if (payload.startsWith('|')) {
            AltiumBinaryReader parameterReader(payload);
            if (!parameterReader.parseCStringParameterData(payload, &record.parameters)) {
                return failRead(QStringLiteral("SchLib 组件参数记录无效，偏移量 %1: %2")
                                    .arg(startPosition)
                                    .arg(parameterReader.errorString()));
            }
            record.hasParameters = true;
            const auto readOptionalParameterAlias =
                [&record](const QString& primary, const QString& alias, int fallback, int* value) {
                    const bool hasPrimary = record.parameters.contains(primary);  // 是否存在主字段。
                    const bool hasAlias = record.parameters.contains(alias);  // 是否存在别名字段。
                    if (hasPrimary && hasAlias) {
                        int primaryValue = fallback;  // 主字段解析值。
                        int aliasValue = fallback;  // 别名字段解析值。
                        if (!readOptionalParameterInt(record.parameters, primary, fallback, &primaryValue) ||
                            !readOptionalParameterInt(record.parameters, alias, fallback, &aliasValue) ||
                            primaryValue != aliasValue)
                            return false;
                        *value = primaryValue;
                        return true;
                    }
                    const QString selectedName = hasPrimary ? primary : alias;  // 实际使用的字段名称。
                    return readOptionalParameterInt(record.parameters, selectedName, fallback, value);
                };
            if (!readOptionalParameterInt(record.parameters, QStringLiteral("RECORD"), -1, &record.recordType) ||
                !readOptionalParameterAlias(
                    QStringLiteral("OWNERPARTID"), QStringLiteral("OwnerPartId"), -1, &record.ownerPartId) ||
                !readOptionalParameterAlias(QStringLiteral("OWNERPARTDISPLAYMODE"),
                                            QStringLiteral("OwnerPartDisplayMode"),
                                            -1,
                                            &record.ownerPartDisplayMode) ||
                !readOptionalParameterInt(
                    record.parameters, QStringLiteral("IndexInSheet"), -1, &record.indexInSheet)) {
                return failRead(QStringLiteral("SchLib 组件参数记录的数值字段无效，偏移量 %1").arg(startPosition));
            }
            if (record.ownerPartDisplayMode < -1)
                return failRead(
                    QStringLiteral("SchLib 组件参数记录的 OWNERPARTDISPLAYMODE 无效，偏移量 %1").arg(startPosition));
            const bool hasFontId = record.parameters.contains(QStringLiteral("FontID")) ||  // 是否存在字体编号字段。
                                   record.parameters.contains(QStringLiteral("FONTID"));
            if (hasFontId) {
                int mixedCaseFontId = -1;  // 混合大小写字段中的字体编号。
                int upperCaseFontId = -1;  // 全大写字段中的字体编号。
                if (!readOptionalParameterInt(record.parameters, QStringLiteral("FontID"), -1, &mixedCaseFontId) ||
                    !readOptionalParameterInt(record.parameters, QStringLiteral("FONTID"), -1, &upperCaseFontId)) {
                    return failRead(QStringLiteral("SchLib 组件参数记录的 FONTID 无效，偏移量 %1").arg(startPosition));
                }
                if (mixedCaseFontId >= 0 && upperCaseFontId >= 0 && mixedCaseFontId != upperCaseFontId)
                    return failRead(
                        QStringLiteral("SchLib 组件参数记录的 FONTID 字段不一致，偏移量 %1").arg(startPosition));
                record.fontId = mixedCaseFontId >= 0 ? mixedCaseFontId : upperCaseFontId;
                if (record.fontId < 1 || record.fontId > m_fonts.size())
                    return failRead(
                        QStringLiteral("SchLib 组件参数记录的 FONTID 超出字体表范围，偏移量 %1").arg(startPosition));
            }
            if (record.ownerPartId < -1)
                return failRead(
                    QStringLiteral("SchLib 组件参数记录的 OWNERPARTID 小于 -1，偏移量 %1").arg(startPosition));
            if (record.ownerPartId >= 0 && record.ownerPartId > m_components.at(componentIndexValue).partCount) {
                return failRead(
                    QStringLiteral("SchLib 组件参数记录的 OWNERPARTID 超出部件范围，偏移量 %1").arg(startPosition));
            }
            QString geometryError;  // 几何参数校验错误说明。
            if (!validateGraphicParameters(record.parameters, record.recordType, &geometryError))
                return failRead(
                    QStringLiteral("SchLib 组件几何记录无效（%1），偏移量 %2").arg(geometryError).arg(startPosition));
            if (record.indexInSheet < -1 || (record.indexInSheet >= 0 && record.indexInSheet <= lastIndexInSheet)) {
                return failRead(
                    QStringLiteral("SchLib 组件参数记录的 IndexInSheet 非递增，偏移量 %1").arg(startPosition));
            }
            if (record.indexInSheet >= 0)
                lastIndexInSheet = record.indexInSheet;
        } else {
            if (!readBinaryRecordMetadata(
                    payload, &record.recordType, &record.ownerPartId, &record.ownerPartDisplayMode)) {
                return failRead(QStringLiteral("SchLib 二进制记录元数据无效，偏移量 %1").arg(startPosition));
            }
            if (record.ownerPartId < -1)
                return failRead(
                    QStringLiteral("SchLib 二进制记录的 OWNERPARTID 小于 -1，偏移量 %1").arg(startPosition));
            if (record.ownerPartId >= 0 && record.ownerPartId > m_components.at(componentIndexValue).partCount) {
                return failRead(
                    QStringLiteral("SchLib 二进制记录的 OWNERPARTID 超出部件范围，偏移量 %1").arg(startPosition));
            }
        }

        // 图元参数记录和二进制引脚共享同一个内容序号。首条内容记录的
        // IndexInSheet=0 可以省略，但后续记录必须紧接前一条内容记录；
        // 否则写入端可能漏掉图元，Altium 的绘制顺序也会发生偏移。
        const bool hasOwnerPartField =
            record.parameters.contains(QStringLiteral("OWNERPARTID")) ||  // 是否存在部件字段。
            record.parameters.contains(QStringLiteral("OwnerPartId"));
        // 判断当前参数记录是否参与内容序号连续性校验。
        const bool isIndexedParameter = record.hasParameters && hasOwnerPartField && record.recordType != 1 &&
                                        record.recordType != 34 && (record.recordType != 41 || record.ownerPartId >= 1);
        const bool isContentRecord =
            (!record.hasParameters && record.recordType == 2) || isIndexedParameter;  // 是否为有序内容记录。
        if (isContentRecord) {
            if (nextContentIndex < 0) {
                nextContentIndex = record.indexInSheet >= 0 ? record.indexInSheet + 1 : 1;
            } else if (!record.hasParameters) {
                ++nextContentIndex;
            } else if (record.indexInSheet < 0 || record.indexInSheet != nextContentIndex) {
                return failRead(
                    QStringLiteral("SchLib 组件的内容记录 IndexInSheet 不连续，偏移量 %1").arg(startPosition));
            } else {
                ++nextContentIndex;
            }
        }
        records->append(record);
    }
    if (records->isEmpty())
        return failRead(QStringLiteral("SchLib 组件 Data 流不包含记录"));
    return true;
}

/**
 * @brief 按组件名称解析其全部原始记录
 * @param componentName 组件名称
 * @param records 用于接收记录列表的输出指针
 * @return 解析成功时返回 true
 */
bool AltiumSchLibReader::readComponentRecords(const QString& componentName, QVector<Record>* records) const {
    return readComponentRecords(componentIndex(componentName), records);
}

/**
 * @brief 读取并校验 SchLib 公共图片 Storage 中的压缩图片条目
 * @param entries 用于接收图片条目的输出指针
 * @return 读取成功时返回 true
 */
bool AltiumSchLibReader::readImageStorage(QVector<ImageStorageEntry>* entries) const {
    if (entries == nullptr) {
        m_errorMessage = QStringLiteral("读取 SchLib 图片 Storage 时输出容器为空");
        return false;
    }
    entries->clear();
    m_errorMessage.clear();

    if (!m_oleReader.containsStream(QStringLiteral("Storage")))
        return true;

    QByteArray storage;  // 图片 Storage 流数据。
    if (!m_oleReader.readStream(QStringLiteral("Storage"), &storage)) {
        m_errorMessage = QStringLiteral("无法读取 SchLib 图片 Storage 流");
        return false;
    }

    AltiumBinaryReader reader(storage);
    QMap<QString, QString> storageParameters;  // Storage 头部参数表。
    if (!reader.readCStringParameterBlock(&storageParameters)) {
        m_errorMessage = QStringLiteral("SchLib 图片 Storage 头部无效: %1").arg(reader.errorString());
        return false;
    }
    bool expectedCountOk = true;  // 声明数量转换是否成功。
    const int expectedCount = storageParameters.contains(QStringLiteral("Weight"))  // 声明的图片条目数量。
                                  ? storageParameters.value(QStringLiteral("Weight")).toInt(&expectedCountOk)
                                  : -1;
    if (!expectedCountOk || expectedCount < 0) {
        m_errorMessage = QStringLiteral("SchLib 图片 Storage 的 Weight 无效");
        return false;
    }

    QSet<QString> names;  // 已读取图片名称集合。
    while (reader.remaining() > 0) {
        QByteArray entryData;  // 图片条目载荷。
        quint8 flags = 0;  // 图片条目标志位。
        if (!reader.readBlock(&entryData, &flags)) {
            m_errorMessage = QStringLiteral("读取 SchLib 图片 Storage 条目失败: %1").arg(reader.errorString());
            entries->clear();
            return false;
        }
        if (flags != 1) {
            m_errorMessage = QStringLiteral("SchLib 图片 Storage 条目标志无效: %1").arg(flags);
            entries->clear();
            return false;
        }

        AltiumBinaryReader entryReader(entryData);
        quint8 marker = 0;  // 图片条目标记字节。
        quint8 nameSize = 0;  // 文件名字节数。
        QByteArray nameData;  // 文件名原始字节。
        quint32 compressedSize = 0;  // 压缩数据字节数。
        QByteArray compressedData;  // 压缩图片数据。
        if (!entryReader.readUInt8(&marker) || marker != 0xD0 || !entryReader.readUInt8(&nameSize) || nameSize == 0 ||
            !entryReader.readBytes(nameSize, &nameData) || nameData.contains('\0') ||
            !entryReader.readUInt32(&compressedSize) || compressedSize == 0 ||
            compressedSize > static_cast<quint32>(entryReader.remaining()) ||
            !entryReader.readBytes(static_cast<int>(compressedSize), &compressedData) || entryReader.remaining() != 0) {
            m_errorMessage = QStringLiteral("SchLib 图片 Storage 条目内容无效");
            entries->clear();
            return false;
        }

        if (!isValidZlibPayload(compressedData)) {
            m_errorMessage = QStringLiteral("SchLib 图片 Storage 压缩数据无效");
            entries->clear();
            return false;
        }

        const QString name = QString::fromLocal8Bit(nameData);  // 图片文件名。
        if (name.toLocal8Bit() != nameData) {
            m_errorMessage = QStringLiteral("SchLib 图片 Storage 文件名编码无效");
            entries->clear();
            return false;
        }
        const QString foldedName = name.toCaseFolded();  // 用于重复检查的折叠文件名。
        if (!AltiumWriterUtils::isValidImageStorageName(name) || names.contains(foldedName)) {
            m_errorMessage = QStringLiteral("SchLib 图片 Storage 包含重复或非法文件名: %1").arg(name);
            entries->clear();
            return false;
        }
        names.insert(foldedName);
        entries->append({flags, name, compressedData});
    }

    if (expectedCount >= 0 && expectedCount != entries->size()) {
        m_errorMessage =
            QStringLiteral("SchLib 图片 Storage 数量不一致，声明 %1，实际 %2").arg(expectedCount).arg(entries->size());
        entries->clear();
        return false;
    }
    return true;
}

/**
 * @brief 返回最近一次 SchLib 读取错误
 * @return 最近一次错误说明；没有错误时为空
 */
QString AltiumSchLibReader::errorString() const {
    return m_errorMessage;
}

/**
 * @brief 判断 SchLib 读取器是否处于错误状态
 * @return 已发生读取错误时返回 true
 */
bool AltiumSchLibReader::hasError() const {
    return !m_errorMessage.isEmpty();
}

}  // namespace EasyKiConverter
