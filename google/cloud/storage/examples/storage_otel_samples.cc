// Copyright 2023 Google LLC
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

#include "google/cloud/internal/port_platform.h"
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC && GOOGLE_CLOUD_CPP_HAVE_COROUTINES
// The final blank line in this section separates the includes from the function
// in the final rendering.
//! [instrumented-client-includes]
#include "google/cloud/opentelemetry/configure_basic_tracing.h"
#include "google/cloud/storage/async/client.h"
#include "google/cloud/opentelemetry_options.h"

//! [instrumented-client-includes]
#include "google/cloud/storage/examples/storage_examples_common.h"
#include "google/cloud/internal/getenv.h"
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {
namespace examples = ::google::cloud::storage::examples;

void InstrumentedClient(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw examples::Usage{
        "instrumented-client <project-id> <bucket-name> <object-name>"};
  }
  //! [instrumented-client]
  namespace gc = ::google::cloud;
  namespace gcs_ex = ::google::cloud::storage_experimental;
  [](std::string const& project_id, std::string const& bucket_name,
     std::string const& object_name) {
    auto configuration =
        gc::otel::ConfigureBasicTracing(gc::Project(project_id));

    auto client = google::cloud::storage_experimental::AsyncClient(
        gc::Options{}.set<gc::OpenTelemetryTracingOption>(true));

    auto coro = [&]() -> gc::future<void> {
      std::vector<std::string> data(1'000'000);
      std::generate(data.begin(), data.end(),
                    [n = 0]() mutable { return std::to_string(++n) + "\n"; });
      auto metadata = co_await client.InsertObject(
          gcs_ex::BucketName(bucket_name), object_name, std::move(data));
      if (!metadata) throw std::move(metadata).status();

      std::int64_t count = 0;
      auto [reader, token] = (co_await client.ReadObject(
                                  gcs_ex::BucketName(bucket_name), object_name))
                                 .value();
      while (token.valid()) {
        auto [payload, t] = (co_await reader.Read(std::move(token))).value();
        token = std::move(t);
        for (auto const& buffer : payload.contents()) {
          count += std::count(buffer.begin(), buffer.end(), '7');
        }
      }
      std::cout << "Counted " << count << " 7's in the GCS object\n";

      auto deleted = co_await client.DeleteObject(
          gcs_ex::BucketName(bucket_name), object_name);
      if (!deleted.ok()) throw gc::Status(std::move(deleted));

      co_return;
    };
    coro().get();
  }
  //! [instrumented-client]
  (argv.at(0), argv.at(1), argv.at(2));
}

void AutoRun(std::vector<std::string> const& argv) {
  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_CPP_PROJECT",
      "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME",
  });
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_PROJECT").value();
  auto bucket_name = google::cloud::internal::GetEnv(
                         "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                         .value();
  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto const object_name = examples::MakeRandomObjectName(generator, "object-");

  std::cout << "Running InstrumentedClient() example" << std::endl;
  InstrumentedClient({project_id, bucket_name, object_name});
}

}  // namespace

int main(int argc, char* argv[]) try {
  examples::Example example({
      {"auto", AutoRun},
      {"instrumented-client", InstrumentedClient},
  });
  return example.Run(argc, argv);
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception thrown: " << ex.what() << "\n";
  return 1;
} catch (...) {
  std::cerr << "Unknown exception thrown\n";
  return 1;
}

#else

int main() { return 0; }

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC &&
        // GOOGLE_CLOUD_CPP_HAVE_COROUTINES
