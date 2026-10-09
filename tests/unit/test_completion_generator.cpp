#include "utils/cli/CompletionGenerator.h"

#include <QString>
#include <QTest>

using namespace EasyKiConverter;

class TestCompletionGenerator : public QObject {
    Q_OBJECT

private slots:
    void testGenerateBash();
    void testGenerateZsh();
    void testGenerateFish();
    void testParseShell();
    void testGetInstallInstructions();

private:
    void verifyBashScript(const QString& script);
    void verifyZshScript(const QString& script);
    void verifyFishScript(const QString& script);
};

/**
 * @brief 验证 testGenerateBash 测试场景。
 */
void TestCompletionGenerator::testGenerateBash() {
    // script：测试用脚本内容。
    QString script = CompletionGenerator::generate(CompletionGenerator::Shell::Bash);

    QVERIFY(!script.isEmpty());
    QVERIFY(script.contains("# EasyKiConverter bash completion script"));
    QVERIFY(script.contains("_easykiconverter_complete()"));
    QVERIFY(script.contains("complete -F _easykiconverter_complete easykiconverter"));
    QVERIFY(script.contains("convert"));
    QVERIFY(script.contains("bom"));
    QVERIFY(script.contains("component"));
    QVERIFY(script.contains("batch"));
    QVERIFY(script.contains("--input"));
    QVERIFY(script.contains("--output"));
    QVERIFY(script.contains("--component"));
    QVERIFY(script.contains("--completion"));
    QVERIFY(script.contains("--complete"));
    QVERIFY(script.contains("lcsc-id"));
}

/**
 * @brief 验证 testGenerateZsh 测试场景。
 */
void TestCompletionGenerator::testGenerateZsh() {
    // script：测试用脚本内容。
    QString script = CompletionGenerator::generate(CompletionGenerator::Shell::Zsh);

    QVERIFY(!script.isEmpty());
    QVERIFY(script.contains("#compdef easykiconverter"));
    QVERIFY(script.contains("_easykiconverter()"));
    QVERIFY(script.contains("convert"));
    QVERIFY(script.contains("bom"));
    QVERIFY(script.contains("component"));
    QVERIFY(script.contains("batch"));
    QVERIFY(script.contains("--input"));
    QVERIFY(script.contains("--output"));
    QVERIFY(script.contains("--component"));
    QVERIFY(script.contains("--completion"));
    QVERIFY(script.contains("--complete"));
    QVERIFY(script.contains("lcsc-id"));
}

/**
 * @brief 验证 testGenerateFish 测试场景。
 */
void TestCompletionGenerator::testGenerateFish() {
    // script：测试用脚本内容。
    QString script = CompletionGenerator::generate(CompletionGenerator::Shell::Fish);

    QVERIFY(!script.isEmpty());
    QVERIFY(script.contains("# EasyKiConverter fish completion script"));
    QVERIFY(script.contains("complete -c easykiconverter"));
    QVERIFY(script.contains("convert"));
    QVERIFY(script.contains("bom"));
    QVERIFY(script.contains("component"));
    QVERIFY(script.contains("batch"));
    QVERIFY(script.contains("-l input"));  // Fish uses -l <long-name>
    QVERIFY(script.contains("-l output"));
    QVERIFY(script.contains("-l component"));
    QVERIFY(script.contains("-l completion"));
    // 注意: --complete 是 bash 动态补全辅助选项，Fish 不需要
}

/**
 * @brief 验证 testParseShell 测试场景。
 */
void TestCompletionGenerator::testParseShell() {
    QCOMPARE(CompletionGenerator::parseShell("bash"), CompletionGenerator::Shell::Bash);
    QCOMPARE(CompletionGenerator::parseShell("BASH"), CompletionGenerator::Shell::Bash);
    QCOMPARE(CompletionGenerator::parseShell("Bash"), CompletionGenerator::Shell::Bash);

    QCOMPARE(CompletionGenerator::parseShell("zsh"), CompletionGenerator::Shell::Zsh);
    QCOMPARE(CompletionGenerator::parseShell("ZSH"), CompletionGenerator::Shell::Zsh);
    QCOMPARE(CompletionGenerator::parseShell("Zsh"), CompletionGenerator::Shell::Zsh);

    QCOMPARE(CompletionGenerator::parseShell("fish"), CompletionGenerator::Shell::Fish);
    QCOMPARE(CompletionGenerator::parseShell("FISH"), CompletionGenerator::Shell::Fish);
    QCOMPARE(CompletionGenerator::parseShell("Fish"), CompletionGenerator::Shell::Fish);

    // Default to bash for unknown
    QCOMPARE(CompletionGenerator::parseShell("unknown"), CompletionGenerator::Shell::Bash);
    QCOMPARE(CompletionGenerator::parseShell(""), CompletionGenerator::Shell::Bash);
}

/**
 * @brief 验证 testGetInstallInstructions 测试场景。
 */
void TestCompletionGenerator::testGetInstallInstructions() {
    // bashInstructions：Bash 补全说明。
    QString bashInstructions = CompletionGenerator::getInstallInstructions(CompletionGenerator::Shell::Bash);
    QVERIFY(!bashInstructions.isEmpty());
    QVERIFY(bashInstructions.contains("Bash"));
    QVERIFY(bashInstructions.contains("--completion bash"));
    QVERIFY(bashInstructions.contains("~/.bashrc"));

    // zshInstructions：Zsh 补全说明。
    QString zshInstructions = CompletionGenerator::getInstallInstructions(CompletionGenerator::Shell::Zsh);
    QVERIFY(!zshInstructions.isEmpty());
    QVERIFY(zshInstructions.contains("Zsh"));
    QVERIFY(zshInstructions.contains("--completion zsh"));
    QVERIFY(zshInstructions.contains("~/.zshrc"));

    // fishInstructions：Fish 补全说明。
    QString fishInstructions = CompletionGenerator::getInstallInstructions(CompletionGenerator::Shell::Fish);
    QVERIFY(!fishInstructions.isEmpty());
    QVERIFY(fishInstructions.contains("Fish"));
    QVERIFY(fishInstructions.contains("--completion fish"));
    QVERIFY(fishInstructions.contains("~/.config/fish"));
}

/**
 * @brief 验证 verifyBashScript 测试场景。
 * @param script 待执行的脚本内容。
 */
void TestCompletionGenerator::verifyBashScript(const QString& script) {
    QVERIFY(script.contains("cur"));
    QVERIFY(script.contains("prev"));
    QVERIFY(script.contains("COMPREPLY"));
}

/**
 * @brief 验证 verifyZshScript 测试场景。
 * @param script 待执行的脚本内容。
 */
void TestCompletionGenerator::verifyZshScript(const QString& script) {
    QVERIFY(script.contains("_arguments"));
    QVERIFY(script.contains("_describe"));
}

/**
 * @brief 验证 verifyFishScript 测试场景。
 * @param script 待执行的脚本内容。
 */
void TestCompletionGenerator::verifyFishScript(const QString& script) {
    QVERIFY(script.contains("complete"));
    QVERIFY(script.contains("-c easykiconverter"));
}

QTEST_MAIN(TestCompletionGenerator)
#include "test_completion_generator.moc"
