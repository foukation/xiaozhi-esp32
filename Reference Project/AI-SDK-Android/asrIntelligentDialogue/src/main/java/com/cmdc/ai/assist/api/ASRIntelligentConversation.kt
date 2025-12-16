package com.cmdc.ai.assist.api

import android.content.Context
import com.cmdc.ai.assist.aiModel.AsrIntelligentDialogueClient
import com.cmdc.ai.assist.aiModel.MicroRecordStream
import com.cmdc.ai.assist.constraint.DialogueResult
import com.cmdc.ai.assist.http.ApiConfig
import com.cmdc.ai.assist.utils.AssistUtils
import com.cmdc.ai.assist.utils.PerformanceMonitor
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Semaphore
import kotlinx.coroutines.sync.withPermit
import org.json.JSONObject
import timber.log.Timber
import java.io.File
import java.io.InputStream

/**
 * 实时麦克风语音识别处理类
 * 负责处理实时语音输入的识别流程
 */
internal class ASRIntelligentConversation internal constructor() {

    companion object {
        private const val CONFIG_FILE_NAME = "hht_ctx4.conf"
    }

    @Volatile
    private var isReleased = false

    private var asrIntelligentDialogueClient: AsrIntelligentDialogueClient? = null
    private val coroutineScope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    private val microphoneSemaphore = Semaphore(1)
    private val TAG = ASRIntelligentConversation::class.simpleName.toString()

    /**
     * 实时语音识别监听器接口
     * 该接口定义了实时语音识别过程中不同阶段的回调方法，包括中间结果、最终结果、对话结果、错误处理和完成通知
     */
    interface RealtimeAsrListener {
        /**
         * 语音识别中间结果回调
         *
         * @param text 识别到的文本信息
         */
        fun onAsrMidResult(text: String) {}

        /**
         * 语音识别最终结果回调
         *
         * @param text 最终识别的文本信息
         */
        fun onAsrFinalResult(text: String) {}

        /**
         * 对话结果回调
         *
         * @param result 包含对话相关信息的JSON对象
         */
        private fun onDialogueResult(result: JSONObject) {}

        /**
         * 对话结果回调
         *
         * @param result 包含对话相关信息的JSON对象
         */
        fun onDialogueResult(result: DialogueResult) {}

        /**
         * 错误回调
         *
         * @param code 错误代码
         * @param message 错误信息
         */
        fun onError(code: Int, message: String) {}

        /**
         * 完成回调
         * 表示 ASR 智能对话过程已完成
         */
        fun onComplete() {}
    }

    private var listener: RealtimeAsrListener? = null

    /**
     * 设置实时语音识别监听器
     *
     * 该方法用于将一个RealtimeAsrListener接口的实现类对象注册为监听器，以便在语音识别过程中接收实时的识别结果
     * 通过此监听器，开发者可以获取到语音识别的中间结果和最终结果，进而进行相应的业务处理
     *
     * @param listener 实现了RealtimeAsrListener接口的监听器对象，用于接收语音识别事件
     */
    fun setListener(listener: RealtimeAsrListener) {
        this.listener = listener
    }

    /**
     * 启动语音识别功能
     *
     * 此函数通过协程在IO调度器上启动语音识别任务，以便不阻塞主线程
     * 它尝试使用麦克风输入进行语音识别如果识别过程中遇到错误，
     * 将通过监听器回调错误信息，并记录错误日志
     *
     * @param context 上下文，用于访问应用环境信息
     */
    fun startRecognition(context: Context) {

        val performanceData = mapOf(
            "memory" to PerformanceMonitor.getAppMemoryUsage(context).toMap(),
            "cpu" to mapOf(
                "process_usage" to PerformanceMonitor.getProcessCpuUsage(),
                "system_usage" to PerformanceMonitor.getSystemCpuUsage()
            )
        )

        coroutineScope.launch(Dispatchers.IO) {

            microphoneSemaphore.withPermit {
                try {
                    startMicrophoneRecognition(context, MicroRecordStream.getInstance())
                } catch (e: Exception) {
                    listener?.onError(-1, e.message ?: "Unknown error")
                    Timber.tag(TAG).e(("错误: code=" + -1 + ", message=" + e.message))
                }
            }

        }

    }

    /**
     * 开始麦克风录音识别
     */
    private fun startMicrophoneRecognition(
        context: Context, microStream: IRecordStream
    ) {

        val configFile = File(context.cacheDir, CONFIG_FILE_NAME).apply {
            context.assets.open("hht_ctx4.conf").use { input ->
                outputStream().use { output ->
                    input.copyTo(output)
                }
            }
        }

        asrIntelligentDialogueClient = AsrIntelligentDialogueClient(
            microStream,
            configFile
        )

        asrIntelligentDialogueClient?.setListener(object :
            AsrIntelligentDialogueClient.AsrIntelligentDialogueClientListener {
            override fun onMidResult(text: String) {

                if (isReleased) return

                coroutineScope.launch(Dispatchers.Main) {
                    listener?.onAsrMidResult(text)
                    Timber.tag(TAG).d("%s%s", "实时识别结果: ", text)
                }

            }

            override fun onFinalResult(text: String) {

                if (isReleased) return

                coroutineScope.launch(Dispatchers.Main) {
                    listener?.onAsrFinalResult(text)
                    Timber.tag(TAG).d("%s%s", "最终识别结果: ", text)
                }

            }

            override fun onDialogueResult(result: JSONObject) {

                if (isReleased) return

                /*log(result)*/
                /*val params = dialogueResultParse(result)
                    listener?.onDialogueResult(dialogueResult)
                    Timber.tag(TAG).d("onDialogueResult params: $params")*/

                coroutineScope.launch(Dispatchers.Main) {
                    dialogueResultParseDialogueResult(listener, result)
                    Timber.tag(TAG).d("%s%s", "智能对话结果: ", result)
                }

            }

            override fun onError(code: Int, message: String) {

                if (isReleased) return

                coroutineScope.launch(Dispatchers.Main) {
                    listener?.onError(code, message)
                    Timber.tag(TAG).e("错误: code=$code, message=$message")
                }

                /*continuation.resumeWithException(Exception(message))*/
            }

            override fun onComplete() {

                if (isReleased) return

                coroutineScope.launch(Dispatchers.Main) {
                    listener?.onComplete()
                    Timber.tag(TAG).d("识别完成")
                }

            }
        })

        try {
            val wsUrl = AssistUtils.wssParameter(ApiConfig.ASR_INTELLIGENT_DIALOGUE_API)
            asrIntelligentDialogueClient?.runSpeechRecognitionFlow(wsUrl)
        } catch (e: Exception) {
            Timber.tag(TAG)
                .e(("错误: code=" + -1 + ", message=" + "Recognition failed: ${e.message}"))
            listener?.onError(-1, "Recognition failed: ${e.message}")
        }
    }

    private fun dialogueResultParseDialogueResult(
        listener: RealtimeAsrListener?,
        result: JSONObject
    ) {

        val isEnd = result.optInt("is_end")
        var assistantAnswerContent: String? = null
        val qid = result.optString("qid")

        try {
            val directives = result.optJSONArray("data")
            if (directives == null) {

                listener?.onDialogueResult(
                    DialogueResult(
                        qid = qid,
                        assistant_answer_content = assistantAnswerContent
                    )
                )

            }
            for (i in 0 until directives.length()) {
                val directive = directives.optJSONObject(i)
                val header = directive.optJSONObject("header")
                val payload = directive.optJSONObject("payload")
                val name = header?.optString("name")
                when (name) {
                    "RenderProcessing" -> {
                        // progress
                        val percent = payload?.optInt("percent")
                        Timber.tag(TAG).d("isGenerating = true 进度 ：%s", percent)

                    }

                    "Nlu" -> {
                        // nlu
                        val nlu = payload?.optJSONArray("nlu")
                        Timber.tag(TAG).d("意图 ：%s", nlu?.toString())

                    }

                    "NluTag" -> {
                        // NluTag
                        val domain = payload?.optString("domian")
                        val intent = payload?.optString("intent")
                        val nluTag = JSONObject().apply {
                            put("domain", domain)
                            put("intent", intent)
                        }
                        Timber.tag(TAG).d("%s%s", "NluTag  = domain: $domain  ", "intent: $intent")

                    }

                    "RenderMultiImageCard" -> {
                        // 图片
                        val images = payload?.optJSONArray("images")
                        if (images == null || images.length() == 0) {
                            continue
                        }
                        val pic = images.optJSONObject(0).optString("url")
                        Timber.tag(TAG).d("percent = 100 图片 ：%s", pic)

                    }

                    "Play" -> {
                        // 音乐
                        val mediaUrl = payload?.optJSONObject("audioItem")?.optJSONObject("stream")
                            ?.optString("url")
                        val albumName =
                            payload?.optJSONObject("audioItem")?.optString("extension")
                        val play = JSONObject().apply {
                            put("mediaUrl", mediaUrl)
                            put("albumName", albumName)
                        }
                        Timber.tag(TAG)
                            .d("%s%s", "Play  = mediaUrl: $mediaUrl  ", "albumName: $albumName")

                    }

                    "RenderStreamCard" -> {
                        // 流式
                        val text = payload?.optString("answer")
                        Timber.tag(TAG).d("answer content ：%s", text)

                    }

                    "Speak" -> {
                        // tts
                        /*asrChatAdapter.stopMedia()*/
                        val ttlUrl = payload?.optString("url")
                        Timber.tag(TAG).d("%s%s", "Speak  = qid: $qid  ", "ttlUrl: $ttlUrl")

                    }

                    else -> {
                        // 处理其他未明确指定的情况
                        Timber.tag(TAG).d("未处理的消息类型: %s, payload: %s", name, payload)
                    }
                }

                listener?.onDialogueResult(
                    DialogueResult(
                        qid = qid,
                        is_end = isEnd,
                        assistant_answer_content = assistantAnswerContent,
                        header = header,
                        payload = payload
                    )
                )

            }

            if (isEnd == 1) {
                val assistantAnswer = result.optString("assistant_answer")
                val content = getAnswer(assistantAnswer)
                assistantAnswerContent = content
                Timber.tag(TAG).d(" isEnd == 1 answer content ：%s", content)

                listener?.onDialogueResult(
                    DialogueResult(
                        qid = qid,
                        is_end = isEnd,
                        assistant_answer_content = assistantAnswerContent,
                    )
                )

            }

        } catch (e: Exception) {
            Timber.tag(TAG).e("analyzePayload err : $e")
        }

    }

    private fun getAnswer(assistantAnswer: String): String {
        return try {
            val obj = JSONObject(assistantAnswer)
            if (obj.has("content")) {
                obj.optString("content")
            } else {
                assistantAnswer
            }
        } catch (e: Exception) {
            Timber.tag(TAG)
                .e(("assistantAnswer parse error: " + "${e.message}"))
            assistantAnswer
        }
    }

    /**
     * 释放所有资源并停止语音识别
     * 在不再需要语音识别时调用此方法
     */
    fun release() {

        try {
            asrIntelligentDialogueClient?.let { client ->
                try {
                    // 停止语音识别
                    client.stopRecognition()
                } catch (e: Exception) {
                    Timber.tag(TAG).e("Error stopping recognition: ${e.message}")
                } finally {
                    MicroRecordStream.getInstance().close()
                    asrIntelligentDialogueClient = null
                    isReleased = true
                }
            }
        } catch (e: Exception) {
            Timber.tag(TAG).e("Error during release: ${e.message}")
        }

        coroutineScope.cancel()

    }
}