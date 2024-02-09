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
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
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

void WriteObject(google::cloud::storage_experimental::AsyncClient& client,
                 std::vector<std::string> const& argv) {
  //! [write-object]
  namespace gcs = google::cloud::storage;
  namespace gcs_ex = google::cloud::storage_experimental;
  auto coro = [](gcs_ex::AsyncClient& client, std::string bucket_name,
                 std::string object_name, std::string const& filename)
      -> google::cloud::future<gcs::ObjectMetadata> {
    std::ifstream is(filename);
    if (is.bad()) throw std::runtime_error("Cannot read " + filename);
    auto [writer, token] = (co_await client.StartUnbufferedUpload(
                                std::move(bucket_name), std::move(object_name)))
                               .value();
    is.seekg(0);  // clear EOF bit
    while (token.valid() && !is.eof()) {
      std::vector<char> buffer(1024 * 1024);
      is.read(buffer.data(), buffer.size());
      buffer.resize(is.gcount());
      token = (co_await writer.Write(std::move(token),
                                     gcs_ex::WritePayload(std::move(buffer))))
                  .value();
    }
    co_return (co_await writer.Finalize(std::move(token))).value();
  };
  //! [write-object]
  // The example is easier to test and run if we call the coroutine and block
  // until it completes.
  auto const metadata = coro(client, argv.at(0), argv.at(1), argv.at(2)).get();
  std::cout << "File successfully uploaded " << metadata << "\n";
}

void WriteObjectWithRetry(
    google::cloud::storage_experimental::AsyncClient& client,
    std::vector<std::string> const& argv) {
  //! [write-object-with-retry]
  namespace gcs = google::cloud::storage;
  namespace gcs_ex = google::cloud::storage_experimental;
  // Make one attempt to upload `is` using `writer`:
  auto attempt =
      [](gcs_ex::AsyncWriter& writer, gcs_ex::AsyncToken& token,
         std::istream& is) -> google::cloud::future<google::cloud::Status> {
    while (token.valid() && !is.eof()) {
      std::vector<char> buffer(1024 * 1024);
      is.read(buffer.data(), buffer.size());
      buffer.resize(is.gcount());
      auto w = co_await writer.Write(std::move(token),
                                     gcs_ex::WritePayload(std::move(buffer)));
      if (!w) co_return std::move(w).status();
      token = *std::move(w);
    }
    co_return google::cloud::Status{};
  };
  // Make multiple attempts to upload the file contents, restarting from the
  // last checkpoint on (partial) failures.
  auto coro = [&attempt](
                  gcs_ex::AsyncClient& client, std::string const& bucket_name,
                  std::string const& object_name, std::string const& filename)
      -> google::cloud::future<gcs::ObjectMetadata> {
    std::ifstream is(filename);
    if (is.bad()) throw std::runtime_error("Cannot read " + filename);
    // The first attempt will create a resumable upload session.
    auto upload_id = gcs::UseResumableUploadSession();
    for (int i = 0; i != 3; ++i) {
      // Start or resume the upload.
      auto [writer, token] = (co_await client.StartUnbufferedUpload(
                                  bucket_name, object_name, upload_id))
                                 .value();
      // If the upload already completed, there is nothing left to do.
      auto state = writer.PersistedState();
      if (absl::holds_alternative<gcs::ObjectMetadata>(state)) {
        co_return absl::get<gcs::ObjectMetadata>(std::move(state));
      }
      // Refresh the upload id and reset the input source to the right offset.
      upload_id = gcs::UseResumableUploadSession(writer.UploadId());
      is.seekg(absl::get<std::int64_t>(std::move(state)));
      auto status = co_await attempt(writer, token, is);
      if (!status.ok()) continue;  // Error in upload, try again.
      auto f = co_await writer.Finalize(std::move(token));
      if (f) co_return *std::move(f);  // Return immediately on success.
    }
    throw std::runtime_error("Too many upload attempts");
  };
  //! [write-object-with-retry]
  // The example is easier to test and run if we call the coroutine and block
  // until it completes.
  auto const metadata = coro(client, argv.at(0), argv.at(1), argv.at(2)).get();
  std::cout << "File successfully uploaded " << metadata << "\n";
}

void StartBufferedUpload(
    google::cloud::storage_experimental::AsyncClient& client,
    std::vector<std::string> const& argv) {
  //! [buffered-upload]
  namespace gcs = google::cloud::storage;
  namespace gcs_ex = google::cloud::storage_experimental;
  auto coro = [](gcs_ex::AsyncClient& client, std::string bucket_name,
                 std::string object_name)
      -> google::cloud::future<gcs::ObjectMetadata> {
    auto generator = std::mt19937_64(std::random_device{}());
    auto const values = std::string("012345689");
    auto random_line = [&] {
      std::string buffer;
      std::generate_n(std::back_inserter(buffer), 64, [&generator, values]() {
        auto const n = values.size();
        return values.at(
            std::uniform_int_distribution<std::size_t>(n)(generator));
      });
      return gcs_ex::WritePayload(
          std::vector<std::string>{std::move(buffer), std::string("\n")});
    };

    auto [writer, token] = (co_await client.StartBufferedUpload(
                                std::move(bucket_name), std::move(object_name)))
                               .value();
    for (int i = 0; i != 1000; ++i) {
      token = (co_await writer.Write(std::move(token), random_line())).value();
    }
    co_return (co_await writer.Finalize(std::move(token))).value();
  };
  //! [buffered-upload]
  // The example is easier to test and run if we call the coroutine and block
  // until it completes.
  auto const metadata = coro(client, argv.at(0), argv.at(1)).get();
  std::cout << "File successfully uploaded " << metadata << "\n";
}

void RewriteObject(google::cloud::storage_experimental::AsyncClient& client,
                   std::vector<std::string> const& argv) {
  //! [rewrite-object]
  namespace g = google::cloud;
  namespace gcs = g::storage;
  namespace gcs_ex = g::storage_experimental;
  auto coro =
      [](gcs_ex::AsyncClient& client, std::string bucket_name,
         std::string object_name,
         std::string destination_name) -> g::future<gcs::ObjectMetadata> {
    auto [rewriter, token] = client.StartRewrite(bucket_name, object_name,
                                                 bucket_name, destination_name);
    while (token.valid()) {
      auto [progress, t] =
          (co_await rewriter.Iterate(std::move(token))).value();
      token = std::move(t);
      std::cout << progress.total_bytes_rewritten << " of "
                << progress.object_size << " bytes rewritten\n";
      if (progress.metadata) co_return *std::move(progress.metadata);
    }
    throw std::runtime_error("rewrite failed before completion");
  };
  //! [rewrite-object]

  // The example is easier to test and run if we call the coroutine and block
  // until it completes.
  auto const metadata = coro(client, argv.at(0), argv.at(1), argv.at(2)).get();
  std::cout << "Object successfully rewritten " << metadata << "\n";
}

void ResumeRewrite(google::cloud::storage_experimental::AsyncClient& client,
                   std::vector<std::string> const& argv) {
  //! [resume-rewrite]
  namespace g = google::cloud;
  namespace gcs = g::storage;
  namespace gcs_ex = g::storage_experimental;
  auto start = [](gcs_ex::AsyncClient& client, std::string bucket_name,
                  std::string object_name,
                  std::string destination_name) -> g::future<std::string> {
    // First start a rewrite. In this example we will limit the number of bytes
    // rewritten by each iteration to capture then token and resume the rewrite
    // later.
    auto [rewriter, token] = client.StartRewrite(
        bucket_name, object_name, bucket_name, destination_name,
        gcs::MaxBytesRewrittenPerCall(1024 * 1024));
    auto [progress, t] = (co_await rewriter.Iterate(std::move(token))).value();
    co_return progress.rewrite_token;
  };
  auto resume =
      [](gcs_ex::AsyncClient& client, std::string bucket_name,
         std::string object_name, std::string destination_name,
         std::string rewrite_token) -> g::future<gcs::ObjectMetadata> {
    // Continue rewriting, this could happen on a separate process, or even
    // after the application restarts.
    auto [rewriter, token] = client.ResumeRewrite(
        bucket_name, object_name, bucket_name, destination_name, rewrite_token,
        gcs::MaxBytesRewrittenPerCall(1024 * 1024));
    while (token.valid()) {
      auto [progress, t] =
          (co_await rewriter.Iterate(std::move(token))).value();
      token = std::move(t);
      std::cout << progress.total_bytes_rewritten << " of "
                << progress.object_size << " bytes rewritten\n";
      if (progress.metadata) co_return *std::move(progress.metadata);
    }
    throw std::runtime_error("rewrite failed before completion");
  };
  //! [resume-rewrite]

  // The example is easier to test and run if we call the coroutine and block
  // until it completes.
  auto const rt = start(client, argv.at(0), argv.at(1), argv.at(2)).get();
  auto const metadata =
      resume(client, argv.at(0), argv.at(1), argv.at(2), rt).get();
  std::cout << "Object successfully rewritten " << metadata << "\n";
}

#else
void ReadObject(google::cloud::storage_experimental::AsyncClient&,
                std::vector<std::string> const&) {
  std::cerr << "AsyncClient::ReadObject() example requires coroutines\n";
}

void ReadObjectWithOptions(google::cloud::storage_experimental::AsyncClient&,
                           std::vector<std::string> const&) {
  std::cerr << "AsyncClient::ReadObject() example requires coroutines\n";
}

void WriteObject(google::cloud::storage_experimental::AsyncClient&,
                 std::vector<std::string> const&) {
  std::cerr
      << "AsyncClient::StartUnbufferedUpload() example requires coroutines\n";
}

void WriteObjectWithRetry(google::cloud::storage_experimental::AsyncClient&,
                          std::vector<std::string> const&) {
  std::cerr
      << "AsyncClient::StartUnbufferedUpload() example requires coroutines\n";
}

void StartBufferedUpload(google::cloud::storage_experimental::AsyncClient&,
                         std::vector<std::string> const&) {
  std::cerr
      << "AsyncClient::StartBufferedUpload() example requires coroutines\n";
}

void RewriteObject(google::cloud::storage_experimental::AsyncClient&,
                   std::vector<std::string> const&) {
  std::cerr << "AsyncClient::RewriteObject() example requires coroutines\n";
}

void ResumeRewrite(google::cloud::storage_experimental::AsyncClient&,
                   std::vector<std::string> const&) {
  std::cerr << "AsyncClient::ResumeRewrite() example requires coroutines\n";
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

std::string MakeRandomFilename(
    google::cloud::internal::DefaultPRNG& generator) {
  auto constexpr kMaxBasenameLength = 28;
  std::string const prefix = "f-";
  return prefix +
         google::cloud::internal::Sample(
             generator, static_cast<int>(kMaxBasenameLength - prefix.size()),
             "abcdefghijklmnopqrstuvwxyz0123456789") +
         ".txt";
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
  // We need different object names because writing to the same object within
  // a second exceeds the service's quota.
  auto const o1 = examples::MakeRandomObjectName(generator, "object-");
  auto const o2 = examples::MakeRandomObjectName(generator, "object-");
  auto const o3 = examples::MakeRandomObjectName(generator, "object-");
  auto const o4 = examples::MakeRandomObjectName(generator, "object-");
  auto const o5 = examples::MakeRandomObjectName(generator, "object-");
  auto const filename = MakeRandomFilename(generator);

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

  if (!examples::UsingEmulator()) {
    std::cout << "Creating file for uploads" << std::endl;
    std::ofstream of(filename);
    for (int i = 0; i != 100'000; ++i) {
      of << i << ": Some text\n";
    }
    of.close();

    std::cout << "Running the WriteObject() example" << std::endl;
    WriteObject(client, {bucket_name, o3, filename});

    std::cout << "Running the WriteObjectWithRetry() example" << std::endl;
    WriteObjectWithRetry(client, {bucket_name, o4, filename});

    std::cout << "Running the MakeWriter() example" << std::endl;
    StartBufferedUpload(client, {bucket_name, o5});

    std::cout << "Removing local file" << std::endl;
    (void)std::remove(filename.c_str());
  }

  std::cout << "Running the RewriteObject() example" << std::endl;
  auto const o6 = examples::MakeRandomObjectName(generator, "object-");
  RewriteObject(client, {bucket_name, object_name, o6});

  std::cout << "Running the ResumeRewrite() example" << std::endl;
  auto const o7 = examples::MakeRandomObjectName(generator, "object-");
  (void)client.InsertObject(bucket_name, o7, std::string(4 * 1024 * 1024, 'A'))
      .get()
      .value();
  auto const o8 = examples::MakeRandomObjectName(generator, "object-");
  ResumeRewrite(client, {bucket_name, object_name, o8});

  std::cout << "Running DeleteObject() example" << std::endl;
  AsyncDeleteObject(client, {bucket_name, object_name});

  namespace g = ::google::cloud;
  std::vector<g::future<g::Status>> pending;
  pending.push_back(client.DeleteObject(bucket_name, o1));
  pending.push_back(client.DeleteObject(bucket_name, o2));
  pending.push_back(client.DeleteObject(bucket_name, o3));
  pending.push_back(client.DeleteObject(bucket_name, o4));
  pending.push_back(client.DeleteObject(bucket_name, o5));
  pending.push_back(client.DeleteObject(bucket_name, o6));
  pending.push_back(client.DeleteObject(bucket_name, o7));
  pending.push_back(client.DeleteObject(bucket_name, o8));
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
      make_entry("write-object", {"<filename>"}, WriteObject),
      make_entry("write-object-with-retry", {"<filename>"},
                 WriteObjectWithRetry),
      make_entry("buffered-upload", {}, StartBufferedUpload),
      make_entry("rewrite-object", {"<destination>"}, RewriteObject),
      make_entry("resume-rewrite-object", {"<destination>"}, ResumeRewrite),
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
