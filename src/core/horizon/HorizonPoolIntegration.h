#pragma once

#include <QStringList>

namespace EasyKiConverter {

/**
 * @brief 通过 Horizon 官方 Python binding 更新并注册 Pool。
 *
 * 该类只负责调用官方 `horizon.Pool` 和 `horizon.PoolManager`，不复制
 * Horizon 的 SQLite schema 或配置文件写入逻辑。解释器默认使用 `python3`，
 * 也可通过 `EASYKICONVERTER_HORIZON_PYTHON` 和
 * `EASYKICONVERTER_HORIZON_PYTHONPATH` 指定官方运行环境。
 */
class HorizonPoolIntegration final {
public:
    /**
     * @brief 更新 Pool 数据库并将 Pool 注册到 Horizon。
     * @param poolPath Pool 根目录，必须包含 pool.json。
     * @param diagnostics 追加可展示的错误或警告信息。
     * @return 官方更新和注册均成功时返回 true。
     */
    static bool updateAndRegister(const QString& poolPath, QStringList& diagnostics);
};

}  // namespace EasyKiConverter
