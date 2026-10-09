#include "core/parser/ArchiveInspector.h"
#include "core/parser/EncodingDetector.h"

#include <QTest>

using namespace EasyKiConverter::Parser;

namespace {

struct ZipEntrySpec {
    // path：测试使用的文件系统路径。
    QByteArray path;
    // compressedSize：测试使用的数量或索引值。
    quint32 compressedSize = 1;
    // uncompressedSize：测试使用的数量或索引值。
    quint32 uncompressedSize = 1;
    // method：压缩方法标识。
    quint16 method = 0;
    // externalAttributes：ZIP 条目外部属性。
    quint32 externalAttributes = 0;
};

/**
     * @brief 向测试 ZIP 写入小端 16 位字段。
     * @param data 待写入的 ZIP 字节数组。
     * @param value 待写入的 16 位字段值。
     */
void append16(QByteArray& data, quint16 value) {
    data.append(static_cast<char>(value & 0xFF));
    data.append(static_cast<char>((value >> 8) & 0xFF));
}

/**
     * @brief 向测试 ZIP 写入小端 32 位字段。
     * @param data 待写入的 ZIP 字节数组。
     * @param value 待写入的 32 位字段值。
     */
void append32(QByteArray& data, quint32 value) {
    append16(data, static_cast<quint16>(value & 0xFFFF));
    append16(data, static_cast<quint16>((value >> 16) & 0xFFFF));
}

/**
     * @brief 构造包含中央目录的最小 ZIP 字节流供安全检查测试使用。
     * @param specs 测试 ZIP 条目规格集合。
     */
QByteArray makeZip(const QList<ZipEntrySpec>& specs) {
    // localData：测试使用的文本或数据内容。
    QByteArray localData;
    // offsets：ZIP 条目数据偏移量。
    QList<quint32> offsets;
    for (const ZipEntrySpec& spec : specs) {
        offsets.append(static_cast<quint32>(localData.size()));
        append32(localData, 0x04034B50);
        append16(localData, 20);
        append16(localData, 0);
        append16(localData, spec.method);
        append16(localData, 0);
        append16(localData, 0);
        append32(localData, 0);
        append32(localData, spec.compressedSize);
        append32(localData, spec.uncompressedSize);
        append16(localData, static_cast<quint16>(spec.path.size()));
        append16(localData, 0);
        localData.append(spec.path);
        localData.append(QByteArray(static_cast<int>(spec.compressedSize), 'x'));
    }
    // centralData：测试使用的文本或数据内容。
    QByteArray centralData;
    for (int index = 0; index < specs.size(); ++index) {
        // spec：当前 ZIP 条目规格。
        const ZipEntrySpec& spec = specs.at(index);
        append32(centralData, 0x02014B50);
        append16(centralData, 20);
        append16(centralData, 20);
        append16(centralData, 0);
        append16(centralData, spec.method);
        append16(centralData, 0);
        append16(centralData, 0);
        append32(centralData, 0);
        append32(centralData, spec.compressedSize);
        append32(centralData, spec.uncompressedSize);
        append16(centralData, static_cast<quint16>(spec.path.size()));
        append16(centralData, 0);
        append16(centralData, 0);
        append16(centralData, 0);
        append16(centralData, 0);
        append32(centralData, spec.externalAttributes);
        append32(centralData, offsets.at(index));
        centralData.append(spec.path);
    }
    // result：测试得到的实际结果数据。
    QByteArray result = localData;
    // centralOffset：中心目录偏移量。
    const quint32 centralOffset = static_cast<quint32>(result.size());
    result.append(centralData);
    append32(result, 0x06054B50);
    append16(result, 0);
    append16(result, 0);
    append16(result, static_cast<quint16>(specs.size()));
    append16(result, static_cast<quint16>(specs.size()));
    append32(result, static_cast<quint32>(centralData.size()));
    append32(result, centralOffset);
    append16(result, 0);
    return result;
}

}  // namespace

class TestParserSecurity : public QObject {
    Q_OBJECT

private slots:

    // 验证正常 ZIP 条目可以通过检查，并保留条目元数据。
    /**
     * @brief 验证 acceptsSafeZip 测试场景。
     */
    void acceptsSafeZip() {
        const ArchiveInspectionResult result =
            ArchiveInspector::inspect(makeZip({{QByteArrayLiteral("symbols/C1.lib"), 4, 4}}));
        QVERIFY(result.safe);
        QCOMPARE(result.entries.size(), 1);
        QCOMPARE(result.entries.first().path, QStringLiteral("symbols/C1.lib"));
        QCOMPARE(result.totalUncompressedBytes, 4);
    }

    // 验证路径穿越、绝对路径和符号链接条目都会被拒绝。
    /**
     * @brief 验证 rejectsUnsafeZipPaths 测试场景。
     */
    void rejectsUnsafeZipPaths() {
        QVERIFY(!ArchiveInspector::inspect(makeZip({{QByteArrayLiteral("../escape"), 1, 1}})).safe);
        QVERIFY(!ArchiveInspector::inspect(makeZip({{QByteArrayLiteral("/absolute"), 1, 1}})).safe);
        QVERIFY(!ArchiveInspector::inspect(makeZip({{QByteArrayLiteral("link"), 1, 1, 0, 0xA0000000}})).safe);
    }

    // 验证条目大小、总展开大小和压缩比限制能阻断压缩炸弹。
    /**
     * @brief 验证 rejectsArchiveBombLimits 测试场景。
     */
    void rejectsArchiveBombLimits() {
        // limits：测试使用的数量或索引值。
        ArchiveLimits limits;
        limits.maxEntryUncompressedBytes = 10;
        QVERIFY(!ArchiveInspector::inspect(makeZip({{QByteArrayLiteral("large"), 1, 100}}), limits).safe);
        limits.maxEntryUncompressedBytes = 1000;
        limits.maxCompressionRatio = 10.0;
        QVERIFY(!ArchiveInspector::inspect(makeZip({{QByteArrayLiteral("ratio"), 1, 100}}), limits).safe);
    }

    // 验证中央目录不能伪造不存在的本地文件头或越界压缩数据范围。
    /**
     * @brief 验证 rejectsInvalidLocalZipStructure 测试场景。
     */
    void rejectsInvalidLocalZipStructure() {
        // invalidHeader：测试使用的元器件标识或名称。
        QByteArray invalidHeader = makeZip({{QByteArrayLiteral("broken"), 1, 1}});
        invalidHeader[0] = 'x';
        QVERIFY(!ArchiveInspector::inspect(invalidHeader).safe);

        // invalidData：测试使用的元器件标识或名称。
        QByteArray invalidData = makeZip({{QByteArrayLiteral("broken-data"), 1, 1}});
        invalidData.chop(1);
        QVERIFY(!ArchiveInspector::inspect(invalidData).safe);
    }

    // 验证 UTF-8、UTF-16 BOM 和非法字节的诊断行为。
    /**
     * @brief 验证 detectsAndDecodesTextEncodings 测试场景。
     */
    void detectsAndDecodesTextEncodings() {
        QCOMPARE(EncodingDetector::detect(QByteArray::fromHex("EFBBBF") + QByteArrayLiteral("hello")).encoding,
                 TextEncoding::Utf8);
        QCOMPARE(EncodingDetector::decode(QByteArray::fromHex("EFBBBF") + QByteArrayLiteral("hello")),
                 QStringLiteral("hello"));
        QCOMPARE(EncodingDetector::decode(QByteArray::fromHex("FFFE") + QByteArray::fromHex("48006900")),
                 QStringLiteral("Hi"));
        // diagnostics：导出过程中收集的诊断信息。
        ParseDiagnostics diagnostics;
        // fallback：测试用回退结果。
        const QString fallback = EncodingDetector::decode(QByteArray::fromHex("FF00FE"), &diagnostics);
        QVERIFY(!fallback.isEmpty());
        QVERIFY(!diagnostics.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestParserSecurity)
#include "test_parser_security.moc"
