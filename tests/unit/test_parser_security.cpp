#include "core/parser/ArchiveInspector.h"
#include "core/parser/EncodingDetector.h"

#include <QTest>

using namespace EasyKiConverter::Parser;

namespace {

struct ZipEntrySpec {
    QByteArray path;
    quint32 compressedSize = 1;
    quint32 uncompressedSize = 1;
    quint16 method = 0;
    quint32 externalAttributes = 0;
};

/** @brief 向测试 ZIP 写入小端 16 位字段。 */
void append16(QByteArray& data, quint16 value) {
    data.append(static_cast<char>(value & 0xFF));
    data.append(static_cast<char>((value >> 8) & 0xFF));
}

/** @brief 向测试 ZIP 写入小端 32 位字段。 */
void append32(QByteArray& data, quint32 value) {
    append16(data, static_cast<quint16>(value & 0xFFFF));
    append16(data, static_cast<quint16>((value >> 16) & 0xFFFF));
}

/** @brief 构造包含中央目录的最小 ZIP 字节流供安全检查测试使用。 */
QByteArray makeZip(const QList<ZipEntrySpec>& specs) {
    QByteArray localData;
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
    QByteArray centralData;
    for (int index = 0; index < specs.size(); ++index) {
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
    QByteArray result = localData;
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
    void acceptsSafeZip() {
        const ArchiveInspectionResult result =
            ArchiveInspector::inspect(makeZip({{QByteArrayLiteral("symbols/C1.lib"), 4, 4}}));
        QVERIFY(result.safe);
        QCOMPARE(result.entries.size(), 1);
        QCOMPARE(result.entries.first().path, QStringLiteral("symbols/C1.lib"));
        QCOMPARE(result.totalUncompressedBytes, 4);
    }

    // 验证路径穿越、绝对路径和符号链接条目都会被拒绝。
    void rejectsUnsafeZipPaths() {
        QVERIFY(!ArchiveInspector::inspect(makeZip({{QByteArrayLiteral("../escape"), 1, 1}})).safe);
        QVERIFY(!ArchiveInspector::inspect(makeZip({{QByteArrayLiteral("/absolute"), 1, 1}})).safe);
        QVERIFY(!ArchiveInspector::inspect(makeZip({{QByteArrayLiteral("link"), 1, 1, 0, 0xA0000000}})).safe);
    }

    // 验证条目大小、总展开大小和压缩比限制能阻断压缩炸弹。
    void rejectsArchiveBombLimits() {
        ArchiveLimits limits;
        limits.maxEntryUncompressedBytes = 10;
        QVERIFY(!ArchiveInspector::inspect(makeZip({{QByteArrayLiteral("large"), 1, 100}}), limits).safe);
        limits.maxEntryUncompressedBytes = 1000;
        limits.maxCompressionRatio = 10.0;
        QVERIFY(!ArchiveInspector::inspect(makeZip({{QByteArrayLiteral("ratio"), 1, 100}}), limits).safe);
    }

    // 验证 UTF-8、UTF-16 BOM 和非法字节的诊断行为。
    void detectsAndDecodesTextEncodings() {
        QCOMPARE(EncodingDetector::detect(QByteArray::fromHex("EFBBBF") + QByteArrayLiteral("hello")).encoding,
                 TextEncoding::Utf8);
        QCOMPARE(EncodingDetector::decode(QByteArray::fromHex("EFBBBF") + QByteArrayLiteral("hello")),
                 QStringLiteral("hello"));
        QCOMPARE(EncodingDetector::decode(QByteArray::fromHex("FFFE") + QByteArray::fromHex("48006900")),
                 QStringLiteral("Hi"));
        ParseDiagnostics diagnostics;
        const QString fallback = EncodingDetector::decode(QByteArray::fromHex("FF00FE"), &diagnostics);
        QVERIFY(!fallback.isEmpty());
        QVERIFY(!diagnostics.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestParserSecurity)
#include "test_parser_security.moc"
