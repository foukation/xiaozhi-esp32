#pragma once

#include <functional>
#include <string>
#include <memory>

namespace ai_sdk {

/**
 * @struct AsrResult
 * @brief ASR识别结果结构体
 *
 * 包含语音识别中间结果或最终结果的信息。
 * - 中间结果：is_final = false，用于实时显示识别过程
 * - 最终结果：is_final = true，用于最终确认和执行指令
 */
struct AsrResult {
    /**
     * 识别文本内容
     * 示例："你好小智"、"今天天气怎么样"
     */
    std::string text;

    /**
     * 是否为最终结果
     * false: 中间识别结果（实时更新）
     * true: 最终确认结果（识别结束）
     */
    bool is_final;
};

/**
 * @struct DialogueResult
 * @brief 智能对话结果结构体
 *
 * 包含AI助手返回的完整对话信息，支持多种指令类型。
 * 支持文本、TTS、文生图、音乐播放等多种响应格式。
 */
struct DialogueResult {
    /**
     * 问题唯一标识
     * 用于追踪对话上下文和关联多轮对话
     */
    std::string qid;

    /**
     * 对话结束标志
     * 0: 对话进行中（流式响应）
     * 1: 对话结束（完整响应）
     */
    int is_end = 0;

    /**
     * 助手回答内容
     * AI生成的完整文本回答
     */
    std::string assistant_answer_content;

    /**
     * 指令类型
     * 用于区分不同类型的响应（文本、TTS、图片、音乐等）
     * 可能值："Speak", "RenderStreamCard", "Play", "RenderMultiImageCard"等
     */
    std::string directive;

    /**
     * 指令载荷
     * 包含具体指令的数据，如TTS URL、图片URL、音乐URL等
     * JSON格式字符串
     */
    std::string payload;
};

/**
 * @class AsrDialogue
 * @brief ASR智能对话管理类（单例模式）
 *
 * 核心功能：
 * - 管理WebSocket连接生命周期（连接、断开、重连）
 * - 协调音频数据流（接收、发送、缓冲）
 * - 解析ASR和对话响应结果
 * - 处理错误和异常情况
 *
 * 使用示例：
 * @code
 * auto& asr = AsrDialogue::getInstance();
 * asr.setCallbacks(
 *     [](const AsrResult& result) { // 处理识别结果
 *         // TODO: 处理识别结果
 *     },
 *     [](const DialogueResult& result) { // 处理对话结果
 *         // TODO: 处理对话结果
 *     },
 *     [](int code, const std::string& msg) { // 处理错误
 *         // TODO: 处理错误
 *     }
 * );
 * asr.start("wss://your-asr-server.com");
 * @endcode
 */
class AsrDialogue {
public:
    /**
     * @typedef ConnectedCallback
     * @brief 连接成功回调函数类型
     * @note 与 Android ASRIntelligentDialogue.onConnected() 保持一致
     */
    using ConnectedCallback = std::function<void()>;

    /**
     * @typedef AsrCallback
     * @brief ASR识别结果回调函数类型
     * @note 合并了 Android 的 onAsrMidResult 和 onAsrFinalResult，通过 AsrResult.is_final 区分
     */
    using AsrCallback = std::function<void(const AsrResult&)>;

    /**
     * @typedef DialogueCallback
     * @brief 智能对话结果回调函数类型
     * @note 对应 Android ASRIntelligentDialogue.onDialogueResult()
     */
    using DialogueCallback = std::function<void(const DialogueResult&)>;

    /**
     * @typedef ErrorCallback
     * @brief 错误回调函数类型
     * @note 对应 Android ASRIntelligentDialogue.onError()
     */
    using ErrorCallback = std::function<void(int, const std::string&)>;

    /**
     * @typedef CompleteCallback
     * @brief 识别完成回调函数类型
     * @note 对应 Android ASRIntelligentDialogue.onComplete()
     */
    using CompleteCallback = std::function<void()>;

    /**
     * @brief 获取单例实例
     * @return AsrDialogue& 单例引用
     */
    static AsrDialogue& getInstance();

    /**
     * @brief 设置回调函数
     * @param connected_cb 连接成功回调（可选，可为nullptr）
     * @param asr_cb ASR识别结果回调
     * @param dialogue_cb 对话结果回调
     * @param error_cb 错误回调
     * @param complete_cb 识别完成回调（可选，可为nullptr）
     *
     * 必须在调用start()之前设置回调。
     * 回调函数将在独立的任务上下文中执行，需注意线程安全。
     * 与 Android ASRIntelligentDialogue.setListener() 功能一致，保持API兼容性。
     */
    void setCallbacks(ConnectedCallback connected_cb,
                     AsrCallback asr_cb,
                     DialogueCallback dialogue_cb,
                     ErrorCallback error_cb,
                     CompleteCallback complete_cb);

    /**
     * @brief 启动ASR识别
     * @param ws_url WebSocket服务器URL
     * @return bool 是否启动成功
     *
     * 启动流程：
     * 1. 建立WebSocket连接
     * 2. 发送配置信息（采样率、格式等）
     * 3. 启动接收任务（处理服务器响应）
     * 4. 进入就绪状态，等待音频数据
     *
     * 如果连接失败，将通过error_cb回调通知。
     */
    bool start(const std::string& ws_url);

    /**
     * @brief 停止ASR识别并释放资源
     *
     * 停止流程：
     * 1. 发送结束信号（type: "finish"）
     * 2. 等待服务器响应（最多2秒）
     * 3. 关闭WebSocket连接
     * 4. 释放所有资源
     * 5. 停止接收任务
     *
     * 调用后对象回到初始状态，可再次调用start()。
     */
    void stop();

    /**
     * @brief 发送音频数据
     * @param data PCM音频数据缓冲区
     * @param len 数据长度（字节）
     *
     * 音频要求：
     * - 格式：PCM 16-bit 小端序
     * - 采样率：16000 Hz
     * - 通道数：单声道
     * - 块大小：5120字节（160ms音频）
     *
     * 内部处理：
     * - 数据缓存和分包
     * - 自动添加时间戳和序列号
     * - 发送到WebSocket
     *
     * 线程安全：可以从任意任务调用，内部有互斥锁保护。
     */
    void sendAudio(const uint8_t* data, size_t len);

    // 禁止拷贝和赋值
    AsrDialogue(const AsrDialogue&) = delete;
    AsrDialogue& operator=(const AsrDialogue&) = delete;

private:
    /**
     * @brief 私有构造函数（单例模式）
     */
    AsrDialogue();

    /**
     * @brief 析构函数
     */
    ~AsrDialogue();

    /**
     * PIMPL模式实现细节
     * 隐藏实现细节，减少编译依赖，提高二进制兼容性
     */
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ai_sdk
