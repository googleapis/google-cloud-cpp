// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/examples/storage_examples_common.h"
#include "google/cloud/internal/getenv.h"
#include <functional>
#include <iostream>
#include <map>

namespace {

void RewriteObject(google::cloud::storage::Client client,
                   std::vector<std::string> const& argv) {
  //! [rewrite object]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string source_bucket_name,
     std::string source_object_name, std::string destination_bucket_name,
     std::string destination_object_name) {
    StatusOr<gcs::ObjectMetadata> object_metadata =
        client.RewriteObjectBlocking(source_bucket_name, source_object_name,
                                     destination_bucket_name,
                                     destination_object_name);

    if (!object_metadata) {
      throw std::runtime_error(object_metadata.status().message());
    }

    std::cout << "Rewrote object " << destination_object_name
              << " Metadata: " << *object_metadata << "\n";
  }
  //! [rewrite object]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void RewriteObjectNonBlocking(google::cloud::storage::Client client,
                              std::vector<std::string> const& argv) {
  //! [rewrite object non blocking]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string source_bucket_name,
     std::string source_object_name, std::string destination_bucket_name,
     std::string destination_object_name) {
    gcs::ObjectRewriter rewriter =
        client.RewriteObject(source_bucket_name, source_object_name,
                             destination_bucket_name, destination_object_name);

    StatusOr<gcs::ObjectMetadata> object_metadata =
        rewriter.ResultWithProgressCallback(
            [](StatusOr<gcs::RewriteProgress> const& progress) {
              if (!progress) {
                throw std::runtime_error(progress.status().message());
              }
              std::cout << "Rewrote " << progress->total_bytes_rewritten << "/"
                        << progress->object_size << "\n";
            });

    if (!object_metadata) {
      // Won't happen if we throw on error from the callback.
      throw std::runtime_error(object_metadata.status().message());
    }

    std::cout << "Rewrote object " << object_metadata->name() << " in bucket "
              << object_metadata->bucket()
              << "\nFull Metadata: " << *object_metadata << "\n";
  }
  //! [rewrite object non blocking]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void RewriteObjectToken(google::cloud::storage::Client client,
                        std::vector<std::string> const& argv) {
  //! [rewrite object token]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string source_bucket_name,
     std::string source_object_name, std::string destination_bucket_name,
     std::string destination_object_name) {
    gcs::ObjectRewriter rewriter = client.RewriteObject(
        source_bucket_name, source_object_name, destination_bucket_name,
        destination_object_name, gcs::MaxBytesRewrittenPerCall(1024 * 1024));

    StatusOr<gcs::RewriteProgress> progress = rewriter.Iterate();

    if (!progress) throw std::runtime_error(progress.status().message());
    if (progress->done) {
      std::cout
          << "The rewrite completed immediately, no token to resume later\n";
      return;
    }
    std::cout << "Rewrite in progress, token " << rewriter.token() << "\n";
  }
  //! [rewrite object token]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void RewriteObjectResume(google::cloud::storage::Client client,
                         std::vector<std::string> const& argv) {
  //! [rewrite object resume]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string source_bucket_name,
     std::string source_object_name, std::string destination_bucket_name,
     std::string destination_object_name, std::string rewrite_token) {
    gcs::ObjectRewriter rewriter = client.ResumeRewriteObject(
        source_bucket_name, source_object_name, destination_bucket_name,
        destination_object_name, rewrite_token,
        gcs::MaxBytesRewrittenPerCall(1024 * 1024));

    StatusOr<gcs::ObjectMetadata> object_metadata =
        rewriter.ResultWithProgressCallback(
            [](StatusOr<gcs::RewriteProgress> const& progress) {
              if (!progress) {
                throw std::runtime_error(progress.status().message());
              }
              std::cout << "Rewrote " << progress->total_bytes_rewritten << "/"
                        << progress->object_size << "\n";
            });

    if (!object_metadata) {
      // Won't happen if we throw on error from the callback.
      throw std::runtime_error(object_metadata.status().message());
    }

    std::cout << "Rewrote object " << object_metadata->name() << " in bucket "
              << object_metadata->bucket()
              << "\nFull Metadata: " << *object_metadata << "\n";
  }
  //! [rewrite object resume]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3),
   argv.at(4));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  namespace gcs = ::google::cloud::storage;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME",
      "GOOGLE_CLOUD_CPP_STORAGE_TEST_DESTINATION_BUCKET_NAME",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const bucket_name = google::cloud::internal::GetEnv(
                               "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                               .value();
  auto const destination_bucket_name =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_STORAGE_TEST_DESTINATION_BUCKET_NAME")
          .value();

  auto client = gcs::Client::CreateDefaultClient().value();
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const src_object_name =
      examples::MakeRandomObjectName(generator, "object-") + ".txt";
  auto const dst_object_name =
      examples::MakeRandomObjectName(generator, "object-") + ".txt";
  auto const text = R"""(Some text to insert in the test objects.)""";

  auto src = client.InsertObject(bucket_name, src_object_name, text).value();

  std::cout << "\nRunning the RewriteObject() example" << std::endl;
  RewriteObject(client, {bucket_name, src_object_name, destination_bucket_name,
                         dst_object_name});

  std::cout << "\nRunning the RewriteObjectNonBlocking() example" << std::endl;
  RewriteObjectNonBlocking(client, {bucket_name, src_object_name,
                                    destination_bucket_name, dst_object_name});

  std::cout << "\nRunning the RewriteObjectToken() example" << std::endl;
  RewriteObjectToken(client, {bucket_name, src_object_name,
                              destination_bucket_name, dst_object_name});

  // Create a large object and obtain a token to rewrite to it.
  std::cout << "\nCreating large object to test rewrites" << std::endl;
  auto constexpr kRewriteBlock = 1024 * 1024L;
  auto constexpr kDesiredSize = 16 * kRewriteBlock;
  auto constexpr kLineSize = 256;
  static_assert(kDesiredSize % kLineSize == 0,
                "Desired size should be a multiple of the line size");
  auto constexpr kLineCount = kDesiredSize / kLineSize;
  auto const line =
      google::cloud::internal::Sample(generator, kLineSize - 1,
                                      "abcdefghijklmnopqrstuvwxyz0123456789") +
      "\n";
  auto writer = client.WriteObject(bucket_name, src_object_name);
  for (int i = 0; i != kLineCount; ++i) {
    writer.write(line.data(), line.size());
  }
  writer.Close();
  src = writer.metadata().value();
  std::cout << "Created an object with size = " << src.size() << "\n";

  std::cout << "\nStarting large object rewrite" << std::endl;
  auto rewriter = client.RewriteObject(
      bucket_name, src_object_name, destination_bucket_name, dst_object_name,
      gcs::MaxBytesRewrittenPerCall(kRewriteBlock));
  auto progress = rewriter.Iterate().value();
  if (progress.done) throw std::runtime_error("Rewrite completed unexpectedly");

  std::cout << "\nRunning the RewriteObjectResume() example" << std::endl;
  RewriteObjectResume(client,
                      {bucket_name, src_object_name, destination_bucket_name,
                       dst_object_name, rewriter.token()});

  (void)client.DeleteObject(destination_bucket_name, dst_object_name);
  (void)client.DeleteObject(bucket_name, src_object_name);
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  auto make_entry = [](std::string const& name,
                       std::vector<std::string> arg_names,
                       examples::ClientCommand const& cmd) {
    arg_names.insert(
        arg_names.begin(),
        {"<source-bucket-name>", "<source-object-name>",
         "<destination-bucket-name>", "<destination-object-name>"});
    return examples::CreateCommandEntry(name, std::move(arg_names), cmd);
  };
  google::cloud::storage::examples::Example example({
      make_entry("rewrite-object", {}, RewriteObject),
      make_entry("rewrite-object-non-blocking", {}, RewriteObjectNonBlocking),
      make_entry("rewrite-object-token", {}, RewriteObjectToken),
      make_entry("rewrite-object-resume", {"<token>"}, RewriteObjectResume),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
