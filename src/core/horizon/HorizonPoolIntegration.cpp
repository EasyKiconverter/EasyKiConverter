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

QString pythonScript() {
    return QStringLiteral(
        "import json, os, sys\n"
        "import horizon\n"
        "pool = os.path.abspath(sys.argv[1])\n"
        "with open(os.path.join(pool, 'pool.json'), encoding='utf-8') as f:\n"
        "    pool_uuid = json.load(f)['uuid']\n"
        "registered = horizon.PoolManager.get_pools()\n"
        "for registered_path, registered_uuid in registered.items():\n"
        "    if os.path.abspath(registered_path) != pool and registered_uuid == pool_uuid:\n"
        "        raise RuntimeError('Horizon Pool UUID is already registered at another path: ' + registered_path)\n"
        "horizon.Pool.update(pool)\n"
        "if pool not in {os.path.abspath(p) for p in registered}:\n"
        "    horizon.PoolManager.add_pool(pool)\n");
}

QString processOutput(QProcess& process) {
    const QByteArray standardError = process.readAllStandardError();
    const QByteArray standardOutput = process.readAllStandardOutput();
    return QString::fromLocal8Bit(standardError + standardOutput).trimmed();
}

}  // namespace

bool HorizonPoolIntegration::updateAndRegister(const QString& poolPath, QStringList& diagnostics) {
    const QFileInfo poolInfo(QDir(poolPath).filePath(QStringLiteral("pool.json")));
    if (!poolInfo.isFile()) {
        diagnostics.append(QStringLiteral("Horizon: Pool 注册前缺少 pool.json：%1").arg(poolInfo.absoluteFilePath()));
        return false;
    }

    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    const QString pythonPath = environment.value(QStringLiteral("EASYKICONVERTER_HORIZON_PYTHONPATH"));
    if (!pythonPath.isEmpty()) {
        const QString currentPythonPath = environment.value(QStringLiteral("PYTHONPATH"));
        environment.insert(
            QStringLiteral("PYTHONPATH"),
            currentPythonPath.isEmpty() ? pythonPath : pythonPath + QDir::listSeparator() + currentPythonPath);
    }
    process.setProcessEnvironment(environment);
    const QString interpreter =
        environment.value(QStringLiteral("EASYKICONVERTER_HORIZON_PYTHON"), QStringLiteral("python3"));
    process.start(interpreter, {QStringLiteral("-c"), pythonScript(), poolInfo.absolutePath()});
    if (!process.waitForStarted(5000)) {
        diagnostics.append(
            QStringLiteral("Horizon: 无法启动官方 Python binding（%1）：%2").arg(interpreter, process.errorString()));
        return false;
    }
    if (!process.waitForFinished(kProcessTimeoutMs)) {
        process.kill();
        process.waitForFinished();
        diagnostics.append(QStringLiteral("Horizon: 官方 PoolUpdater 超时：%1").arg(poolInfo.absolutePath()));
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QString detail = processOutput(process);
        if (detail.isEmpty())
            detail = process.errorString();
        diagnostics.append(QStringLiteral("Horizon: 官方 PoolUpdater/PoolManager 失败：%1").arg(detail));
        return false;
    }
    return true;
}

}  // namespace EasyKiConverter
