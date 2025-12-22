# AI SDK组件配置文件 - 预编译库版本
#
# 该文件用于配置使用预编译静态库的AI SDK组件，不编译源码文件。
#
# 使用方法：
# 1. 确保静态库文件已存在：lib/libai_sdk.a
# 2. 将此文件重命名为 CMakeLists.txt
# 3. 备份或移除 src/ 目录
# 4. 编译项目验证功能
#
# 优势：
# - 编译速度更快：跳过源码编译步骤
# - 保护源码：只提供头文件和库文件
# - 版本稳定：避免开发中的代码变更影响稳定性
# - 便于分发：可以提供给其他业务使用

idf_component_register(
    # 源文件列表 - 空列表（使用预编译库）
    SRCS ""

    # 头文件目录
    # 公共头文件路径，其他组件可以通过#include "ai_sdk/xxx.h"引用
    # 与源码版本保持一致，确保API兼容性
    INCLUDE_DIRS
        "include"

    # 依赖组件
    # SDK运行所需的外部组件（与源码版本相同）
    REQUIRES
        
    
    # 私有依赖组件（预编译库内部使用）
    PRIV_REQUIRES
        esp_http_client   # HTTP/HTTPS通信
        json              # JSON数据处理
        esp_timer         # 硬件定时器支持
)

# 使用 add_prebuilt_library 添加预编译库
add_prebuilt_library(ai_sdk_prebuilt 
    "${CMAKE_CURRENT_LIST_DIR}/lib/libai_sdk.a"
    REQUIRES esp_http_client json esp_timer)

# 链接预编译库
target_link_libraries(${COMPONENT_LIB} INTERFACE ai_sdk_prebuilt)

# 可选：添加编译时信息提示
message(STATUS "AI SDK: Using precompiled library configuration")
message(STATUS "AI SDK Library: ${CMAKE_CURRENT_LIST_DIR}/lib/libai_sdk.a")