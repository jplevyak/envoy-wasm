// NOLINT(namespace-envoy)
#include <string>

#include "proxy_wasm_intrinsics.h"

extern "C" PROXY_WASM_KEEPALIVE uint32_t proxy_on_vm_start(uint32_t, uint32_t) {
  logDebug("onStart");
  return 1;
}
