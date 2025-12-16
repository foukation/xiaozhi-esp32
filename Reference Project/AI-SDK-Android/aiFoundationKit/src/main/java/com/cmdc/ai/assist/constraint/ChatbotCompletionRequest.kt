package com.cmdc.ai.assist.constraint

import com.google.gson.JsonArray
import com.google.gson.JsonElement
import com.google.gson.JsonObject

/**
 * 表示聊天完成请求的数据类
 */
data class ChatbotCompletionRequest(

    /**
     * 必选参数
     * 示例 ：[{"role": "user", "content": "Hello!"}]
     * 对话消息列表，包含角色和内容
     *
     * */
    val messages: List<ChatbotMessage>,
    /**
     * 必选参数
     * 使用的模型，例如 "ikutan_7.5b"
     * */
    private val model: String = "jiutian_75b",
    /**
     * 可选参数
     * 是否启用流式响应
     * */
    val stream: Boolean = false,
    /**
     * 可选参数
     * 采样温度，范围0-2，较高的值会使输出更随机，较低的值会使输出更确定
     * */
    val temperature: Double? = null,
    /**
     * 可选参数
     * 核采样，范围0-1，替代temperature的另一种采样方法
     * */
    val top_p: Double? = null,
)

/**
 * 表示对话消息的数据类
 */
data class ChatbotMessage(
    /**
     * 消息角色，如"user", "assistant", "system"等
     * */
    val role: String,
    /**
     * 消息内容
     * */
    val content: String
)