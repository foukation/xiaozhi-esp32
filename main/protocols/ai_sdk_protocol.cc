/**
 * @file ai_sdk_protocol.cc
 * @brief AI SDK 协议实现
 *
 * 实现 AiSdkProtocol 类，将 ai_sdk::AsrDialogue 封装为 Protocol 接口。
 * 使得 Application 层可以无缝切换到云端 ASR 服务器。
 */

#include "ai_sdk_protocol.h"

#include <esp_log.h>
#include <cJSON.h>
#include <cstring>

#define TAG "AiSdkProtocol"

// ============================================================================
// 构造函数和析构函数
// ============================================================================

AiSdkProtocol::AiSdkProtocol()
    : asr_(ai_sdk::AsrDialogue::getInstance()) {
    ESP_LOGI(TAG, "Initializing AI SDK Protocol");
    SetupAsrCallbacks();
}

AiSdkProtocol::~AiSdkProtocol() {
    ESP_LOGI(TAG, "Destroying AI SDK Protocol");
    // 确保连接已关闭
    if (asr_.isConnected()) {
        asr_.stop();
    }
}

// ============================================================================
// 回调设置
// ============================================================================

void AiSdkProtocol::SetupAsrCallbacks() {
    asr_.setCallbacks(
        // ========================================
        // onConnected 回调
        // 对应 Android: listener?.onConnected()
        // 转换为 Protocol: on_audio_channel_opened_
        // ========================================
        [this]() {
            ESP_LOGI(TAG, "ASR connected, audio channel opened");
            
            // 更新时间戳（防止超时判断）
            last_incoming_time_ = std::chrono::steady_clock::now();
            
            // 通知 Application 音频通道已打开
            if (on_audio_channel_opened_) {
                on_audio_channel_opened_();
            }
        },

        // ========================================
        // onAsrResult 回调
        // 对应 Android: listener?.onAsrMidResult() / onAsrFinalResult()
        // 转换为 Protocol: on_incoming_json_ (type: "stt")
        // ========================================
        [this](const ai_sdk::AsrResult& result) {
            last_incoming_time_ = std::chrono::steady_clock::now();

            // 转换为现有的 JSON 格式：{"type":"stt", "text":"..."}
            // 这样 Application::OnIncomingJson 可以直接处理
            cJSON* root = cJSON_CreateObject();
            if (!root) {
                ESP_LOGE(TAG, "Failed to create JSON for ASR result");
                return;
            }

            cJSON_AddStringToObject(root, "type", "stt");
            cJSON_AddStringToObject(root, "text", result.text.c_str());

            if (on_incoming_json_) {
                on_incoming_json_(root);
            }

            cJSON_Delete(root);
        },

        // ========================================
        // onDialogueResult 回调
        // 对应 Android: listener?.onDialogueResult()
        // 根据 directive 类型转换为不同的 Protocol 回调
        // ========================================
        [this](const ai_sdk::DialogueResult& result) {
            last_incoming_time_ = std::chrono::steady_clock::now();

            ESP_LOGD(TAG, "Dialogue result: directive=%s, is_end=%d",
                     result.directive.c_str(), result.is_end);

            // 根据 directive 类型处理
            if (result.directive == "RenderStreamCard") {
                // 流式文本 → 转换为 tts sentence_start
                // 让 Application 显示助手回复
                cJSON* payload = cJSON_Parse(result.payload.c_str());
                if (payload) {
                    cJSON* answer = cJSON_GetObjectItem(payload, "answer");
                    if (answer && cJSON_IsString(answer)) {
                        cJSON* root = cJSON_CreateObject();
                        if (root) {
                            cJSON_AddStringToObject(root, "type", "tts");
                            cJSON_AddStringToObject(root, "state", "sentence_start");
                            cJSON_AddStringToObject(root, "text", answer->valuestring);

                            if (on_incoming_json_) {
                                on_incoming_json_(root);
                            }
                            cJSON_Delete(root);
                        }
                    }
                    cJSON_Delete(payload);
                }
            } else if (result.directive == "Speak") {
                // TTS 播放指令
                // TODO: 从 payload 中解析 URL，下载音频并播放
                // 当前云端 ASR 返回的是 TTS URL，与现有流程（直接收 Opus 流）不同
                // 需要额外实现 HTTP 下载和播放逻辑
                ESP_LOGW(TAG, "Speak directive received, TTS URL playback not implemented");
            } else if (result.directive == "Play") {
                // 音乐播放指令
                ESP_LOGD(TAG, "Play directive received");
            }

            // is_end=1 时表示对话结束
            if (result.is_end == 1 && !result.assistant_answer_content.empty()) {
                ESP_LOGI(TAG, "Dialogue completed, answer: %s",
                         result.assistant_answer_content.c_str());
            }
        },

        // ========================================
        // onError 回调
        // 对应 Android: listener?.onError()
        // 转换为 Protocol: SetError() → on_network_error_
        // ========================================
        [this](int code, const std::string& message) {
            ESP_LOGE(TAG, "ASR error: code=%d, message=%s", code, message.c_str());
            SetError(message);
        },

        // ========================================
        // onComplete 回调
        // 对应 Android: listener?.onComplete()
        // 转换为 Protocol: on_audio_channel_closed_
        // ========================================
        [this]() {
            ESP_LOGI(TAG, "ASR session completed, audio channel closed");

            if (on_audio_channel_closed_) {
                on_audio_channel_closed_();
            }
        }
    );

    ESP_LOGI(TAG, "ASR callbacks configured");
}

// ============================================================================
// Protocol 接口实现
// ============================================================================

bool AiSdkProtocol::Start() {
    // ai_sdk 不需要预启动，连接在 OpenAudioChannel() 中建立
    ESP_LOGI(TAG, "Protocol started (no pre-connection needed)");
    return true;
}

bool AiSdkProtocol::OpenAudioChannel() {
    ESP_LOGI(TAG, "Opening audio channel...");

    // 清空音频缓冲区
    audio_buffer_.clear();

    // 启动 ai_sdk 连接
    // 内部会建立 WebSocket 连接并发送 Start Signal
    if (!asr_.start()) {
        ESP_LOGE(TAG, "Failed to start ASR connection");
        return false;
    }

    ESP_LOGI(TAG, "Audio channel opened successfully");
    return true;
}

void AiSdkProtocol::CloseAudioChannel() {
    ESP_LOGI(TAG, "Closing audio channel...");

    // 发送剩余缓冲区数据
    if (!audio_buffer_.empty()) {
        ESP_LOGD(TAG, "Sending remaining buffer: %zu bytes", audio_buffer_.size());
        asr_.sendAudio(audio_buffer_.data(), audio_buffer_.size());
        audio_buffer_.clear();
    }

    // 停止 ai_sdk 连接
    // 内部会发送 Finish Signal 并断开 WebSocket
    asr_.stop();

    ESP_LOGI(TAG, "Audio channel closed");
}

bool AiSdkProtocol::IsAudioChannelOpened() const {
    return asr_.isConnected() && !error_occurred_ && !IsTimeout();
}

bool AiSdkProtocol::SendAudio(std::unique_ptr<AudioStreamPacket> packet) {
    // 检查连接状态
    if (!asr_.isRecognizing()) {
        return false;
    }

    // 将 Opus 编码的音频数据加入缓冲区
    // packet->payload 包含 Opus 编码后的数据
    audio_buffer_.insert(audio_buffer_.end(),
                         packet->payload.begin(),
                         packet->payload.end());

    // 达到 BYTES_PER_FRAME 时发送
    // 与 Android RealTimeUploader.BYTES_PER_FRAME (640 字节) 保持一致
    while (audio_buffer_.size() >= BYTES_PER_FRAME) {
        // 发送一个帧
        asr_.sendAudio(audio_buffer_.data(), BYTES_PER_FRAME);

        // 移除已发送的数据
        audio_buffer_.erase(audio_buffer_.begin(),
                           audio_buffer_.begin() + BYTES_PER_FRAME);
    }

    return true;
}

// ============================================================================
// 可选覆盖方法
// ============================================================================

void AiSdkProtocol::SendStartListening(ListeningMode mode) {
    // ai_sdk 在 start() 时自动发送 Start Signal
    // 此方法不需要额外操作
    ESP_LOGD(TAG, "SendStartListening called (no-op for AI SDK)");
}

void AiSdkProtocol::SendStopListening() {
    // 停止监听 = 关闭音频通道
    ESP_LOGI(TAG, "SendStopListening called, closing audio channel");
    CloseAudioChannel();
}

bool AiSdkProtocol::SendText(const std::string& text) {
    // ai_sdk 不需要发送 JSON 文本消息
    // 现有 Protocol 的 SendWakeWordDetected、SendAbortSpeaking 等方法
    // 会调用此函数，但 ai_sdk 不需要这些消息
    ESP_LOGD(TAG, "SendText called (no-op for AI SDK): %s", text.c_str());
    return true;
}

