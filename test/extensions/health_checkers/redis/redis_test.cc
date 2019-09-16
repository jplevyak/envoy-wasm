#include <memory>

#include "envoy/api/api.h"

#include "extensions/health_checkers/redis/redis.h"
#include "extensions/health_checkers/redis/utility.h"

#include "test/common/upstream/utility.h"
#include "test/extensions/filters/network/common/redis/mocks.h"
#include "test/extensions/filters/network/redis_proxy/mocks.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/runtime/mocks.h"
#include "test/mocks/upstream/mocks.h"

using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::NiceMock;
using testing::Ref;
using testing::Return;
using testing::WithArg;

namespace Envoy {
namespace Extensions {
namespace HealthCheckers {
namespace RedisHealthChecker {

class RedisHealthCheckerTest
    : public testing::Test,
      public Extensions::NetworkFilters::Common::Redis::Client::ClientFactory {
public:
  RedisHealthCheckerTest()
      : cluster_(new NiceMock<Upstream::MockClusterMockPrioritySet>()),
        event_logger_(new Upstream::MockHealthCheckEventLogger()), api_(Api::createApiForTest()) {}

  void setup() {
    const std::string yaml = R"EOF(
    timeout: 1s
    interval: 1s
    no_traffic_interval: 5s
    interval_jitter: 1s
    unhealthy_threshold: 1
    healthy_threshold: 1
    custom_health_check:
      name: envoy.health_checkers.redis
      config:
    )EOF";

    const auto& health_check_config = Upstream::parseHealthCheckFromV2Yaml(yaml);
    const auto& redis_config = getRedisHealthCheckConfig(
        health_check_config, ProtobufMessage::getStrictValidationVisitor());

    health_checker_.reset(new RedisHealthChecker(
        *cluster_, health_check_config, redis_config, dispatcher_, runtime_, random_,
        Upstream::HealthCheckEventLoggerPtr(event_logger_), *api_, *this));
  }

  void setupAlwaysLogHealthCheckFailures() {
    const std::string yaml = R"EOF(
    timeout: 1s
    interval: 1s
    no_traffic_interval: 5s
    interval_jitter: 1s
    unhealthy_threshold: 1
    healthy_threshold: 1
    always_log_health_check_failures: true
    custom_health_check:
      name: envoy.health_checkers.redis
      config:
    )EOF";

    const auto& health_check_config = Upstream::parseHealthCheckFromV2Yaml(yaml);
    const auto& redis_config = getRedisHealthCheckConfig(
        health_check_config, ProtobufMessage::getStrictValidationVisitor());

    health_checker_.reset(new RedisHealthChecker(
        *cluster_, health_check_config, redis_config, dispatcher_, runtime_, random_,
        Upstream::HealthCheckEventLoggerPtr(event_logger_), *api_, *this));
  }

  void setupExistsHealthcheck() {
    const std::string yaml = R"EOF(
    timeout: 1s
    interval: 1s
    no_traffic_interval: 5s
    interval_jitter: 1s
    unhealthy_threshold: 1
    healthy_threshold: 1
    custom_health_check:
      name: envoy.health_checkers.redis
      config:
        key: foo
    )EOF";

    const auto& health_check_config = Upstream::parseHealthCheckFromV2Yaml(yaml);
    const auto& redis_config = getRedisHealthCheckConfig(
        health_check_config, ProtobufMessage::getStrictValidationVisitor());

    health_checker_.reset(new RedisHealthChecker(
        *cluster_, health_check_config, redis_config, dispatcher_, runtime_, random_,
        Upstream::HealthCheckEventLoggerPtr(event_logger_), *api_, *this));
  }

  void setupDontReuseConnection() {
    const std::string yaml = R"EOF(
    timeout: 1s
    interval: 1s
    no_traffic_interval: 5s
    interval_jitter: 1s
    unhealthy_threshold: 1
    healthy_threshold: 1
    reuse_connection: false
    custom_health_check:
      name: envoy.health_checkers.redis
      config:
    )EOF";

    const auto& health_check_config = Upstream::parseHealthCheckFromV2Yaml(yaml);
    const auto& redis_config = getRedisHealthCheckConfig(
        health_check_config, ProtobufMessage::getStrictValidationVisitor());

    health_checker_.reset(new RedisHealthChecker(
        *cluster_, health_check_config, redis_config, dispatcher_, runtime_, random_,
        Upstream::HealthCheckEventLoggerPtr(event_logger_), *api_, *this));
  }

  Extensions::NetworkFilters::Common::Redis::Client::ClientPtr
  create(Upstream::HostConstSharedPtr, Event::Dispatcher&,
         const Extensions::NetworkFilters::Common::Redis::Client::Config&,
         const Extensions::NetworkFilters::Common::Redis::RedisCommandStatsSharedPtr&,
         Stats::Scope&, const std::string&) override {
    return Extensions::NetworkFilters::Common::Redis::Client::ClientPtr{create_()};
  }

  MOCK_METHOD0(create_, Extensions::NetworkFilters::Common::Redis::Client::Client*());

  void expectSessionCreate() {
    interval_timer_ = new Event::MockTimer(&dispatcher_);
    timeout_timer_ = new Event::MockTimer(&dispatcher_);
  }

  void expectClientCreate() {
    client_ = new Extensions::NetworkFilters::Common::Redis::Client::MockClient();
    EXPECT_CALL(*this, create_()).WillOnce(Return(client_));
    EXPECT_CALL(*client_, addConnectionCallbacks(_));
  }

  void expectExistsRequestCreate() {
    EXPECT_CALL(*client_, makeRequest(Ref(RedisHealthChecker::existsHealthCheckRequest("")), _))
        .WillOnce(DoAll(WithArg<1>(SaveArgAddress(&pool_callbacks_)), Return(&pool_request_)));
    EXPECT_CALL(*timeout_timer_, enableTimer(_, _));
  }

  void expectPingRequestCreate() {
    EXPECT_CALL(*client_, makeRequest(Ref(RedisHealthChecker::pingHealthCheckRequest()), _))
        .WillOnce(DoAll(WithArg<1>(SaveArgAddress(&pool_callbacks_)), Return(&pool_request_)));
    EXPECT_CALL(*timeout_timer_, enableTimer(_, _));
  }

  void exerciseStubs() {
    Upstream::HostSharedPtr host = Upstream::makeTestHost(cluster_->info_, "tcp://127.0.0.1:100");
    RedisHealthChecker::RedisActiveHealthCheckSessionPtr session =
        std::make_unique<RedisHealthChecker::RedisActiveHealthCheckSession>(*health_checker_, host);

    EXPECT_TRUE(session->disableOutlierEvents());
    EXPECT_EQ(session->opTimeout(),
              std::chrono::milliseconds(2000)); // Timeout is 1s is test configurations.
    EXPECT_FALSE(session->enableHashtagging());
    EXPECT_TRUE(session->enableRedirection());
    EXPECT_EQ(session->maxBufferSizeBeforeFlush(), 0);
    EXPECT_EQ(session->bufferFlushTimeoutInMs(), std::chrono::milliseconds(1));
    EXPECT_EQ(session->maxUpstreamUnknownConnections(), 0);
    EXPECT_FALSE(session->enableCommandStats());
    session->onDeferredDeleteBase(); // This must be called to pass assertions in the destructor.
  }

  std::shared_ptr<Upstream::MockClusterMockPrioritySet> cluster_;
  NiceMock<Event::MockDispatcher> dispatcher_;
  NiceMock<Runtime::MockLoader> runtime_;
  NiceMock<Runtime::MockRandomGenerator> random_;
  Upstream::MockHealthCheckEventLogger* event_logger_{};
  Event::MockTimer* timeout_timer_{};
  Event::MockTimer* interval_timer_{};
  Extensions::NetworkFilters::Common::Redis::Client::MockClient* client_{};
  Extensions::NetworkFilters::Common::Redis::Client::MockPoolRequest pool_request_;
  Extensions::NetworkFilters::Common::Redis::Client::PoolCallbacks* pool_callbacks_{};
  std::shared_ptr<RedisHealthChecker> health_checker_;
  Api::ApiPtr api_;
};

TEST_F(RedisHealthCheckerTest, PingAndVariousFailures) {
  InSequence s;
  setup();

  // Exercise stubbed out interfaces for coverage.
  exerciseStubs();

  cluster_->prioritySet().getMockHostSet(0)->hosts_ = {
      Upstream::makeTestHost(cluster_->info_, "tcp://127.0.0.1:80")};

  expectSessionCreate();
  expectClientCreate();
  expectPingRequestCreate();
  health_checker_->start();

  client_->runHighWatermarkCallbacks();
  client_->runLowWatermarkCallbacks();

  // Success
  EXPECT_CALL(*timeout_timer_, disableTimer());
  EXPECT_CALL(*interval_timer_, enableTimer(_, _));
  NetworkFilters::Common::Redis::RespValuePtr response(
      new NetworkFilters::Common::Redis::RespValue());
  response->type(NetworkFilters::Common::Redis::RespType::SimpleString);
  response->asString() = "PONG";
  pool_callbacks_->onResponse(std::move(response));

  expectPingRequestCreate();
  interval_timer_->invokeCallback();

  // Failure
  EXPECT_CALL(*event_logger_, logEjectUnhealthy(_, _, _));
  EXPECT_CALL(*timeout_timer_, disableTimer());
  EXPECT_CALL(*interval_timer_, enableTimer(_, _));
  response = std::make_unique<NetworkFilters::Common::Redis::RespValue>();
  pool_callbacks_->onResponse(std::move(response));

  expectPingRequestCreate();
  interval_timer_->invokeCallback();

  // Redis failure via disconnect
  EXPECT_CALL(*timeout_timer_, disableTimer());
  EXPECT_CALL(*interval_timer_, enableTimer(_, _));
  pool_callbacks_->onFailure();
  client_->raiseEvent(Network::ConnectionEvent::RemoteClose);

  expectClientCreate();
  expectPingRequestCreate();
  interval_timer_->invokeCallback();

  // Timeout
  EXPECT_CALL(pool_request_, cancel());
  EXPECT_CALL(*client_, close());
  EXPECT_CALL(*timeout_timer_, disableTimer());
  EXPECT_CALL(*interval_timer_, enableTimer(_, _));
  timeout_timer_->invokeCallback();

  expectClientCreate();
  expectPingRequestCreate();
  interval_timer_->invokeCallback();

  // Shutdown with active request.
  EXPECT_CALL(pool_request_, cancel());
  EXPECT_CALL(*client_, close());

  EXPECT_EQ(5UL, cluster_->info_->stats_store_.counter("health_check.attempt").value());
  EXPECT_EQ(1UL, cluster_->info_->stats_store_.counter("health_check.success").value());
  EXPECT_EQ(3UL, cluster_->info_->stats_store_.counter("health_check.failure").value());
  EXPECT_EQ(2UL, cluster_->info_->stats_store_.counter("health_check.network_failure").value());
}

TEST_F(RedisHealthCheckerTest, FailuresLogging) {
  InSequence s;
  setupAlwaysLogHealthCheckFailures();

  cluster_->prioritySet().getMockHostSet(0)->hosts_ = {
      Upstream::makeTestHost(cluster_->info_, "tcp://127.0.0.1:80")};

  expectSessionCreate();
  expectClientCreate();
  expectPingRequestCreate();
  health_checker_->start();

  client_->runHighWatermarkCallbacks();
  client_->runLowWatermarkCallbacks();

  // Success
  EXPECT_CALL(*timeout_timer_, disableTimer());
  EXPECT_CALL(*interval_timer_, enableTimer(_, _));
  NetworkFilters::Common::Redis::RespValuePtr response(
      new NetworkFilters::Common::Redis::RespValue());
  response->type(NetworkFilters::Common::Redis::RespType::SimpleString);
  response->asString() = "PONG";
  pool_callbacks_->onResponse(std::move(response));

  expectPingRequestCreate();
  interval_timer_->invokeCallback();

  // Failure
  EXPECT_CALL(*event_logger_, logEjectUnhealthy(_, _, _));
  EXPECT_CALL(*event_logger_, logUnhealthy(_, _, _, false));
  EXPECT_CALL(*timeout_timer_, disableTimer());
  EXPECT_CALL(*interval_timer_, enableTimer(_, _));
  response = std::make_unique<NetworkFilters::Common::Redis::RespValue>();
  pool_callbacks_->onResponse(std::move(response));

  expectPingRequestCreate();
  interval_timer_->invokeCallback();

  // Fail again
  EXPECT_CALL(*event_logger_, logUnhealthy(_, _, _, false));
  EXPECT_CALL(*timeout_timer_, disableTimer());
  EXPECT_CALL(*interval_timer_, enableTimer(_, _));
  response = std::make_unique<NetworkFilters::Common::Redis::RespValue>();
  pool_callbacks_->onResponse(std::move(response));

  expectPingRequestCreate();
  interval_timer_->invokeCallback();

  // Shutdown with active request.
  EXPECT_CALL(pool_request_, cancel());
  EXPECT_CALL(*client_, close());

  EXPECT_EQ(4UL, cluster_->info_->stats_store_.counter("health_check.attempt").value());
  EXPECT_EQ(1UL, cluster_->info_->stats_store_.counter("health_check.success").value());
  EXPECT_EQ(2UL, cluster_->info_->stats_store_.counter("health_check.failure").value());
  EXPECT_EQ(0UL, cluster_->info_->stats_store_.counter("health_check.network_failure").value());
}

TEST_F(RedisHealthCheckerTest, LogInitialFailure) {
  InSequence s;
  setup();

  cluster_->prioritySet().getMockHostSet(0)->hosts_ = {
      Upstream::makeTestHost(cluster_->info_, "tcp://127.0.0.1:80")};

  expectSessionCreate();
  expectClientCreate();
  expectPingRequestCreate();
  health_checker_->start();

  client_->runHighWatermarkCallbacks();
  client_->runLowWatermarkCallbacks();

  // Redis failure via disconnect
  EXPECT_CALL(*event_logger_, logEjectUnhealthy(_, _, _));
  EXPECT_CALL(*event_logger_, logUnhealthy(_, _, _, true));
  EXPECT_CALL(*timeout_timer_, disableTimer());
  EXPECT_CALL(*interval_timer_, enableTimer(_, _));
  pool_callbacks_->onFailure();
  client_->raiseEvent(Network::ConnectionEvent::RemoteClose);

  expectClientCreate();
  expectPingRequestCreate();
  interval_timer_->invokeCallback();

  // Success
  EXPECT_CALL(*event_logger_, logAddHealthy(_, _, false));
  EXPECT_CALL(*timeout_timer_, disableTimer());
  EXPECT_CALL(*interval_timer_, enableTimer(_, _));
  NetworkFilters::Common::Redis::RespValuePtr response(
      new NetworkFilters::Common::Redis::RespValue());
  response->type(NetworkFilters::Common::Redis::RespType::SimpleString);
  response->asString() = "PONG";
  pool_callbacks_->onResponse(std::move(response));

  expectPingRequestCreate();
  interval_timer_->invokeCallback();

  // Shutdown with active request.
  EXPECT_CALL(pool_request_, cancel());
  EXPECT_CALL(*client_, close());

  EXPECT_EQ(3UL, cluster_->info_->stats_store_.counter("health_check.attempt").value());
  EXPECT_EQ(1UL, cluster_->info_->stats_store_.counter("health_check.success").value());
  EXPECT_EQ(1UL, cluster_->info_->stats_store_.counter("health_check.failure").value());
  EXPECT_EQ(1UL, cluster_->info_->stats_store_.counter("health_check.network_failure").value());
}

TEST_F(RedisHealthCheckerTest, Exists) {
  InSequence s;
  setupExistsHealthcheck();

  cluster_->prioritySet().getMockHostSet(0)->hosts_ = {
      Upstream::makeTestHost(cluster_->info_, "tcp://127.0.0.1:80")};

  expectSessionCreate();
  expectClientCreate();
  expectExistsRequestCreate();
  health_checker_->start();

  client_->runHighWatermarkCallbacks();
  client_->runLowWatermarkCallbacks();

  // Success
  EXPECT_CALL(*timeout_timer_, disableTimer());
  EXPECT_CALL(*interval_timer_, enableTimer(_, _));
  NetworkFilters::Common::Redis::RespValuePtr response(
      new NetworkFilters::Common::Redis::RespValue());
  response->type(NetworkFilters::Common::Redis::RespType::Integer);
  response->asInteger() = 0;
  pool_callbacks_->onResponse(std::move(response));

  expectExistsRequestCreate();
  interval_timer_->invokeCallback();

  // Failure, exists
  EXPECT_CALL(*event_logger_, logEjectUnhealthy(_, _, _));
  EXPECT_CALL(*timeout_timer_, disableTimer());
  EXPECT_CALL(*interval_timer_, enableTimer(_, _));
  response = std::make_unique<NetworkFilters::Common::Redis::RespValue>();
  response->type(NetworkFilters::Common::Redis::RespType::Integer);
  response->asInteger() = 1;
  pool_callbacks_->onResponse(std::move(response));

  expectExistsRequestCreate();
  interval_timer_->invokeCallback();

  // Failure, no value
  EXPECT_CALL(*timeout_timer_, disableTimer());
  EXPECT_CALL(*interval_timer_, enableTimer(_, _));
  response = std::make_unique<NetworkFilters::Common::Redis::RespValue>();
  pool_callbacks_->onResponse(std::move(response));

  EXPECT_CALL(*client_, close());

  EXPECT_EQ(3UL, cluster_->info_->stats_store_.counter("health_check.attempt").value());
  EXPECT_EQ(1UL, cluster_->info_->stats_store_.counter("health_check.success").value());
  EXPECT_EQ(2UL, cluster_->info_->stats_store_.counter("health_check.failure").value());
}

TEST_F(RedisHealthCheckerTest, ExistsRedirected) {
  InSequence s;
  setupExistsHealthcheck();

  cluster_->prioritySet().getMockHostSet(0)->hosts_ = {
      Upstream::makeTestHost(cluster_->info_, "tcp://127.0.0.1:80")};

  expectSessionCreate();
  expectClientCreate();
  expectExistsRequestCreate();
  health_checker_->start();

  client_->runHighWatermarkCallbacks();
  client_->runLowWatermarkCallbacks();

  // Success with moved redirection
  EXPECT_CALL(*timeout_timer_, disableTimer());
  EXPECT_CALL(*interval_timer_, enableTimer(_, _));
  NetworkFilters::Common::Redis::RespValue moved_response;
  moved_response.type(NetworkFilters::Common::Redis::RespType::Error);
  moved_response.asString() = "MOVED 1111 127.0.0.1:81"; // exact values not important
  pool_callbacks_->onRedirection(moved_response);

  expectExistsRequestCreate();
  interval_timer_->invokeCallback();

  // Success with ask redirection
  EXPECT_CALL(*timeout_timer_, disableTimer());
  EXPECT_CALL(*interval_timer_, enableTimer(_, _));
  NetworkFilters::Common::Redis::RespValue ask_response;
  ask_response.type(NetworkFilters::Common::Redis::RespType::Error);
  ask_response.asString() = "ASK 1111 127.0.0.1:81"; // exact values not important
  pool_callbacks_->onRedirection(ask_response);

  EXPECT_CALL(*client_, close());

  EXPECT_EQ(2UL, cluster_->info_->stats_store_.counter("health_check.attempt").value());
  EXPECT_EQ(2UL, cluster_->info_->stats_store_.counter("health_check.success").value());
  EXPECT_EQ(0UL, cluster_->info_->stats_store_.counter("health_check.failure").value());
}

// Tests that redis client will behave appropriately when reuse_connection is false.
TEST_F(RedisHealthCheckerTest, NoConnectionReuse) {
  InSequence s;
  setupDontReuseConnection();

  cluster_->prioritySet().getMockHostSet(0)->hosts_ = {
      Upstream::makeTestHost(cluster_->info_, "tcp://127.0.0.1:80")};

  expectSessionCreate();
  expectClientCreate();
  expectPingRequestCreate();
  health_checker_->start();

  // The connection will close on success.
  EXPECT_CALL(*timeout_timer_, disableTimer());
  EXPECT_CALL(*interval_timer_, enableTimer(_, _));
  EXPECT_CALL(*client_, close());
  NetworkFilters::Common::Redis::RespValuePtr response(
      new NetworkFilters::Common::Redis::RespValue());
  response->type(NetworkFilters::Common::Redis::RespType::SimpleString);
  response->asString() = "PONG";
  pool_callbacks_->onResponse(std::move(response));

  expectClientCreate();
  expectPingRequestCreate();
  interval_timer_->invokeCallback();

  // The connection will close on failure.
  EXPECT_CALL(*event_logger_, logEjectUnhealthy(_, _, _));
  EXPECT_CALL(*timeout_timer_, disableTimer());
  EXPECT_CALL(*interval_timer_, enableTimer(_, _));
  EXPECT_CALL(*client_, close());
  response = std::make_unique<NetworkFilters::Common::Redis::RespValue>();
  pool_callbacks_->onResponse(std::move(response));

  expectClientCreate();
  expectPingRequestCreate();
  interval_timer_->invokeCallback();

  // Redis failure via disconnect, the connection was closed by the other end.
  EXPECT_CALL(*timeout_timer_, disableTimer());
  EXPECT_CALL(*interval_timer_, enableTimer(_, _));
  pool_callbacks_->onFailure();
  client_->raiseEvent(Network::ConnectionEvent::RemoteClose);

  expectClientCreate();
  expectPingRequestCreate();
  interval_timer_->invokeCallback();

  // Timeout, the connection will be closed.
  EXPECT_CALL(pool_request_, cancel());
  EXPECT_CALL(*client_, close());
  EXPECT_CALL(*timeout_timer_, disableTimer());
  EXPECT_CALL(*interval_timer_, enableTimer(_, _));
  timeout_timer_->invokeCallback();

  expectClientCreate();
  expectPingRequestCreate();
  interval_timer_->invokeCallback();

  // Shutdown with active request.
  EXPECT_CALL(pool_request_, cancel());
  EXPECT_CALL(*client_, close());

  // The metrics expected after all tests have run.
  EXPECT_EQ(5UL, cluster_->info_->stats_store_.counter("health_check.attempt").value());
  EXPECT_EQ(1UL, cluster_->info_->stats_store_.counter("health_check.success").value());
  EXPECT_EQ(3UL, cluster_->info_->stats_store_.counter("health_check.failure").value());
  EXPECT_EQ(2UL, cluster_->info_->stats_store_.counter("health_check.network_failure").value());
}

} // namespace RedisHealthChecker
} // namespace HealthCheckers
} // namespace Extensions
} // namespace Envoy
