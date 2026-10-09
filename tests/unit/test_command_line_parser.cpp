#include "core/LanguageManager.h"
#include "services/ConfigService.h"
#include "utils/CommandLineParser.h"
#include "utils/cli/CompletionGenerator.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QStringList>
#include <QTest>

using namespace EasyKiConverter;

class TestCommandLineParser : public QObject {
    Q_OBJECT

private slots:
    // CLI 模式检测（核心修复：parse() 使用存储的 argv，不依赖 QCoreApplication::arguments()）
    void testParseStoredArgv();  // 验证 argc/argv 被正确存储和使用
    void testParseConvertBom();  // convert bom → CliMode::ConvertBom
    void testParseConvertComponent();  // convert component → CliMode::ConvertComponent
    void testParseConvertBatch();  // convert batch → CliMode::ConvertBatch
    void testParseConvertInvalid();  // convert abc → hasConvertCommand=true 但 isCliMode=false
    void testLibNameDefault();
    void testLibNameExplicit();

    // --symbol / --footprint / --3d-model 默认语义
    void testExportSymbolDefaultTrue();  // 默认 true，未设置时不导出符号库
    void testExportFootprintDefaultTrue();  // 默认 true，未设置时不导出封装库
    void testExport3dModelDefaultFalse();  // 默认 false（opt-in），未设置时不导出 3D 模型
    void testExportSymbolExplicitTrue();  // --symbol=true 显式启用
    void testExport3dModelExplicitTrue();  // --3d-model 显式启用
    void testModel3DFormatDefaultWrl();
    void testModel3DFormatBoth();
    void testModel3DFormatInvalid();
    void testCacheOptions();
    void testCacheSizeInvalid();
    void testCliRequiredOptionValidation();
    void testGlobalOptionValidation();

    // 补全脚本契约（验证不再暴露不存在的命令）
    void testCompletionScriptNoHelpVersionSubcommands();
    void testCompletionScriptNoBoolValueHints();

    // hasConvertCommand() 辅助方法
    void testHasConvertCommand();

    // 新增 CLI 标志：默认值
    void testWeakNetworkDefaultFalse();
    void testUpdateModeDefaultFalse();
    void test3dPathModeDefaultRelative();
    void testOverwriteDefaultTrue();
    void testNoOverwriteExplicit();
    void testSymbolDescriptionDefaultEmpty();
    void testFootprintDescriptionDefaultEmpty();

    // 新增 CLI 标志：显式值
    void testWeakNetworkExplicit();
    void testUpdateModeExplicit();
    void test3dPathModeAbsolute();
    void testSymbolDescriptionExplicit();
    void testFootprintDescriptionExplicit();

    // 新增 CLI 标志：验证
    void test3dPathModeInvalid();

    // 补全脚本包含新选项
    void testCompletionScriptContainsNewOptions();

    // CLI 国际化测试
    void testValidationErrorTranslated();
    void testCliHelpTextTranslated();

private:
    // Helper: 构建 argc/argv 并构造 CommandLineParser
    CommandLineParser* makeParser(const QStringList& args);

    // m_argc：命令行参数数量。
    int m_argc = 0;
    // m_argv：命令行参数列表。
    char** m_argv = nullptr;
    QList<QByteArray> m_argStorage;  // 存储字符串数据（生命周期与 parser 同长）
    QList<char*> m_argvPtrs;  // argv 指针数组（指向 m_argStorage）
};

/**
 * @brief 验证 makeParser 测试场景。
 * @param args 命令行参数列表。
 */
CommandLineParser* TestCommandLineParser::makeParser(const QStringList& args) {
    // 清理上一次的 argv
    m_argStorage.clear();
    m_argc = args.size();

    // 将 QStringList 转换为 char* []（存储在 m_argStorage 中保证生命周期）
    for (const QString& arg : args) {
        m_argStorage.append(arg.toUtf8());
    }

    // 构建 argv 指针数组（指向 m_argStorage 中的数据，保证与 parser 生命周期一致）
    m_argvPtrs.clear();
    for (const QByteArray& a : m_argStorage) {
        m_argvPtrs.append(const_cast<char*>(a.constData()));
    }
    m_argv = m_argvPtrs.isEmpty() ? nullptr : m_argvPtrs.data();

    return new CommandLineParser(m_argc, m_argv);
}

// ========== CLI 模式检测 ==========

/**
 * @brief 验证 testParseStoredArgv 测试场景。
 */
void TestCommandLineParser::testParseStoredArgv() {
    // 验证 parse() 使用存储的 argv 而非 QCoreApplication::arguments()
    // 构造一个带有明确标记的 argv，parse() 解析后检查状态
    QStringList args = {"easykiconverter", "convert", "bom", "-i", "input.xlsx", "-o", "output"};
    // parser：负责解析测试输入的解析器实例。
    auto* parser = makeParser(args);

    QVERIFY(parser->parse());
    QVERIFY(parser->isCliMode());
    QVERIFY(parser->hasConvertCommand());
    QVERIFY(parser->isCliMode());
    QVERIFY(parser->cliMode() == CommandLineParser::CliMode::ConvertBom);

    delete parser;
}

/**
 * @brief 验证 testParseConvertBom 测试场景。
 */
void TestCommandLineParser::testParseConvertBom() {
    // parser：负责解析测试输入的解析器实例。
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "bom.xlsx", "-o", "out"});
    QVERIFY(parser->parse());
    QVERIFY(parser->isCliMode());
    QVERIFY(parser->cliMode() == CommandLineParser::CliMode::ConvertBom);
    QVERIFY(parser->hasConvertCommand());
    delete parser;
}

/**
 * @brief 验证 testParseConvertComponent 测试场景。
 */
void TestCommandLineParser::testParseConvertComponent() {
    // parser：负责解析测试输入的解析器实例。
    auto* parser = makeParser({"easykiconverter", "convert", "component", "-c", "C12345", "-o", "out"});
    QVERIFY(parser->parse());
    QVERIFY(parser->isCliMode());
    QVERIFY(parser->cliMode() == CommandLineParser::CliMode::ConvertComponent);
    QVERIFY(parser->hasConvertCommand());
    delete parser;
}

/**
 * @brief 验证 testParseConvertBatch 测试场景。
 */
void TestCommandLineParser::testParseConvertBatch() {
    // parser：负责解析测试输入的解析器实例。
    auto* parser = makeParser({"easykiconverter", "convert", "batch", "-i", "list.txt", "-o", "out"});
    QVERIFY(parser->parse());
    QVERIFY(parser->isCliMode());
    QVERIFY(parser->cliMode() == CommandLineParser::CliMode::ConvertBatch);
    QVERIFY(parser->hasConvertCommand());
    delete parser;
}

/**
 * @brief 验证 testParseConvertInvalid 测试场景。
 */
void TestCommandLineParser::testParseConvertInvalid() {
    // 关键回归测试：convert 后面跟无效子命令，hasConvertCommand=true 但 isCliMode=false
    auto* parser = makeParser({"easykiconverter", "convert", "bmo", "-o", "out"});

    QVERIFY(parser->parse());
    QVERIFY(parser->hasConvertCommand());  // convert 命令存在
    QVERIFY2(!parser->isCliMode(), "convert 后跟无效子命令不应进入 CLI 模式");  // 但没有合法子命令

    delete parser;
}

/**
 * @brief 验证 testLibNameDefault 测试场景。
 */
void TestCommandLineParser::testLibNameDefault() {
    // parser：负责解析测试输入的解析器实例。
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "bom.xlsx", "-o", "out"});
    parser->parse();
    QCOMPARE(parser->libName(), QStringLiteral("EasyKiConverter"));
    delete parser;
}

/**
 * @brief 验证 testLibNameExplicit 测试场景。
 */
void TestCommandLineParser::testLibNameExplicit() {
    auto* parser =
        makeParser({"easykiconverter", "convert", "bom", "-i", "bom.xlsx", "-o", "out", "--lib-name", "my_lib"});
    parser->parse();
    QCOMPARE(parser->libName(), QStringLiteral("my_lib"));
    delete parser;
}

// ========== 导出选项默认语义 ==========

/**
 * @brief 验证 testExportSymbolDefaultTrue 测试场景。
 */
void TestCommandLineParser::testExportSymbolDefaultTrue() {
    // --symbol 未传时默认为 true（核心产物默认开启）
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o"});
    parser->parse();
    QVERIFY(parser->validate());  // 有 output 就验证通过
    QVERIFY2(parser->exportSymbol() == true, "--symbol 未设置时默认导出符号库");
    delete parser;
}

/**
 * @brief 验证 testExportFootprintDefaultTrue 测试场景。
 */
void TestCommandLineParser::testExportFootprintDefaultTrue() {
    // --footprint 未传时默认为 true（核心产物默认开启）
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o"});
    parser->parse();
    QVERIFY2(parser->exportFootprint() == true, "--footprint 未设置时默认导出封装库");
    delete parser;
}

/**
 * @brief 验证 testExport3dModelDefaultFalse 测试场景。
 */
void TestCommandLineParser::testExport3dModelDefaultFalse() {
    // --3d-model 未传时默认为 false（opt-in，不默认导出 3D）
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o"});
    parser->parse();
    QVERIFY2(parser->export3DModel() == false, "--3d-model 未设置时不应导出 3D 模型（opt-in）");
    delete parser;
}

/**
 * @brief 验证 testExportSymbolExplicitTrue 测试场景。
 */
void TestCommandLineParser::testExportSymbolExplicitTrue() {
    // --symbol 显式设置 true
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o", "--symbol"});
    parser->parse();
    QVERIFY(parser->exportSymbol() == true);
    delete parser;
}

/**
 * @brief 验证 testExport3dModelExplicitTrue 测试场景。
 */
void TestCommandLineParser::testExport3dModelExplicitTrue() {
    // --3d-model 显式启用
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o", "--3d-model"});
    parser->parse();
    QVERIFY(parser->export3DModel() == true);
    delete parser;
}

/**
 * @brief 验证 testModel3DFormatDefaultWrl 测试场景。
 */
void TestCommandLineParser::testModel3DFormatDefaultWrl() {
    // parser：负责解析测试输入的解析器实例。
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o", "--3d-model"});
    parser->parse();
    QVERIFY(parser->validate());
    QCOMPARE(parser->model3DFormat(), QStringLiteral("wrl"));
    delete parser;
}

/**
 * @brief 验证 testModel3DFormatBoth 测试场景。
 */
void TestCommandLineParser::testModel3DFormatBoth() {
    // parser：负责解析测试输入的解析器实例。
    auto* parser = makeParser(
        {"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o", "--3d-model", "--3d-model-format", "both"});
    parser->parse();
    QVERIFY(parser->validate());
    QCOMPARE(parser->model3DFormat(), QStringLiteral("both"));
    delete parser;
}

/**
 * @brief 验证 testModel3DFormatInvalid 测试场景。
 */
void TestCommandLineParser::testModel3DFormatInvalid() {
    // parser：负责解析测试输入的解析器实例。
    auto* parser = makeParser(
        {"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o", "--3d-model", "--3d-model-format", "obj"});
    parser->parse();
    QVERIFY(!parser->validate());
    QVERIFY(parser->validationError().contains(QStringLiteral("无效的 3D 模型格式")));
    delete parser;
}

/**
 * @brief 验证 testCacheOptions 测试场景。
 */
void TestCommandLineParser::testCacheOptions() {
    // parser：负责解析测试输入的解析器实例。
    auto* parser = makeParser({"easykiconverter",
                               "convert",
                               "component",
                               "-c",
                               "C12345",
                               "-o",
                               "out",
                               "--cache-dir",
                               "/tmp/ekc-cache",
                               "--cache-size-mb",
                               "256"});
    parser->parse();
    QVERIFY(parser->validate());
    QVERIFY(parser->isCacheDirSet());
    QCOMPARE(parser->cacheDir(), QStringLiteral("/tmp/ekc-cache"));
    QVERIFY(parser->isDiskCacheLimitSet());
    QCOMPARE(parser->diskCacheLimitMB(), 256);
    delete parser;
}

/**
 * @brief 验证 testCacheSizeInvalid 测试场景。
 */
void TestCommandLineParser::testCacheSizeInvalid() {
    auto* parser =
        makeParser({"easykiconverter", "convert", "component", "-c", "C12345", "-o", "out", "--cache-size-mb", "0"});
    parser->parse();
    QVERIFY(!parser->validate());
    QVERIFY(parser->validationError().contains(QStringLiteral("磁盘缓存大小必须是大于 0 的整数")));
    delete parser;
}

/**
 * @brief 验证 testCliRequiredOptionValidation 测试场景。
 */
void TestCommandLineParser::testCliRequiredOptionValidation() {
    // missingBomInput：缺失 BOM 输入的错误结果。
    auto* missingBomInput = makeParser({"easykiconverter", "convert", "bom", "-o", "out"});
    missingBomInput->parse();
    QVERIFY(!missingBomInput->validate());
    QVERIFY(missingBomInput->validationError().contains(QStringLiteral("BOM 表转换必须指定输入文件")));
    delete missingBomInput;

    // missingComponentId：测试使用的元器件标识或名称。
    auto* missingComponentId = makeParser({"easykiconverter", "convert", "component", "-o", "out"});
    missingComponentId->parse();
    QVERIFY(!missingComponentId->validate());
    QVERIFY(missingComponentId->validationError().contains(QStringLiteral("单个元器件转换必须指定 LCSC 编号")));
    delete missingComponentId;

    // missingBatchInput：缺失批处理输入的错误结果。
    auto* missingBatchInput = makeParser({"easykiconverter", "convert", "batch", "-o", "out"});
    missingBatchInput->parse();
    QVERIFY(!missingBatchInput->validate());
    QVERIFY(missingBatchInput->validationError().contains(QStringLiteral("批量转换必须指定输入文件")));
    delete missingBatchInput;

    // missingOutput：测试得到的实际结果数据。
    auto* missingOutput = makeParser({"easykiconverter", "convert", "component", "-c", "C12345"});
    missingOutput->parse();
    QVERIFY(!missingOutput->validate());
    QVERIFY(missingOutput->validationError().contains(QStringLiteral("CLI 模式必须指定输出目录")));
    delete missingOutput;
}

/**
 * @brief 验证 testGlobalOptionValidation 测试场景。
 */
void TestCommandLineParser::testGlobalOptionValidation() {
    // invalidLogLevel：测试使用的元器件标识或名称。
    auto* invalidLogLevel = makeParser({"easykiconverter", "--log-level", "verbose"});
    invalidLogLevel->parse();
    QVERIFY(!invalidLogLevel->validate());
    QVERIFY(invalidLogLevel->validationError().contains(QStringLiteral("无效的日志级别")));
    delete invalidLogLevel;

    // invalidLanguage：测试使用的元器件标识或名称。
    auto* invalidLanguage = makeParser({"easykiconverter", "--language", "fr"});
    invalidLanguage->parse();
    QVERIFY(!invalidLanguage->validate());
    QVERIFY(invalidLanguage->validationError().contains(QStringLiteral("无效的语言设置")));
    delete invalidLanguage;

    // invalidTheme：测试使用的元器件标识或名称。
    auto* invalidTheme = makeParser({"easykiconverter", "--theme", "blue"});
    invalidTheme->parse();
    QVERIFY(!invalidTheme->validate());
    QVERIFY(invalidTheme->validationError().contains(QStringLiteral("无效的主题设置")));
    delete invalidTheme;
}

// ========== 补全脚本契约 ==========

/**
 * @brief 验证 testCompletionScriptNoHelpVersionSubcommands 测试场景。
 */
void TestCommandLineParser::testCompletionScriptNoHelpVersionSubcommands() {
    // 验证补全脚本不再暴露 help/version 子命令（它们是 --help/--version 选项）
    QString bashScript = CompletionGenerator::generate(CompletionGenerator::Shell::Bash);
    // zshScript：Zsh 测试脚本内容。
    QString zshScript = CompletionGenerator::generate(CompletionGenerator::Shell::Zsh);
    // fishScript：Fish 测试脚本内容。
    QString fishScript = CompletionGenerator::generate(CompletionGenerator::Shell::Fish);

    // Bash: subcommands 变量应只包含 "convert"
    QVERIFY2(bashScript.contains("local subcommands=\"convert\""), "Bash 补全的 subcommands 不应包含 help/version");

    // Zsh: commands 数组应只包含 convert
    QVERIFY2(!zshScript.contains("'help:显示帮助信息'"), "Zsh 补全不应暴露 help 子命令");
    QVERIFY2(!zshScript.contains("'version:显示版本信息'"), "Zsh 补全不应暴露 version 子命令");

    // Fish: 不应有 help/version 子命令补全
    QVERIFY2(!fishScript.contains("'__fish_use_subcommand' -a help"), "Fish 补全不应暴露 help 子命令");
    QVERIFY2(!fishScript.contains("'__fish_use_subcommand' -a version"), "Fish 补全不应暴露 version 子命令");
}

/**
 * @brief 验证 testCompletionScriptNoBoolValueHints 测试场景。
 */
void TestCommandLineParser::testCompletionScriptNoBoolValueHints() {
    // 验证补全脚本不再为 --symbol/--footprint/--3d-model 补全 true/false
    QString bashScript = CompletionGenerator::generate(CompletionGenerator::Shell::Bash);
    // zshScript：Zsh 测试脚本内容。
    QString zshScript = CompletionGenerator::generate(CompletionGenerator::Shell::Zsh);
    // fishScript：Fish 测试脚本内容。
    QString fishScript = CompletionGenerator::generate(CompletionGenerator::Shell::Fish);

    // Bash: --symbol|--footprint|--3d-model 后不应跟 compgen -W "true false"
    QVERIFY2(!bashScript.contains("compgen -W \"true false\""), "Bash 补全不应为 flag 选项提供 true/false 补全");

    // Zsh: --symbol/--footprint/--3d-model 的补全描述中不应有 :(true false)
    QVERIFY2(!zshScript.contains(":value:(true false)"), "Zsh 补全不应为 flag 选项提供 :(true false)");

    // Fish: -a 'true false' 不应出现在 symbol/footprint/3d-model 补全中
    QVERIFY2(!fishScript.contains("-a 'true false'"), "Fish 补全不应为 flag 选项提供 true/false 补全");
}

// ========== 辅助方法 ==========

/**
 * @brief 验证 testHasConvertCommand 测试场景。
 */
void TestCommandLineParser::testHasConvertCommand() {
    // 有 convert 命令（无论子命令是否有效）
    auto* p1 = makeParser({"easykiconverter", "convert", "-o", "out"});
    QVERIFY(p1->parse());
    QVERIFY(p1->hasConvertCommand());
    delete p1;

    // 无 convert 命令
    auto* p2 = makeParser({"easykiconverter", "--version"});
    QVERIFY(p2->parse());
    QVERIFY2(!p2->hasConvertCommand(), "--version 不应设置 hasConvertCommand");
    delete p2;
}

// ========== 新增 CLI 标志：默认值 ==========

/**
 * @brief 验证 testWeakNetworkDefaultFalse 测试场景。
 */
void TestCommandLineParser::testWeakNetworkDefaultFalse() {
    // parser：负责解析测试输入的解析器实例。
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o"});
    parser->parse();
    QVERIFY2(parser->weakNetworkSupport() == false, "--weak-network 未设置时默认关闭");
    delete parser;
}

/**
 * @brief 验证 testUpdateModeDefaultFalse 测试场景。
 */
void TestCommandLineParser::testUpdateModeDefaultFalse() {
    // parser：负责解析测试输入的解析器实例。
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o"});
    parser->parse();
    QVERIFY2(parser->updateMode() == false, "--update-mode 未设置时默认关闭");
    delete parser;
}

/**
 * @brief 验证 test3dPathModeDefaultRelative 测试场景。
 */
void TestCommandLineParser::test3dPathModeDefaultRelative() {
    // parser：负责解析测试输入的解析器实例。
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o"});
    parser->parse();
    QVERIFY(parser->validate());
    QCOMPARE(parser->model3DPathMode(), QStringLiteral("relative"));
    delete parser;
}

/**
 * @brief 验证 testOverwriteDefaultTrue 测试场景。
 */
void TestCommandLineParser::testOverwriteDefaultTrue() {
    // parser：负责解析测试输入的解析器实例。
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o"});
    parser->parse();
    QVERIFY2(parser->overwriteExistingFiles() == true, "--no-overwrite 未设置时默认 true");
    delete parser;
}

/**
 * @brief 验证 testSymbolDescriptionDefaultEmpty 测试场景。
 */
void TestCommandLineParser::testSymbolDescriptionDefaultEmpty() {
    // parser：负责解析测试输入的解析器实例。
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o"});
    parser->parse();
    QVERIFY(parser->symbolDescription().isEmpty());
    delete parser;
}

/**
 * @brief 验证 testFootprintDescriptionDefaultEmpty 测试场景。
 */
void TestCommandLineParser::testFootprintDescriptionDefaultEmpty() {
    // parser：负责解析测试输入的解析器实例。
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o"});
    parser->parse();
    QVERIFY(parser->footprintDescription().isEmpty());
    delete parser;
}

// ========== 新增 CLI 标志：显式值 ==========

/**
 * @brief 验证 testWeakNetworkExplicit 测试场景。
 */
void TestCommandLineParser::testWeakNetworkExplicit() {
    // parser：负责解析测试输入的解析器实例。
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o", "--weak-network"});
    parser->parse();
    QVERIFY(parser->weakNetworkSupport() == true);
    delete parser;
}

/**
 * @brief 验证 testUpdateModeExplicit 测试场景。
 */
void TestCommandLineParser::testUpdateModeExplicit() {
    // parser：负责解析测试输入的解析器实例。
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o", "--update-mode"});
    parser->parse();
    QVERIFY(parser->updateMode() == true);
    delete parser;
}

/**
 * @brief 验证 test3dPathModeAbsolute 测试场景。
 */
void TestCommandLineParser::test3dPathModeAbsolute() {
    auto* parser =
        makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o", "--3d-path-mode", "absolute"});
    parser->parse();
    QVERIFY(parser->validate());
    QCOMPARE(parser->model3DPathMode(), QStringLiteral("absolute"));
    delete parser;
}

/**
 * @brief 验证 testNoOverwriteExplicit 测试场景。
 */
void TestCommandLineParser::testNoOverwriteExplicit() {
    // --no-overwrite 设置时，overwriteExistingFiles() 返回 false
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o", "--no-overwrite"});
    parser->parse();
    QVERIFY(parser->overwriteExistingFiles() == false);
    delete parser;
}

/**
 * @brief 验证 testSymbolDescriptionExplicit 测试场景。
 */
void TestCommandLineParser::testSymbolDescriptionExplicit() {
    // parser：负责解析测试输入的解析器实例。
    auto* parser = makeParser(
        {"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o", "--symbol-description", "My Symbol Lib"});
    parser->parse();
    QCOMPARE(parser->symbolDescription(), QStringLiteral("My Symbol Lib"));
    delete parser;
}

/**
 * @brief 验证 testFootprintDescriptionExplicit 测试场景。
 */
void TestCommandLineParser::testFootprintDescriptionExplicit() {
    // parser：负责解析测试输入的解析器实例。
    auto* parser = makeParser({"easykiconverter",
                               "convert",
                               "bom",
                               "-i",
                               "b.xlsx",
                               "-o",
                               "o",
                               "--footprint-description",
                               "My Footprint Lib"});
    parser->parse();
    QCOMPARE(parser->footprintDescription(), QStringLiteral("My Footprint Lib"));
    delete parser;
}

// ========== 新增 CLI 标志：验证 ==========

/**
 * @brief 验证 test3dPathModeInvalid 测试场景。
 */
void TestCommandLineParser::test3dPathModeInvalid() {
    auto* parser =
        makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o", "--3d-path-mode", "relativee"});
    parser->parse();
    QVERIFY(!parser->validate());
    QVERIFY(parser->validationError().contains(QStringLiteral("无效的 3D 模型路径模式")));
    delete parser;
}

// ========== 补全脚本包含新选项 ==========

/**
 * @brief 验证 testCompletionScriptContainsNewOptions 测试场景。
 */
void TestCommandLineParser::testCompletionScriptContainsNewOptions() {
    // bashScript：Bash 补全脚本。
    QString bashScript = CompletionGenerator::generate(CompletionGenerator::Shell::Bash);
    // zshScript：Zsh 测试脚本内容。
    QString zshScript = CompletionGenerator::generate(CompletionGenerator::Shell::Zsh);
    // fishScript：Fish 测试脚本内容。
    QString fishScript = CompletionGenerator::generate(CompletionGenerator::Shell::Fish);

    // Bash: convert_opts 应包含新选项
    QVERIFY2(bashScript.contains("--weak-network"), "Bash 补全应包含 --weak-network");
    QVERIFY2(bashScript.contains("--update-mode"), "Bash 补全应包含 --update-mode");
    QVERIFY2(bashScript.contains("--3d-path-mode"), "Bash 补全应包含 --3d-path-mode");
    QVERIFY2(bashScript.contains("--no-overwrite"), "Bash 补全应包含 --no-overwrite");
    QVERIFY2(bashScript.contains("--symbol-description"), "Bash 补全应包含 --symbol-description");
    QVERIFY2(bashScript.contains("--footprint-description"), "Bash 补全应包含 --footprint-description");
    QVERIFY2(bashScript.contains("--3d-model-format"), "Bash 补全应包含 --3d-model-format");
    QVERIFY2(bashScript.contains("--datasheet"), "Bash 补全应包含 --datasheet");
    QVERIFY2(bashScript.contains("--lib-name"), "Bash 补全应包含 --lib-name");

    // Bash: --3d-path-mode 应有值补全
    QVERIFY2(bashScript.contains("relative absolute"), "Bash 补全 --3d-path-mode 应有 relative/absolute 值");

    // Bash: --3d-model-format 应有值补全
    QVERIFY2(bashScript.contains("wrl step both"), "Bash 补全 --3d-model-format 应有 wrl/step/both 值");

    // Zsh: convert_opts 应包含新选项
    QVERIFY2(zshScript.contains("--weak-network"), "Zsh 补全应包含 --weak-network");
    QVERIFY2(zshScript.contains("--update-mode"), "Zsh 补全应包含 --update-mode");
    QVERIFY2(zshScript.contains("--3d-path-mode"), "Zsh 补全应包含 --3d-path-mode");
    QVERIFY2(zshScript.contains("--no-overwrite"), "Zsh 补全应包含 --no-overwrite");
    QVERIFY2(zshScript.contains("--symbol-description"), "Zsh 补全应包含 --symbol-description");
    QVERIFY2(zshScript.contains("--footprint-description"), "Zsh 补全应包含 --footprint-description");
    QVERIFY2(zshScript.contains("--3d-model-format"), "Zsh 补全应包含 --3d-model-format");
    QVERIFY2(zshScript.contains("--datasheet"), "Zsh 补全应包含 --datasheet");
    QVERIFY2(zshScript.contains("--lib-name"), "Zsh 补全应包含 --lib-name");

    // Zsh: --3d-path-mode 应有值补全
    QVERIFY2(zshScript.contains("(relative absolute)"), "Zsh 补全 --3d-path-mode 应有 (relative absolute) 值");

    // Zsh: --3d-model-format 应有值补全
    QVERIFY2(zshScript.contains("(wrl step both)"), "Zsh 补全 --3d-model-format 应有 (wrl step both) 值");

    // Fish: 应包含新选项
    QVERIFY2(fishScript.contains("weak-network"), "Fish 补全应包含 weak-network");
    QVERIFY2(fishScript.contains("update-mode"), "Fish 补全应包含 update-mode");
    QVERIFY2(fishScript.contains("3d-path-mode"), "Fish 补全应包含 3d-path-mode");
    QVERIFY2(fishScript.contains("no-overwrite"), "Fish 补全应包含 no-overwrite");
    QVERIFY2(fishScript.contains("symbol-description"), "Fish 补全应包含 symbol-description");
    QVERIFY2(fishScript.contains("footprint-description"), "Fish 补全应包含 footprint-description");
    QVERIFY2(fishScript.contains("3d-model-format"), "Fish 补全应包含 3d-model-format");
    QVERIFY2(fishScript.contains("datasheet"), "Fish 补全应包含 datasheet");
    QVERIFY2(fishScript.contains("lib-name"), "Fish 补全应包含 lib-name");

    // Fish: --3d-model-format 应有值补全
    QVERIFY2(fishScript.contains("wrl step both"), "Fish 补全 --3d-model-format 应有 wrl/step/both 值");
}

// ========== CLI 国际化测试 ==========

/**
 * @brief 验证 testValidationErrorTranslated 测试场景。
 */
void TestCommandLineParser::testValidationErrorTranslated() {
    // 安装英文翻译器
    ConfigService::instance()->setLanguage("en");
    // lang：测试使用的语言代码。
    auto* lang = LanguageManager::instance();
    lang->setLanguage("en", /*force=*/true);

    // 验证错误消息结构正确（源字符串可读，翻译依赖 .qm 文件可用性）
    auto* parser =
        makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o", "--3d-model-format", "obj"});
    parser->parse();
    QVERIFY(!parser->validate());
    // error：测试期间收集的错误或诊断信息。
    QString error = parser->validationError();
    // 无论语言，错误消息应包含格式标识 "3D" 和有效值列表
    QVERIFY2(error.contains("3D"), qPrintable("Expected '3D' in error, got: " + error));
    QVERIFY2(error.contains("wrl"), qPrintable("Expected 'wrl' in error, got: " + error));
    delete parser;
}

/**
 * @brief 验证 testCliHelpTextTranslated 测试场景。
 */
void TestCommandLineParser::testCliHelpTextTranslated() {
    // 安装英文翻译器
    ConfigService::instance()->setLanguage("en");
    // lang：测试使用的语言代码。
    auto* lang = LanguageManager::instance();
    lang->setLanguage("en", /*force=*/true);

    // parser：负责解析测试输入的解析器实例。
    auto* parser = makeParser({"easykiconverter", "convert", "bom", "-i", "b.xlsx", "-o", "o"});
    parser->parse();

    // help：帮助文本。
    QString help = parser->cliHelpText();
    // 帮助文本应包含关键选项（无论语言）
    QVERIFY2(help.contains("--input"), qPrintable("Expected '--input' in help, got: " + help.left(200)));
    QVERIFY2(help.contains("--output"), qPrintable("Expected '--output' in help, got: " + help.left(200)));
    QVERIFY2(help.contains("--3d-model"), qPrintable("Expected '--3d-model' in help, got: " + help.left(200)));
    QVERIFY2(help.contains("bom"), qPrintable("Expected 'bom' in help, got: " + help.left(200)));
    // 新增选项
    QVERIFY2(help.contains("--weak-network"), qPrintable("Expected '--weak-network' in help"));
    QVERIFY2(help.contains("--update-mode"), qPrintable("Expected '--update-mode' in help"));
    QVERIFY2(help.contains("--3d-path-mode"), qPrintable("Expected '--3d-path-mode' in help"));
    QVERIFY2(help.contains("--no-overwrite"), qPrintable("Expected '--no-overwrite' in help"));
    QVERIFY2(help.contains("--symbol-description"), qPrintable("Expected '--symbol-description' in help"));
    QVERIFY2(help.contains("--footprint-description"), qPrintable("Expected '--footprint-description' in help"));
    delete parser;
}

QTEST_MAIN(TestCommandLineParser)
#include "test_command_line_parser.moc"
