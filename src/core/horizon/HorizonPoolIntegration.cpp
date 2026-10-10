#include "HorizonPoolIntegration.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>

namespace EasyKiConverter {
namespace {

constexpr int kProcessTimeoutMs = 120000;  ///< 官方 Horizon Python 操作的最大等待时间（毫秒）。

/**
 * @brief 生成更新 Horizon Pool 数据库的官方 Python 脚本。
 */
QString updateScript() {
    return QStringLiteral(
        "import os, sys\n"
        "import horizon\n"
        "pool = os.path.abspath(sys.argv[1])\n"
        "horizon.Pool.update(pool)\n"
        "if not os.path.isfile(os.path.join(pool, 'pool.db')):\n"
        "    raise RuntimeError('Horizon PoolUpdater returned without creating pool.db')\n");
}

/**
 * @brief 生成注册 Horizon Pool 的官方 Python 脚本。
 */
QString registerScript() {
    return QStringLiteral(
        "import json, os, sys\n"
        "import horizon\n"
        "pool = os.path.abspath(sys.argv[1])\n"
        "with open(os.path.join(pool, 'pool.json'), encoding='utf-8') as f:\n"
        "    pool_uuid = json.load(f)['uuid']\n"
        "registered = horizon.PoolManager.get_pools()\n"
        "if pool in registered and registered[pool] != pool_uuid:\n"
        "    raise RuntimeError('Horizon Pool at this path has a different UUID: ' + pool)\n"
        "for registered_path, registered_uuid in registered.items():\n"
        "    if os.path.abspath(registered_path) != pool and registered_uuid == pool_uuid:\n"
        "        raise RuntimeError('Horizon Pool UUID is already registered at another path: ' + registered_path)\n"
        "horizon.PoolManager.add_pool(pool)\n"
        "registered_after = horizon.PoolManager.get_pools()\n"
        "if pool not in {os.path.abspath(p) for p in registered_after}:\n"
        "    raise RuntimeError('Horizon Pool was not registered: ' + pool)\n");
}

/**
 * @brief 合并进程标准错误和标准输出，生成可读诊断文本。
 * @param process 已完成或正在结束的官方脚本进程。
 * @return 合并并清理后的诊断文本。
 */
QString processOutput(QProcess& process) {
    const QByteArray standardError = process.readAllStandardError();  ///< 官方脚本的标准错误输出。
    const QByteArray standardOutput = process.readAllStandardOutput();  ///< 官方脚本的标准输出。
    return QString::fromLocal8Bit(standardError + standardOutput).trimmed();
}

}  // namespace

/**
 * @brief 执行 Horizon 官方 Python binding 脚本并收集失败诊断。
 * @param poolPath 待操作的 Horizon Pool 路径。
 * @param script 要执行的 Python 脚本内容。
 * @param operation 操作名称。
 * @param diagnostics 用于追加诊断信息的列表。
 * @return 脚本成功完成返回 true，否则返回 false。
 */
bool runOfficialScript(const QString& poolPath,
                       const QString& script,
                       const QString& operation,
                       QStringList& diagnostics) {
    const QFileInfo poolInfo(QDir(poolPath).filePath(QStringLiteral("pool.json")));
    if (!poolInfo.isFile()) {
        diagnostics.append(
            QStringLiteral("Horizon: %1 前缺少 pool.json：%2").arg(operation, poolInfo.absoluteFilePath()));
        return false;
    }

    QProcess process;  ///< 官方 Python binding 的子进程。
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();  ///< 子进程环境变量。
    // 可选的 Horizon Python 模块路径。
    QString pythonPath = environment.value(QStringLiteral("EASYKICONVERTER_HORIZON_PYTHONPATH")).trimmed();
    if (!pythonPath.isEmpty()) {
        const QFileInfo modulePathInfo(pythonPath);  ///< Python 模块路径的文件信息。
        if (!modulePathInfo.isAbsolute())
            pythonPath = QDir::cleanPath(QDir::current().absoluteFilePath(pythonPath));
        const QString currentPythonPath =
            environment.value(QStringLiteral("PYTHONPATH"));  ///< 原有 Python 模块搜索路径。
        environment.insert(
            QStringLiteral("PYTHONPATH"),
            currentPythonPath.isEmpty() ? pythonPath : pythonPath + QDir::listSeparator() + currentPythonPath);
    }
    process.setProcessEnvironment(environment);
    const QString interpreter =  ///< Horizon Python binding 使用的解释器路径。
        environment.value(QStringLiteral("EASYKICONVERTER_HORIZON_PYTHON"), QStringLiteral("python3"));
    process.start(interpreter, {QStringLiteral("-c"), script, poolInfo.absolutePath()});
    if (!process.waitForStarted(5000)) {
        diagnostics.append(QStringLiteral("Horizon: %1 无法启动官方 Python binding（%2）：%3")
                               .arg(operation, interpreter, process.errorString()));
        return false;
    }
    if (!process.waitForFinished(kProcessTimeoutMs)) {
        process.kill();
        process.waitForFinished();
        diagnostics.append(QStringLiteral("Horizon: 官方 %1 超时：%2").arg(operation, poolInfo.absolutePath()));
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QString detail = processOutput(process);  ///< 官方脚本返回的详细错误文本。
        if (detail.isEmpty())
            detail = process.errorString();
        diagnostics.append(QStringLiteral("Horizon: 官方 %1 失败：%2").arg(operation, detail));
        return false;
    }
    return true;
}

/**
 * @brief 使用官方 PoolUpdater 更新 Horizon Pool 数据库。
 * @param poolPath 待更新的 Horizon Pool 路径。
 * @param diagnostics 用于追加诊断信息的列表。
 * @return 更新成功返回 true，否则返回 false。
 */
bool HorizonPoolIntegration::updatePool(const QString& poolPath, QStringList& diagnostics) {
    if (!runOfficialScript(poolPath, updateScript(), QStringLiteral("PoolUpdater"), diagnostics))
        return false;
    const QFileInfo databaseInfo(
        QDir(poolPath).filePath(QStringLiteral("pool.db")));  ///< 更新后的 Pool 数据库文件信息。
    if (!databaseInfo.isFile() || databaseInfo.size() <= 0) {
        diagnostics.append(
            QStringLiteral("Horizon: 官方 PoolUpdater 未生成有效 pool.db：%1").arg(databaseInfo.absoluteFilePath()));
        return false;
    }
    return true;
}

/**
 * @brief 使用官方 PoolManager 注册已更新的 Horizon Pool。
 * @param poolPath 待注册的 Horizon Pool 路径。
 * @param diagnostics 用于追加诊断信息的列表。
 * @return 注册成功返回 true，否则返回 false。
 */
bool HorizonPoolIntegration::registerPool(const QString& poolPath, QStringList& diagnostics) {
    const QFileInfo databaseInfo(
        QDir(poolPath).filePath(QStringLiteral("pool.db")));  ///< 注册前的 Pool 数据库文件信息。
    if (!databaseInfo.isFile() || databaseInfo.size() <= 0) {
        diagnostics.append(
            QStringLiteral("Horizon: Pool 注册前缺少有效 pool.db：%1").arg(databaseInfo.absoluteFilePath()));
        return false;
    }
    if (!runOfficialScript(poolPath, registerScript(), QStringLiteral("PoolManager 注册"), diagnostics))
        return false;

    // Horizon 的运行时广播只对由自身管理器注入 endpoint 和 cookie 的进程开放。
    // 导出器不能安全发现或伪造该内部 IPC，因此已运行实例仍需手动重载或重启。
    diagnostics.append(QStringLiteral(
        "Horizon: Pool 已注册；已运行的 Horizon 不会通过外部接口自动刷新，请手动重新加载或重启 Horizon"));
    return true;
}

/**
 * @brief 依次更新并注册 Horizon Pool。
 * @param poolPath 待处理的 Horizon Pool 路径。
 * @param diagnostics 用于追加诊断信息的列表。
 * @return 更新和注册均成功返回 true，否则返回 false。
 */
bool HorizonPoolIntegration::updateAndRegister(const QString& poolPath, QStringList& diagnostics) {
    if (!updatePool(poolPath, diagnostics))
        return false;
    return registerPool(poolPath, diagnostics);
}

}  // namespace EasyKiConverter
