#include "utils/cli/FileReader.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

using namespace EasyKiConverter;

class TestFileReader : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testReadComponentListFile();
    void testReadComponentListFileTrimsBlankLinesAndPreservesDuplicates();
    void testReadComponentListFileEmpty();
    void testReadComponentListFileNotFound();
    void testReadComponentListFileOpenDirectoryFails();
    void testFileExists();
    void testGetFileExtension();

private:
    // m_tempDir：测试使用的文件系统路径。
    QTemporaryDir* m_tempDir = nullptr;
    QString createTempFile(const QString& name, const QString& content);
};

/**
 * @brief 验证 initTestCase 测试场景。
 */
void TestFileReader::initTestCase() {
    m_tempDir = new QTemporaryDir();
    QVERIFY(m_tempDir->isValid());
}

/**
 * @brief 清理测试用例使用的资源。
 * @param content 待验证的文本或数据内容。
 */
void TestFileReader::cleanupTestCase() {
    delete m_tempDir;
    m_tempDir = nullptr;
}

/**
 * @brief 验证 createTempFile 测试场景。
 * @param name 测试使用的标识或名称。
 */
QString TestFileReader::createTempFile(const QString& name, const QString& content) {
    // filePath：测试使用的文件系统路径。
    QString filePath = m_tempDir->path() + "/" + name;
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << content;
        file.close();
    }
    return filePath;
}

/**
 * @brief 验证 testReadComponentListFile 测试场景。
 */
void TestFileReader::testReadComponentListFile() {
    // content：测试使用的文本或数据内容。
    QString content = "C23186\nC23166\nC23102\n";
    // filePath：测试使用的文件系统路径。
    QString filePath = createTempFile("components.txt", content);

    // error：测试期间收集的错误或诊断信息。
    QString error;
    // ids：测试使用的元器件标识或名称。
    QStringList ids = FileReader::readComponentListFile(filePath, error);

    QVERIFY(error.isEmpty());
    QCOMPARE(ids.size(), 3);
    QVERIFY(ids.contains("C23186"));
    QVERIFY(ids.contains("C23166"));
    QVERIFY(ids.contains("C23102"));
}

/**
 * @brief 验证 testReadComponentListFileTrimsBlankLinesAndPreservesDuplicates 测试场景。
 */
void TestFileReader::testReadComponentListFileTrimsBlankLinesAndPreservesDuplicates() {
    // content：测试使用的文本或数据内容。
    QString content = "  C23186  \n\n\tC23166\t\nC23186\n";
    // filePath：测试使用的文件系统路径。
    QString filePath = createTempFile("components_with_spaces.txt", content);

    // error：测试期间收集的错误或诊断信息。
    QString error;
    // ids：测试使用的元器件标识或名称。
    QStringList ids = FileReader::readComponentListFile(filePath, error);

    QVERIFY(error.isEmpty());
    QCOMPARE(ids, QStringList({QStringLiteral("C23186"), QStringLiteral("C23166"), QStringLiteral("C23186")}));
}

/**
 * @brief 验证 testReadComponentListFileEmpty 测试场景。
 */
void TestFileReader::testReadComponentListFileEmpty() {
    // content：测试使用的文本或数据内容。
    QString content = "\n\n\n";
    // filePath：测试使用的文件系统路径。
    QString filePath = createTempFile("empty.txt", content);

    // error：测试期间收集的错误或诊断信息。
    QString error;
    // ids：测试使用的元器件标识或名称。
    QStringList ids = FileReader::readComponentListFile(filePath, error);

    QVERIFY(error.isEmpty());
    QVERIFY(ids.isEmpty());
}

/**
 * @brief 验证 testReadComponentListFileNotFound 测试场景。
 */
void TestFileReader::testReadComponentListFileNotFound() {
    // error：测试期间收集的错误或诊断信息。
    QString error;
    // ids：测试使用的元器件标识或名称。
    QStringList ids = FileReader::readComponentListFile("/nonexistent/file.txt", error);

    QVERIFY(!error.isEmpty());
    QVERIFY(error.contains("不存在"));
    QVERIFY(ids.isEmpty());
}

/**
 * @brief 验证 testReadComponentListFileOpenDirectoryFails 测试场景。
 */
void TestFileReader::testReadComponentListFileOpenDirectoryFails() {
    // error：测试期间收集的错误或诊断信息。
    QString error;
    // ids：测试使用的元器件标识或名称。
    QStringList ids = FileReader::readComponentListFile(m_tempDir->path(), error);

    QVERIFY(!error.isEmpty());
    QVERIFY(error.contains(QStringLiteral("无法打开输入文件")));
    QVERIFY(ids.isEmpty());
}

/**
 * @brief 验证 testFileExists 测试场景。
 */
void TestFileReader::testFileExists() {
    // filePath：测试使用的文件系统路径。
    QString filePath = createTempFile("exists.txt", "test");

    QVERIFY(FileReader::fileExists(filePath));
    QVERIFY(!FileReader::fileExists("/nonexistent/file.txt"));
}

/**
 * @brief 验证 testGetFileExtension 测试场景。
 */
void TestFileReader::testGetFileExtension() {
    QCOMPARE(FileReader::getFileExtension("file.txt"), "txt");
    QCOMPARE(FileReader::getFileExtension("file.xlsx"), "xlsx");
    QCOMPARE(FileReader::getFileExtension("file.CSV"), "csv");
    QCOMPARE(FileReader::getFileExtension("file"), "");
    QCOMPARE(FileReader::getFileExtension("/path/to/file.txt"), "txt");
    QCOMPARE(FileReader::getFileExtension("/path/to/archive.tar.gz"), "gz");
    QCOMPARE(FileReader::getFileExtension("/path/to/.env"), "env");
}

QTEST_MAIN(TestFileReader)
#include "test_file_reader.moc"
