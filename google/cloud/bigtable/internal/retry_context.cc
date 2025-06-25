// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/internal/retry_context.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// RetryContext::RetryContext(std::shared_ptr<Metrics> metrics,
//                            std::string const& client_uid,
//                            std::string const& method,
//                            std::string const& streaming,
//                            std::string const& table_name,
//                            std::string const& app_profile)
//     : metrics_(std::move(metrics)) {
//   labels_.client_name = "Bigtable-C++";
//   labels_.client_uid = client_uid;
//   labels_.method = method;
//   labels_.streaming = streaming;
//   labels_.table_name = table_name;
//   labels_.app_profile = app_profile;
// }

// RetryContext::RetryContext(
//     std::shared_ptr<std::vector<std::shared_ptr<Metric>>> metric_collection,
//     std::string const& client_uid, std::string const& method,
//     std::string const& streaming, std::string const& table_name,
//     std::string const& app_profile)
//     : metrics_(nullptr), metric_collection_(std::move(metric_collection)) {
//   labels_.client_name = "Bigtable-C++";
//   labels_.client_uid = client_uid;
//   labels_.method = method;
//   labels_.streaming = streaming;
//   labels_.table_name = table_name;
//   labels_.app_profile = app_profile;
// }

// for use with RetryContextFactory
// RetryContext::RetryContext(
//    ResourceLabels resource_labels, DataLabels data_labels,
//    std::vector<std::shared_ptr<Metric>> stub_applicable_metrics)
//    : resource_labels_(std::move(resource_labels)),
//      data_labels_(std::move(data_labels)),
//      stub_applicable_metrics_(std::move(stub_applicable_metrics)) {}

RetryContext::RetryContext(
    std::vector<std::shared_ptr<Metric>> stub_applicable_metrics)
    : stub_applicable_metrics_(std::move(stub_applicable_metrics)) {}

void RetryContext::PreCall(grpc::ClientContext& context) {
  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  auto attempt_start = std::chrono::system_clock::now();
  for (auto& m : stub_applicable_metrics_) {
    m->PreCall(otel_context, PreCallParams{attempt_start});
  }
  //  if (metrics_) {
  //    attempt_start_ = attempt_start;
  //  }
  for (auto const& h : cookies_) {
    context.AddMetadata(h.first, h.second);
  }
  context.AddMetadata("bigtable-attempt", std::to_string(attempt_number_++));
}

void RetryContext::PostCall(grpc::ClientContext const& context,
                            google::cloud::Status const& status) {
  //  std::cout << __func__ << std::endl;
  ProcessMetadata(context.GetServerInitialMetadata());
  ProcessMetadata(context.GetServerTrailingMetadata());
  // NOTE : metrics_ should always exist. But I have only instrumented
  // `Apply()`.
  auto attempt_end = std::chrono::system_clock::now();
  //  if (metrics_) {
  //    auto attempt_elapsed =
  //        std::chrono::duration_cast<std::chrono::milliseconds>(attempt_end -
  //                                                              attempt_start_);
  //    metrics_->RecordAttemptLatency(attempt_elapsed.count(), labels_);
  //  }
  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  for (auto& m : stub_applicable_metrics_) {
    //    std::cout << __func__ << ": calling metric" << std::endl;
    m->PostCall(otel_context, context.GetServerInitialMetadata(),
                context.GetServerTrailingMetadata(),
                PostCallParams{attempt_end, status});
  }
}

void RetryContext::OnDone(Status const&) {
  auto operation_end = std::chrono::system_clock::now();
  // NOTE : metrics_ should always exist. But I have only instrumented
  // `Apply()`.
  //  if (metrics_) {
  //    auto operation_elapsed =
  //        std::chrono::duration_cast<std::chrono::milliseconds>(operation_end
  //        -
  //                                                              operation_start_);
  //    metrics_->RecordOperationLatency(operation_elapsed.count(), labels_);
  //  }
  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  for (auto& m : stub_applicable_metrics_) {
    m->OnDone(otel_context, OnDoneParams{operation_end});
  }
}

void RetryContext::FirstResponse(grpc::ClientContext const&) {
  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  auto first_response = std::chrono::system_clock::now();
  for (auto& m : stub_applicable_metrics_) {
    m->FirstResponse(otel_context, FirstResponseParams{first_response});
  }
}

void RetryContext::ProcessMetadata(
    std::multimap<grpc::string_ref, grpc::string_ref> const& metadata) {
  //  Metadata m;
  for (auto const& kv : metadata) {
    //    std::cout << __func__ << " kv.first="  <<  kv.first << " : "
    //              << "kv.second=" << kv.second << std::endl;
    auto key = std::string{kv.first.data(), kv.first.size()};
    if (absl::StartsWith(key, "x-goog-cbt-cookie")) {
      cookies_[std::move(key)] =
          std::string{kv.second.data(), kv.second.size()};
    }
    //    if (absl::StartsWith(key, "x-goog-ext-425905942-bin")) {
    //      //          std::cout << __func__ << " kv.first="  <<  kv.first << "
    //      : "
    //      //              << "kv.second=" << kv.second << std::endl;
    //      absl::string_view v{kv.second.data(), kv.second.size()};
    //      v = absl::StripAsciiWhitespace(v);
    //      //      // us-central1-ftest-instance-c1
    //      std::vector<std::string> parts = absl::StrSplit(v, '-');
    //      //      std::cout << __func__ << ": parts.size()=" << parts.size()
    //      <<
    //      //      std::endl;
    //      m.zone =
    //          absl::StrCat(parts[0], "-", parts[1], "-", parts[2].substr(0,
    //          1));
    //      m.cluster =
    //          absl::StrCat(parts[2].substr(1), "-", parts[3], "-", parts[4]);
    //      //      std::cout << __func__ << ": zone=" << labels_.zone <<
    //      std::endl;
    //      //      std::cout << __func__ << ": cluster=" << labels_.cluster <<
    //      //      std::endl;
    //    }
  }

  // TODO : Capture cluster, zone, server_latencies in here.
  // For now we hardcode them.
  //  labels_.cluster = "test-instance-c1";
  //  labels_.zone = "us-central1-f";
  //  return m;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
