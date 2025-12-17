#include "ai_sdk/gateway.h"

namespace ai_sdk {

Gateway Gateway::instance_;

Gateway::Gateway() {
}

Gateway::~Gateway() {
}

Gateway* Gateway::getInstance() {
    return &instance_;
}

} // namespace ai_sdk