#ifndef BATCHCONVERTER_H
#define BATCHCONVERTER_H

#include "BaseConverter.h"

namespace EasyKiConverter {

/**
 * @brief 批量转换器
 *
 * 处理批量元器件的转换逻辑。
 */
class BatchConverter : public BaseConverter {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param context CLI 上下文
     * @param parent 父对象指针
     */
    explicit BatchConverter(CliContext* context, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~BatchConverter() override = default;

    /**
     * @brief 执行批量转换
     * @return 成功返回 true，失败返回 false
     */
    bool execute() override;
};

}  // namespace EasyKiConverter

#endif  // BATCHCONVERTER_H
