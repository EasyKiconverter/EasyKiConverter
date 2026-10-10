#include "ComponentCacheWritePolicy.h"

namespace EasyKiConverter {

/**
 * @brief 判断异步缓存写入是否仍属于有效缓存上下文。
 * @details 同步写入请求的 expectedGeneration 为零时保持原有的无条件放行行为。
 * @param currentGeneration 当前缓存代次。
 * @param expectedGeneration 写入请求携带的缓存代次。
 * @param tombstones 当前缓存删除标记集合。
 * @param componentId 元器件编号。
 */
bool ComponentCacheWritePolicy::isAllowed(uint64_t currentGeneration,
                                          uint64_t expectedGeneration,
                                          const CacheTombstoneRegistry& tombstones,
                                          const QString& componentId) {
    if (expectedGeneration == 0) {
        return true;
    }
    return currentGeneration == expectedGeneration && !tombstones.isBlocked(componentId);
}

}  // namespace EasyKiConverter
