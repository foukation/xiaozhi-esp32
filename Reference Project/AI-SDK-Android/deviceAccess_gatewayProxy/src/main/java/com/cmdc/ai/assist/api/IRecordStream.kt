package com.cmdc.ai.assist.api

/**
 * 录音流接口
 * 定义了录音流的基本操作，包括开始录音、读取录音数据和关闭录音流
 */
interface IRecordStream {

    /**
     * 开始录音
     */
    fun startRecording()

    /**
     * 从录音流中读取数据
     * @param buffer 用于存储读取数据的字节数组
     * @param i 读取的起始位置
     * @param size 要读取的数据大小
     * @return 实际读取的数据大小
     */
    fun read(buffer: ByteArray, i: Int, size: Int): Int

    /**
     * 关闭录音流
     */
    fun close()

    /**
     * 从录音流中读取数据，从索引0开始读取整个缓冲区大小的数据
     * @param buffer 用于存储读取数据的字节数组
     * @return 实际读取的数据大小
     */
    fun read(buffer: ByteArray): Int

}