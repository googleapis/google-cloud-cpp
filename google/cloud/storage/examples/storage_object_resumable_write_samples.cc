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
#include "google/cloud/storage/parallel_upload.h"
#include "google/cloud/internal/getenv.h"
#include <iostream>
#include <map>
#include <string>
#include <thread>

namespace {
void StartResumableUpload(google::cloud::storage::Client client,
                          std::vector<std::string> const& argv) {
  //! [start resumable upload]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name) {
    gcs::ObjectWriteStream stream = client.WriteObject(
        bucket_name, object_name, gcs::NewResumableUploadSession());
    std::cout << "Created resumable upload: " << stream.resumable_session_id()
              << "\n";
    // As it is customary in C++, the destructor automatically closes the
    // stream, that would finish the upload and create the object. For this
    // example we want to restore the session as-if the application had crashed,
    // where no destructors get called.
    stream << "This data will not get uploaded, it is too small\n";
    std::move(stream).Suspend();
  }
  //! [start resumable upload]
  (std::move(client), argv.at(0), argv.at(1));
}

void ResumeResumableUpload(google::cloud::storage::Client client,
                           std::vector<std::string> const& argv) {
  //! [resume resumable upload]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::string const& session_id) {
    // Restore a resumable upload stream, the library automatically queries the
    // state of the upload and discovers the next expected byte.
    gcs::ObjectWriteStream stream =
        client.WriteObject(bucket_name, object_name,
                           gcs::RestoreResumableUploadSession(session_id));
    if (!stream.IsOpen() && stream.metadata().ok()) {
      std::cout << "The upload has already been finalized.  The object "
                << "metadata is: " << *stream.metadata() << "\n";
    }
    if (stream.next_expected_byte() == 0) {
      // In this example we create a small object, smaller than the resumable
      // upload quantum (256 KiB), so either all the data is there or not.
      // Applications use `next_expected_byte()` to find the position in their
      // input where they need to start uploading.
      stream << R"""(
Lorem ipsum dolor sit amet, consectetur adipiscing
elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea
commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit
esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat
non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.
)""";
    }

    stream.Close();

    StatusOr<gcs::ObjectMetadata> metadata = stream.metadata();
    if (!metadata) throw std::runtime_error(metadata.status().message());
    std::cout << "Upload completed, the new object metadata is: " << *metadata
              << "\n";
  }
  //! [resume resumable upload]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void DeleteResumableUpload(google::cloud::storage::Client client,
                           std::vector<std::string> const& argv) {
  //! [delete resumable upload]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name) {
    gcs::ObjectWriteStream stream = client.WriteObject(
        bucket_name, object_name, gcs::NewResumableUploadSession());
    std::cout << "Created resumable upload: " << stream.resumable_session_id()
              << "\n";

    auto status = client.DeleteResumableUpload(stream.resumable_session_id());
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Deleted resumable upload: " << stream.resumable_session_id()
              << "\n";

    stream.Close();
  }
  //! [delete resumable upload]
  (std::move(client), argv.at(0), argv.at(1));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  namespace gcs = ::google::cloud::storage;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const bucket_name = google::cloud::internal::GetEnv(
                               "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                               .value();
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const object_name =
      examples::MakeRandomObjectName(generator, "ob-resumable-upload-");

  auto client = gcs::Client();

  std::cout << "\nRunning StartResumableUpload() example" << std::endl;
  StartResumableUpload(client, {bucket_name, object_name});

  std::cout << "\nCreating and capturing new resumable session id" << std::endl;
  auto session_id = [&] {
    auto stream = client.WriteObject(bucket_name, object_name,
                                     gcs::NewResumableUploadSession());
    auto id = stream.resumable_session_id();
    std::move(stream).Suspend();
    return id;
  }();

  std::cout << "\nRunning ResumeResumableUpload() example" << std::endl;
  ResumeResumableUpload(client, {bucket_name, object_name, session_id});

  std::cout << "\nRunning DeleteResumableUpload() example" << std::endl;
  DeleteResumableUpload(client, {bucket_name, object_name});

  (void)client.DeleteObject(bucket_name, object_name);
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  auto make_entry = [](std::string const& name,
                       std::vector<std::string> arg_names,
                       examples::ClientCommand const& cmd) {
    arg_names.insert(arg_names.begin(), {"<bucket-name>", "<object-name>"});
    return examples::CreateCommandEntry(name, std::move(arg_names), cmd);
  };

  examples::Example example({
      make_entry("start-resumable-upload", {}, StartResumableUpload),
      make_entry("resume-resumable-upload", {"<session-id>"},
                 ResumeResumableUpload),
      make_entry("delete-resumable-upload", {}, DeleteResumableUpload),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
