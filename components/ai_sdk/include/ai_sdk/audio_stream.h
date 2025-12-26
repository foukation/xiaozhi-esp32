#pragma once

#include <cstddef>
#include <cstdint>
#include "esp_err.h"

namespace ai_sdk {

/**
 * @class IAudioStream
 * @brief 音频流抽象接口
 *
 * 该接口定义了音频数据采集的抽象契约，用于将不同硬件的音频采集
 * 实现与ASR模块解耦。每个开发板提供一个具体实现。
 *
 * 实现要求：
 * - 线程安全：支持多任务并发调用
 * - 非阻塞：start/stop操作快速返回
 * - 错误处理：明确返回错误码
 * - 资源管理：正确初始化和清理硬件资源
 */
class IAudioStream {
public:
    /**
     * @brief 虚析构函数
     * 确保派生类正确释放资源
     */
    virtual ~IAudioStream() = default;

    /**
     * @brief 启动音频录制
     * @return esp_err_t 错误码
     *   - ESP_OK: 启动成功
     *   - ESP_ERR_INVALID_STATE: 已处于录制状态
     *   - ESP_ERR_NOT_FOUND: 音频硬件不存在
     *   - ESP_FAIL: 其他错误
     *
     * 实现步骤：
     * 1. 检查状态（是否已启动）
     * 2. 初始化音频硬件（I2S/PDM/ADC）
     * 3. 配置DMA（如果使用DMA）
     * 4. 启动采样（中断或轮询）
     * 5. 设置标志位
     */
    virtual esp_err_t start() = 0;

    /**
     * @brief 读取音频数据（阻塞）
     * @param buffer 数据缓冲区
     * @param size 缓冲区大小（字节）
     * @param timeout_ms 超时时间（毫秒），默认1000ms
     * @return int 实际读取的字节数，
     *   - > 0: 成功读取的字节数
     *   - 0: 超时
     *   - < 0: 错误
     *
     * 实现要求：
     * - 阻塞调用，直到有足够数据或超时
     * - 数据格式：PCM 16-bit 小端序，单声道，16kHz
     * - 返回值必须与size对齐（16-bit边界）
     * - 超时处理：返回0，不设置错误码
     *
     * 数据生产：
     * - I2S DMA: 在I2S中断中写入环形缓冲区
     * - PDM: 在PDM滤波任务中写入缓冲区
     * - ADC: 在定时器中断中采样并写入
     */
    virtual int read(uint8_t* buffer, size_t size, int timeout_ms = 1000) = 0;

    /**
     * @brief 停止音频录制
     * @return esp_err_t 错误码
     *   - ESP_OK: 停止成功
     *   - ESP_ERR_INVALID_STATE: 未处于录制状态
     *
     * 实现步骤：
     * 1. 清除录制标志位
     * 2. 停止硬件采样
     * 3. 停止DMA（如果使用）
     * 4. 清理缓冲区
     * 5. 关闭外设时钟（可选）
     */
    virtual esp_err_t stop() = 0;

    /**
     * @brief 获取音频采样率
     * @return uint32_t 采样率（Hz）
     *   必须返回16000（ASR服务器要求）
     */
    virtual uint32_t sampleRate() const = 0;

    /**
     * @brief 获取音频通道数
     * @return uint16_t 通道数
     *   必须返回1（单声道）
     */
    virtual uint16_t channels() const = 0;

    /**
     * @brief 获取音频位深度
     * @return uint16_t 位深度（bit）
     *   必须返回16（16-bit PCM）
     */
    virtual uint16_t bitsPerSample() const = 0;

    /**
     * @brief 检查音频流是否可用
     * @return bool 是否可用
     *
     * 用于无音频硬件的设备（如DevKitC）优雅降级。
     * 返回false时，ASR模块将不启动。
     */
    virtual bool isAvailable() const = 0;
};

}  // namespace ai_sdk
