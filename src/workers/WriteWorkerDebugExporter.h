#pragma once

namespace EasyKiConverter {

class ComponentExportStatus;
class WriteWorker;

/**
 * @brief 导出单个元件的调试原始数据和结构化摘要。
 *
 * 调试文件写入与正常导出文件写入相互独立，因此单独维护其目录和序列化职责。
 */
class WriteWorkerDebugExporter final {
public:
    /**
     * @brief 保存所属写入工作线程的状态引用。
     * @param owner 所属写入工作线程。
     */
    explicit WriteWorkerDebugExporter(WriteWorker& owner);

    /**
     * @brief 将当前元件的调试数据写入输出目录。
     * @param status 当前元件的导出状态。
     */
    bool exportData(ComponentExportStatus& status);

private:
    /**
     * @brief 被复用输出目录策略的写入工作线程。
     */
    WriteWorker& m_owner;
};

}  // namespace EasyKiConverter
