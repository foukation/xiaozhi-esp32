# AI SDK组件配置文件
#
# 该文件定义了ai_sdk组件的编译配置，用于ESP-IDF构建系统。
#
# 组件介绍：
# ai_sdk是用于ESP32设备的AI/IoT云端连接SDK，提供以下功能：
# - HTTP客户端封装（支持HTTPS）
# - 设备注册认证
# - 网关/代理配置
# - 设备数据上报（心跳）
# - 定时任务调度
# - WebSocket通信（用于ASR语音识别）
#
# 依赖组件：
# - esp_http_client: ESP-IDF HTTP客户端（用于云端通信）
# - esp_timer: ESP32硬件定时器（用于定时上报）
# - json: cJSON库（用于JSON数据解析和生成）
# - esp_websocket_client: WebSocket客户端（用于ASR实时通信）

idf_component_register(
    # 源文件列表
    # SDK实现文件，包括配置、客户端、网关管理和HTTP通信
    SRCS
        "src/api_config.cc"           # API配置管理
        "src/gateway_client.cc"       # 网关客户端（代理配置）
        "src/device_client.cc"        # 设备客户端（注册认证）
        "src/report_client.cc"        # 上报客户端（数据上报和心跳）
        "src/http_client.cc"          # HTTP客户端封装
        "src/ai_assistant_manager.cc" # AI助手管理器（中央管理类）
        "src/gate_way.cc"             # GateWay 网关转发层（与 Android API 保持一致）
        "src/asr_websocket.cc"        # WebSocket客户端封装（用于ASR通信）
        "src/asr_dialogue.cc"         # ASR智能对话管理

    # 头文件目录
    # 公共头文件路径，其他组件可以通过#include "ai_sdk/xxx.h"引用
    INCLUDE_DIRS
        "include"

    # 依赖组件
    # SDK运行所需的外部组件
    REQUIRES
        esp_http_client     # HTTP/HTTPS通信
        esp_timer           # 硬件定时器支持
        json                # JSON数据处理
        esp_websocket_client # WebSocket通信（用于ASR）
)