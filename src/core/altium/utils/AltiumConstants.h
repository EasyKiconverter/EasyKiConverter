#pragma once

#include <cstdint>

namespace EasyKiConverter {

/**
 * @brief Altium 格式常量定义
 * @details 集中定义 Altium SchLib/PcbLib 格式中使用的魔数、标志位和尺寸常量。
 *          固定记录长度与字段偏移集中在此处，作为写入器和回归测试共享的协议契约。
 */
namespace AltiumConstants {

// ==================== OLE 复合文档常量 ====================

/**
 * @brief OLE V3 扇区大小（字节）
 */
constexpr uint32_t OLE_SECTOR_SIZE = 512;

/**
 * @brief OLE Mini 扇区大小（字节）
 */
constexpr uint32_t OLE_MINI_SECTOR_SIZE = 64;

/**
 * @brief OLE Mini-Stream 截止大小（字节）
 */
constexpr uint32_t OLE_MINI_STREAM_CUTOFF = 4096;

/**
 * @brief OLE 目录条目大小（字节）
 */
constexpr uint32_t OLE_DIR_ENTRY_SIZE = 128;

/**
 * @brief OLE FAT 链终结标记
 */
constexpr uint32_t OLE_ENDOFCHAIN = 0xFFFFFFFE;

/**
 * @brief OLE 空闲扇区标记
 */
constexpr uint32_t OLE_FREESECT = 0xFFFFFFFF;

/**
 * @brief OLE 无子节点标记
 */
constexpr uint32_t OLE_NOSTREAM = 0xFFFFFFFF;

// ==================== SchLib 记录类型 ====================

/**
 * @brief 组件记录（RECORD=1）
 */
constexpr int SCH_RECORD_COMPONENT = 1;

/**
 * @brief 引脚记录（RECORD=2，二进制格式）
 */
constexpr int SCH_RECORD_PIN = 2;

/**
 * @brief 矩形记录（RECORD=14）
 */
constexpr int SCH_RECORD_RECTANGLE = 14;

/**
 * @brief 线段记录（RECORD=13）
 */
constexpr int SCH_RECORD_LINE = 13;

/**
 * @brief 弧线记录（RECORD=12）
 */
constexpr int SCH_RECORD_ARC = 12;

/**
 * @brief 多边形记录（RECORD=7）
 */
constexpr int SCH_RECORD_POLYGON = 7;

/**
 * @brief 椭圆记录（RECORD=8）
 */
constexpr int SCH_RECORD_ELLIPSE = 8;

/**
 * @brief 折线记录（RECORD=6）
 */
constexpr int SCH_RECORD_POLYLINE = 6;

/**
 * @brief 文本标签记录（RECORD=4）
 */
constexpr int SCH_RECORD_LABEL = 4;

/**
 * @brief 实现列表容器（RECORD=44）
 */
constexpr int SCH_RECORD_IMPLEMENTATION_LIST = 44;

/**
 * @brief 实现记录（RECORD=45）
 */
constexpr int SCH_RECORD_IMPLEMENTATION = 45;

/**
 * @brief 映射定义器列表（RECORD=46）
 */
constexpr int SCH_RECORD_MAP_DEFINER_LIST = 46;

/**
 * @brief 实现参数（RECORD=48）
 */
constexpr int SCH_RECORD_IMPLEMENTATION_PARAMS = 48;

// ==================== SchLib 块标志 ====================

/**
 * @brief 二进制引脚记录标志（size header 高字节）
 */
constexpr uint8_t SCH_BLOCK_FLAG_BINARY_PIN = 0x01;

/**
 * @brief 引脚隐藏标志（PinConglomerate Bit 2）
 */
constexpr uint8_t SCH_PIN_HIDDEN = 0x04;

/**
 * @brief 引脚名称显示标志（PinConglomerate Bit 3）
 */
constexpr uint8_t SCH_PIN_SHOW_NAME = 0x08;

/**
 * @brief 引脚编号显示标志（PinConglomerate Bit 4）
 */
constexpr uint8_t SCH_PIN_SHOW_DESIGNATOR = 0x10;

// ==================== PcbLib 图元对象 ID ====================

/**
 * @brief 弧线
 */
constexpr uint8_t PCB_OBJECT_ARC = 1;

/**
 * @brief 焊盘
 */
constexpr uint8_t PCB_OBJECT_PAD = 2;

/**
 * @brief 过孔
 */
constexpr uint8_t PCB_OBJECT_VIA = 3;

/**
 * @brief 走线
 */
constexpr uint8_t PCB_OBJECT_TRACK = 4;

/**
 * @brief 文本
 */
constexpr uint8_t PCB_OBJECT_TEXT = 5;

/**
 * @brief 填充
 */
constexpr uint8_t PCB_OBJECT_FILL = 6;

/**
 * @brief 区域
 */
constexpr uint8_t PCB_OBJECT_REGION = 11;

/**
 * @brief 3D 元件体
 */
constexpr uint8_t PCB_OBJECT_COMPONENT_BODY = 12;

// ==================== PcbLib 标志位 ====================

/**
 * @brief 已保存标志（Bit 3）
 */
constexpr uint16_t PCB_FLAG_SAVED = 0x08;

/**
 * @brief 顶部阻焊扩展标志（Bit 5）
 */
constexpr uint16_t PCB_FLAG_TENTING_TOP = 0x20;

/**
 * @brief 底部阻焊扩展标志（Bit 6）
 */
constexpr uint16_t PCB_FLAG_TENTING_BOTTOM = 0x40;

/**
 * @brief 禁止布线标志（Bit 9）
 */
constexpr uint16_t PCB_FLAG_KEEPOUT = 0x200;

/**
 * @brief 无网络索引
 */
constexpr uint16_t PCB_NET_NONE = 0xFFFF;

/**
 * @brief 无元件索引
 */
constexpr uint16_t PCB_COMPONENT_NONE = 0xFFFF;

/**
 * @brief 无多边形索引
 */
constexpr uint16_t PCB_POLYGON_NONE = 0xFFFF;

// ==================== PcbLib 焊盘形状 ====================

/**
 * @brief 圆形焊盘
 */
constexpr uint8_t PCB_PAD_SHAPE_ROUND = 1;

/**
 * @brief 矩形焊盘
 */
constexpr uint8_t PCB_PAD_SHAPE_RECTANGULAR = 2;

/**
 * @brief 八角形焊盘
 */
constexpr uint8_t PCB_PAD_SHAPE_OCTAGONAL = 3;

/**
 * @brief 圆角矩形焊盘
 */
constexpr uint8_t PCB_PAD_SHAPE_ROUNDED_RECT = 9;

// ==================== PcbLib 层常量 ====================

/**
 * @brief 顶层
 */
constexpr uint8_t PCB_LAYER_TOP = 1;

/**
 * @brief 底层
 */
constexpr uint8_t PCB_LAYER_BOTTOM = 32;

/**
 * @brief 顶层丝印
 */
constexpr uint8_t PCB_LAYER_TOP_OVERLAY = 33;

/**
 * @brief 底层丝印
 */
constexpr uint8_t PCB_LAYER_BOTTOM_OVERLAY = 34;

/**
 * @brief 多层（通孔焊盘）
 */
constexpr uint8_t PCB_LAYER_MULTI = 74;

// ==================== 默认值 ====================

/**
 * @brief 默认填充色（白色）
 */
constexpr uint32_t DEFAULT_AREA_COLOR = 0xFFFFFF;

/**
 * @brief 默认颜色（黑色）
 */
constexpr uint32_t DEFAULT_COLOR = 0x000000;

/**
 * @brief 默认引脚长度（10mil = 100,000 原始单位）
 */
constexpr int DEFAULT_PIN_LENGTH = 100000;

/**
 * @brief 默认文字高度（6mil = 60,000 原始单位）
 */
constexpr int DEFAULT_TEXT_HEIGHT = 60000;

/**
 * @brief 默认笔画宽度（1mil = 10,000 原始单位）
 */
constexpr int DEFAULT_STROKE_WIDTH = 10000;

// ==================== PcbLib 焊盘记录布局 ====================

/**
 * @brief PcbLib v6 焊盘主记录负载大小（不含四字节 block length）
 */
constexpr int PCB_PAD_MAIN_RECORD_SIZE = 114;

/**
 * @brief 焊盘尺寸/形状覆盖数据总大小（字节）
 */
constexpr int PCB_PAD_SIZE_OVERRIDE_SIZE = 596;

/**
 * @brief 焊盘尺寸覆盖中的层循环数
 */
constexpr int PCB_PAD_LAYER_COUNT = 29;

/**
 * @brief 焊盘尺寸覆盖中 Y 尺寸起始偏移（字节）
 */
constexpr int PCB_PAD_SIZE_Y_OFFSET = 116;

/**
 * @brief 焊盘尺寸覆盖中形状起始偏移（字节）
 */
constexpr int PCB_PAD_SHAPE_OFFSET = 232;

/**
 * @brief 焊盘尺寸覆盖中孔形状偏移（字节）
 */
constexpr int PCB_PAD_HOLE_SHAPE_OFFSET = 262;

// ==================== PcbLib 文本记录布局 ====================

/**
 * @brief 文本记录总大小（字节）
 */
constexpr int PCB_TEXT_RECORD_SIZE = 252;

/**
 * @brief 文本记录中 WideString 索引的字段偏移
 */
constexpr int PCB_TEXT_WS_INDEX_FIELD_OFFSET = 115;

/**
 * @brief 文本记录中文字种类字段偏移
 */
constexpr int PCB_TEXT_KIND_FIELD_OFFSET = 160;

/**
 * @brief 文本记录中 V7 Layer ID 字段偏移
 */
constexpr int PCB_TEXT_V7LAYER_FIELD_OFFSET = 226;

}  // namespace AltiumConstants
}  // namespace EasyKiConverter
