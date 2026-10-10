#ifndef LOGMODULE_H
#define LOGMODULE_H

#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace EasyKiConverter {

/**
 * @brief 日志模块定义
 * 
 * 使用位掩码支持模块组合过滤，例如：
 * LogModule::Network | LogModule::Export
 */
enum class LogModule : uint32_t {
    None = 0,
    Core = 1 << 0,  ///< 核心功能 (main, LanguageManager)
    Network = 1 << 1,  ///< 网络请求 (NetworkClient, FetchWorker)
    Export = 1 << 2,  ///< 导出功能 (ExportService, ExportWorker)
    UI = 1 << 3,  ///< 用户界面 (ViewModels)
    Parser = 1 << 4,  ///< 数据解析 (BOM, JSON)
    KiCad = 1 << 5,  ///< KiCad 导出器 (Symbol, Footprint, 3D)
    EasyEDA = 1 << 6,  ///< EasyEDA API 集成
    Pipeline = 1 << 7,  ///< 流水线管理
    Worker = 1 << 8,  ///< 工作线程 (所有 Worker)
    Config = 1 << 9,  ///< 配置管理 (ConfigService)
    I18N = 1 << 10,  ///< 国际化 (LanguageManager)
    All = 0xFFFFFFFF
};

/**
 * @brief 支持位运算符 |
 */
inline LogModule operator|(LogModule a, LogModule b) {
    return static_cast<LogModule>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

/**
 * @brief 支持位运算符 &
 */
inline LogModule operator&(LogModule a, LogModule b) {
    return static_cast<LogModule>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

/**
 * @brief 支持位运算符 |=
 */
inline LogModule& operator|=(LogModule& a, LogModule b) {
    a = a | b;
    return a;
}

/**
 * @brief 检查模块是否包含指定模块
 * @param modules 日志模块集合。
 * @param target 目标对象。
 */
inline bool hasModule(LogModule modules, LogModule target) {
    return (static_cast<uint32_t>(modules) & static_cast<uint32_t>(target)) != 0;
}

/**
 * @brief 将模块转换为字符串名称
 * @param module 日志模块。
 */
inline QString logModuleToString(LogModule module) {
    // 检查单一模块
    if (module == LogModule::None)
        return QStringLiteral("None");
    if (module == LogModule::All)
        return QStringLiteral("All");

    // 单一模块名称
    if (module == LogModule::Core)
        return QStringLiteral("Core");
    if (module == LogModule::Network)
        return QStringLiteral("Network");
    if (module == LogModule::Export)
        return QStringLiteral("Export");
    if (module == LogModule::UI)
        return QStringLiteral("UI");
    if (module == LogModule::Parser)
        return QStringLiteral("Parser");
    if (module == LogModule::KiCad)
        return QStringLiteral("KiCad");
    if (module == LogModule::EasyEDA)
        return QStringLiteral("EasyEDA");
    if (module == LogModule::Pipeline)
        return QStringLiteral("Pipeline");
    if (module == LogModule::Worker)
        return QStringLiteral("Worker");
    if (module == LogModule::Config)
        return QStringLiteral("Config");
    if (module == LogModule::I18N)
        return QStringLiteral("I18N");

    // 组合模块（返回第一个匹配的）
    QStringList names;
    auto checkAndAdd = [&](LogModule m, const QString& name) {
        if (hasModule(module, m))
            names << name;
    };

    checkAndAdd(LogModule::Core, "Core");
    checkAndAdd(LogModule::Network, "Network");
    checkAndAdd(LogModule::Export, "Export");
    checkAndAdd(LogModule::UI, "UI");
    checkAndAdd(LogModule::Parser, "Parser");
    checkAndAdd(LogModule::KiCad, "KiCad");
    checkAndAdd(LogModule::EasyEDA, "EasyEDA");
    checkAndAdd(LogModule::Pipeline, "Pipeline");
    checkAndAdd(LogModule::Worker, "Worker");
    checkAndAdd(LogModule::Config, "Config");
    checkAndAdd(LogModule::I18N, "I18N");

    return names.join("|");
}

/**
 * @brief 从字符串解析模块
 * @param str 待解析的字符串。
 */
inline LogModule logModuleFromString(const QString& str) {
    QString upper = str.toUpper().trimmed();  ///< 规范化后的大写模块名称。
    if (upper == "CORE")
        return LogModule::Core;
    if (upper == "NETWORK")
        return LogModule::Network;
    if (upper == "EXPORT")
        return LogModule::Export;
    if (upper == "UI")
        return LogModule::UI;
    if (upper == "PARSER")
        return LogModule::Parser;
    if (upper == "KICAD")
        return LogModule::KiCad;
    if (upper == "EASYEDA")
        return LogModule::EasyEDA;
    if (upper == "PIPELINE")
        return LogModule::Pipeline;
    if (upper == "WORKER")
        return LogModule::Worker;
    if (upper == "CONFIG")
        return LogModule::Config;
    if (upper == "I18N")
        return LogModule::I18N;
    if (upper == "ALL")
        return LogModule::All;
    return LogModule::Core;  // 默认
}

}  // namespace EasyKiConverter

#endif  // LOGMODULE_H
