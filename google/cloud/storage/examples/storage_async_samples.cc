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
#include <algorithm>
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

void StartBufferedUpload(
    google::cloud::storage_experimental::AsyncClient& client,
    std::vector<std::string> const& argv) {
  //! [start-buffered-upload]
  namespace gcs = google::cloud::storage;
  namespace gcs_ex = google::cloud::storage_experimental;
  auto coro = [](gcs_ex::AsyncClient& client, std::string bucket_name,
                 std::string object_name)
      -> google::cloud::future<gcs::ObjectMetadata> {
    auto [writer, token] = (co_await client.StartBufferedUpload(
                                gcs_ex::BucketName(std::move(bucket_name)),
                                std::move(object_name)))
                               .value();
    for (int i = 0; i != 1000; ++i) {
      auto line = gcs_ex::WritePayload(std::vector<std::string>{
          std::string("line number "), std::to_string(i), std::string("\n")});
      token =
          (co_await writer.Write(std::move(token), std::move(line))).value();
    }
    co_return (co_await writer.Finalize(std::move(token))).value();
  };
  //! [start-buffered-upload]
  // The example is easier to test and run if we call the coroutine and block
  // until it completes.
  auto const metadata = coro(client, argv.at(0), argv.at(1)).get();
  std::cout << "Object successfully uploaded " << metadata << "\n";
}

std::string SuspendBufferedUpload(
    google::cloud::storage_experimental::AsyncClient& client,
    std::vector<std::string> const& argv) {
  //! [suspend-buffered-upload]
  namespace gcs = google::cloud::storage;
  namespace gcs_ex = google::cloud::storage_experimental;
  auto coro =
      [](gcs_ex::AsyncClient& client, std::string bucket_name,
         std::string object_name) -> google::cloud::future<std::string> {
    // Use the overload consuming
    // `google::storage::v2::StartResumableWriteRequest` and show how to set
    // additional parameters in the request.
    auto request = google::storage::v2::StartResumableWriteRequest{};
    auto& spec = *request.mutable_write_object_spec();
    spec.mutable_resource()->set_bucket(
        gcs_ex::BucketName(bucket_name).FullName());
    spec.mutable_resource()->set_name(std::move(object_name));
    spec.mutable_resource()->mutable_metadata()->emplace("custom-field",
                                                         "example");
    spec.mutable_resource()->set_content_type("text/plain");
    spec.set_if_generation_match(0);
    auto [writer, token] =
        (co_await client.StartBufferedUpload(std::move(request))).value();
    // This example does not finalize the upload, so it can be resumed in a
    // separate example.
    co_return writer.UploadId();
  };
  //! [suspend-buffered-upload]
  // The example is easier to test and run if we call the coroutine and block
  // until it completes.
  auto upload_id = coro(client, argv.at(0), argv.at(1)).get();
  std::cout << "Object upload successfully created " << upload_id << "\n";
  return upload_id;
}

void ResumeBufferedUpload(
    google::cloud::storage_experimental::AsyncClient& client,
    std::vector<std::string> const& argv) {
  //! [resume-buffered-upload]
  namespace gcs = google::cloud::storage;
  namespace gcs_ex = google::cloud::storage_experimental;
  auto coro =
      [](gcs_ex::AsyncClient& client,
         std::string upload_id) -> google::cloud::future<gcs::ObjectMetadata> {
    auto [writer, token] =
        (co_await client.ResumeBufferedUpload(std::move(upload_id))).value();
    auto state = writer.PersistedState();
    if (std::holds_alternative<gcs::ObjectMetadata>(state)) {
      std::cout << "The upload " << writer.UploadId()
                << " was already finalized\n";
      co_return std::get<gcs::ObjectMetadata>(std::move(state));
    }
    auto persisted_bytes = std::get<std::int64_t>(state);
    if (persisted_bytes != 0) {
      // This example naively assumes it will resume from the beginning of the
      // object. Applications should be prepared to handle partially uploaded
      // objects.
      throw std::invalid_argument("example cannot resume after partial upload");
    }
    for (int i = 0; i != 1000; ++i) {
      auto line = gcs_ex::WritePayload(std::vector<std::string>{
          std::string("line number "), std::to_string(i), std::string("\n")});
      token =
          (co_await writer.Write(std::move(token), std::move(line))).value();
    }
    co_return (co_await writer.Finalize(std::move(token))).value();
  };
  //! [resume-buffered-upload]
  // The example is easier to test and run if we call the coroutine and block
  // until it completes.
  auto const metadata = coro(client, argv.at(0)).get();
  std::cout << "Object successfully uploaded " << metadata << "\n";
}

void StartUnbufferedUpload(
    google::cloud::storage_experimental::AsyncClient& client,
    std::vector<std::string> const& argv) {
  //! [start-unbuffered-upload]
  namespace gcs = google::cloud::storage;
  namespace gcs_ex = google::cloud::storage_experimental;
  auto coro = [](gcs_ex::AsyncClient& client, std::string bucket_name,
                 std::string object_name, std::string const& filename)
      -> google::cloud::future<gcs::ObjectMetadata> {
    std::ifstream is(filename);
    if (is.bad()) throw std::runtime_error("Cannot read " + filename);

    auto [writer, token] = (co_await client.StartUnbufferedUpload(
                                gcs_ex::BucketName(std::move(bucket_name)),
                                std::move(object_name)))
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
  //! [start-unbuffered-upload]
  // The example is easier to test and run if we call the coroutine and block
  // until it completes..
  auto const metadata = coro(client, argv.at(0), argv.at(1), argv.at(2)).get();
  std::cout << "File successfully uploaded " << metadata << "\n";
}

std::string SuspendUnbufferedUpload(
    google::cloud::storage_experimental::AsyncClient& client,
    std::vector<std::string> const& argv) {
  //! [suspend-unbuffered-upload]
  namespace gcs = google::cloud::storage;
  namespace gcs_ex = google::cloud::storage_experimental;
  auto coro =
      [](gcs_ex::AsyncClient& client, std::string bucket_name,
         std::string object_name,
         std::string const& filename) -> google::cloud::future<std::string> {
    std::ifstream is(filename);
    if (is.bad()) throw std::runtime_error("Cannot read " + filename);

    // Use the overload consuming
    // `google::storage::v2::StartResumableWriteRequest` and show how to set
    // additional parameters in the request.
    auto request = google::storage::v2::StartResumableWriteRequest{};
    auto& spec = *request.mutable_write_object_spec();
    spec.mutable_resource()->set_bucket(
        gcs_ex::BucketName(bucket_name).FullName());
    spec.mutable_resource()->set_name(std::move(object_name));
    spec.mutable_resource()->mutable_metadata()->emplace("custom-field",
                                                         "example");
    spec.mutable_resource()->set_content_type("text/plain");
    spec.set_if_generation_match(0);  // Create the object if it does not exist
    auto [writer, token] =
        (co_await client.StartUnbufferedUpload(std::move(request))).value();

    // Write some data and then return. That data may or may not be received
    // and persisted by the service.
    std::vector<char> buffer(1024 * 1024);
    is.read(buffer.data(), buffer.size());
    buffer.resize(is.gcount());
    token = (co_await writer.Write(std::move(token),
                                   gcs_ex::WritePayload(std::move(buffer))))
                .value();

    // This example does not finalize the upload, so it can be resumed in a
    // separate example.
    co_return writer.UploadId();
  };
  //! [suspend-unbuffered-upload]
  // The example is easier to test and run if we call the coroutine and block
  // until it completes.
  auto upload_id = coro(client, argv.at(0), argv.at(1), argv.at(2)).get();
  std::cout << "Object upload successfully created " << upload_id << "\n";
  return upload_id;
}

void ResumeUnbufferedUpload(
    google::cloud::storage_experimental::AsyncClient& client,
    std::vector<std::string> const& argv) {
  //! [resume-unbuffered-upload]
  namespace gcs = google::cloud::storage;
  namespace gcs_ex = google::cloud::storage_experimental;
  auto coro =
      [](gcs_ex::AsyncClient& client, std::string upload_id,
         std::string filename) -> google::cloud::future<gcs::ObjectMetadata> {
    std::ifstream is(filename);
    if (is.bad()) throw std::runtime_error("Cannot read " + filename);
    auto [writer, token] =
        (co_await client.ResumeUnbufferedUpload(std::move(upload_id))).value();

    auto state = writer.PersistedState();
    if (std::holds_alternative<gcs::ObjectMetadata>(state)) {
      std::cout << "The upload " << writer.UploadId()
                << " was already finalized\n";
      co_return std::get<gcs::ObjectMetadata>(std::move(state));
    }

    auto persisted_bytes = std::get<std::int64_t>(state);
    is.seekg(persisted_bytes);
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
  //! [resume-unbuffered-upload]
  // The example is easier to test and run if we call the coroutine and block
  // until it completes.
  auto const metadata = coro(client, argv.at(0), argv.at(1)).get();
  std::cout << "Object successfully uploaded " << metadata << "\n";
}

void RewriteObject(google::cloud::storage_experimental::AsyncClient& client,
                   std::vector<std::string> const& argv) {
  //! [rewrite-object]
  namespace g = google::cloud;
  namespace gcs = g::storage;
  namespace gcs_ex = g::storage_experimental;
  auto coro = [](gcs_ex::AsyncClient& client, std::string bucket_name,
                 std::string object_name, std::string destination_name)
      -> g::future<google::storage::v2::Object> {
    auto [rewriter, token] =
        client.StartRewrite(gcs_ex::BucketName(bucket_name), object_name,
                            gcs_ex::BucketName(bucket_name), destination_name);
    while (token.valid()) {
      auto [progress, t] =
          (co_await rewriter.Iterate(std::move(token))).value();
      token = std::move(t);
      std::cout << progress.total_bytes_rewritten() << " of "
                << progress.object_size() << " bytes rewritten\n";
      if (progress.has_resource()) co_return std::move(progress.resource());
    }
    throw std::runtime_error("rewrite failed before completion");
  };
  //! [rewrite-object]

  // The example is easier to test and run if we call the coroutine and block
  // until it completes.
  auto const object = coro(client, argv.at(0), argv.at(1), argv.at(2)).get();
  std::cout << "Object successfully rewritten " << object.DebugString() << "\n";
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
    // rewritten by each iteration, then capture the token, and then resume the
    // rewrite operation.
    auto bucket = gcs_ex::BucketName(std::move(bucket_name));
    auto request = google::storage::v2::RewriteObjectRequest{};
    request.set_destination_name(destination_name);
    request.set_destination_bucket(bucket.FullName());
    request.set_source_object(std::move(object_name));
    request.set_source_bucket(bucket.FullName());
    request.set_max_bytes_rewritten_per_call(1024 * 1024);
    auto [rewriter, token] = client.StartRewrite(std::move(request));
    auto [progress, t] = (co_await rewriter.Iterate(std::move(token))).value();
    co_return progress.rewrite_token();
  };
  auto resume =
      [](gcs_ex::AsyncClient& client, std::string bucket_name,
         std::string object_name, std::string destination_name,
         std::string rewrite_token) -> g::future<google::storage::v2::Object> {
    // Continue rewriting, this could happen on a separate process, or even
    // after the application restarts.
    auto bucket = gcs_ex::BucketName(std::move(bucket_name));
    auto request = google::storage::v2::RewriteObjectRequest();
    request.set_destination_bucket(bucket.FullName());
    request.set_destination_name(std::move(destination_name));
    request.set_source_bucket(bucket.FullName());
    request.set_source_object(std::move(object_name));
    request.set_rewrite_token(std::move(rewrite_token));
    request.set_max_bytes_rewritten_per_call(1024 * 1024);
    auto [rewriter, token] = client.ResumeRewrite(std::move(request));
    while (token.valid()) {
      auto [progress, t] =
          (co_await rewriter.Iterate(std::move(token))).value();
      token = std::move(t);
      std::cout << progress.total_bytes_rewritten() << " of "
                << progress.object_size() << " bytes rewritten\n";
      if (progress.has_resource()) co_return progress.resource();
    }
    throw std::runtime_error("rewrite failed before completion");
  };
  //! [resume-rewrite]

  // The example is easier to test and run if we call the coroutine and block
  // until it completes.
  auto const rt = start(client, argv.at(0), argv.at(1), argv.at(2)).get();
  auto const object =
      resume(client, argv.at(0), argv.at(1), argv.at(2), rt).get();
  std::cout << "Object successfully rewritten " << object.DebugString() << "\n";
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

void StartBufferedUpload(google::cloud::storage_experimental::AsyncClient&,
                         std::vector<std::string> const&) {
  std::cerr
      << "AsyncClient::StartBufferedUpload() example requires coroutines\n";
}

std::string SuspendBufferedUpload(
    google::cloud::storage_experimental::AsyncClient&,
    std::vector<std::string> const&) {
  std::cerr
      << "AsyncClient::StartBufferedUpload() example requires coroutines\n";
  return {};
}

void ResumeBufferedUpload(google::cloud::storage_experimental::AsyncClient&,
                          std::vector<std::string> const&) {
  std::cerr
      << "AsyncClient::ResumeBufferedUpload() example requires coroutines\n";
}

void StartUnbufferedUpload(google::cloud::storage_experimental::AsyncClient&,
                           std::vector<std::string> const&) {
  std::cerr
      << "AsyncClient::StartUnbufferedUpload() example requires coroutines\n";
}

std::string SuspendUnbufferedUpload(
    google::cloud::storage_experimental::AsyncClient&,
    std::vector<std::string> const&) {
  std::cerr
      << "AsyncClient::StartUnbufferedUpload() example requires coroutines\n";
  return {};
}

void ResumeUnbufferedUpload(google::cloud::storage_experimental::AsyncClient&,
                            std::vector<std::string> const&) {
  std::cerr
      << "AsyncClient::ResumeUnbufferedUpload() example requires coroutines\n";
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
  namespace gcs_ex = g::storage_experimental;
  [](gcs_ex::AsyncClient& client, std::string bucket_name,
     std::string object_name, std::string name1, std::string name2) {
    auto make_source = [](std::string name) {
      google::storage::v2::ComposeObjectRequest::SourceObject source;
      source.set_name(std::move(name));
      return source;
    };
    client
        .ComposeObject(
            gcs_ex::BucketName(std::move(bucket_name)), std::move(object_name),
            {make_source(std::move(name1)), make_source(std::move(name2))})
        .then([](auto f) {
          auto metadata = f.get();
          if (!metadata) throw std::move(metadata).status();
          std::cout << "Object successfully composed: "
                    << metadata->DebugString() << "\n";
        })
        .get();
  }
  //! [compose-object]
  (client, argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void ComposeObjectRequest(
    google::cloud::storage_experimental::AsyncClient& client,
    std::vector<std::string> const& argv) {
  //! [compose-object-request]
  namespace g = google::cloud;
  namespace gcs_ex = g::storage_experimental;
  [](gcs_ex::AsyncClient& client, std::string bucket_name,
     std::string object_name, std::string name1, std::string name2) {
    google::storage::v2::ComposeObjectRequest request;
    request.mutable_destination()->set_bucket(
        gcs_ex::BucketName(std::move(bucket_name)).FullName());
    request.mutable_destination()->set_name(std::move(object_name));
    // Only create the destination object if it does not already exist.
    request.set_if_generation_match(0);
    request.add_source_objects()->set_name(std::move(name1));
    request.add_source_objects()->set_name(std::move(name2));

    client.ComposeObject(std::move(request))
        .then([](auto f) {
          auto metadata = f.get();
          if (!metadata) throw std::move(metadata).status();
          std::cout << "Object successfully composed: "
                    << metadata->DebugString() << "\n";
        })
        .get();
  }
  //! [compose-object-request]
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
    client
        .DeleteObject(gcs_ex::BucketName(std::move(bucket_name)),
                      std::move(object_name))
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
  auto const filename = MakeRandomFilename(generator);
  std::vector<std::string> scheduled_for_delete;

  std::cout << "Running AsyncClient() example" << std::endl;
  CreateClientCommand({});

  std::cout << "Running AsyncClientWithDP() example" << std::endl;
  CreateClientWithDPCommand({});

  auto client = google::cloud::storage_experimental::AsyncClient();

  // We need different object names because writing to the same object within
  // a second exceeds the service's quota.
  auto object_name = examples::MakeRandomObjectName(generator, "object-");

  std::cout << "Running InsertObject() example" << std::endl;
  InsertObject(client, {bucket_name, object_name});
  scheduled_for_delete.push_back(std::move(object_name));
  object_name = examples::MakeRandomObjectName(generator, "object-");

  std::cout << "Running InsertObjectVectorString() example" << std::endl;
  InsertObjectVectorStrings(client, {bucket_name, object_name});
  scheduled_for_delete.push_back(std::move(object_name));
  object_name = examples::MakeRandomObjectName(generator, "object-");

  std::cout << "Running InsertObjectVector() example" << std::endl;
  InsertObjectVector(client, {bucket_name, object_name});
  scheduled_for_delete.push_back(std::move(object_name));
  object_name = examples::MakeRandomObjectName(generator, "object-");

  std::cout << "Running InsertObjectVectorVector() example" << std::endl;
  InsertObjectVectorVectors(client, {bucket_name, object_name});
  scheduled_for_delete.push_back(std::move(object_name));
  object_name = examples::MakeRandomObjectName(generator, "object-");

  std::cout << "Running InsertObject() example [o1]" << std::endl;
  auto const o1 = object_name;
  InsertObject(client, {bucket_name, o1});
  scheduled_for_delete.push_back(std::move(object_name));
  object_name = examples::MakeRandomObjectName(generator, "object-");

  std::cout << "Running InsertObject() example [o2]" << std::endl;
  auto const o2 = object_name;
  InsertObject(client, {bucket_name, o2});
  scheduled_for_delete.push_back(std::move(object_name));
  object_name = examples::MakeRandomObjectName(generator, "object-");

  std::cout << "Running ComposeObject() example" << std::endl;
  auto const composed_name = object_name;
  ComposeObject(client, {bucket_name, object_name, o1, o2});
  scheduled_for_delete.push_back(std::move(object_name));
  object_name = examples::MakeRandomObjectName(generator, "object-");

  std::cout << "Running the ReadObject() example" << std::endl;
  ReadObject(client, {bucket_name, composed_name});

  std::cout << "Retrieving object metadata" << std::endl;
  auto response =
      client.ReadObjectRange(bucket_name, composed_name, 0, 1).get();
  if (!response.ok()) throw std::move(response).status();

  auto const metadata = response->metadata();
  if (!metadata.has_value()) {
    std::cout << "Running the ReadObjectWithOptions() example" << std::endl;
    ReadObjectWithOptions(client, {bucket_name, metadata->name(),
                                   std::to_string(metadata->generation())});
  }

  if (!examples::UsingEmulator()) {
    std::cout << "Creating file for uploads" << std::endl;
    std::ofstream of(filename);
    for (int i = 0; i != 100'000; ++i) {
      of << i << ": Some text\n";
    }
    of.close();

    std::cout << "Running the StartBufferedUpload() example" << std::endl;
    StartBufferedUpload(client, {bucket_name, object_name});
    scheduled_for_delete.push_back(std::move(object_name));
    object_name = examples::MakeRandomObjectName(generator, "object-");

    std::cout << "Running the SuspendBufferedUpload() example" << std::endl;
    auto upload_id = SuspendBufferedUpload(client, {bucket_name, object_name});

    std::cout << "Running the ResumeBufferedUpload() example" << std::endl;
    ResumeUnbufferedUpload(client, {upload_id});
    scheduled_for_delete.push_back(std::move(object_name));
    object_name = examples::MakeRandomObjectName(generator, "object-");

    std::cout << "Running the StartUnbufferedUpload() example" << std::endl;
    StartUnbufferedUpload(client, {bucket_name, object_name, filename});
    scheduled_for_delete.push_back(std::move(object_name));
    object_name = examples::MakeRandomObjectName(generator, "object-");

    std::cout << "Running the SuspendUnbufferedUpload() example" << std::endl;
    upload_id =
        SuspendUnbufferedUpload(client, {bucket_name, object_name, filename});

    std::cout << "Running the ResumeUnbufferedUpload() example" << std::endl;
    ResumeUnbufferedUpload(client, {upload_id, filename});
    scheduled_for_delete.push_back(std::move(object_name));
    object_name = examples::MakeRandomObjectName(generator, "object-");

    std::cout << "Removing local file" << std::endl;
    (void)std::remove(filename.c_str());
  }

  std::cout << "Running the RewriteObject() example" << std::endl;
  RewriteObject(client, {bucket_name, composed_name, object_name});
  scheduled_for_delete.push_back(std::move(object_name));
  object_name = examples::MakeRandomObjectName(generator, "object-");

  std::cout << "Running the ResumeRewrite() example" << std::endl;
  auto const rewrite_source = object_name;
  (void)client
      .InsertObject(bucket_name, object_name, std::string(4 * 1024 * 1024, 'A'))
      .get()
      .value();
  scheduled_for_delete.push_back(std::move(object_name));
  object_name = examples::MakeRandomObjectName(generator, "object-");

  auto const dest = examples::MakeRandomObjectName(generator, "object-");
  ResumeRewrite(client, {bucket_name, rewrite_source, object_name});
  scheduled_for_delete.push_back(std::move(object_name));
  object_name = examples::MakeRandomObjectName(generator, "object-");

  std::cout << "Running ComposeObjectRequest() example" << std::endl;
  auto const to_delete = object_name;
  ComposeObjectRequest(client, {bucket_name, object_name, o1, o2});
  scheduled_for_delete.push_back(std::move(object_name));
  object_name = examples::MakeRandomObjectName(generator, "object-");

  std::cout << "Running DeleteObject() example" << std::endl;
  AsyncDeleteObject(client, {bucket_name, to_delete});
  scheduled_for_delete.push_back(std::move(object_name));
  object_name = examples::MakeRandomObjectName(generator, "object-");

  auto bucket = google::cloud::storage_experimental::BucketName(bucket_name);
  namespace g = ::google::cloud;
  std::vector<g::future<g::Status>> pending(scheduled_for_delete.size());
  std::transform(scheduled_for_delete.begin(), scheduled_for_delete.end(),
                 pending.begin(), [&client, &bucket](auto object_name) {
                   return client.DeleteObject(bucket, std::move(object_name));
                 });
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

  auto make_resume_entry =
      [](std::string const& name, std::vector<std::string> arg_names,
         Command const& command) -> examples::Commands::value_type {
    arg_names.insert(arg_names.begin(), "<upload-id>");
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
      make_entry("compose-object-request", {"<o1> <o2>"}, ComposeObjectRequest),
      make_entry("delete-object", {}, AsyncDeleteObject),

      make_entry("buffered-upload", {}, StartBufferedUpload),
      make_entry("suspend-buffered-upload", {}, SuspendBufferedUpload),
      make_resume_entry("resume-buffered-upload", {}, ResumeBufferedUpload),

      make_entry("start-unbuffered-upload", {"<filename>"},
                 StartBufferedUpload),
      make_entry("suspend-unbuffered-upload", {}, SuspendUnbufferedUpload),
      make_resume_entry("resume-unbuffered-upload", {"<filename>"},
                        ResumeUnbufferedUpload),

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
