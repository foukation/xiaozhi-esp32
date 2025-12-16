package com.cmdc.ai.assist.aiModel

import com.cmdc.ai.assist.AIAssistantManager
import com.cmdc.ai.assist.aiModel.DealSotaOne.Context
import com.cmdc.ai.assist.api.IRecordStream
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import okio.ByteString
import org.json.JSONArray
import org.json.JSONObject
import timber.log.Timber
import java.io.File
import java.io.InputStream
import java.lang.Exception
import java.util.UUID
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

internal class AsrIntelligentDialogueClient
    (microStream: IRecordStream, private val conf: File) {

    companion object {
        private const val TIME_CUT = 160 // 每个音频段的时间间隔(毫秒)
        private const val ADD_OPUS = "off" // 压缩标志
        private const val LOG_LEN = 400 // 日志长度限制

        /*private const val CHUNK_SIZE = 10240 // 音频分段大小(字节)*/
        private const val CHUNK_SIZE = 5120 // 音频分段大小(字节)
        /*private const val CHUNK_SIZE = 1024 // 音频分段大小(字节)*/
    }

    private var microStream: IRecordStream? = microStream
    private var ctx: Context? = null
    private val TAG = AsrIntelligentDialogueClient::class.simpleName.toString()

    // 添加回调接口
    interface AsrIntelligentDialogueClientListener {
        fun onMidResult(text: String)  // ASR中间结果
        fun onFinalResult(text: String)  // ASR最终结果
        fun onDialogueResult(result: JSONObject)  // 直接传递data对象用于analyzeDirectives
        fun onError(code: Int, message: String)
        fun onComplete()
    }

    private var listener: AsrIntelligentDialogueClientListener? = null

    fun setListener(listener: AsrIntelligentDialogueClientListener) {
        this.listener = listener
    }

    /**
     * 发送音频数据
     * 包括发送start信号、音频数据和finish信号
     *
     * @param webSocket WebSocket连接对象
     * @param ctx 上下文对象
     * @param wavSegs 音频数据分段列表
     */
    private fun sendAudioData(webSocket: WebSocket?, ctx: Context) {
        if (webSocket == null) {
            Timber.tag(TAG).d("[sendAudioData] Error: WebSocket is null")
            ctx.ret = 1
            return
        }

        try {
            if (ctx.ret != 0) {
                Timber.tag(TAG).d("[sendAudioData] Error: Context ret is not 0")
                return
            }

            // 发送start信号
            val startSignal = createStartSignal(ctx)

            Timber.tag(TAG)
                .d("[sendAudioData] ${System.currentTimeMillis() / 1000.0} Sending start signal:")
            Timber.tag(TAG).d("[sendAudioData] Start signal content: ${startSignal.toString(4)}")

            if (!webSocket.send(startSignal.toString())) {
                Timber.tag(TAG).d("[sendAudioData] Failed to send start signal")
                ctx.ret = 1
                return
            }
            Timber.tag(TAG).d("[sendAudioData] Start signal sent successfully")

            if (ctx.ret != 0) {
                Timber.tag(TAG)
                    .d("[sendAudioData] Error: Context ret became non-zero after sending start signal")
                return
            }

            // 分段发送音频数据
            /*val sleepTime = TIME_CUT.toLong() * 2*/
            val sleepTime = TIME_CUT.toLong()


            /*sendAudioMicrophoneData(webSocket, ctx)*/

            val realTimeUploader = RealTimeUploader(microStream)
            realTimeUploader.sendAudioFrames(webSocket, ctx)


            // 发送结束信号
            if (ctx.ret == 0) {
                Timber.tag(TAG)
                    .d("[sendAudioData] ${System.currentTimeMillis() / 1000.0} Sending finish signal")
                val endSignal = JSONObject().apply {
                    put("type", "finish")
                }
                if (webSocket.send(endSignal.toString())) {
                    Timber.tag(TAG)
                        .d("[sendAudioData] Finish signal sent successfully")
                    /*val audioData = audioBuffer.toByteArray()
                    val wavFilePath = AudioFileUtils.saveToWavFile(audioData)
                    val pcmFilePath = AudioFilePcmUtils.saveToPcmFile(audioData)
                    AIAssistantManager.getInstance().aiAssistConfig.currentPcmFilePath =
                        pcmFilePath.toString()
                    Timber.tag(TAG).d("[sendAudioData] Audio saved to: $wavFilePath")
                    Timber.tag(TAG).d("[sendAudioData] Audio saved to: $pcmFilePath")*/
                } else {
                    Timber.tag(TAG)
                        .d("[sendAudioData] Failed to send finish signal")
                }
            }

        } catch (e: Exception) {
            Timber.tag(TAG)
                .e("[sendAudioData] Error occurred: ${e.message}")
            ctx.ret = 1
        } finally {
            ctx.timeFinal = System.currentTimeMillis()
            Timber.tag(TAG)
                .d("[sendAudioData] Final time recorded: ${ctx.timeFinal}")
            Timber.tag(TAG)
                .d("[sendAudioData] Closing microphone resources")
            closeResources()
        }
    }

    /**
     * 主要处理函数，执行完整的语音识别流程
     * 使用协程处理IO操作
     */
    fun runSpeechRecognitionFlow(uri: String) {

        try {

            val conf = conf.readText()

            ctx = Context(startData = JSONObject(conf))
            /*ctx?.startData?.put("client_id", BuildConfig.SDK_VERSION)*/
            ctx?.startData?.put("cuid", AIAssistantManager.getInstance().aiAssistConfig.deviceNo)
            ctx?.startData?.put(
                "client_ip", ""
            )
            ctx?.startData?.put(
                "dialog_request_id",
                UUID.randomUUID().toString() + System.currentTimeMillis()
            )

            val currentCtx = ctx
            val formattedUri = uri

            val client = OkHttpClient.Builder()
                .connectTimeout(10, TimeUnit.SECONDS)
                .readTimeout(30, TimeUnit.SECONDS)
                .writeTimeout(30, TimeUnit.SECONDS)
                .pingInterval(5, TimeUnit.SECONDS)
                .build()

            val request = Request.Builder()
                .url(formattedUri)
                .build()

            val connectionLatch = CountDownLatch(1)

            val listener = object : WebSocketListener() {
                override fun onOpen(webSocket: WebSocket, response: Response) {
                    wsConnection = webSocket
                    Timber.tag(TAG)
                        .d("${System.currentTimeMillis() / 1000.0} WebSocket connected successfully")
                    connectionLatch.countDown()
                }

                override fun onMessage(webSocket: WebSocket, text: String) {
                    try {
                        Timber.tag(TAG)
                            .d("[WebSocket.onMessage] Response: $text")
                        val json = JSONObject(text)
                        when (json.optString("type")) {
                            "mid_result" -> {
                                val result = json.optString("result") ?: ""
                                Timber.tag(TAG)
                                    .d("[WebSocket.onMessage.mid_result] $result")
                                listener?.onMidResult(result)
                            }

                            "fin_result" -> {
                                val result = json.optString("result") ?: ""
                                Timber.tag(TAG)
                                    .d("[WebSocket.onMessage.fin_result] $result")
                                ctx?.asrText = result
                                listener?.onFinalResult(result)
                            }

                            "inside_rc" -> {
                                val data = json.optJSONObject("data")
                                if (data != null) {
                                    Timber.tag(TAG)
                                        .d("[WebSocket.onMessage.inside_rc] Data received")
                                    listener?.onDialogueResult(data)  // 直接传递整个data对象
                                }
                            }

                            "ready", "inside_rc" -> {
                                Timber.tag(TAG)
                                    .d("[WebSocket.onMessage.${json.optString("type")}] Received")
                            }

                            "dcs_decide" -> {
                                if (json.optInt("end", 0) == 1) {
                                    Timber.tag(TAG)
                                        .d("[WebSocket.onMessage.dcs_decide] End flag received")
                                    Timber.tag(TAG)
                                        .d("[WebSocket.onMessage.dcs_decide] Stop receiving")
                                }
                            }

                            else -> {
                                // 处理directive消息
                                if (text.contains("directive")) {
                                    Timber.tag(TAG)
                                        .d("[WebSocket.onMessage.directive] Directive received:")
                                    Timber.tag(TAG)
                                        .d("[WebSocket.onMessage.directive] Content: $text")
                                    val directive = json.optJSONObject("directive")
                                    if (directive != null) {
                                        val header = directive.optJSONObject("header")
                                        if (header != null) {
                                            val namespace = header.optString("namespace")
                                            val name = header.optString("name")
                                            val directiveName = "$namespace.$name"
                                            Timber.tag(TAG)
                                                .d("[WebSocket.onMessage.directive] Namespace: $namespace, Name: $name")

                                            val payload = directive.optJSONObject("payload")
                                            val payloadMap = if (payload != null) {
                                                mutableMapOf<String, Any>().apply {
                                                    payload.keys().forEach { key ->
                                                        put(key, payload.get(key))
                                                    }
                                                }
                                            } else {
                                                emptyMap()
                                            }
                                            Timber.tag(TAG)
                                                .d("[WebSocket.onMessage.directive] Payload: $payloadMap")

                                            directives.add(directiveName to payloadMap)
                                        }
                                    }
                                }
                            }
                        }
                    } catch (e: Exception) {
                        Timber.tag(TAG)
                            .e("[WebSocket.onMessage] Error parsing message: ${e.message}")
                        listener?.onError(-1, "Error parsing message: ${e.message}")
                        cleanupResources()
                    }
                }

                override fun onMessage(webSocket: WebSocket, bytes: ByteString) {
                    // 不输出二进制消息日志
                }

                override fun onClosing(webSocket: WebSocket, code: Int, reason: String) {
                    Timber.tag(TAG)
                        .d("[WebSocket.onClosing] code: $code, reason: $reason")
                    webSocket.close(code, reason)
                    listener?.onComplete()
                    cleanupResources()
                }

                override fun onClosed(webSocket: WebSocket, code: Int, reason: String) {
                    Timber.tag(TAG)
                        .d("[WebSocket.onClosed] code: $code, reason: $reason")
                    /*onClosed()
                    listener?.onComplete()
                    cleanupResources()*/
                }

                override fun onFailure(
                    webSocket: WebSocket, t: Throwable, response: Response?
                ) {
                    Timber.tag(TAG)
                        .e("[WebSocket.onFailure] Connection failed: ${t.message}")
                    listener?.onError(1, t.message ?: "Connection failed")
                    currentCtx?.ret = 1
                    cleanupResources()
                    onClosed()
                }
            }

            client.newWebSocket(request, listener)

            // 连接成功发送数据 sendAudioData
            if (connectionLatch.await(5, TimeUnit.SECONDS)) {
                sendAudioData(wsConnection, currentCtx!!)
            }

        } catch (e: Exception) {
            Timber.tag(TAG).e("[runSpeechRecognitionFlow] Error: ${e.message}")
            cleanupResources()
            ctx?.ret = 1
            throw e
        }
    }

    private fun cleanupResources() {
        try {
            wsConnection?.cancel()
            wsConnection = null
            ctx?.ret = 1
        } catch (e: Exception) {
            Timber.tag(TAG)
                .e("[cleanupResources] Error during cleanup: ${e.message}")
        }
    }

    /**
     * 获取ASR识别结果
     *
     * @return 识别的文本结果，如果没有结果返回空字符串
     */
    fun getAsrResult(): String {
        return ctx?.asrText ?: ""
    }

    // 存储收到的directive消息
    private val directives = mutableListOf<Pair<String, Map<String, Any>>>()

    /**
     * WebSocket连接关闭时的处理
     * 输出统计信息和结果
     */
    private fun onClosed() {
        Timber.tag(TAG)
            .d("${System.currentTimeMillis() / 1000.0} all done")
        Timber.tag(TAG)
            .d("fail?: ${ctx?.ret ?: 1}")
        Timber.tag(TAG)
            .d("asr_result: ${ctx?.asrText ?: ""}")

        // 计算并输出时间统计
        ctx?.let { ctx ->
            val sendTime = (ctx.timeSend - ctx.timeBegin) / 1000.0
            val asrTime = (ctx.timeAsr - ctx.timeBegin) / 1000.0
            val finalTime = (ctx.timeFinal - ctx.timeBegin) / 1000.0
            Timber.tag(TAG)
                .d("send: %.2f, asr: %.2f, final: %.2f".format(sendTime, asrTime, finalTime))
        }

        // 输出收集的directives信息
        if (directives.isNotEmpty()) {
            Timber.tag(TAG)
                .d("directive:")
            directives.forEach { (name, payload) ->
                Timber.tag(TAG)
                    .d("\t$name")
                if (payload.isNotEmpty()) {
                    Timber.tag(TAG)
                        .d("\t\t$payload")
                }
            }
        }
    }

    /**
     * 关闭资源
     */
    private fun closeResources() {
        microStream?.close()
        microStream = null
    }

    /**
     * 创建开始信号
     *
     * @param ctx 上下文对象
     * @return 开始信号的JSON对象
     */
    private fun createStartSignal(ctx: Context): JSONObject {
        // 创建client_context数组，包含设备状态信息
        val clientContext = JSONArray().apply {
            // 1. Voice Output State
            put(JSONObject().apply {
                put("payload", JSONObject().apply {
                    put("playerActivity", "FINISHED")
                    put("offsetInMilliseconds", 0)
                    put("token", "")
                })
                put("header", JSONObject().apply {
                    put("namespace", "ai.fxzsos.device_interface.voice_output")
                    put("name", "SpeechState")
                })
            })

            // 2. Speaker Controller State
            put(JSONObject().apply {
                put("payload", JSONObject().apply {
                    put("volume", 25)
                    put("muted", false)
                })
                put("header", JSONObject().apply {
                    put("namespace", "ai.fxzsos.device_interface.speaker_controller")
                    put("name", "Volume")
                })
            })

            // 3. Audio Player State
            put(JSONObject().apply {
                put("payload", JSONObject().apply {
                    put("playerActivity", "FINISHED")
                    put("offsetInMilliseconds", 0)
                    put("token", "")
                })
                put("header", JSONObject().apply {
                    put("namespace", "ai.fxzsos.device_interface.audio_player")
                    put("name", "PlaybackState")
                })
            })
        }

        // 构建开始信号
        return JSONObject().apply {
            put("type", "start")
            put("data", JSONObject(ctx.startData.toString()).apply {
                put("client_context", clientContext)
            })
        }
    }

    private var wsConnection: WebSocket? = null

    /**
     * 停止语音识别
     * 关闭WebSocket连接并清理资源
     */
    fun stopRecognition() {
        cleanupResources()
    }

}
