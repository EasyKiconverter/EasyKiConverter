#pragma once

/**
 * @file IRTypes.h
 * @brief EDA 无关的通用枚举和类型定义
 *
 * 所有枚举值在各 EDA 格式间保持语义一致。
 * 来源特有值通过各 Importer 的映射表转换为这些通用枚举。
 */

#include <QColor>
#include <QString>

namespace EasyKiConverter {
namespace IR {

// ==================== EasyEDA 单位转换常量 ====================

/**
 * @brief EasyEDA 坐标单位 → mm 转换因子（1 EasyEDA 单位 = 10 mil = 0.254 mm）
 */
constexpr double EASYEDA_PX_TO_MM = 0.254;

/**
 * @brief EasyEDA pt → mm 转换因子
 */
constexpr double EASYEDA_PT_TO_MM = 0.352778;

/**
 * @brief 焊盘形状枚举
 * @note 替代 FootprintPad.shape 原始字符串（"RECT"/"ELLIPSE"/"POLYGON" 等）
 */
enum class PadShape {
    Rect,  ///< 矩形焊盘
    Ellipse,  ///< 椭圆/圆形焊盘
    Oval,  ///< 椭圆长圆焊盘
    RoundRect,  ///< 圆角矩形焊盘
    Polygon,  ///< 多边形异形焊盘
    Trapezoid  ///< 梯形焊盘
};

/**
 * @brief 焊盘类型枚举
 * @note 替代通过 holeRadius > 0 推断 SMD/通孔的逻辑
 */
enum class PadType {
    Smd,  ///< 表面贴装焊盘（无孔）
    ThroughHole  ///< 通孔焊盘（有孔）
};

/**
 * @brief 引脚电气类型枚举
 * @note 替代 EasyEDA PinType 整数枚举 (0-4)
 */
enum class PinElectricalType {
    Unspecified = 0,  ///< 未指定
    Input,  ///< 输入
    Output,  ///< 输出
    Bidirectional,  ///< 双向
    Passive,  ///< 无源（电阻、电容等）
    Power,  ///< 电源引脚
    OpenCollector,  ///< 集电极开路
    OpenEmitter  ///< 发射极开路
};

/**
 * @brief 引脚方向枚举
 * @note 替代 EasyEDA 的 rotation 整数 (0/90/180/270)
 */
enum class PinDirection {
    Right = 0,  ///< 向右
    Left,  ///< 向左
    Up,  ///< 向上
    Down  ///< 向下
};

/**
 * @brief 引脚 IEEE 图形装饰枚举
 *
 * 描述引脚端点的 IEEE 标准图形符号，与电气类型解耦。
 * KiCad 通过 pin style 映射，Altium 通过四个边缘字节映射。
 */
enum class PinDecoration {
    None = 0,  ///< 无装饰
    Dot,  ///< 取反圆圈（低电平有效）
    Clock,  ///< 时钟三角
    InvertedClock,  ///< 取反 + 时钟
    ActiveLow,  ///< 低电平有效（与 Dot 类似，语义不同）
    Postponed,  ///< 延迟输出
    OpenCollector,  ///< 集电极开路
    OpenEmitter,  ///< 发射极开路
    HiZ,  ///< 高阻
    ShiftLeft,  ///< 左移符号
    AnalogInput,  ///< 模拟输入
    NoConnect,  ///< 无连接
    Pulse,  ///< 脉冲
    GroupLine,  ///< 总线组
    FlagRight,  ///< 右侧标志
    FlagLeft,  ///< 左侧标志
    ShiftRight,  ///< 右移符号
    HighCurrent,  ///< 高电流
    Schmitt,  ///< 施密特触发器
    Delay,  ///< 延迟
    ActiveLowOutput,  ///< 低电平有效输出
    OpenCollectorPullUp,  ///< 上拉集电极开路
    OpenEmitterPullUp,  ///< 上拉发射极开路
    DigitalInput,  ///< 数字输入
    GroupBinary,  ///< 二进制总线组
    InputOutput,  ///< 输入输出
    OpenCircuitOutput,  ///< 开路输出
    Pi,  ///< Pi 符号
    GreaterEqual,  ///< 大于等于
    LessEqual,  ///< 小于等于
    Sigma,  ///< Sigma 符号
    And,  ///< 与逻辑符号
    Inverter,  ///< 反相器符号
    Or,  ///< 或逻辑符号
    Xor,  ///< 异或逻辑符号
    BidirectionalSignalFlow  ///< 双向信号流
};

/**
 * @brief 文本锚点方向枚举
 *
 * 描述文本相对于锚点的对齐方向。
 */
enum class TextOrientation {
    Horizontal,  ///< 水平
    Vertical  ///< 垂直
};

/**
 * @brief PCB 层类型枚举（EDA 无关）
 * @note 替代 EasyEDA 数字层 ID（1=Top, 2=Bottom, 99=KeepOut 等）
 *
 * 各来源的层 ID 通过各自的映射表转换为此枚举。
 * 导出器从此枚举映射到目标格式的层表示。
 */
enum class LayerType {
    TopCopper,  ///< 顶层铜
    BottomCopper,  ///< 底层铜
    InnerCopper1,  ///< 内层 1
    InnerCopper2,  ///< 内层 2
    InnerCopper3,  ///< 内层 3
    InnerCopper4,  ///< 内层 4
    TopSilk,  ///< 顶层丝印
    BottomSilk,  ///< 底层丝印
    TopPaste,  ///< 顶层锡膏
    BottomPaste,  ///< 底层锡膏
    TopMask,  ///< 顶层阻焊
    BottomMask,  ///< 底层阻焊
    TopOverlay,  ///< 顶层覆盖
    BottomOverlay,  ///< 底层覆盖
    TopAssembly,  ///< 顶层装配
    BottomAssembly,  ///< 底层装配
    MultiLayer,  ///< 多层（通孔）
    KeepOut,  ///< 禁止布线区
    EdgeCuts,  ///< 板框切割
    Mechanical1,  ///< 机械层 1
    Mechanical2,  ///< 机械层 2
    Mechanical3,  ///< 机械层 3
    Mechanical4,  ///< 机械层 4
    UserDefined,  ///< 用户自定义层
    Unknown  ///< 未知层（无法映射时的默认值）
};

/**
 * @brief 笔划样式枚举
 * @note 替代 EasyEDA strokeStyle 字符串
 */
enum class StrokeStyle {
    Solid,  ///< 实线
    Dashed,  ///< 虚线
    Dotted  ///< 点线
};

/**
 * @brief 文本对齐锚点枚举
 * @note 替代 EasyEDA anchor/textAnchor 字符串
 */
enum class TextAnchor {
    Start,  ///< 左对齐
    Middle,  ///< 居中
    End  ///< 右对齐
};

/**
 * @brief 将颜色十六进制字符串解析为 QColor
 * @param colorStr 颜色字符串（如 "#FF0000" 或 "rgb(255,0,0)"）
 * @param fallback 解析失败时的默认颜色
 * @return 解析后的 QColor
 */
QColor parseColor(const QString& colorStr, const QColor& fallback = Qt::black);

}  // namespace IR
}  // namespace EasyKiConverter
