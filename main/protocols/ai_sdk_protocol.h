/**
 * @file ai_sdk_protocol.h
 * @brief AI SDK 协议实现 - 连接云端 ASR 服务器
 *
 * 本文件实现了 Protocol 接口，用于与云端 ASR 服务器通信。
 * 内部封装 ai_sdk::AsrDialogue 模块，复用现有的 Opus 编码音频流程。
 *
 * 与现有协议的区别：
 * - WebsocketProtocol/MqttProtocol：连接小智私有服务器
 * - AiSdkProtocol：连接云端 ASR 服务器（使用 ai_sdk 模块）
 *
 * 音频数据流：
 *   AudioProcessor → Opus 编码 → audio_send_queue_ → SendAudio()
 *                                                         ↓
 *                                              asr_.sendAudio(Opus)
 *                                                         ↓
 *                                              云端 ASR 服务器
 *
 * @note 需要在 InitializeProtocol() 中根据配置选择使用此协议
 */

#ifndef AI_SDK_PROTOCOL_H
#define AI_SDK_PROTOCOL_H

#include "protocol.h"
#include "ai_sdk/asr_dialogue.h"

#include <vector>
#include <cstdint>

/**
 * @class AiSdkProtocol
 * @brief AI SDK 协议实现类
 *
 * 继承自 Protocol 基类，实现与云端 ASR 服务器的通信。
 * 将 ai_sdk::AsrDialogue 的回调转换为 Protocol 的回调格式，
 * 使得 Application 层代码无需修改即可使用不同的后端服务器。
 */
class AiSdkProtocol : public Protocol {
public:
    /**
     * @brief 构造函数
     *
     * 初始化 ai_sdk 回调，将 ASR 结果转换为 Protocol 回调格式
     */
    AiSdkProtocol();

    /**
     * @brief 析构函数
     */
    ~AiSdkProtocol() override;

    // ========================================
    // Protocol 接口实现
    // ========================================

    /**
     * @brief 启动协议
     * @return 始终返回 true（ai_sdk 不需要预启动）
     */
    bool Start() override;

    /**
     * @brief 打开音频通道
     *
     * 调用 ai_sdk::AsrDialogue::start() 建立 WebSocket 连接
     *
     * @return true 连接成功，false 连接失败
     */
    bool OpenAudioChannel() override;

    /**
     * @brief 关闭音频通道
     *
     * 发送剩余缓冲区数据，然后调用 ai_sdk::AsrDialogue::stop()
     */
    void CloseAudioChannel() override;

    /**
     * @brief 检查音频通道是否打开
     * @return true 已连接，false 未连接
     */
    bool IsAudioChannelOpened() const override;

    /**
     * @brief 发送音频数据
     *
     * 将 Opus 编码的音频数据发送到云端 ASR 服务器。
     * 数据会先积累到缓冲区，达到 CHUNK_SIZE (10240) 后发送。
     *
     * @param packet Opus 编码的音频数据包
     * @return true 发送成功，false 未连接或发送失败
     */
    bool SendAudio(std::unique_ptr<AudioStreamPacket> packet) override;

    // ========================================
    // Protocol 可选覆盖方法
    // ========================================

    /**
     * @brief 发送开始监听命令
     *
     * ai_sdk 在 start() 时自动发送 Start Signal，此方法为空实现
     *
     * @param mode 监听模式（未使用）
     */
    void SendStartListening(ListeningMode mode) override;

    /**
     * @brief 发送停止监听命令
     *
     * 调用 CloseAudioChannel() 关闭连接
     */
    void SendStopListening() override;

protected:
    /**
     * @brief 发送文本消息
     *
     * ai_sdk 不需要发送 JSON 文本消息，此方法始终返回 true
     *
     * @param text 文本内容（未使用）
     * @return 始终返回 true
     */
    bool SendText(const std::string& text) override;

private:
    // ai_sdk 语音助手单例引用
    ai_sdk::AsrDialogue& asr_;

    // 音频缓冲区（用于积累 Opus 数据到 chunk_size）
    std::vector<uint8_t> audio_buffer_;

    // chunk_size：与 Android hht_ctx4.conf 保持一致
    static constexpr size_t CHUNK_SIZE = 10240;

    /**
     * @brief 设置 ai_sdk 回调
     *
     * 将 ai_sdk 的回调转换为 Protocol 的回调格式：
     * - onConnected → on_audio_channel_opened_
     * - onAsrResult → on_incoming_json_ (type: "stt")
     * - onDialogueResult → on_incoming_json_ (根据 directive 类型)
     * - onError → on_network_error_
     * - onComplete → on_audio_channel_closed_
     */
    void SetupAsrCallbacks();
};

#endif // AI_SDK_PROTOCOL_H

