#include "ai_sdk/gateway.h"

namespace ai_sdk {

// 网关单例实例，程序启动时初始化
Gateway Gateway::instance_;

/**
 * 构造函数
 * 初始化网关客户端的各个组件
 */
Gateway::Gateway() {
}

/**
 * 析构函数
 * 清理网关资源
 */
Gateway::~Gateway() {
}

/**
 * 获取网关单例实例
 * @return 网关实例指针，每次调用返回同一实例
 */
Gateway* Gateway::getInstance() {
    return &instance_;
}

} // namespace ai_sdk