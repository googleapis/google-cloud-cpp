// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/version.h"
#include "generator/integration_tests/golden/v1/golden_kitchen_sink_client.h"
#include "generator/integration_tests/golden/v1/golden_kitchen_sink_connection.h"
#include "generator/integration_tests/golden/v1/internal/golden_kitchen_sink_connection_impl.h"
#include "generator/integration_tests/golden/v1/internal/golden_kitchen_sink_logging_decorator.h"
#include "generator/integration_tests/golden/v1/internal/golden_kitchen_sink_metadata_decorator.h"
#include "generator/integration_tests/golden/v1/internal/golden_kitchen_sink_option_defaults.h"
#include "generator/integration_tests/golden/v1/internal/golden_kitchen_sink_stub.h"
#include <benchmark/benchmark.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

// Run on (96 X 2000 MHz CPU s)
// CPU Caches:
//   L1 Data 32 KiB (x48)
//   L1 Instruction 32 KiB (x48)
//   L2 Unified 1024 KiB (x48)
//   L3 Unified 39424 KiB (x2)
// Load Average: 0.38, 0.38, 0.73
// ----------------------------------------------------------------------------
// Benchmark                                  Time             CPU   Iterations
// ----------------------------------------------------------------------------
// BM_ClientRoundTripStubOnly              2330 ns         2329 ns       293640
// BM_ClientRoundTripMetadata              2693 ns         2693 ns       253897
// BM_ClientRoundTripLogging               5551 ns         5551 ns       126334
// BM_ClientRoundTripTenExtraOptions       5352 ns         5351 ns       131036

using ::google::cloud::golden_v1_internal::GoldenKitchenSinkConnectionImpl;
using ::google::cloud::golden_v1_internal::GoldenKitchenSinkDefaultOptions;
using ::google::cloud::golden_v1_internal::GoldenKitchenSinkLogging;
using ::google::cloud::golden_v1_internal::GoldenKitchenSinkMetadata;
using ::google::cloud::golden_v1_internal::GoldenKitchenSinkStub;

class TestStub : public GoldenKitchenSinkStub {
 public:
  StatusOr<google::test::admin::database::v1::GenerateAccessTokenResponse>
  GenerateAccessToken(
      grpc::ClientContext&,
      google::test::admin::database::v1::GenerateAccessTokenRequest const&)
      override {
    return internal::UnimplementedError("unimplemented");
  }

  StatusOr<google::test::admin::database::v1::GenerateIdTokenResponse>
  GenerateIdToken(
      grpc::ClientContext&,
      google::test::admin::database::v1::GenerateIdTokenRequest const&)
      override {
    return internal::UnimplementedError("unimplemented");
  }

  StatusOr<google::test::admin::database::v1::WriteLogEntriesResponse>
  WriteLogEntries(
      grpc::ClientContext&,
      google::test::admin::database::v1::WriteLogEntriesRequest const&)
      override {
    return internal::UnimplementedError("unimplemented");
  }

  StatusOr<google::test::admin::database::v1::ListLogsResponse> ListLogs(
      grpc::ClientContext&,
      google::test::admin::database::v1::ListLogsRequest const&) override {
    return internal::UnimplementedError("unimplemented");
  }

  StatusOr<google::test::admin::database::v1::ListServiceAccountKeysResponse>
  ListServiceAccountKeys(
      grpc::ClientContext&,
      google::test::admin::database::v1::ListServiceAccountKeysRequest const&)
      override {
    return internal::UnimplementedError("unimplemented");
  }

  Status DoNothing(grpc::ClientContext&,
                   google::protobuf::Empty const&) override {
    return Status();
  }

  std::unique_ptr<google::cloud::internal::StreamingReadRpc<
      google::test::admin::database::v1::Response>>
  StreamingRead(std::unique_ptr<grpc::ClientContext>,
                google::test::admin::database::v1::Request const&) override {
    return nullptr;
  }

  std::unique_ptr<::google::cloud::internal::StreamingWriteRpc<
      google::test::admin::database::v1::Request,
      google::test::admin::database::v1::Response>>
  StreamingWrite(std::unique_ptr<grpc::ClientContext>) override {
    return nullptr;
  }

  std::unique_ptr<::google::cloud::AsyncStreamingReadWriteRpc<
      google::test::admin::database::v1::Request,
      google::test::admin::database::v1::Response>>
  AsyncStreamingReadWrite(google::cloud::CompletionQueue const&,
                          std::unique_ptr<grpc::ClientContext>) override {
    return nullptr;
  }

  Status ExplicitRouting1(
      grpc::ClientContext&,
      google::test::admin::database::v1::ExplicitRoutingRequest const&)
      override {
    return Status();
  }

  Status ExplicitRouting2(
      grpc::ClientContext&,
      google::test::admin::database::v1::ExplicitRoutingRequest const&)
      override {
    return Status();
  }

  std::unique_ptr<::google::cloud::internal::AsyncStreamingReadRpc<
      google::test::admin::database::v1::Response>>
  AsyncStreamingRead(
      google::cloud::CompletionQueue const&,
      std::unique_ptr<grpc::ClientContext>,
      google::test::admin::database::v1::Request const&) override {
    return nullptr;
  }

  std::unique_ptr<::google::cloud::internal::AsyncStreamingWriteRpc<
      google::test::admin::database::v1::Request,
      google::test::admin::database::v1::Response>>
  AsyncStreamingWrite(google::cloud::CompletionQueue const&,
                      std::unique_ptr<grpc::ClientContext>) override {
    return nullptr;
  }
};

std::shared_ptr<GoldenKitchenSinkConnection> MakeTestConnection(
    std::shared_ptr<GoldenKitchenSinkStub> stub, Options options) {
  options = GoldenKitchenSinkDefaultOptions(std::move(options));
  auto background = internal::MakeBackgroundThreadsFactory(options)();
  return std::make_shared<GoldenKitchenSinkConnectionImpl>(
      std::move(background), std::move(stub), std::move(options));
}

void BM_ClientRoundTripStubOnly(benchmark::State& state) {
  auto options = Options{};
  auto stub = std::make_shared<TestStub>();
  auto conn = MakeTestConnection(std::move(stub), std::move(options));
  auto client = GoldenKitchenSinkClient(std::move(conn));

  for (auto _ : state) {
    auto status = client.DoNothing();
    benchmark::DoNotOptimize(status);
  }
}
BENCHMARK(BM_ClientRoundTripStubOnly);

void BM_ClientRoundTripMetadata(benchmark::State& state) {
  auto options = Options{};
  std::shared_ptr<GoldenKitchenSinkStub> stub = std::make_shared<TestStub>();
  stub = std::make_shared<GoldenKitchenSinkMetadata>(std::move(stub));
  auto conn = MakeTestConnection(std::move(stub), std::move(options));
  auto client = GoldenKitchenSinkClient(std::move(conn));

  for (auto _ : state) {
    auto status = client.DoNothing();
    benchmark::DoNotOptimize(status);
  }
}
BENCHMARK(BM_ClientRoundTripMetadata);

void BM_ClientRoundTripLogging(benchmark::State& state) {
  auto options = Options{};
  std::shared_ptr<GoldenKitchenSinkStub> stub = std::make_shared<TestStub>();
  stub = std::make_shared<GoldenKitchenSinkLogging>(
      std::move(stub), TracingOptions{}, std::set<std::string>{"rpc"});
  auto conn = MakeTestConnection(std::move(stub), std::move(options));
  auto client = GoldenKitchenSinkClient(std::move(conn));

  for (auto _ : state) {
    auto status = client.DoNothing();
    benchmark::DoNotOptimize(status);
  }
}
BENCHMARK(BM_ClientRoundTripLogging);

void BM_ClientRoundTripTenExtraOptions(benchmark::State& state) {
  // clang-format off
  struct ExtraOption0 { using Type = int; };
  struct ExtraOption1 { using Type = int; };
  struct ExtraOption2 { using Type = int; };
  struct ExtraOption3 { using Type = int; };
  struct ExtraOption4 { using Type = int; };
  struct ExtraOption5 { using Type = int; };
  struct ExtraOption6 { using Type = int; };
  struct ExtraOption7 { using Type = int; };
  struct ExtraOption8 { using Type = int; };
  struct ExtraOption9 { using Type = int; };
  // clang-format on
  auto options = Options{}
                     .set<ExtraOption0>(0)
                     .set<ExtraOption1>(1)
                     .set<ExtraOption2>(2)
                     .set<ExtraOption3>(3)
                     .set<ExtraOption4>(4)
                     .set<ExtraOption5>(5)
                     .set<ExtraOption6>(6)
                     .set<ExtraOption7>(7)
                     .set<ExtraOption8>(8)
                     .set<ExtraOption9>(9);
  auto stub = std::make_shared<TestStub>();
  auto conn = MakeTestConnection(std::move(stub), std::move(options));
  auto client = GoldenKitchenSinkClient(std::move(conn));

  for (auto _ : state) {
    auto status = client.DoNothing();
    benchmark::DoNotOptimize(status);
  }
}
BENCHMARK(BM_ClientRoundTripTenExtraOptions);

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1
}  // namespace cloud
}  // namespace google
