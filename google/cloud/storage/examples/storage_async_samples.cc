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
#include "google/cloud/storage/async_client.h"

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
  auto client = google::cloud::storage_experimental::MakeAsyncClient();
  // Use the client.
  //! [async-client]
}

//! [async-client-with-dp]
void CreateClientWithDP() {
  namespace g = ::google::cloud;
  auto client = google::cloud::storage_experimental::MakeAsyncClient(
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
                            std::string("Hello World!"));
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

void DeleteObject(google::cloud::storage_experimental::AsyncClient& client,
                  std::vector<std::string> const& argv) {
  //! [delete-object]
  namespace g = google::cloud;
  namespace gcs_ex = google::cloud::storage_experimental;
  [](gcs_ex::AsyncClient& client, std::string const& bucket_name,
     std::string const& object_name) {
    client.DeleteObject(bucket_name, object_name)
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

  std::cout << "Running AsyncClient() example" << std::endl;
  CreateClientCommand({});

  std::cout << "Running AsyncClientWithDP() example" << std::endl;
  CreateClientWithDPCommand({});

  auto client = google::cloud::storage_experimental::MakeAsyncClient();

  std::cout << "Running InsertObject() example" << std::endl;
  InsertObject(client, {bucket_name, object_name});

  std::cout << "Running InsertObjectVectorString() example" << std::endl;
  InsertObjectVectorStrings(client, {bucket_name, object_name});

  std::cout << "Running InsertObjectVector() example" << std::endl;
  InsertObjectVector(client, {bucket_name, object_name});

  std::cout << "Running InsertObjectVectorVector() example" << std::endl;
  InsertObjectVectorVectors(client, {bucket_name, object_name});

  std::cout << "Running DeleteObject() example" << std::endl;
  DeleteObject(client, {bucket_name, object_name});
}

}  // namespace

int main(int argc, char* argv[]) {
  using Command =
      std::function<void(google::cloud::storage_experimental::AsyncClient&,
                         std::vector<std::string>)>;
  auto make_entry =
      [](std::string const& name, std::vector<std::string> arg_names,
         Command const& command) -> examples::Commands::value_type {
    arg_names.insert(arg_names.begin(), "<bucket-name>");
    arg_names.insert(arg_names.begin(), "<object-name>");
    auto adapter = [=](std::vector<std::string> const& argv) {
      if (argv.size() != 2 || (argv.size() == 2 && argv[0] == "--help")) {
        std::ostringstream os;
        os << name;
        for (auto const& a : arg_names) {
          os << " " << a;
        }
        throw examples::Usage{std::move(os).str()};
      }
      auto client = google::cloud::storage_experimental::MakeAsyncClient();
      command(client, std::move(argv));
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
      make_entry("delete-object", {}, DeleteObject),
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}

#else

int main() { return 0; }

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
