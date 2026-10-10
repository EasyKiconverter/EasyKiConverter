#ifndef KICADLIBRARYTABLEMANAGER_H
#define KICADLIBRARYTABLEMANAGER_H

#include <QString>

namespace EasyKiConverter {

/**
 * @brief 管理导出库在 KiCad 工程库表中的登记。
 */
class KiCadLibraryTableManager {
public:
    /**
     * @brief 符号库或封装库登记结果。
     */
    enum class RegistrationResult {
        Registered,  ///< 已写入目标工程库表。
        ImportGuideWritten,  ///< 未找到工程，已生成手动导入指南。
        Failed,  ///< 登记或指南写入失败。
    };

    /**
     * @brief 登记符号库。
     * @param outputDir 导出输出目录。
     * @param libName 库名称。
     * @param libFilePath 符号库文件路径。
     * @param libraryDescription 库描述。
     * @return 登记结果。
     */
    static RegistrationResult registerSymbolLibrary(const QString& outputDir,
                                                    const QString& libName,
                                                    const QString& libFilePath,
                                                    const QString& libraryDescription);

    /**
     * @brief 登记封装库。
     * @param outputDir 导出输出目录。
     * @param libName 库名称。
     * @param libDirPath 封装库目录路径。
     * @param libraryDescription 库描述。
     * @return 登记结果。
     */
    static RegistrationResult registerFootprintLibrary(const QString& outputDir,
                                                       const QString& libName,
                                                       const QString& libDirPath,
                                                       const QString& libraryDescription);

private:
    static bool writeImportGuide(const QString& outputDir);
    static QString findKiCadProjectRoot(const QString& outputDir);

    static bool updateProjectLibraryTable(const QString& projectRoot,
                                          const QString& tableFileName,
                                          const QString& tableRootName,
                                          const QString& libName,
                                          const QString& uri,
                                          const QString& libraryDescription);

    static QString makeLibraryEntry(const QString& libName, const QString& uri, const QString& libraryDescription);
    static QString makeProjectUri(const QString& projectRoot, const QString& libraryPath);
};

}  // namespace EasyKiConverter

#endif  // KICADLIBRARYTABLEMANAGER_H
