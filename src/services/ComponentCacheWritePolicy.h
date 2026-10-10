#ifndef COMPONENTCACHEWRITEPOLICY_H
#define COMPONENTCACHEWRITEPOLICY_H

#include "CacheTombstoneRegistry.h"

#include <QString>

#include <cstdint>

namespace EasyKiConverter {

/**
 * @brief 判断缓存写入请求是否仍然允许提交。
 * @details 集中封装缓存代次和 tombstone 的共同校验，不负责加锁或执行文件写入。
 */
class ComponentCacheWritePolicy final {
public:
    /**
     * @brief 校验写入请求是否对应当前缓存上下文。
     * @param currentGeneration 当前缓存目录代次
     * @param expectedGeneration 写入请求捕获的代次，零表示不启用代次校验
     * @param tombstones 缓存删除屏蔽状态注册表
     * @param componentId 元器件编号
     * @return 当前请求可以继续写入时返回 true
     */
    static bool isAllowed(uint64_t currentGeneration,
                          uint64_t expectedGeneration,
                          const CacheTombstoneRegistry& tombstones,
                          const QString& componentId);
};

}  // namespace EasyKiConverter

#endif  // COMPONENTCACHEWRITEPOLICY_H
