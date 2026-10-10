#ifndef BOMCONVERTER_H
#define BOMCONVERTER_H

#include "BaseConverter.h"

namespace EasyKiConverter {

/**
 * @brief BOM 表转换器
 *
 * 处理 BOM 表文件的转换逻辑。
 */
class BomConverter : public BaseConverter {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param context CLI 上下文
     * @param parent 父对象指针
     */
    explicit BomConverter(CliContext* context, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~BomConverter() override = default;

    /**
     * @brief 执行 BOM 表转换
     * @return 成功返回 true，失败返回 false
     */
    bool execute() override;
};

}  // namespace EasyKiConverter

#endif  // BOMCONVERTER_H
