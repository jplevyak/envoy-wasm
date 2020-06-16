#include "extensions/stat_sinks/wasm/config.h"

#include <memory>

#include "envoy/config/metrics/v3/stats.pb.h"
#include "envoy/config/metrics/v3/stats.pb.validate.h"
#include "envoy/registry/registry.h"

#include "common/network/resolver_impl.h"

#include "extensions/stat_sinks/common/statsd/statsd.h"
#include "extensions/stat_sinks/well_known_names.h"
#include "extensions/stat_sinks/wasm/wasm_stat_sinks_impl.h"

#include "extensions/common/wasm/wasm.h"
#include "source/server/_virtual_includes/configuration_lib/server/configuration_impl.h"


namespace Envoy {
namespace Extensions {
namespace StatSinks {
namespace Wasm {

Stats::SinkPtr 
WasmSinkFactory::createStatsSink(const Protobuf::Message& config,
                                                   Server::Instance& server) {
  const auto& wasm_config =
      MessageUtil::downcastAndValidate<const envoy::extensions::StatSinks::v3::Wasm&>(
          config, server.messageValidationContext().staticValidationVisitor());

  auto wasm_sink = std::make_unique<WasmStatSink>(config.config().root_id(), nullptr);

  auto plugin = std::make_shared<Common::Wasm::Plugin>(
      wasm_config.config().name(), wasm_config.config().root_id(), wasm_config.config().vm_config().vm_id(),
      Common::Wasm::anyToBytes(wasm_config.config().configuration()),
      envoy::config::core::v3::TrafficDirection::UNSPECIFIED, context.localInfo(),
      nullptr /* listener_metadata */);

  auto callback = [wasm_sink, &context, singleton, plugin](Common::Wasm::WasmHandleSharedPtr base_wasm) {
    // Per-thread WASM VM.
    // NB: the Slot set() call doesn't complete inline, so all arguments must outlive this call.
    auto tls_slot = context.threadLocal().allocateSlot();
    tls_slot->set([base_wasm, plugin](Event::Dispatcher& dispatcher) {
      return std::static_pointer_cast<ThreadLocal::ThreadLocalObject>(
          Common::Wasm::getOrCreateThreadLocalWasm(base_wasm, plugin, dispatcher));
    });
    wasm_sink->setTlsSlot(std::move(tls_slot));
  };

  Common::Wasm::createWasm(wasm_config.config().vm_config(), plugin, context.scope().createScope(""),
                           context.clusterManager(), context.initManager(), context.dispatcher(),
                           context.random(), context.api(), context.lifecycleNotifier(),
                           remote_data_provider_, std::move(callback));

  return wasm_sink;
}

ProtobufTypes::MessagePtr WasmSinkFactory::createEmptyConfigProto() {
  return std::make_unique<envoy::extensions::StatSinks::Wasm>();
}

std::string WasmSinkFactory::name() const { return StatsSinkNames::get().Wasm; }

/**
 * Static registration for the wasm access log. @see RegisterFactory.
 */
REGISTER_FACTORY(WasmSinkFactory, Server::Configuration::StatsSinkFactory);

} // namespace Wasm
} // namespace StatSinks
} // namespace Extensions
} // namespace Envoy
