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

#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
// The final blank line in this section separates the includes from the function
// in the final rendering.
//! [async-includes]
#include "google/cloud/storage/async/client.h"

//! [async-includes]
#include "google/cloud/storage/examples/storage_examples_common.h"
#include "google/cloud/internal/getenv.h"
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {
namespace examples = ::google::cloud::storage::examples;

void CreateClient() {
  //! [async-client]
  auto client = google::cloud::storage_experimental::AsyncClient();
  // Use the client.
  //! [async-client]
}

//! [async-client-with-dp]
void CreateClientWithDP() {
  namespace g = ::google::cloud;
  auto client = google::cloud::storage_experimental::AsyncClient(
      g::Options{}.set<g::EndpointOption>(
          "google-c2p:///storage.googleapis.com"));
  // Use the client.
}
//! [async-client-with-dp]

void InsertObject(google::cloud::storage_experimental::AsyncClient& client,
                  std::vector<std::string> const& argv) {
  //! [insert-object]
  namespace gcs_ex = google::cloud::storage_experimental;
  [](gcs_ex::AsyncClient& client, std::string bucket_name,
     std::string object_name) {
    auto object =
        client.InsertObject(std::move(bucket_name), std::move(object_name),
                            std::string("Hello World!\n"));
    // Attach a callback, this is called when the upload completes.
    auto done = object.then([](auto f) {
      auto metadata = f.get();
      if (!metadata) throw std::move(metadata).status();
      std::cerr << "Object successfully inserted " << *metadata << "\n";
    });
    // To simplify example, block until the operation completes.
    done.get();
  }
  //! [insert-object]
  (client, argv.at(0), argv.at(1));
}

void InsertObjectVectorStrings(
    google::cloud::storage_experimental::AsyncClient& client,
    std::vector<std::string> const& argv) {
  //! [insert-object-vs]
  namespace gcs_ex = google::cloud::storage_experimental;
  [](gcs_ex::AsyncClient& client, std::string bucket_name,
     std::string object_name) {
    auto contents = std::vector<std::string>{"Hello", " ", "World!"};
    auto object = client.InsertObject(
        std::move(bucket_name), std::move(object_name), std::move(contents));
    // Attach a callback, this is called when the upload completes.
    auto done = object.then([](auto f) {
      auto metadata = f.get();
      if (!metadata) throw std::move(metadata).status();
      std::cerr << "Object successfully inserted " << *metadata << "\n";
    });
    // To simplify example, block until the operation completes.
    done.get();
  }
  //! [insert-object-vs]
  (client, argv.at(0), argv.at(1));
}

void InsertObjectVector(
    google::cloud::storage_experimental::AsyncClient& client,
    std::vector<std::string> const& argv) {
  //! [insert-object-v]
  namespace gcs_ex = google::cloud::storage_experimental;
  [](gcs_ex::AsyncClient& client, std::string bucket_name,
     std::string object_name) {
    auto contents = std::vector<std::uint8_t>(1024, 0xFF);
    auto object = client.InsertObject(
        std::move(bucket_name), std::move(object_name), std::move(contents));
    // Attach a callback, this is called when the upload completes.
    auto done = object.then([](auto f) {
      auto metadata = f.get();
      if (!metadata) throw std::move(metadata).status();
      std::cerr << "Object successfully inserted " << *metadata << "\n";
    });
    // To simplify example, block until the operation completes.
    done.get();
  }
  //! [insert-object-vs]
  (client, argv.at(0), argv.at(1));
}

void InsertObjectVectorVectors(
    google::cloud::storage_experimental::AsyncClient& client,
    std::vector<std::string> const& argv) {
  //! [insert-object-vv]
  namespace gcs_ex = google::cloud::storage_experimental;
  [](gcs_ex::AsyncClient& client, std::string bucket_name,
     std::string object_name) {
    using Buffer = std::vector<char>;
    auto contents = std::vector<Buffer>{Buffer(1024, 'a'), Buffer(1024, 'b'),
                                        Buffer(1024, 'c')};
    auto object = client.InsertObject(
        std::move(bucket_name), std::move(object_name), std::move(contents));
    // Attach a callback, this is called when the upload completes.
    auto done = object.then([](auto f) {
      auto metadata = f.get();
      if (!metadata) throw std::move(metadata).status();
      std::cerr << "Object successfully inserted " << *metadata << "\n";
    });
    // To simplify example, block until the operation completes.
    done.get();
  }
  //! [insert-object-vv]
  (client, argv.at(0), argv.at(1));
}

#if GOOGLE_CLOUD_CPP_HAVE_COROUTINES
void ReadObject(google::cloud::storage_experimental::AsyncClient& client,
                std::vector<std::string> const& argv) {
  //! [read-object]
  namespace gcs_ex = google::cloud::storage_experimental;
  auto coro =
      [](gcs_ex::AsyncClient& client, std::string bucket_name,
         std::string object_name) -> google::cloud::future<std::uint64_t> {
    auto [reader, token] = (co_await client.ReadObject(std::move(bucket_name),
                                                       std::move(object_name)))
                               .value();
    std::uint64_t count = 0;
    while (token.valid()) {
      auto [payload, t] = (co_await reader.Read(std::move(token))).value();
      token = std::move(t);
      for (auto const& buffer : payload.contents()) {
        count += std::count(buffer.begin(), buffer.end(), '\n');
      }
    }
    co_return count;
  };
  //! [read-object]
  // The example is easier to test and run if we call the coroutine and block
  // until it completes.
  auto const count = coro(client, argv.at(0), argv.at(1)).get();
  std::cout << "The object contains " << count << " lines\n";
}

void ReadObjectWithOptions(
    google::cloud::storage_experimental::AsyncClient& client,
    std::vector<std::string> const& argv) {
  //! [read-object-with-options]
  namespace gcs = google::cloud::storage;
  namespace gcs_ex = google::cloud::storage_experimental;
  auto coro =
      [](gcs_ex::AsyncClient& client, std::string bucket_name,
         std::string object_name,
         std::int64_t generation) -> google::cloud::future<std::uint64_t> {
    auto [reader, token] = (co_await client.ReadObject(
                                std::move(bucket_name), std::move(object_name),
                                gcs::Generation(generation)))
                               .value();
    std::uint64_t count = 0;
    while (token.valid()) {
      auto [payload, t] = (co_await reader.Read(std::move(token))).value();
      token = std::move(t);
      for (auto const& buffer : payload.contents()) {
        count += std::count(buffer.begin(), buffer.end(), '\n');
      }
    }
    co_return count;
  };
  //! [read-object-with-options]
  // The example is easier to test and run if we call the coroutine and block
  // until it completes.
  auto const count =
      coro(client, argv.at(0), argv.at(1), std::stoll(argv.at(2))).get();
  std::cout << "The object contains " << count << " lines\n";
}

#else
void ReadObject(google::cloud::storage_experimental::AsyncClient&,
                std::vector<std::string> const&) {
  std::cerr
      << "AsyncClient::ReadObjectStreaming() example requires coroutines\n";
}

void ReadObjectWithOptions(google::cloud::storage_experimental::AsyncClient&,
                           std::vector<std::string> const&) {
  std::cerr
      << "AsyncClient::ReadObjectStreaming() example requires coroutines\n";
}
#endif  // GOOGLE_CLOUD_CPP

void ComposeObject(google::cloud::storage_experimental::AsyncClient& client,
                   std::vector<std::string> const& argv) {
  //! [compose-object]
  namespace g = google::cloud;
  namespace gcs = g::storage;
  namespace gcs_ex = g::storage_experimental;
  [](gcs_ex::AsyncClient& client, std::string bucket_name,
     std::string object_name, std::string o1, std::string o2) {
    std::vector<gcs::ComposeSourceObject> sources;
    sources.push_back({std::move(o1), absl::nullopt, absl::nullopt});
    sources.push_back({std::move(o2), absl::nullopt, absl::nullopt});
    client
        .ComposeObject(std::move(bucket_name), std::move(sources),
                       std::move(object_name))
        .then([](auto f) {
          auto metadata = f.get();
          if (!metadata) throw std::move(metadata).status();
          std::cout << "Object successfully composed: " << *metadata << "\n";
        })
        .get();
  }
  //! [compose-object]
  (client, argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

// We would like to call this function `DeleteObject()`, but that conflicts with
// a global `DeleteObject()` function on Windows.
void AsyncDeleteObject(google::cloud::storage_experimental::AsyncClient& client,
                       std::vector<std::string> const& argv) {
  //! [delete-object]
  namespace g = google::cloud;
  namespace gcs_ex = google::cloud::storage_experimental;
  [](gcs_ex::AsyncClient& client, std::string bucket_name,
     std::string object_name) {
    client.DeleteObject(std::move(bucket_name), std::move(object_name))
        .then([](auto f) {
          auto status = f.get();
          if (!status.ok()) throw g::Status(std::move(status));
          std::cout << "Object successfully deleted\n";
        })
        .get();
  }
  //! [delete-object]
  (client, argv.at(0), argv.at(1));
}

void CreateClientCommand(std::vector<std::string> const& argv) {
  if (!argv.empty()) throw examples::Usage("create-client");
  CreateClient();
}

void CreateClientWithDPCommand(std::vector<std::string> const& argv) {
  if (!argv.empty()) throw examples::Usage("client-client-with-dp");
  CreateClientWithDP();
}

void AutoRun(std::vector<std::string> const& argv) {
  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME",
  });
  auto bucket_name = google::cloud::internal::GetEnv(
                         "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                         .value();
  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto const object_name = examples::MakeRandomObjectName(generator, "object-");
  auto const o1 = examples::MakeRandomObjectName(generator, "object-");
  auto const o2 = examples::MakeRandomObjectName(generator, "object-");

  std::cout << "Running AsyncClient() example" << std::endl;
  CreateClientCommand({});

  std::cout << "Running AsyncClientWithDP() example" << std::endl;
  CreateClientWithDPCommand({});

  auto client = google::cloud::storage_experimental::AsyncClient();

  std::cout << "Running InsertObject() example" << std::endl;
  InsertObject(client, {bucket_name, object_name});

  std::cout << "Running InsertObjectVectorString() example" << std::endl;
  InsertObjectVectorStrings(client, {bucket_name, object_name});

  std::cout << "Running InsertObjectVector() example" << std::endl;
  InsertObjectVector(client, {bucket_name, object_name});

  std::cout << "Running InsertObjectVectorVector() example" << std::endl;
  InsertObjectVectorVectors(client, {bucket_name, object_name});

  std::cout << "Running InsertObject() example [o1]" << std::endl;
  InsertObject(client, {bucket_name, o1});

  std::cout << "Running InsertObject() example [o2]" << std::endl;
  InsertObject(client, {bucket_name, o2});

  std::cout << "Running ComposeObject() example" << std::endl;
  ComposeObject(client, {bucket_name, object_name, o1, o2});

  std::cout << "Running the ReadObject() example" << std::endl;
  ReadObject(client, {bucket_name, object_name});

  std::cout << "Retrieving object metadata" << std::endl;
  auto response = client.ReadObjectRange(bucket_name, object_name, 0, 1).get();
  if (!response.ok()) throw std::move(response).status();

  auto const metadata = response->metadata();
  if (!metadata.has_value()) {
    std::cout << "Running the ReadObjectWithOptions() example" << std::endl;
    ReadObjectWithOptions(client, {bucket_name, object_name,
                                   std::to_string(metadata->generation())});
  }

  std::cout << "Running DeleteObject() example" << std::endl;
  AsyncDeleteObject(client, {bucket_name, object_name});

  namespace g = ::google::cloud;
  std::vector<g::future<g::Status>> pending;
  pending.push_back(client.DeleteObject(bucket_name, o1));
  pending.push_back(client.DeleteObject(bucket_name, o2));
  for (auto& f : pending) (void)f.get();
}

}  // namespace

int main(int argc, char* argv[]) try {
  using Command =
      std::function<void(google::cloud::storage_experimental::AsyncClient&,
                         std::vector<std::string>)>;
  auto make_entry =
      [](std::string const& name, std::vector<std::string> arg_names,
         Command const& command) -> examples::Commands::value_type {
    arg_names.insert(arg_names.begin(), "<object-name>");
    arg_names.insert(arg_names.begin(), "<bucket-name>");
    auto adapter = [=](std::vector<std::string> const& argv) {
      if (argv.size() != arg_names.size() ||
          (!argv.empty() && argv[0] == "--help")) {
        std::ostringstream os;
        os << name;
        for (auto const& a : arg_names) {
          os << " " << a;
        }
        throw examples::Usage{std::move(os).str()};
      }
      auto client = google::cloud::storage_experimental::AsyncClient();
      command(client, argv);
    };
    return {name, std::move(adapter)};
  };

  examples::Example example({
      {"create-client", CreateClientCommand},
      {"create-client-with-dp", CreateClientWithDPCommand},
      make_entry("insert-object", {}, InsertObject),
      make_entry("insert-object-vector", {}, InsertObjectVector),
      make_entry("insert-object-vector-strings", {}, InsertObjectVectorStrings),
      make_entry("insert-object-vector-vectors", {}, InsertObjectVectorVectors),
      make_entry("read-object", {}, ReadObject),
      make_entry("read-object-with-options", {"<generation>"},
                 ReadObjectWithOptions),
      make_entry("compose-object", {"<o1> <o2>"}, ComposeObject),
      make_entry("delete-object", {}, AsyncDeleteObject),
      {"auto", AutoRun},
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

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
