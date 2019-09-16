#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "envoy/common/exception.h"
#include "envoy/config/filter/http/ip_tagging/v2/ip_tagging.pb.h"
#include "envoy/http/filter.h"
#include "envoy/runtime/runtime.h"
#include "envoy/stats/scope.h"

#include "common/network/cidr_range.h"
#include "common/network/lc_trie.h"
#include "common/stats/symbol_table_impl.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace IpTagging {

/**
 * Type of requests the filter should apply to.
 */
enum class FilterRequestType { INTERNAL, EXTERNAL, BOTH };

/**
 * Configuration for the HTTP IP Tagging filter.
 */
class IpTaggingFilterConfig {
public:
  IpTaggingFilterConfig(const envoy::config::filter::http::ip_tagging::v2::IPTagging& config,
                        const std::string& stat_prefix, Stats::Scope& scope,
                        Runtime::Loader& runtime);

  Runtime::Loader& runtime() { return runtime_; }
  Stats::Scope& scope() { return scope_; }
  FilterRequestType requestType() const { return request_type_; }
  const Network::LcTrie::LcTrie<std::string>& trie() const { return *trie_; }

  void incHit(absl::string_view tag) { incCounter(hit_, tag); }
  void incNoHit() { incCounter(no_hit_); }
  void incTotal() { incCounter(total_); }

private:
  static FilterRequestType requestTypeEnum(
      envoy::config::filter::http::ip_tagging::v2::IPTagging::RequestType request_type) {
    switch (request_type) {
    case envoy::config::filter::http::ip_tagging::v2::IPTagging_RequestType_BOTH:
      return FilterRequestType::BOTH;
    case envoy::config::filter::http::ip_tagging::v2::IPTagging_RequestType_INTERNAL:
      return FilterRequestType::INTERNAL;
    case envoy::config::filter::http::ip_tagging::v2::IPTagging_RequestType_EXTERNAL:
      return FilterRequestType::EXTERNAL;
    default:
      NOT_REACHED_GCOVR_EXCL_LINE;
    }
  }

  void incCounter(Stats::StatName name1, absl::string_view tag = "");

  const FilterRequestType request_type_;
  Stats::Scope& scope_;
  Runtime::Loader& runtime_;
  Stats::StatNameSet stat_name_set_;
  const Stats::StatName stats_prefix_;
  const Stats::StatName hit_;
  const Stats::StatName no_hit_;
  const Stats::StatName total_;
  std::unique_ptr<Network::LcTrie::LcTrie<std::string>> trie_;
};

using IpTaggingFilterConfigSharedPtr = std::shared_ptr<IpTaggingFilterConfig>;

/**
 * A filter that gets all tags associated with a request's downstream remote address and
 * sets a header `x-envoy-ip-tags` with those values.
 */
class IpTaggingFilter : public Http::StreamDecoderFilter {
public:
  IpTaggingFilter(IpTaggingFilterConfigSharedPtr config);
  ~IpTaggingFilter() override;

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::HeaderMap& headers, bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance& data, bool end_stream) override;
  Http::FilterTrailersStatus decodeTrailers(Http::HeaderMap& trailers) override;
  void setDecoderFilterCallbacks(Http::StreamDecoderFilterCallbacks& callbacks) override;

private:
  IpTaggingFilterConfigSharedPtr config_;
  Http::StreamDecoderFilterCallbacks* callbacks_{};
};

} // namespace IpTagging
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
