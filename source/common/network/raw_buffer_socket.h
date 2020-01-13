#pragma once

#include "envoy/buffer/buffer.h"
#include "envoy/config/filter/network/tcp_proxy/v2/tcp_proxy.pb.h"
#include "envoy/network/connection.h"
#include "envoy/network/transport_socket.h"

#include "common/common/logger.h"

namespace Envoy {
namespace Network {

class RawBufferSocket : public TransportSocket, protected Logger::Loggable<Logger::Id::connection> {
public:
  // Network::TransportSocket
  void setTransportSocketCallbacks(TransportSocketCallbacks& callbacks) override;
  std::string protocol() const override;
  absl::string_view failureReason() const override;
  bool canFlushClose() override { return true; }
  void closeSocket(Network::ConnectionEvent) override {}
  void onConnected() override;
  IoResult doRead(Buffer::Instance& buffer) override;
  IoResult doWrite(Buffer::Instance& buffer, bool end_stream) override;
  Ssl::ConnectionInfoConstSharedPtr ssl() const override { return nullptr; }

  void enableFastPath(TransportSocket* socket,
                      envoy::config::filter::network::tcp_proxy::v2::FastPathType fast_path_type) {
    fast_path_socket_ = socket;
    fast_path_type_ = fast_path_type;
  }

private:
  TransportSocketCallbacks* callbacks_{};
  TransportSocket* fast_path_socket_{};
  envoy::config::filter::network::tcp_proxy::v2::FastPathType fast_path_type_;
  bool shutdown_{};
};

class RawBufferSocketFactory : public TransportSocketFactory {
public:
  // Network::TransportSocketFactory
  TransportSocketPtr createTransportSocket(TransportSocketOptionsSharedPtr options) const override;
  bool implementsSecureTransport() const override;
};

} // namespace Network
} // namespace Envoy
