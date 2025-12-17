#pragma once

#include "ai_sdk/types.h"
#include <functional>

namespace ai_sdk {

/**
 * 网关客户端类
 */
class GatewayClient {
public:
    using GatewayCallback = std::function<void(const GatewayInfo&, const std::string&)>;

    /**
     * 获取网关信息
     */
    void getGateWay(GatewayCallback onSuccess, std::function<void(const std::string&)> onError);

private:
    void onResponse(const std::string& response, GatewayCallback onSuccess, std::function<void(const std::string&)> onError);
    void onError(const std::string& error, std::function<void(const std::string&)> onError);
};

} // namespace ai_sdk