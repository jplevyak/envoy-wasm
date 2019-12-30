// NOLINT(namespace-envoy)
#include <string>

#include "proxy_wasm_intrinsics.h"
#include "proxy_wasm_intrinsics_lite.pb.h"

uint32_t token = -1;

int xDoNotRemove = 0;

extern "C" PROXY_WASM_KEEPALIVE void proxy_on_tick(uint32_t /* context_zero */) {
  /*
  uint64_t t;
  if (WasmResult::Ok != proxy_get_current_time_nanoseconds(&t)) {
    logError("bad result from getCurrentTimeNanoseconds");
  }
  */

  /*
  std::string property = "plugin_name";
  const char* value_ptr = nullptr;
  size_t value_size = 0;
  auto result = proxy_get_property(property.data(), property.size(), &value_ptr, &value_size);
  if (WasmResult::Ok != result) {
    logError("bad result for getProperty");
  }
  if (value_size != 107) {
    logError("bad size");
  }
  ::free((void*)value_ptr);
  */

  /*
  if (token == -1) {
    std::string expr = "size(plugin_name)";
    if (WasmResult::Ok != proxy_expr_create(expr.data(), expr.size(), &token)) {
      logError("bad expression");
    }
  }

  const char* value_ptr = nullptr;
  size_t value_size = 0;
  if (WasmResult::Ok != proxy_expr_eval(token, &value_ptr, &value_size)) {
    logError("bad evaluation");
  }
  if (*reinterpret_cast<const int64_t*>(value_ptr) != 107) {
    logError("bad result");
  }
  ::free((void*)value_ptr);
  */

  /*
  if (token == -1) {
    std::string expr = "envoy.api.v2.core.GrpcService{ envoy_grpc:
  envoy.api.v2.core.GrpcService.EnvoyGrpc { cluster_name: plugin_name }}"; if (WasmResult::Ok !=
  proxy_expr_create(expr.data(), expr.size(), &token)) { logError("bad expression");
    }
  }

  const char* value_ptr = nullptr;
  size_t value_size = 0;
  if (WasmResult::Ok != proxy_expr_eval(token, &value_ptr, &value_size)) {
    logError("bad evaluation");
  }
  ::free((void*)value_ptr);
  */
#if 0
  uint64_t t;
  if (WasmResult::Ok != proxy_get_current_time_nanoseconds(&t)) {
    logError("bad result from getCurrentTimeNanoseconds");
  }
#endif
#if 0
  std::string s = "foo";
  s += "bar";
  xDoNotRemove = s.size();
#endif
#if 0
  std::string property = "plugin_name";
  const char* value_ptr = nullptr;
  size_t value_size = 0;
  auto result = proxy_get_property(property.data(), property.size(), &value_ptr, &value_size);
  if (WasmResult::Ok != result) {
    logError("bad result for getProperty");
  }
  if (value_size != 107) {
    logError("bad size");
  }
#endif
#if 1
  const char* value_ptr = "foo";
  size_t value_size = 3;
#endif
#if 1
  GrpcService grpc_service;
  grpc_service.mutable_envoy_grpc()->set_cluster_name(std::string(value_ptr, value_size));
  std::string grpc_service_string;
  grpc_service.SerializeToString(&grpc_service_string);
#endif
#if 0
  ::free((void*)value_ptr);
#endif
}
