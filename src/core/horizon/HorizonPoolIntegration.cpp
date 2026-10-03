#include "HorizonPoolIntegration.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>

namespace EasyKiConverter {
namespace {

constexpr int kProcessTimeoutMs = 120000;

QString updateScript() {
    return QStringLiteral(
        "import os, sys\n"
        "import horizon\n"
        "pool = os.path.abspath(sys.argv[1])\n"
        "horizon.Pool.update(pool)\n"
        "if not os.path.isfile(os.path.join(pool, 'pool.db')):\n"
        "    raise RuntimeError('Horizon PoolUpdater returned without creating pool.db')\n");
}

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

QString processOutput(QProcess& process) {
    const QByteArray standardError = process.readAllStandardError();
    const QByteArray standardOutput = process.readAllStandardOutput();
    return QString::fromLocal8Bit(standardError + standardOutput).trimmed();
}

}  // namespace

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

    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    QString pythonPath = environment.value(QStringLiteral("EASYKICONVERTER_HORIZON_PYTHONPATH")).trimmed();
    if (!pythonPath.isEmpty()) {
        const QFileInfo modulePathInfo(pythonPath);
        if (!modulePathInfo.isAbsolute())
            pythonPath = QDir::cleanPath(QDir::current().absoluteFilePath(pythonPath));
        const QString currentPythonPath = environment.value(QStringLiteral("PYTHONPATH"));
        environment.insert(
            QStringLiteral("PYTHONPATH"),
            currentPythonPath.isEmpty() ? pythonPath : pythonPath + QDir::listSeparator() + currentPythonPath);
    }
    process.setProcessEnvironment(environment);
    const QString interpreter =
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
        QString detail = processOutput(process);
        if (detail.isEmpty())
            detail = process.errorString();
        diagnostics.append(QStringLiteral("Horizon: 官方 %1 失败：%2").arg(operation, detail));
        return false;
    }
    return true;
}

bool HorizonPoolIntegration::updatePool(const QString& poolPath, QStringList& diagnostics) {
    if (!runOfficialScript(poolPath, updateScript(), QStringLiteral("PoolUpdater"), diagnostics))
        return false;
    const QFileInfo databaseInfo(QDir(poolPath).filePath(QStringLiteral("pool.db")));
    if (!databaseInfo.isFile() || databaseInfo.size() <= 0) {
        diagnostics.append(
            QStringLiteral("Horizon: 官方 PoolUpdater 未生成有效 pool.db：%1").arg(databaseInfo.absoluteFilePath()));
        return false;
    }
    return true;
}

bool HorizonPoolIntegration::registerPool(const QString& poolPath, QStringList& diagnostics) {
    const QFileInfo databaseInfo(QDir(poolPath).filePath(QStringLiteral("pool.db")));
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

bool HorizonPoolIntegration::updateAndRegister(const QString& poolPath, QStringList& diagnostics) {
    if (!updatePool(poolPath, diagnostics))
        return false;
    return registerPool(poolPath, diagnostics);
}

}  // namespace EasyKiConverter
