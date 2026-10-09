#ifndef COMPONENTLISTCLIPBOARDCOORDINATOR_H
#define COMPONENTLISTCLIPBOARDCOORDINATOR_H

namespace EasyKiConverter {

class ComponentListViewModel;

/**
 * @brief 协调元件列表与系统剪贴板之间的复制和粘贴操作。
 *
 * 该类只承载剪贴板文本处理和列表去重，不改变 ComponentListViewModel 的公开槽接口。
 */
class ComponentListClipboardCoordinator final {
public:
    /**
     * @brief 从剪贴板提取编号并批量添加到元件列表。
     * @param owner 所属组件列表视图模型。
     */
    static void paste(ComponentListViewModel& owner);

    /**
     * @brief 将当前元件列表中的编号复制到剪贴板。
     * @param owner 所属组件列表视图模型。
     */
    static void copyAll(ComponentListViewModel& owner);
};

}  // namespace EasyKiConverter

#endif  // COMPONENTLISTCLIPBOARDCOORDINATOR_H
