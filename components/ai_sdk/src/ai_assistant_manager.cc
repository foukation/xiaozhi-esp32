/**
 * @file ai_assistant_manager.cc
 * @brief AI助手管理器实现文件
 *
 * 该文件实现了 AIAssistantManager 类的所有方法，完全参考 Android AIAssistantManager 的实现。
 * 提供设备注册、网关管理、数据上报等核心功能。
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
    : context_(context),
      config_(std::move(config)),  // 转移所有权
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
    initialized_ = true;
    ESP_LOGI(TAG, "AIAssistantManager internal components initialized");
}

// ===== 核心管理方法实现 =====

/**
 * @brief 获取网关客户端
 *
 * 参考 Android 的 gateWayHelp() 实现。
 * 提供对网关客户端的访问，用于管理网关相关的操作。
 *
 * 返回同一个 GatewayClient 实例（单例模式），
 * 确保全局使用同一个网关配置。
 *
 * @return GatewayClient 实例的引用
 */
GatewayClient& AIAssistantManager::gateWayHelp() {
    return gatewayClient_;
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

    /**
     * @brief 获取设备信息
     *
     * 参考 Android 的 obtainDeviceInformation() 实现。
     * 这是启动流程的第一步，获取设备在平台上的注册信息。
     *
     * 完整的工作流程：
     * 1. 调用 DeviceClient 进行设备注册
     * 2. 成功回调 -> onDeviceInfoResponse() -> getGateWay()
     * 3. 获取网关成功 -> onGatewayResponse() -> dataReport()
     * 4. 上报成功 -> 启动完成
     *
     * 异步设计：
     * - 立即返回，不阻塞调用线程
     * - 结果通过回调通知
     * - 错误通过 onDeviceInfoError() 处理
     *
     * 前置条件：必须在 initialize() 之后调用
     */
    void AIAssistantManager::obtainDeviceInformation() {
        ESP_LOGI(TAG, "Starting device information acquisition...");

        // 验证配置是否已设置
        if (!config_) {
            ESP_LOGE(TAG, "Cannot obtain device info: config_ is null");
            lastError_ = "Configuration not set";
            return;
        }

        // 验证必要参数是否已配置
        if (config_->productId.empty() || config_->deviceNo.empty()) {
            ESP_LOGE(TAG, "Product ID or Device No not configured");
            lastError_ = "Missing product ID or device number";
            return;
        }

        // 构建设备信息请求
        DeviceInfoRequest request;
        request.deviceNoType = config_->deviceNoType.empty() ? "SN" : config_->deviceNoType;
        request.deviceNo = config_->deviceNo;
        request.productId = config_->productId;
        request.productKey = config_->productKey;

        ESP_LOGI(TAG, "Device info request:");
        ESP_LOGI(TAG, "  Device No Type: %s", request.deviceNoType.c_str());
        ESP_LOGI(TAG, "  Device No: %s", request.deviceNo.c_str());
        ESP_LOGI(TAG, "  Product ID: %s", request.productId.c_str());

        // 调用设备客户端进行注册（异步）
        deviceClient_.obtainDeviceInformation(
            request,
            [this](const DeviceInfoResponse& response) {
                onDeviceInfoResponse(response);
            },
            [this](const std::string& error) {
                onDeviceInfoError(error);
            }
        );
    }

    /**
     * @brief 获取网关信息
     *
     * 参考 Android 的 getGateWay() 实现。
     * 从云端获取网关配置，包括代理服务器地址和认证信息。
     *
     * 调用时机：设备注册成功后自动调用
     *
     * 网关配置包括：
     * - token: 网关验证令牌
     * - http_url: HTTP代理地址
     * - ws_url: WebSocket代理地址
     * - expires_in: 代理有效期（秒）
     * - status: 是否启用代理（1=使用，0=不使用）
     */
    void AIAssistantManager::getGateWay() {
        ESP_LOGI(TAG, "Fetching gateway configuration...");

        // 调用网关客户端获取配置（异步）
        gatewayClient_.getGateWay(
            [this](const GatewayInfo& info, const std::string& empty) {
                onGatewayResponse(info, empty);
            },
            [this](const std::string& error) {
                onGatewayError(error);
            }
        );
    }

    /**
     * @brief 执行设备数据上报
     *
     * 参考 Android 的 dataReport() 实现。
     * 向平台上报设备数据和心跳信息。
     *
     * 上报策略：
     * - 使用 ReportClient 发送数据
     * - 包含设备身份信息
     * - 包含 SDK 版本号
     * - 异步执行，不阻塞
     *
     * 上报数据内容：
     * - 设备ID（如果已获得）
     * - 设备密钥（如果已获得）
     * - 产品ID
     * - 产品密钥
     * - 自定义参数
     * - 事件类型（如 heartbeat）
     */
    void AIAssistantManager::dataReport() {
        ESP_LOGI(TAG, "Sending device data report...");

        // 验证配置是否存在
        if (!config_) {
            ESP_LOGE(TAG, "Cannot send data report: config_ is null");
            return;
        }

        // 构建上报请求
        DeviceReportRequest request;

        // 如果有设备ID，使用已注册的设备ID
        if (!deviceId_.empty()) {
            request.params["deviceId"] = deviceId_;
        } else {
            // 否则使用配置中的设备号作为临时标识
            request.params["deviceNo"] = config_->deviceNo;
        }

        // 添加其他身份信息
        request.params["productId"] = config_->productId;

        // 如果有设备密钥，添加到请求中
        if (!deviceSecret_.empty()) {
            request.params["deviceSecret"] = deviceSecret_;
        }

        // 添加 SDK 版本号
        request.params["sdkVersion"] = "2.1.0";

        // 设置事件类型为心跳
        request.eventType = "heartbeat";

        ESP_LOGI(TAG, "Data report request:");
        ESP_LOGI(TAG, "  Event Type: %s", request.eventType.c_str());
        ESP_LOGI(TAG, "  Product ID: %s", config_->productId.c_str());

        // 调用上报客户端发送数据（异步）
        reportClient_.dataReport(
            request,
            [](const DeviceReportResponse& response) {
                ESP_LOGI(TAG, "Data report successful, status: %d", response.status);

                // 检查响应状态码
                if (response.status != 0) {
                    ESP_LOGW(TAG, "Server returned non-zero status: %d", response.status);
                }
            },
            [](const std::string& error) {
                ESP_LOGE(TAG, "Data report failed: %s", error.c_str());
            }
        );
    }

// ===== 回调方法实现 =====

    /**
     * @brief 设备信息获取成功回调
     *
     * 参考 Android 设备注册成功后的处理流程。
     *
     * 处理步骤：
     * 1. 保存设备ID和密钥
     * 2. 打印成功日志（脱敏）
     * 3. 自动调用 getGateWay() 获取网关
     * 4. 自动调用 dataReport() 上报数据
     *
     * 参数说明：
     * - response.data.deviceId: 设备唯一标识
     * - response.data.deviceSecret: 设备认证密钥
     * - response.status: 状态码（0=成功）
     *
     * @param response 设备注册响应
     */
    void AIAssistantManager::onDeviceInfoResponse(const DeviceInfoResponse& response) {
        // 验证响应数据结构
        if (response.data.deviceId.empty()) {
            ESP_LOGE(TAG, "Device registration success but deviceId is empty");
            onDeviceInfoError("Empty deviceId in response");
            return;
        }

        // 保存设备凭证
        deviceId_ = response.data.deviceId;
        deviceSecret_ = response.data.deviceSecret;

        // 打印成功日志（对敏感信息脱敏）
        ESP_LOGI(TAG, "Device registration successful");
        ESP_LOGI(TAG, "  Device ID: %s", deviceId_.c_str());
        ESP_LOGI(TAG, "  Status: %d", response.status);

        // 如果配置了 TTS，可以在这里初始化（虽然 ESP32 上未实现）
        if (!config_->ttsConfig.empty()) {
            ESP_LOGI(TAG, "TTS config set: %s", config_->ttsConfig.c_str());
        }

        ESP_LOGI(TAG, "Device information obtained successfully, proceeding to get gateway...");

        // 参考 Android 流程：获取网关配置
        getGateWay();

        ESP_LOGI(TAG, "Proceeding to send initial data report...");

        // 参考 Android 流程：上报初始数据
        dataReport();
    }

    /**
     * @brief 设备注册失败回调
     *
     * 处理设备注册过程中的错误。
     *
     * 错误来源：
     * - 网络连接失败
     * - 产品ID/密钥错误
     * - 设备号已存在但不匹配
     * - 服务器内部错误
     * - JSON解析错误
     *
     * 处理方式：
     * - 记录详细错误日志
     * - 保存错误信息 lastError_
     * - 不会自动重试（由上层应用决定重试策略）
     *
     * 调试建议：
     * - 检查网络连接
     * - 验证产品ID和密钥
     * - 确认设备号格式正确
     *
     * @param error 错误描述字符串
     */
    void AIAssistantManager::onDeviceInfoError(const std::string& error) {
        // 记录错误日志
        ESP_LOGE(TAG, "Device registration failed: %s", error.c_str());

        // 保存错误信息（限制长度，避免内存浪费）
        const size_t maxErrorLen = 200;
        if (error.length() > maxErrorLen) {
            lastError_ = error.substr(0, maxErrorLen) + "...";
        } else {
            lastError_ = error;
        }

        // 提供详细的错误排查信息
        ESP_LOGW(TAG, "Troubleshooting tips:");
        ESP_LOGW(TAG, "  - Check network connectivity");
        ESP_LOGW(TAG, "  - Verify product ID and key");
        ESP_LOGW(TAG, "  - Ensure device number format is correct");
        ESP_LOGW(TAG, "  - Check server status");
    }

    /**
     * @brief 网关配置成功回调
     *
     * 处理网关获取成功响应。
     *
     * 网关信息解析：
     * - status = 1: 启用代理，使用 http_url 和 ws_url
     * - status = 0: 不使用代理，直接连接
     * - token: 代理认证令牌
     * - expires_in: 代理有效期（秒）
     *
     * 配置更新：
     * 如果 status = 1，会自动更新全局 ApiConfig（在 GatewayClient 中完成）
     *
     * 日志记录：
     * - 网关状态
     * - 是否使用代理
     * - 代理地址（如果启用）
     *
     * @param info 网关配置信息
     * @param empty 空字符串，保持与 Android API 兼容
     */
    void AIAssistantManager::onGatewayResponse(const GatewayInfo& info, const std::string& empty) {
        // 记录网关响应状态
        ESP_LOGI(TAG, "Gateway configuration obtained successfully");
        ESP_LOGI(TAG, "  Status: %d", info.status);
        ESP_LOGI(TAG, "  Token: %s", info.token.empty() ? "empty" : "present");
        ESP_LOGI(TAG, "  HTTP URL: %s", info.http_url.empty() ? "none" : info.http_url.c_str());

        // 检查是否启用代理
        if (info.status == 1) {
            // 启用了代理
            if (!info.http_url.empty()) {
                ESP_LOGI(TAG, "Proxy enabled, HTTP endpoint: %s", info.http_url.c_str());
            }

            if (!info.ws_url.empty()) {
                ESP_LOGI(TAG, "WebSocket proxy: %s", info.ws_url.c_str());
            }

            // 记录有效期
            if (info.expires_in > 0) {
                ESP_LOGI(TAG, "Proxy expires in %d seconds", info.expires_in);
            }
        } else {
            // 不使用代理
            ESP_LOGI(TAG, "Proxy disabled, using direct connection");
        }
    }

    /**
     * @brief 网关配置失败回调
     *
     * 处理网关获取过程中的错误。
     *
     * 错误场景：
     * - 网络连接失败
     * - 认证令牌无效或过期
     * - 代理服务器不可用
     * - 服务器内部错误
     * - 配置格式错误
     *
     * 虽然请求失败，但设备仍可正常工作（不使用代理）
     *
     * @param error 错误信息
     */
    void AIAssistantManager::onGatewayError(const std::string& error) {
        ESP_LOGE(TAG, "Gateway configuration failed: %s", error.c_str());
        ESP_LOGW(TAG, "Device will continue without proxy");

        // 保存错误信息
        lastError_ = error;

        // 即使网关失败，也继续上报流程（不使用代理）
        ESP_LOGI(TAG, "Continuing with direct connection");
    }

    /**
     * @brief 动态加载类（占位实现）
     *
     * 参考 Android 的 Class.forName() 反射机制。
     *
     * 设计考虑：
     * - Android 使用 Java 反射动态加载类
     * - ESP32（C++）不支持运行时类加载
     * - 为了保持 API 兼容性，提供此占位方法
     * - 始终返回 nullptr，记录警告日志
     *
     * 替代方案：
     * - 编译时静态链接需要的类
     * - 使用工厂模式和编译时多态
     * - 使用函数指针或策略模式
     *
     * 可能的扩展：
     * - 未来可以实现基于配置文件的静态工厂
     * - 或者使用插件架构（需要动态链接支持）
     *
     * @param className 完整的类名
     * @return 实例指针，ESP32 上始终返回 nullptr
     */
    void* AIAssistantManager::loadClass(const std::string& className) {
        // 记录警告日志，说明此功能不受支持
        ESP_LOGW(TAG, "Dynamic class loading not supported on ESP32");
        ESP_LOGW(TAG, "Requested class: %s", className.c_str());
        ESP_LOGW(TAG, "This API is for Android compatibility only");
        ESP_LOGW(TAG, "Consider using static linking for ESP32");

        // 提供替代方案建议
        ESP_LOGW(TAG, "Alternative: Use factory pattern or compile-time polymorphism");

        return nullptr;  // 始终返回 nullptr
    }

} // namespace ai_sdk
