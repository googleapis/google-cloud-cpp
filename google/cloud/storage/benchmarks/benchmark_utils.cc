// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include "google/cloud/storage/benchmarks/bounded_queue.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/compute_engine_util.h"
#include "google/cloud/internal/curl_options.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/options.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "absl/time/time.h"
#include <future>
#include <sstream>

namespace google {
namespace cloud {
namespace storage_benchmarks {

namespace gcs = ::google::cloud::storage;

void DeleteAllObjects(google::cloud::storage::Client client,
                      std::string const& bucket_name, int thread_count) {
  return DeleteAllObjects(std::move(client), bucket_name,
                          google::cloud::storage::Prefix(), thread_count);
}

void DeleteAllObjects(google::cloud::storage::Client client,
                      std::string const& bucket_name,
                      google::cloud::storage::Prefix prefix, int thread_count) {
  using WorkQueue = BoundedQueue<google::cloud::storage::ObjectMetadata>;
  using std::chrono::duration_cast;
  using std::chrono::milliseconds;
  namespace gcs = ::google::cloud::storage;

  std::cout << "# Deleting test objects [" << thread_count << "]\n";
  auto start = std::chrono::steady_clock::now();
  WorkQueue work_queue;
  std::vector<std::future<google::cloud::Status>> workers;
  std::generate_n(
      std::back_inserter(workers), thread_count, [&client, &work_queue] {
        auto worker_task = [](gcs::Client client, WorkQueue& wq) {
          google::cloud::Status status{};
          for (auto object = wq.Pop(); object.has_value(); object = wq.Pop()) {
            auto s = client.DeleteObject(object->bucket(), object->name(),
                                         gcs::Generation(object->generation()));
            if (!s.ok()) status = s;
          }
          return status;
        };
        return std::async(std::launch::async, worker_task, client,
                          std::ref(work_queue));
      });

  for (auto& o : client.ListObjects(bucket_name, gcs::Versions(true),
                                    std::move(prefix))) {
    if (!o) break;
    work_queue.Push(*std::move(o));
  }
  work_queue.Shutdown();
  int count = 0;
  for (auto& t : workers) {
    auto status = t.get();
    if (!status.ok()) {
      std::cerr << "Error return task[" << count << "]: " << status << "\n";
    }
    ++count;
  }
  auto elapsed = std::chrono::steady_clock::now() - start;
  std::cout << "# Deleted in " << duration_cast<milliseconds>(elapsed).count()
            << "ms\n";
}

char const* ToString(ApiName api) {
  switch (api) {
    case ApiName::kApiJson:
      return "JSON";
    case ApiName::kApiXml:
      return "XML";
    case ApiName::kApiGrpc:
      return "GRPC";
  }
  return "";
}

StatusOr<ApiName> ParseApiName(std::string const& val) {
  for (auto a : {ApiName::kApiJson, ApiName::kApiXml, ApiName::kApiGrpc}) {
    if (val == ToString(a)) return a;
  }
  return Status{StatusCode::kInvalidArgument, "unknown ApiName " + val};
}

StatusOr<ExperimentLibrary> ParseExperimentLibrary(std::string const& val) {
  for (auto v : {ExperimentLibrary::kRaw, ExperimentLibrary::kCppClient}) {
    if (val == ToString(v)) return v;
  }
  return Status{StatusCode::kInvalidArgument,
                "unknown ExperimentLibrary " + val};
}

StatusOr<ExperimentTransport> ParseExperimentTransport(std::string const& val) {
  for (auto v : {ExperimentTransport::kDirectPath, ExperimentTransport::kGrpc,
                 ExperimentTransport::kJson, ExperimentTransport::kXml,
                 ExperimentTransport::kJsonV2, ExperimentTransport::kXmlV2}) {
    if (val == ToString(v)) return v;
  }
  return Status{StatusCode::kInvalidArgument,
                "unknown ExperimentTransport " + val};
}

std::string ToString(ExperimentLibrary v) {
  switch (v) {
    case ExperimentLibrary::kCppClient:
      return "CppClient";
    case ExperimentLibrary::kRaw:
      return "Raw";
  }
  return "";
}

std::string ToString(ExperimentTransport v) {
  switch (v) {
    case ExperimentTransport::kDirectPath:
      return "DirectPath";
    case ExperimentTransport::kGrpc:
      return "Grpc";
    case ExperimentTransport::kJson:
      return "Json";
    case ExperimentTransport::kXml:
      return "Xml";
    case ExperimentTransport::kJsonV2:
      return "JsonV2";
    case ExperimentTransport::kXmlV2:
      return "XmlV2";
  }
  return "";
}

std::string RandomBucketPrefix() {
  return "gcs-grpc-team-cloud-cpp-testing-bm";
}

std::string MakeRandomBucketName(google::cloud::internal::DefaultPRNG& gen) {
  return storage::testing::MakeRandomBucketName(gen, RandomBucketPrefix());
}

std::string MakeRandomObjectName(google::cloud::internal::DefaultPRNG& gen) {
  auto constexpr kObjectNameLength = 32;
  return google::cloud::internal::Sample(gen, kObjectNameLength,
                                         "abcdefghijklmnopqrstuvwxyz"
                                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                         "0123456789") +
         ".txt";
}

void PrintOptions(std::ostream& os, std::string const& prefix,
                  Options const& options) {
  if (options.has<GrpcBackgroundThreadPoolSizeOption>()) {
    os << "\n# " << prefix << " Grpc Background Threads: "
       << options.get<GrpcBackgroundThreadPoolSizeOption>();
  }
  if (options.has<GrpcNumChannelsOption>()) {
    os << "\n# " << prefix
       << " gRPC Channel Count: " << options.get<GrpcNumChannelsOption>();
  }
  if (options.has<EndpointOption>()) {
    os << "\n# " << prefix
       << " Grpc Endpoint: " << options.get<EndpointOption>();
  }
  if (options.has<gcs::ConnectionPoolSizeOption>()) {
    os << "\n# " << prefix << " REST Connection Pool Size: "
       << options.get<gcs::ConnectionPoolSizeOption>();
  }
  if (options.has<gcs::RestEndpointOption>()) {
    os << "\n# " << prefix
       << " REST Endpoint: " << options.get<gcs::RestEndpointOption>();
  }
  if (options.has<gcs::TransferStallTimeoutOption>()) {
    os << "\n# " << prefix << " Transfer Stall Timeout: "
       << absl::FormatDuration(
              absl::FromChrono(options.get<gcs::TransferStallTimeoutOption>()));
  }
  if (options.has<gcs::TransferStallMinimumRateOption>()) {
    os << "\n# " << prefix << " Transfer Stall Minimum Rate: "
       << testing_util::FormatSize(
              options.get<gcs::TransferStallMinimumRateOption>());
  }
  if (options.has<gcs::DownloadStallTimeoutOption>()) {
    os << "\n# " << prefix << " Download Stall Timeout: "
       << absl::FormatDuration(
              absl::FromChrono(options.get<gcs::DownloadStallTimeoutOption>()));
  }
  if (options.has<gcs::DownloadStallMinimumRateOption>()) {
    os << "\n# " << prefix << " Download Stall Minimum Rate: "
       << testing_util::FormatSize(
              options.get<gcs::DownloadStallMinimumRateOption>());
  }

  if (options.has<google::cloud::storage::internal::TargetApiVersionOption>()) {
    os << "\n# " << prefix << " Api Version Path: "
       << options
              .has<google::cloud::storage::internal::TargetApiVersionOption>();
  }
}

// Format a timestamp
std::string FormatTimestamp(std::chrono::system_clock::time_point tp) {
  auto constexpr kFormat = "%E4Y-%m-%dT%H:%M:%E*SZ";
  auto const t = absl::FromChrono(tp);
  return absl::FormatTime(kFormat, t, absl::UTCTimeZone());
}

absl::optional<std::string> GetLabel(std::vector<std::string> const& labels,
                                     std::string const& prefix) {
  for (auto const& label : labels) {
    if (absl::StartsWith(label, prefix)) {
      return std::string{absl::StripPrefix(label, prefix)};
    }
  }
  return absl::nullopt;
}

absl::optional<std::string> GetLabel(std::string const& labels,
                                     std::string const& prefix) {
  return GetLabel(absl::StrSplit(labels, ','), prefix);
}

absl::optional<std::string> Zone(std::string const& labels) {
  return GetLabel(labels, "zone:");
}

absl::optional<std::string> Job(std::string const& labels) {
  return GetLabel(labels, "job:");
}

absl::optional<std::string> Task(std::string const& labels) {
  return GetLabel(labels, "task:");
}

using ::google::cloud::rest_internal::ReadAll;
using ::google::cloud::rest_internal::RestClient;
using ::google::cloud::rest_internal::RestRequest;

absl::optional<std::string> GetMetadata(RestClient& metadata_server,
                                        std::string const& path) {
  RestRequest request(path);
  request.AddHeader("Metadata-Flavor", "Google");
  auto response_status = metadata_server.Get(request);
  if (!response_status) return absl::nullopt;
  auto response = *std::move(response_status);
  auto const status_code = response->StatusCode();
  auto contents = ReadAll(std::move(*response).ExtractPayload());
  if (status_code != 200) return absl::nullopt;
  if (!contents) return absl::nullopt;
  // A lot of metadata attributes have the full resource name (e.e.,
  // projects/.../zones/..), we just want the last portion.
  std::vector<absl::string_view> split = absl::StrSplit(*contents, '/');
  return std::string{split.back()};
}

std::string AddDefaultLabels(std::string const& labels) {
  using google::cloud::rest_internal::ConnectionPoolSizeOption;
  auto metadata_server = google::cloud::rest_internal::MakePooledRestClient(
      absl::StrCat("http", "://",
                   google::cloud::internal::GceMetadataHostname()),
      google::cloud::Options{}.set<ConnectionPoolSizeOption>(4));
  struct {
    std::string prefix;
    std::string path;
  } defaults[] = {
      {"zone:", "computeMetadata/v1/instance/zone"},
      {"machine-type:", "computeMetadata/v1/instance/machine-type"},
      {"instance-name:", "computeMetadata/v1/instance/name"},
      {"instance-id:", "computeMetadata/v1/instance/id"},
      {"project-id:", "computeMetadata/v1/project/project-id"},
      {"project-number:", "computeMetadata/v1/project/numeric-project-id"},
  };
  std::vector<std::string> components =
      absl::StrSplit(labels, ',', absl::SkipWhitespace());
  for (auto const& d : defaults) {
    if (!GetLabel(components, d.prefix).has_value()) {
      auto contents = GetMetadata(*metadata_server, d.path);
      if (!contents.has_value()) continue;
      components.push_back(d.prefix + *contents);
    }
  }
  return absl::StrJoin(components, ",");
}

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
