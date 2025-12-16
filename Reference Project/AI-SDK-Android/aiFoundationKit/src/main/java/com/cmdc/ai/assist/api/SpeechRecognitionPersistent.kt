package com.cmdc.ai.assist.api

import com.cmdc.ai.assist.aiModel.AsrClientPersistent
import com.cmdc.ai.assist.aiModel.MicroRecordStream
import com.cmdc.ai.assist.constraint.SpeechRecognitionPersistentData
import com.cmdc.ai.assist.http.ApiConfig
import com.cmdc.ai.assist.utils.AssistUtils
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Semaphore
import kotlinx.coroutines.sync.withPermit
import timber.log.Timber
import java.net.URI
import java.nio.ByteBuffer

/**
 * 用于处理语音识别功能。（持续识别）
 * 实时处理音频数据的发送和接收。
 */
class SpeechRecognitionPersistent {

    private var listener: ASRListener? = null
    private var client: AsrClientPersistent? = null
    private val coroutineScope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    private val TAG = SpeechRecognitionPersistent::class.simpleName.toString()

    private var microRecordStream: IRecordStream

    private object AudioResourceManager {
        val microphoneSemaphore = Semaphore(1) // 全局共享
    }

    /**
     * 无参构造函数
     * 使用默认的MicroRecordStream实例
     */
    internal constructor() {
        this.microRecordStream = MicroRecordStream.getInstance()
    }

    /**
     * 带参数的构造函数
     *
     * @param recordStream 音频录制流实例，用于获取音频数据
     */
    internal constructor(recordStream: IRecordStream) {
        this.microRecordStream = recordStream
    }

    /**
     * 定义处理消息接收、错误处理和连接状态变化的方法。
     */
    interface ASRListener {
        /**
         * 当接收到音频识别后的数据消息时调用
         *
         * @param message 可能为空的识别数据对象
         */
        fun onMessageReceived(message: SpeechRecognitionPersistentData?)

        /**
         * 当接收到 tts 输出流时调用。
         * 返回 pcm 格式的 tts 音频播报数据。
         *
         * @param bytes 可能为空的字节缓冲区
         */
        fun onMessageReceived(bytes: ByteBuffer?)

        /**
         * 当连接关闭时调用
         *
         * @param code 关闭连接的状态码
         * @param reason 可能为空的关闭原因描述
         * @param remote 是否由远程主机发起的关闭
         */
        fun onClose(code: Int, reason: String?, remote: Boolean)

        /**
         * 当发生错误时调用
         *
         * @param ex 可能为空的异常对象
         */
        fun onError(ex: Exception?)
    }

    /**
     * 设置 ASRListener 以监听消息和连接状态。
     * @param listener 实现了 ASRListener 接口的监听器。
     */
    fun setListener(listener: ASRListener) {
        this.listener = listener
    }

    fun startRecognition() {
        startRecognitionDefault(null)
    }

    fun startRecognition(pid: Int) {
        startRecognitionDefault(pid)
    }

    /**
     * 开始语音识别过程。
     */
    private fun startRecognitionDefault(pid: Int?) {

        // 检查是否有可用的许可，如果没有说明正在被使用
        if (AudioResourceManager.microphoneSemaphore.availablePermits == 0) {
            Timber.tag(TAG).d("Microphone is busy, skipping this recognition request")
            listener?.onError(Exception("The microphone is in use, please try again later."))
            coroutineScope.cancel()
            return
        }

        coroutineScope.launch {
            AudioResourceManager.microphoneSemaphore.withPermit {
                try {
                    Timber.tag(TAG).d("startRecognition")
                    microRecordStream.startRecording()
                    initAndConnectClient()
                    try {
                        client?.sendStartFrame(pid)
                        client?.sendAudioDataByStream(microRecordStream)
                        /*client?.sendAudioDataByStreamCompress(MicroRecordStream.getInstance())*/
                    } catch (e: Exception) {
                        listener?.onError(e)
                        Timber.tag(TAG).d(e)
                        release()
                        coroutineScope.cancel()
                    }
                    Timber.tag(TAG).d("sendAudioDataByStream end")
                    microRecordStream.close()
                } catch (e: Exception) {
                    Timber.tag(TAG).d("Exception %s", e.message)
                    microRecordStream.close()
                    listener?.onError(e)
                }
            }
        }

    }

    /**
     * 初始化 WebSocket 客户端并建立连接
     */
    private fun initAndConnectClient() {
        val wsUrl =
            AssistUtils.wssParameter(ApiConfig.AUTOMATIC_SPEECH_RECOGNITION_PERSISTENT_API)

        client = AsrClientPersistent(URI(wsUrl))
        client?.setMessageListener(object : AsrClientPersistent.WebSocketListener {
            override fun onMessageReceived(message: SpeechRecognitionPersistentData?) {
                coroutineScope.launch(Dispatchers.Main) {
                    listener?.onMessageReceived(message)
                    /*Timber.tag(TAG).d("SpeechRecognitionPersistent onMessage: %s", message)*/
                }
            }

            override fun onMessageReceived(bytes: ByteBuffer?) {
                coroutineScope.launch(Dispatchers.Main) {
                    listener?.onMessageReceived(bytes)
                }
            }

            override fun onClose(code: Int, reason: String?, remote: Boolean) {
                coroutineScope.launch(Dispatchers.Main) {
                    try {
                        listener?.onClose(code, reason, remote)
                        Timber.tag(TAG).d("onClose")
                    } finally {
                        microRecordStream.close()
                        coroutineScope.cancel()
                    }
                }
            }

            override fun onError(ex: Exception?) {
                coroutineScope.launch(Dispatchers.Main) {
                    try {
                        listener?.onError(ex)
                        Timber.tag(TAG).d("onError")
                    } finally {
                        microRecordStream.close()
                        coroutineScope.cancel()
                    }
                }
            }
        })
        try {
            client?.connectBlocking()
        } catch (e: Exception) {
            listener?.onError(e)
            Timber.tag(TAG).d(e)
            release()
            coroutineScope.cancel()
        }
    }

    /**
     * 语音识别完成之后，可调用此方法，停止语音识别
     * 释放资源，包括关闭音频流和断开与服务器的连接。
     */
    fun release() {

        coroutineScope.launch {
            try {
                Timber.tag(TAG).d("release")
                microRecordStream.close()
                try {
                    client?.sendFinishFrame()
                } catch (e: Exception) {
                    listener?.onError(e)
                }
                client = null
            } catch (e: Exception) {
                listener?.onError(e)
            } finally {

            }
        }

    }

    /**
     * 取消当前正在进行的操作
     * 释放资源，包括关闭音频流和断开与服务器的连接。
     * 取消与结束不同，结束表示音频正常结束，取消表示不再需要识别结果，服务端会迅速关闭连接
     */
    fun cancel() {

        coroutineScope.launch {
            try {
                Timber.tag(TAG).d("cancel")
                microRecordStream.close()
                try {
                    client?.sendCancelFrame()
                } catch (e: Exception) {
                    listener?.onError(e)
                }
                client = null
            } catch (e: Exception) {
                listener?.onError(e)
            } finally {
            }
        }

    }

}