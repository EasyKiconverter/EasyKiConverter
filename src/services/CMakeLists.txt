# Services 模块

# 先定义目标，这样子目录里的 target_include_directories 才能找到它
add_library(EasyKiConverterServices STATIC
    BomParser.cpp
    LcscImageService.cpp
    LcscProductParser.cpp
    LcscProductParser.h
    ComponentService.cpp
    ComponentCacheLoadCoordinator.cpp
    ComponentCacheLoadCoordinator.h
    ComponentApiCallbackCoordinator.cpp
    ComponentApiCallbackCoordinator.h
    ComponentCadFetchCoordinator.cpp
    ComponentCadFetchCoordinator.h
    ComponentCacheService.cpp
    ComponentCacheCadDataWriter.cpp
    ComponentCacheCadDataWriter.h
    ComponentCacheFileReadCoordinator.cpp
    ComponentCacheFileReadCoordinator.h
    ComponentCachePreviewImageWriter.cpp
    ComponentCachePreviewImageWriter.h
    ComponentCacheReadCoordinator.cpp
    ComponentCacheReadCoordinator.h
    ComponentCacheQuotaEnforcer.cpp
    ComponentCacheQuotaEnforcer.h
    CacheFileLayout.h
    CacheDirectoryMigrator.cpp
    CacheDirectoryMigrator.h
    CacheSafety.cpp
    CacheSafety.h
    CacheMetadataStore.cpp
    CacheMetadataStore.h
    CacheDataValidator.cpp
    CacheDataValidator.h
    CacheComponentDataReader.cpp
    CacheComponentDataReader.h
    CacheMemoryStore.cpp
    CacheMemoryStore.h
    ComponentCacheMemoryStore.cpp
    ComponentCacheMemoryStore.h
    CacheTombstoneRegistry.cpp
    CacheTombstoneRegistry.h
    ComponentDataMemoryStore.cpp
    ComponentDataMemoryStore.h
    ComponentCacheBinaryFileStore.cpp
    ComponentCacheBinaryFileStore.h
    ComponentCacheLoadWorker.cpp
    ComponentCacheLoadWorker.h
    ComponentCacheWritePolicy.cpp
    ComponentCacheWritePolicy.h
    ComponentCacheMetadataWriter.cpp
    ComponentCacheMetadataWriter.h
    ComponentCacheModel3DCoordinator.cpp
    ComponentCacheModel3DCoordinator.h
    ComponentInfoParser.cpp
    ComponentInfoParser.h
    CacheHealthManager.cpp
    CachePruner.cpp
    CadDataLoader.cpp
    ConfigService.cpp
    UpdateCheckerService.cpp
    MemoryMonitor.cpp
    ComponentQueueManager.cpp
    ParallelFetchContext.cpp
    PreviewImageExporter.cpp
    PreviewImageDataEncoder.cpp
    PreviewImageDataEncoder.h
    Model3DCacheFileStore.cpp
    Model3DCacheFileStore.h
    DatasheetCacheFileStore.cpp
    DatasheetCacheFileStore.h
    DatasheetDownloadService.cpp
    DatasheetDownloadService.h
    CachePathResolver.cpp
    CachePathResolver.h
    CacheDirectoryCoordinator.cpp
    CacheDirectoryCoordinator.h
    PreviewImageDownloadService.cpp
    PreviewImageDownloadService.h
    ComponentCacheMaintenance.cpp
    ComponentCacheMaintenance.h
    ComponentParallelFetchCoordinator.cpp
    ComponentParallelFetchCoordinator.h
    ComponentRequestCancellationCoordinator.cpp
    ComponentRequestCancellationCoordinator.h
    ComponentRequestCoordinator.cpp
    ComponentRequestCoordinator.h
    ComponentMediaCallbackCoordinator.cpp
    ComponentMediaCallbackCoordinator.h
    DatasheetExporter.cpp
)

# 新架构导出模块
add_subdirectory(export)

# 将子目录定义的源码加入目标
target_sources(EasyKiConverterServices PRIVATE
    ${EXPORT_SOURCES}
    ${EXPORT_HEADERS}
)

target_include_directories(EasyKiConverterServices
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(EasyKiConverterServices
    PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::Concurrent
        Qt6::Quick
        EasyKiConverterModels
        EasyKiConverterIR
        EasyKiConverterCore
        EasyKiConverterEasyeda
        EasyKiConverterKicad
        EasyKiConverterUtils
        EasyKiConverterSafeUtils
        EasyKiConverterWorkers
)

# 条件链接 QXlsx
# 注意：QXlsx 库的 CMake 配置会触发 GuiPrivate 警告，这是 QXlsx 库的问题，不是我们项目的问题
if(TARGET QXlsx::QXlsx)
    target_link_libraries(EasyKiConverterServices PUBLIC QXlsx::QXlsx)
endif()
