#pragma once

#include "ai_sdk/types.h"
#include "ai_sdk/gate_way.h"
#include <functional>
#include <memory>
#include <string>

namespace ai_sdk {

/**
 * AI助手配置类
 *
 * 该类完全参考 Android AIAssistConfig 的实现，提供 Builder 模式进行配置构建。
 * 在 ESP32 平台上，虽然功能有所简化，但保持 API 的一致性以便于移植和理解。
 *
 * 配置内容包括设备信息、产品信息、网关设置等基础配置。
 * 使用 Builder 模式可以链式调用，提高代码的可读性和可维护性。
 */
class AIAssistConfig {
public:
    /**
     * Builder 类
     *
     * 采用构建者模式，通过链式调用来构建 AIAssistConfig 对象。
     * 每个设置方法都返回 Builder 自身的引用，支持流畅的 API 调用。
     *
     * 使用示例：
     * auto builder = std::make_unique<AIAssistConfig::Builder>();
     * auto config = builder->deviceId("device-001")
     *                      ->productId("PROD001")
     *                      ->productKey("KEY001")
     *                      ->build();
     */
    class Builder {
    public:
        /**
         * 构造函数
         * 创建一个新的 AIAssistConfig 实例并开始构建
         */
        Builder() : config(std::make_unique<AIAssistConfig>()) {}

        /**
         * 设置设备ID
         *
         * 设备ID是设备在平台上注册成功后获得的唯一标识
         *
         * @param deviceId 设备唯一标识符
         * @return Builder 引用，支持链式调用
         */
        Builder& deviceId(const std::string& deviceId) {
            config->deviceId = deviceId;
            return *this;
        }

        /**
         * 设置设备密钥
         *
         * 设备密钥是设备在平台上注册成功后获得的认证密钥，用于后续请求的签名验证
         *
         * @param secret 设备密钥
         * @return Builder 引用，支持链式调用
         */
        Builder& deviceSecret(const std::string& secret) {
            config->deviceSecret = secret;
            return *this;
        }

        /**
         * 设置产品ID
         *
         * 产品ID是平台创建产品时生成的唯一标识
         *
         * @param productId 产品标识符
         * @return Builder 引用，支持链式调用
         */
        Builder& productId(const std::string& productId) {
            config->productId = productId;
            return *this;
        }

        /**
         * 设置产品密钥
         *
         * 产品密钥是平台创建产品时生成的认证密钥
         *
         * @param productKey 产品密钥
         * @return Builder 引用，支持链式调用
         */
        Builder& productKey(const std::string& productKey) {
            config->productKey = productKey;
            return *this;
        }

        /**
         * 设置设备号类型
         *
         * 设备号类型标识设备编号的类型，如 SN、MAC 等
         *
         * @param type 设备号类型，默认为 "SN"
         * @return Builder 引用，支持链式调用
         */
        Builder& deviceNoType(const std::string& type) {
            config->deviceNoType = type;
            return *this;
        }

        /**
         * 设置设备号
         *
         * 设备号是产品内唯一标识设备的序列号
         *
         * @param deviceNo 设备序列号
         * @return Builder 引用，支持链式调用
         */
        Builder& deviceNo(const std::string& deviceNo) {
            config->deviceNo = deviceNo;
            return *this;
        }

        /**
         * 启用或禁用网关代理
         *
         * 如果启用，SDK 会自动从云端获取网关配置并使用代理服务器
         *
         * @param enable 是否启用网关代理，默认为 true
         * @return Builder 引用，支持链式调用
         */
        Builder& enableGateway(bool enable) {
            config->enableGateway = enable;
            return *this;
        }

        /**
         * 构建 AIAssistConfig 对象
         *
         * 完成所有配置后，调用此方法生成最终的配置对象
         *
         * @return 构建好的 AIAssistConfig 唯一指针
         */
        std::unique_ptr<AIAssistConfig> build() {
            return std::move(config);
        }

    private:
        std::unique_ptr<AIAssistConfig> config;  ///< 正在构建的配置对象
    };

    // ===== 配置字段 =====

    std::string deviceId;          ///< 设备唯一标识
    std::string deviceSecret;      ///< 设备密钥
    std::string productId;         ///< 产品ID
    std::string productKey;        ///< 产品密钥
    std::string deviceNoType;      ///< 设备号类型，默认为 "SN"
    std::string deviceNo;          ///< 设备序列号
    bool enableGateway;            ///< 是否启用网关代理，默认为 true
    std::string packageName;       ///< 包名，ESP32中模拟使用
    void* context;                 ///< 应用上下文，ESP32中可能为nullptr

    /**
     * TTS 配置
     * 参考 Android 的 TtsConfig 设计，在 ESP32 中以字符串形式存储
     * 包含语音合成相关参数，如音色、语速等
     */
    std::string ttsConfig;         ///< TTS 配置字符串

private:
    /**
     * 私有构造函数
     * 只能通过 Builder 创建实例
     */
    AIAssistConfig()
        : enableGateway(true), context(nullptr) {}
};

/**
 * AI助手管理器
 *
 * 完全参考 Android AIAssistantManager 的实现，是 AI SDK 的中央管理类。
 * 采用单例模式设计，提供统一的入口点来管理所有 AI 相关的服务和功能。
 *
 * 核心功能：
 * - 管理设备注册和认证流程
 * - 管理网关配置和代理设置
 * - 管理数据上报和心跳
 * - 管理各种 AI 服务（虽然 ESP32 上部分功能受限）
 * - 提供统一的状态管理和错误处理
 *
 * 生命周期：
 * 1. 创建配置 AIAssistConfig
 * 2. 调用 initialize() 初始化管理器
 * 3. 调用 getInstance() 获取实例
 * 4. 调用各功能方法
 * 5. 程序结束时自动销毁
 *
 * 使用示例：
 * // 1. 创建配置
 * auto builder = std::make_unique<AIAssistConfig::Builder>();
 * auto config = builder->deviceNo("ESP32-001")
 *                      ->productId("PROD001")
 *                      ->productKey("KEY001")
 *                      ->build();
 *
 * // 2. 初始化管理器
 * AIAssistantManager::initialize(nullptr, std::move(config));
 *
 * // 3. 获取实例并调用方法
 * auto& manager = AIAssistantManager::getInstance();
 * manager.obtainDeviceInformation();
 */
class AIAssistantManager {
public:
    /**
     * 初始化 AI 助手管理器
     *
     * 参考 Android 的 initialize(context, config) 实现。
     * 必须在调用 getInstance() 之前先调用此方法，否则会抛出异常。
     *
     * 初始化过程：
     * 1. 创建单例实例
     * 2. 保存配置信息
     * 3. 初始化内部组件
     * 4. 记录初始化日志
     *
     * @param context 应用上下文，在 Android 中用于访问应用资源，ESP32 中可为 nullptr
     * @param config AI 助手配置对象，包含所有必要的配置信息
     *
     * 安全说明：
     * - 线程安全，可以被多次调用（第二次及之后调用会被忽略）
     * - 幂等性，重复调用不会产生副作用
     *
     * 使用示例：
     * AIAssistantManager::initialize(nullptr, std::move(config));
     */
    static void initialize(void* context, std::unique_ptr<AIAssistConfig> config);

    /**
     * 获取 AI 助手管理器单例实例
     *
     * 参考 Android 的 getInstance() 实现。
     * 返回唯一的全局实例，如果未初始化会抛出异常。
     *
     * 注意事项：
     * - 必须在 initialize() 成功后调用
     * - 非线程安全，建议在程序启动时单线程调用
     * - 返回实例的引用，确保实例不会被销毁
     *
     * @return AIAssistantManager 实例的引用
     * @throws std::runtime_error 如果管理器未初始化
     *
     * 使用示例：
     * try {
     *     auto& manager = AIAssistantManager::getInstance();
     *     manager.obtainDeviceInformation();
     * } catch (const std::exception& e) {
     *     ESP_LOGE(TAG, "Manager not initialized: %s", e.what());
     * }
     */
    static AIAssistantManager& getInstance();

    /**
     * 销毁 AI 助手管理器实例
     *
     * 清理所有资源，释放内存。
     * 虽然 Android 版本没有提供销毁方法，但为了资源安全和测试方便，添加此方法。
     *
     * 调用时机：
     * - 程序退出时
     * - 需要重置 SDK 状态时
     * - 单元测试结束时
     *
     * 注意事项：
     * 销毁后，必须重新调用 initialize() 才能再次使用
     */
    static void destroyInstance();

    // ===== 核心管理方法（完全参考 Android 实现）=====

    /**
     * 提供 Gateway 网关和设备接入实例
     *
     * 参考 Android 的 gateWayHelp() 实现。
     * 每次调用返回同一个 GateWay 实例，提供网关相关的操作。
     *
     * 网关功能包括：
     * - 获取网关配置
     * - 获取设备信息
     * - 数据上报
     *
     * 设计模式：单例模式（GateWay 在管理器中是单例）
     *
     * @return GateWay 实例的引用
     *
     * 使用示例：
     * auto& gateway = manager.gateWayHelp();
     * gateway.getGateWay(onSuccess, onError);
     * gateway.obtainDeviceInformation(onSuccess, onError);
     * gateway.dataReport(request, onSuccess, onError);
     */
    GateWay& gateWayHelp();

    /**
     * 提供 ASR 智能对话助手实例
     *
     * 参考 Android 的 asrIntelligentDialogueHelp() 实现。
     * 在 Android 中，每次调用返回不同实例，使用反射动态加载。
     *
     * 在 ESP32 上的实现：
     * - 由于硬件资源限制，ASR 功能可能无法完整实现
     * - 为了保持 API 一致性，返回 nullptr
     * - 提供统一的日志警告，方便调试
     *
     * @return ASR 实例指针，ESP32 上返回 nullptr
     *
     * 使用示例：
     * void* asr = manager.asrIntelligentDialogueHelp();
     * if (asr) {
     *     // 使用 ASR 功能
     * } else {
     *     ESP_LOGW(TAG, "ASR not available on ESP32");
     * }
     */
    void* asrIntelligentDialogueHelp();

    /**
     * 提供 ASR 翻译帮助实例
     *
     * 参考 Android 的 asrTranslationHelp() 实现。
     * 提供语音识别翻译功能。
     *
     * ESP32 实现：返回 nullptr，功能未实现
     *
     * @return 翻译实例指针，ESP32 上返回 nullptr
     */
    void* asrTranslationHelp();

    /**
     * 获取 AI 基础套件实例
     *
     * 参考 Android 的 aiFoundationKit() 实现。
     * 提供基础的 AI 功能和工具集。
     *
     * ESP32 实现：返回 nullptr，功能未实现
     *
     * @return AI 基础套件指针，ESP32 上返回 nullptr
     */
    void* aiFoundationKit();

    /**
     * 修改 TTS 配置信息
     *
     * 参考 Android 的 changeTtsConfig() 实现。
     * 用于更新文本转语音的配置，如音色、语速等参数。
     *
     * 在 ESP32 上，配置以字符串形式存储，实际使用时需要解析
     *
     * @param config TTS 配置字符串
     *
     * 使用示例：
     * manager.changeTtsConfig("voice=male;speed=1.0;volume=1.0");
     */
    void changeTtsConfig(const std::string& config);

    // ===== 工作流方法（Android 内部实现，保持私有）=====

private:
    /**
     * 私有构造函数
     *
     * 单例模式，只能在 initialize() 中创建实例
     *
     * @param context 应用上下文
     * @param config AI助手配置
     */
    AIAssistantManager(void* context, std::unique_ptr<AIAssistConfig> config);

    /**
     * 析构函数
     *
     * 清理所有资源
     */
    ~AIAssistantManager();

    // 禁止拷贝和赋值（单例模式）
    AIAssistantManager(const AIAssistantManager&) = delete;
    AIAssistantManager& operator=(const AIAssistantManager&) = delete;

    // ===== 内部方法 =====

    /**
     * 使用配置初始化各个组件
     *
     * 参考 Android 的 initWithConfig() 实现。
     *
     * 初始化流程：
     * 1. 验证配置有效性
     * 2. 保存 packageName（虽然ESP32可能用不到）
     * 3. 初始化设备信息
     * 4. 初始化设备管理器
     *
     * 注意事项：
     * - 此方法在构造函数中调用
     * - 失败不会抛出异常，但会记录错误日志
     */
    void initWithConfig();

    // ===== 静态成员 =====

    static AIAssistantManager* instance_;           ///< 单例唯一实例

    // ===== 实例成员 =====

    std::unique_ptr<AIAssistConfig> config_;        ///< AI 助手配置
    void* context_;                                  ///< 应用上下文
    GateWay gateWay_;                               ///< 网关对象
    bool initialized_;                              ///< 初始化标志位
    std::string lastError_;                         ///< 最后错误信息
};

} // namespace ai_sdk
