#pragma once

#include "ai_sdk/types.h"
#include "ai_sdk/gateway_client.h"
#include "ai_sdk/device_client.h"
#include "ai_sdk/report_client.h"
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
     * 每次调用返回同一个 GatewayClient 实例，提供网关相关的操作。
     *
     * 网关功能包括：
     * - 获取网关配置
     * - 管理代理设置
     * - 处理网关认证
     *
     * 设计模式：单例模式（GatewayClient 在管理器中是单例）
     *
     * @return GatewayClient 实例的引用
     *
     * 使用示例：
     * auto& gateway = manager.gateWayHelp();
     * gateway.getGateWay(onSuccess, onError);
     */
    GatewayClient& gateWayHelp();

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
     * 提供 ASR 智能对话（会话版本）实例
     *
     * 参考 Android 的 asrIntelligentConversationHelp() 实现。
     * 支持多轮对话的 ASR 功能。
     *
     * ESP32 实现：返回 nullptr，功能未实现
     *
     * @return 对话实例指针，ESP32 上返回 nullptr
     */
    void* asrIntelligentConversationHelp();

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

    /**
     * 创建一个新的会话
     *
     * 参考 Android 的 newSession() 实现。
     * 用于开始一个新的交互会话。
     *
     * 在 Android 中，这会创建新的 AIFoundationKit 实例
     * 在 ESP32 中，仅记录日志，因为基础套件未实现
     *
     * 使用场景：
     * - 用户开始新的对话
     * - 需要重置上下文时
     * - 切换功能模块时
     */
    void newSession();

    // ===== 工作流方法（Android 内部实现，保持私有）=====

    /**
     * 获取设备信息
     *
     * 参考 Android 的 obtainDeviceInformation() 实现。
     *
     * 工作流程：
     * 1. 构建设备信息请求
     * 2. 调用 DeviceClient 进行设备注册
     * 3. 注册成功后自动调用 getGateWay()
     * 4. 然后自动调用 dataReport()
     *
     * 错误处理：
     * - 记录详细错误日志
     * - 更新内部错误状态
     * - 不会抛出异常（异步设计）
     *
     * 注意事项：
     * - 必须在 initialize() 之后调用
     * - 是启动流程的第一步
     *
     * 使用示例：
     * manager.obtainDeviceInformation();
     */
    void obtainDeviceInformation();

    /**
     * 获取网关信息
     *
     * 参考 Android 的 getGateWay() 实现。
     * 从云端获取网关配置，包括代理服务器地址和认证令牌。
     *
     * 调用时机：设备信息获取成功后自动调用
     *
     * 成功回调：记录日志并更新配置
     * 错误回调：记录错误信息
     *
     * 网关功能：
     * - 提供代理服务器地址
     * - 提供认证令牌
     * - 设置代理使用状态
     */
    void getGateWay();

    /**
     * 执行设备参数采集上报
     *
     * 参考 Android 的 dataReport() 实现。
     *
     * 功能说明：
     * - 心跳接口，定时向云端发送消息
     * - 设备至少每24小时上报一次，更新最后活动时间
     * - 上报间隔12小时，带随机偏移量避免服务器压力
     *
     * 上报策略：
     * 1. 设备首次启动时上报
     * 2. 计算下次上报时间：T_next = T_current + 12小时 + 随机偏移量(-15到+15分钟)
     * 3. 调度上报任务
     *
     * 数据内容：
     * - 设备ID、产品ID等身份信息
     * - SDK版本号
     * - 自定义参数
     */
    void dataReport();

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

    /**
     * 设备注册成功回调
     *
     * 参考 Android 回调流程。
     *
     * 成功处理：
     * 1. 更新设备ID和密钥
     * 2. 打印成功日志
     * 3. 自动调用 getGateWay()
     * 4. 自动调用 dataReport()
     *
     * @param response 设备注册响应
     */
    void onDeviceInfoResponse(const DeviceInfoResponse& response);

    /**
     * 设备注册失败回调
     *
     *
     * 错误处理：
     * - 记录错误日志
     * - 保存错误信息
     * - 不会自动重试（由上层应用决定）
     *
     * @param error 错误信息
     */
    void onDeviceInfoError(const std::string& error);

    /**
     * 网关配置成功回调
     *
     * 成功处理：
     * - 记录响应日志
     * - 如果启用代理，记录代理地址
     *
     * @param info 网关信息
     * @param empty 空字符串，保持与 Android 兼容
     */
    void onGatewayResponse(const GatewayInfo& info, const std::string& empty);

    /**
     * 网关配置失败回调
     *
     * 错误处理：
     * - 记录错误日志
     * - 保存错误信息
     *
     * @param error 错误信息
     */
    void onGatewayError(const std::string& error);

    /**
     * 动态加载类
     *
     * 参考 Android 的 Class.forName() 反射机制。
     *
     * ESP32 不支持 Java/Kotlin 的反射机制，因此此方法是 stub 实现。
     * 它只会记录日志并返回 nullptr，但保持 API 一致性。
     *
     * @param className 完整的类名
     * @return 实例指针，ESP32 上始终返回 nullptr
     *
     * 注意事项：
     * - ESP32 不支持运行时动态加载类
     * - 此方法仅用于保持与 Android API 的兼容性
     * - 如果需要类似功能，需要在编译时静态链接
     */
    void* loadClass(const std::string& className);

    // ===== 静态成员 =====

    static AIAssistantManager* instance_;           ///< 单例唯一实例

    // ===== 实例成员 =====

    std::unique_ptr<AIAssistConfig> config_;        ///< AI 助手配置
    void* context_;                                  ///< 应用上下文
    GatewayClient gatewayClient_;                   ///< 网关客户端（单例）
    DeviceClient deviceClient_;                     ///< 设备客户端（单例）
    ReportClient reportClient_;                     ///< 上报客户端（单例）
    bool initialized_;                              ///< 初始化标志位
    std::string lastError_;                         ///< 最后错误信息
};

} // namespace ai_sdk
