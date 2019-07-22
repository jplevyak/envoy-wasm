// NOLINT(namespace-envoy)
#include <string>

#include "proxy_wasm_intrinsics.h"

extern "C" EMSCRIPTEN_KEEPALIVE void proxy_onStart(uint32_t, uint32_t, uint32_t) {
  logInfo("before exception");
  try {
    throw 13;
  } catch (int e) {
    logError("exception " + std::to_string(e));
  }
  logWarn("after exception");
}
