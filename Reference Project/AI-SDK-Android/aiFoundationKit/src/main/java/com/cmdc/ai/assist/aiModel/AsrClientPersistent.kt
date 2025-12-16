package com.cmdc.ai.assist.aiModel

import com.cmdc.ai.assist.api.IRecordStream
import com.cmdc.ai.assist.constraint.SpeechRecognitionPersistentData
import com.google.gson.Gson
import com.google.gson.GsonBuilder
import org.java_websocket.client.WebSocketClient
import org.java_websocket.handshake.ServerHandshake
import org.json.JSONException
import org.json.JSONObject
import timber.log.Timber
import java.net.URI
import java.nio.ByteBuffer

internal class AsrClientPersistent(serverUri: URI) : WebSocketClient(serverUri) {

    private val TAG = AsrClientPersistent::class.simpleName.toString()
    private val receiveBinaryData: MutableList<Byte?>
    private var mWebSocketListener: WebSocketListener? = null
    private val gson: Gson = GsonBuilder().create()

    // 音频采样率
    private val AUDIO_RATE: Int = 16000
    private val TIME_CUT = 160 // 每个音频段的时间间隔(毫秒)
    private val CHUNK_SIZE = 5120 // 音频分段大小(字节)

    // default 语言模型 ， 可以修改为其它语言模型测试，如远场普通话19362
    private val DEV_PID = 15372

    // 每帧音频数据时长: 毫秒
    private val AUDIO_SLICE_MS: Int = 40
    private val START_FRAME: MutableMap<String?, Any?> = object : HashMap<String?, Any?>() {
        init {
            put("dev_pid", DEV_PID)
            put("format", "pcm")
            put("sample", AUDIO_RATE)
        }
    }
    private val FINISH_FRAME: MutableMap<String?, Any?> = object : HashMap<String?, Any?>() {
        init {
            put("type", "finish")
        }
    }

    private val CANCEL_FRAME: MutableMap<String?, Any?> = object : HashMap<String?, Any?>() {
        init {
            put("type", "CANCEL")
        }
    }

    init {
        Timber.tag(TAG).d("AsrClient serverUri=%s", serverUri)
        receiveBinaryData = ArrayList<Byte?>()
        Timber.tag(TAG).d("AsrClient init end")
    }

    fun setMessageListener(listener: WebSocketListener?) {
        this.mWebSocketListener = listener
    }

    interface WebSocketListener {
        fun onMessageReceived(message: SpeechRecognitionPersistentData?)

        fun onMessageReceived(bytes: ByteBuffer?)

        fun onClose(code: Int, reason: String?, remote: Boolean)

        fun onError(ex: Exception?)
    }

    internal fun sendStartFrame(pid: Int?) {

        if (pid != null) {
            START_FRAME["dev_pid"] = pid
        }

        val requestMap = mutableMapOf(
            "type" to "START",
            "data" to START_FRAME
        )
        val jsonString = Gson().toJson(requestMap)

        send(jsonString)
        Timber.tag(TAG).d("%s%s", "sendStartFrame  ", jsonString)
    }

    internal fun sendFinishFrame() {
        if (this.isOpen) {
            val jsonString = JSONObject.wrap(FINISH_FRAME)?.toString()
            send(jsonString)
            Timber.tag(TAG).d("%s%s", "sendFinishFrame  ", jsonString)
        } else {
            Timber.tag(TAG).d("WebSocket is not connected.")
        }
    }

    internal fun sendCancelFrame() {
        if (this.isOpen) {
            val jsonString = JSONObject.wrap(CANCEL_FRAME)?.toString()
            send(jsonString)
            Timber.tag(TAG).d("%s%s", "sendCancelFrame  ", jsonString)
        } else {
            Timber.tag(TAG).d("WebSocket is not connected.")
        }
    }

    internal fun sendAudioDataByStream(microStream: IRecordStream?) {
        // 麦克风模式：实时读取并发送数据
        val buffer = ByteArray(CHUNK_SIZE)

        while (this.isOpen) {
            if (microStream != null) {
                try {
                    val bytesRead = microStream.read(buffer, 0, buffer.size)
                    Timber.tag(TAG).d("sendAudioDateByStream bytesRead= $bytesRead")
                    if (bytesRead > 0) {
                        /*Timber.tag(TAG).d("sendAudioDateByStream send buffer to client")*/
                        send(buffer)
                    }
                    Thread.sleep(TIME_CUT.toLong())
                } catch (e: Exception) {
                    Timber.tag(TAG).e("%s%s", "Error sending audio data", e.message)
                }
            } else {
                Timber.tag(TAG).d("sendAudioDateByStream microStream != null")
            }
        }
    }

    internal fun sendAudioDataByStreamCompress(microStream: MicroRecordStream?) {

        val realTimeUploader = CompressUploader(microStream)
        realTimeUploader.sendAudioFrames(this)

    }

    override fun onOpen(handshakedata: ServerHandshake?) {
        Timber.tag(TAG).d("AsrClientPersistent Connected")
    }

    override fun onMessage(message: String) {

        Timber.tag(TAG).d("AsrClientPersistent onMessage: %s", message)

        try {

            val resp =
                gson.fromJson(message, SpeechRecognitionPersistentData::class.java)

            if (mWebSocketListener != null) {
                mWebSocketListener!!.onMessageReceived(resp)
            }

        } catch (e: JSONException) {
            Timber.tag(TAG).d("Error parsing JSON %s", e.message)
        }

    }

    override fun onMessage(bytes: ByteBuffer) {
        // 返回 pcm 格式的 tts 音频播报数据

        /*val temp = bytes.array()
        for (i in 1 until temp.size) {
            receiveBinaryData.add(temp[i])
        }*/

        if (mWebSocketListener != null) {
            mWebSocketListener!!.onMessageReceived(bytes) // 通知 MainActivity
        }
        //Log.d(TAG, "AsrClientPersistent onMessage: " + receiveBinaryData.toString());
        Timber.tag(TAG).d("AsrClientPersistent onMessage ByteBuffer %s", "接收到音频数据")
    }

    override fun onClose(code: Int, reason: String?, remote: Boolean) {
        Timber.tag(TAG).d("AsrClientPersistent onClose")
        if (mWebSocketListener != null) {
            mWebSocketListener!!.onClose(code, reason, remote)
        }
    }

    fun disconnect() {
        if (this.isOpen) {
            this.close() // 关闭连接
            Timber.tag(TAG).d("AsrClientPersistent disconnect")
        }
    }

    override fun onError(ex: Exception) {
        Timber.tag(TAG).e("AsrClientPersistent Error: %s", ex.message)
        if (mWebSocketListener != null) {
            mWebSocketListener!!.onError(ex)
        }
    }

    fun getAudioBinaryData(): MutableList<Byte?> {
        return receiveBinaryData
    }

}
