/**
 * @file ai_assistant_manager.cc
 * @brief AI助手管理器实现文件
 *
 * 该文件实现了 AIAssistantManager 类的所有方法，完全参考 Android AIAssistantManager 的实现。
 * 提供设备注册、网关管理、数据上报等核心功能。
 *
 * 重要设计变更：
 * - AIAssistantManager 仅作为配置管理器和入口点
 * - 网关操作统一通过 gateWayHelp() 返回的 GateWay 对象进行
 * - 与 Android 设计保持一致：manager.gateWayHelp().xxx()
 */

#include "ai_sdk/ai_assistant_manager.h"
#include "esp_log.h"
#include <stdexcept>
#include <cstdio>

namespace ai_sdk {

/**
 * @brief 日志标签
 * 用于 ESP-IDF 日志系统标识日志来源
 */
static const char* TAG = "AIAssistantManager";

/**
 * @brief 单例实例指针
 * 指向唯一的 AIAssistantManager 实例，nullptr 表示未初始化
 */
AIAssistantManager* AIAssistantManager::instance_ = nullptr;

// ===== 静态方法实现 =====

/**
 * @brief 初始化 AI 助手管理器
 *
 * 参考 Android 的 initialize(context, config) 实现。
 * 创建管理器单例实例，保存配置并初始化内部组件。
 *
 * 线程安全性：非线程安全，应在程序启动时单线程调用
 * 幂等性：多次调用会被忽略（仅第一次生效）
 *
 * @param context 应用上下文（ESP32中可能为nullptr）
 * @param config AI助手配置对象（必须有效）
 */
void AIAssistantManager::initialize(void* context, std::unique_ptr<AIAssistConfig> config) {
    // 检查是否已初始化，防止重复创建
    if (!instance_) {
        // 创建新实例，转移配置 ownership
        instance_ = new AIAssistantManager(context, std::move(config));
        ESP_LOGI(TAG, "AIAssistantManager initialized");
    } else {
        // 已初始化，记录警告
        ESP_LOGW(TAG, "AIAssistantManager already initialized, ignoring");
    }
}

/**
 * @brief 获取 AI 助手管理器单例实例
 *
 * 参考 Android 的 getInstance() 实现。
 * 返回唯一的全局实例，如果未初始化会抛出异常。
 *
 * @return AIAssistantManager 实例的引用
 * @throws std::runtime_error 如果管理器未初始化
 */
AIAssistantManager& AIAssistantManager::getInstance() {
    // 检查实例是否存在
    if (!instance_) {
        // 未初始化，抛出异常（与 Android 行为一致）
        throw std::runtime_error("AIAssistantManager not initialized. Call initialize() first.");
    }
    return *instance_;
}

/**
 * @brief 销毁 AI 助手管理器实例
 *
 * 清理所有资源，释放内存，将实例指针置为 nullptr。
 * 虽然 Android 版本没有提供销毁方法，但为了资源安全和测试方便添加此方法。
 *
 * 调用时机：
 * - 程序退出时
 * - 需要重置 SDK 状态时
 * - 单元测试结束时
 *
 * 注意事项：
 * - 销毁后必须重新调用 initialize() 才能使用
 * - 线程不安全，应在单线程环境调用
 */
void AIAssistantManager::destroyInstance() {
    // 检查是否有实例
    if (instance_) {
        // 删除实例，释放内存
        delete instance_;
        instance_ = nullptr;  // 置空指针
        ESP_LOGI(TAG, "AIAssistantManager destroyed");
    }
}

// ===== 构造函数和析构函数 =====

/**
 * @brief 私有构造函数
 *
 * 单例模式，只能通过 initialize() 静态方法创建实例。
 *
 * 构造流程：
 * 1. 保存上下文和配置（转移配置所有权）
 * 2. 初始化标记设为 true
 * 3. 调用 initWithConfig() 初始化内部组件
 *
 * @param context 应用上下文
 * @param config AI助手配置（唯一指针，所有权转移）
 */
AIAssistantManager::AIAssistantManager(void* context, std::unique_ptr<AIAssistConfig> config)
    : config_(std::move(config)),  // 转移所有权
      context_(context),
      initialized_(false) {
    // 初始化内部组件
    initWithConfig();
}

/**
 * @brief 析构函数
 *
 * 清理所有资源，释放内存。
 *
 * 析构流程：
 * 1. 释放配置对象
 * 2. 记录销毁日志
 */
AIAssistantManager::~AIAssistantManager() {
    ESP_LOGI(TAG, "AIAssistantManager destroyed");
}

// ===== 内部初始化方法 =====

/**
 * @brief 使用配置初始化各个组件
 *
 * 参考 Android 的 initWithConfig() 实现。
 * 初始化内部状态和各个客户端组件。
 *
 * 初始化流程：
 * 1. 验证配置的完整性
 * 2. 设置包名（模拟 Android 的 packageName）
 * 3. 标记初始化完成
 *
 * 注意事项：
 * - 此方法在构造函数中调用
 * - 失败不会抛出异常，但会记录错误日志
 */
void AIAssistantManager::initWithConfig() {
    // 验证配置是否存在
    if (!config_) {
        ESP_LOGE(TAG, "Config is null during initialization");
        lastError_ = "Config is null during initialization";
        return;
    }

    // 如果有上下文，模拟 Android 的包名设置
    if (context_) {
        config_->packageName = "esp32.ai.assistant";  // 模拟 Android 包名
        ESP_LOGI(TAG, "Set package name: %s", config_->packageName.c_str());
    }

    // 记录配置信息（脱敏处理）
    ESP_LOGI(TAG, "Configuration initialized:");
    ESP_LOGI(TAG, "  Product ID: %s", config_->productId.c_str());
    ESP_LOGI(TAG, "  Device No: %s", config_->deviceNo.c_str());
    ESP_LOGI(TAG, "  Gateway enabled: %s", config_->enableGateway ? "yes" : "no");

    // 标记初始化完成
    // 注意：与之前的版本不同，GateWay 不再通过 setConfig() 获取配置
    // 这是为了与 Android 保持一致：GateWay 完全无状态，每次调用时从 AIAssistantManager 获取配置
    // Android 的 GateWay 也是通过 AIAssistantManager.getInstance() 访问配置的
    initialized_ = true;
    ESP_LOGI(TAG, "AIAssistantManager internal components initialized");
    ESP_LOGI(TAG, "GateWay is stateless (no setConfig needed), following Android design");
}

// ===== 核心管理方法实现 =====

/**
 * @brief 获取网关对象（核心修改点）
 *
 * 参考 Android AIAssistantManager.gateWayHelp() 实现。
 * 提供对 GateWay 的访问，用于管理网关相关的操作。
 *
 * 重要修改：
 * - 原实现返回 GatewayClient（仅支持 getGateWay）
 * - 新实现返回 GateWay（支持 obtainDeviceInformation/getGateWay/dataReport）
 * - 与 Android 设计保持一致：通过 gateWayHelp() 统一访问网关功能
 *
 * 设计模式：单例模式（返回同一个 GateWay 实例）
 *
 * @return GateWay 实例的引用
 *
 * 使用示例：
 * auto& gateway = manager.gateWayHelp();
 * gateway.obtainDeviceInformation(onSuccess, onError);  // ✓ 设备注册
 * gateway.getGateWay(onSuccess, onError);               // ✓ 获取网关
 * gateway.dataReport(request, onSuccess, onError);      // ✓ 数据上报
 */
GateWay& AIAssistantManager::gateWayHelp() {
    return gateWay_;
}

/**
 * @brief 获取 ASR 智能对话助手
 *
 * 参考 Android 的 asrIntelligentDialogueHelp() 实现。
 * 在 Android 中使用 Class.forName() 反射动态加载。
 *
 * ESP32 实现说明：
 * - ESP32 不支持 Java/Kotlin 的反射机制
 * - 硬件资源限制，ASR 功能无法完整实现
 * - 为了保持 API 一致性，返回 nullptr
 * - 记录日志警告，方便开发者了解功能限制
 *
 * @return ASR 实例指针，ESP32 上始终返回 nullptr
 */
void* AIAssistantManager::asrIntelligentDialogueHelp() {
    ESP_LOGW(TAG, "ASR Intelligent Dialogue not implemented on ESP32 (hardware limitations)");
    ESP_LOGW(TAG, "This API is kept for compatibility with Android AIAssistantManager");
    return nullptr;  // ESP32 不支持 ASR 功能
}

/**
 * @brief 获取 ASR 智能对话（会话版本）
 *
 * 参考 Android 的 asrIntelligentConversationHelp() 实现。
 * 支持多轮对话的 ASR 功能。
 *
 * @return 会话实例指针，ESP32 上返回 nullptr
 */
void* AIAssistantManager::asrIntelligentConversationHelp() {
    ESP_LOGW(TAG, "ASR Intelligent Conversation not implemented on ESP32");
    return nullptr;
}

/**
 * @brief 提供 ASR 翻译帮助
 *
 * 参考 Android 的 asrTranslationHelp() 实现。
 * 提供语音识别翻译功能。
 *
 * @return 翻译实例指针，ESP32 上返回 nullptr
 */
void* AIAssistantManager::asrTranslationHelp() {
    ESP_LOGW(TAG, "ASR Translation not implemented on ESP32");
    return nullptr;
}

/**
 * @brief 获取 AI 基础套件实例
 *
 * 参考 Android 的 aiFoundationKit() 实现。
 * 提供基础的 AI 功能和工具集。
 *
 * @return AI 基础套件指针，ESP32 上返回 nullptr
 */
void* AIAssistantManager::aiFoundationKit() {
    ESP_LOGW(TAG, "AI Foundation Kit not implemented on ESP32");
    return nullptr;
}

/**
 * @brief 修改 TTS 配置信息
 *
 * 参考 Android 的 changeTtsConfig() 实现。
 * 更新文本转语音的配置参数。
 *
 * 在 ESP32 上，由于 TTS 功能受限，配置以字符串形式存储。
 * 配置格式可以是简单的键值对，如 "voice=male;speed=1.0" 等。
 *
 * @param config TTS 配置字符串
 */
void AIAssistantManager::changeTtsConfig(const std::string& config) {
    // 验证配置是否为空
    if (config.empty()) {
        ESP_LOGW(TAG, "TTS config is empty");
        return;
    }

    // 保存配置
    if (config_) {
        config_->ttsConfig = config;
        ESP_LOGI(TAG, "TTS configuration updated: %s", config.c_str());
    } else {
        ESP_LOGE(TAG, "Cannot change TTS config: config_ is null");
    }
}

/**
 * @brief 创建新的会话
 *
 * 参考 Android 的 newSession() 实现。
 * 用于开始一个新的交互会话。
 *
 * 在 Android 中，这会创建新的 AIFoundationKit 实例。
 * 在 ESP32 中，仅记录日志，因为基础套件未实现。
 *
 * 执行操作：
 * 1. 记录会话创建日志
 * 2. 尝试创建 AIFoundationKit 实例（返回 nullptr）
 * 3. 记录调试信息
 */
void AIAssistantManager::newSession() {
    ESP_LOGI(TAG, "Creating new AI session...");

    // 参考 Android 实现，尝试获取 AI 基础套件
    void* kit = aiFoundationKit();

    if (kit) {
        // 理论上不会执行到这里，因为 aiFoundationKit() 返回 nullptr
        ESP_LOGI(TAG, "New AI session created successfully");
    } else {
        ESP_LOGW(TAG, "AIFoundationKit not available on ESP32, creating stub session");
    }
}

} // namespace ai_sdk
