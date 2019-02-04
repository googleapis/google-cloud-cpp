// Copyright 2018 Google LLC
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
#include "google/cloud/storage/oauth2/google_credentials.h"
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

namespace {
struct Usage {
  std::string msg;
};

char const* ConsumeArg(int& argc, char* argv[]) {
  if (argc < 2) {
    return nullptr;
  }
  char const* result = argv[1];
  std::copy(argv + 2, argv + argc, argv + 1);
  argc--;
  return result;
}

std::string command_usage;

void PrintUsage(int argc, char* argv[], std::string const& msg) {
  std::string const cmd = argv[0];
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n"
            << command_usage << std::endl;
}

void ListObjects(google::cloud::storage::Client client, int& argc,
                 char* argv[]) {
  if (argc != 2) {
    throw Usage{"list-objects <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [list objects] [START storage_list_files]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name) {
    for (auto&& object_metadata : client.ListObjects(bucket_name)) {
      if (!object_metadata) {
        std::cerr << "Error reading object list for " << bucket_name
                  << ", status=" << object_metadata.status();
        return;
      }

      std::cout << "bucket_name=" << object_metadata->bucket()
                << ", object_name=" << object_metadata->name() << std::endl;
    }
  }
  //! [list objects] [END storage_list_files]
  (std::move(client), bucket_name);
}

void InsertObject(google::cloud::storage::Client client, int& argc,
                  char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "insert-object <bucket-name> <object-name> <object-contents (string)>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto contents = ConsumeArg(argc, argv);
  //! [insert object]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string contents) {
    StatusOr<gcs::ObjectMetadata> object_metadata =
        client.InsertObject(bucket_name, object_name, std::move(contents));

    if (!object_metadata) {
      std::cerr << "Error inserting object " << object_name << " in bucket "
                << bucket_name << ", status=" << object_metadata.status()
                << std::endl;
      return;
    }

    std::cout << "The object " << object_metadata->name()
              << " was created in bucket " << object_metadata->bucket()
              << "\nFull metadata: " << *object_metadata << std::endl;
  }
  //! [insert object]
  (std::move(client), bucket_name, object_name, contents);
}

void InsertObjectStrictIdempotency(google::cloud::storage::Client unused,
                                   int& argc, char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "insert-object-strict-idempotency <bucket-name> <object-name> "
        "<object-contents (string)>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto contents = ConsumeArg(argc, argv);
  //! [insert object strict idempotency]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](std::string bucket_name, std::string object_name, std::string contents) {
    // Create a client that only retries idempotent operations, the default is
    // to retry all operations.
    StatusOr<gcs::ClientOptions> options =
        gcs::ClientOptions::CreateDefaultClientOptions();
    if (!options) {
      std::cerr << "Error creating default ClientOptions, status="
                << options.status() << std::endl;
      return;
    }
    gcs::Client client{*options, gcs::StrictIdempotencyPolicy()};
    StatusOr<gcs::ObjectMetadata> object_metadata =
        client.InsertObject(bucket_name, object_name, std::move(contents),
                            gcs::IfGenerationMatch(0));

    if (!object_metadata) {
      std::cerr << "Error inserting object " << object_name << " in bucket "
                << bucket_name << ", status=" << object_metadata.status()
                << std::endl;
      return;
    }

    std::cout << "The object " << object_metadata->name()
              << " was created in bucket " << object_metadata->bucket()
              << "\nFull metadata: " << *object_metadata << std::endl;
  }
  //! [insert object strict idempotency]
  (bucket_name, object_name, contents);
}

void InsertObjectModifiedRetry(google::cloud::storage::Client unused, int& argc,
                               char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "insert-object-modified-retry <bucket-name> <object-name> "
        "<object-contents (string)>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto contents = ConsumeArg(argc, argv);
  //! [insert object modified retry]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](std::string bucket_name, std::string object_name, std::string contents) {
    auto credentials = gcs::oauth2::GoogleDefaultCredentials();
    if (!credentials) {
      std::cerr << "Error obtaining default credentials, status="
                << credentials.status() << std::endl;
      return;
    }
    // Create a client that only gives up on the third error. The default policy
    // is to retry for several minutes.
    StatusOr<gcs::ClientOptions> options =
        gcs::ClientOptions::CreateDefaultClientOptions();
    if (!options) {
      std::cerr << "Error creating default ClientOptions, status="
                << options.status() << std::endl;
      return;
    }
    gcs::Client client{*options, gcs::LimitedErrorCountRetryPolicy(3)};

    StatusOr<gcs::ObjectMetadata> object_metadata =
        client.InsertObject(bucket_name, object_name, std::move(contents),
                            gcs::IfGenerationMatch(0));

    if (!object_metadata) {
      std::cerr << "Error inserting object " << object_name << " in bucket "
                << bucket_name << ", status=" << object_metadata.status()
                << std::endl;
      return;
    }

    std::cout << "The object " << object_metadata->name()
              << " was created in bucket " << object_metadata->bucket()
              << "\nFull metadata: " << *object_metadata << std::endl;
  }
  //! [insert object modified retry]
  (bucket_name, object_name, contents);
}

void InsertObjectMultipart(google::cloud::storage::Client client, int& argc,
                           char* argv[]) {
  if (argc != 5) {
    throw Usage{
        "insert-object-multipart <bucket-name> <object-name>"
        " <content-type> <object-contents (string)>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto content_type = ConsumeArg(argc, argv);
  auto contents = ConsumeArg(argc, argv);
  //! [insert object multipart]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string content_type, std::string contents) {
    // Setting the object metadata (via the `gcs::WithObjectMadata` option)
    // requires a multipart upload, the library prefers simple uploads unless
    // required as in this case.
    StatusOr<gcs::ObjectMetadata> object_metadata = client.InsertObject(
        bucket_name, object_name, std::move(contents),
        gcs::WithObjectMetadata(
            gcs::ObjectMetadata().set_content_type(content_type)));

    if (!object_metadata) {
      std::cerr << "Error inserting object " << object_name << " in bucket "
                << bucket_name << ", status=" << object_metadata.status()
                << std::endl;
      return;
    }

    std::cout << "The object " << object_metadata->name()
              << " was created in bucket " << object_metadata->bucket()
              << "\nThe contentType was set to "
              << object_metadata->content_type()
              << "\nFull metadata: " << *object_metadata << std::endl;
  }
  //! [insert object multipart]
  (std::move(client), bucket_name, object_name, content_type, contents);
}

void CopyObject(google::cloud::storage::Client client, int& argc,
                char* argv[]) {
  if (argc != 5) {
    throw Usage{
        "copy-object <source-bucket-name> <source-object-name>"
        " <destination-bucket-name> <destination-object-name>"};
  }
  auto source_bucket_name = ConsumeArg(argc, argv);
  auto source_object_name = ConsumeArg(argc, argv);
  auto destination_bucket_name = ConsumeArg(argc, argv);
  auto destination_object_name = ConsumeArg(argc, argv);
  //! [copy object] [START storage_copy_file]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string source_bucket_name,
     std::string source_object_name, std::string destination_bucket_name,
     std::string destination_object_name) {
    StatusOr<gcs::ObjectMetadata> new_copy_meta =
        client.CopyObject(source_bucket_name, source_object_name,
                          destination_bucket_name, destination_object_name);

    if (!new_copy_meta) {
      std::cerr << "Error copying object " << source_bucket_name
                << " in bucket " << source_bucket_name << " to bucket "
                << destination_bucket_name << " with name "
                << destination_object_name
                << ", status=" << new_copy_meta.status() << std::endl;
      return;
    }

    std::cout << "Successfully copied " << source_object_name << " in bucket "
              << source_bucket_name << " to bucket " << new_copy_meta->bucket()
              << " with name " << new_copy_meta->name()
              << ".\nThe full metadata after the copy is: " << *new_copy_meta
              << std::endl;
  }
  //! [copy object] [END storage_copy_file]
  (std::move(client), source_bucket_name, source_object_name,
   destination_bucket_name, destination_object_name);
}

void CopyEncryptedObject(google::cloud::storage::Client client, int& argc,
                         char* argv[]) {
  if (argc != 6) {
    throw Usage{
        "copy-encrypted-object <source-bucket-name> <source-object-name>"
        " <destination-bucket-name> <destination-object-name>"
        " <encryption-key-base64>"};
  }
  auto source_bucket_name = ConsumeArg(argc, argv);
  auto source_object_name = ConsumeArg(argc, argv);
  auto destination_bucket_name = ConsumeArg(argc, argv);
  auto destination_object_name = ConsumeArg(argc, argv);
  auto key = ConsumeArg(argc, argv);
  //! [copy encrypted object]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string source_bucket_name,
     std::string source_object_name, std::string destination_bucket_name,
     std::string destination_object_name, std::string key_base64) {
    StatusOr<gcs::ObjectMetadata> new_copy_meta = client.CopyObject(
        source_bucket_name, source_object_name, destination_bucket_name,
        destination_object_name, gcs::EncryptionKey::FromBase64Key(key_base64));

    if (!new_copy_meta) {
      std::cerr << "Error copying object " << source_bucket_name
                << " in bucket " << source_bucket_name << " to bucket "
                << destination_bucket_name << " with name "
                << destination_object_name
                << ", status=" << new_copy_meta.status() << std::endl;
      return;
    }

    std::cout << "Successfully copied " << source_object_name << " in bucket "
              << source_bucket_name << " to bucket " << new_copy_meta->bucket()
              << " with name " << new_copy_meta->name()
              << ".\nThe full metadata after the copy is: " << *new_copy_meta
              << std::endl;
  }
  //! [copy encrypted object]
  (std::move(client), source_bucket_name, source_object_name,
   destination_bucket_name, destination_object_name, key);
}

void GetObjectMetadata(google::cloud::storage::Client client, int& argc,
                       char* argv[]) {
  if (argc != 3) {
    throw Usage{"get-object-metadata <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  //! [get object metadata] [START storage_get_metadata]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name) {
    StatusOr<gcs::ObjectMetadata> object_metadata =
        client.GetObjectMetadata(bucket_name, object_name);

    if (!object_metadata) {
      std::cerr << "Error getting object metadata for object " << object_name
                << " in bucket " << bucket_name
                << ", status=" << object_metadata.status() << std::endl;
      return;
    }

    std::cout << "The metadata for object " << object_metadata->name()
              << " in bucket " << object_metadata->bucket() << " is "
              << *object_metadata << std::endl;
  }
  //! [get object metadata] [END storage_get_metadata]
  (std::move(client), bucket_name, object_name);
}

void ReadObject(google::cloud::storage::Client client, int& argc,
                char* argv[]) {
  if (argc != 3) {
    throw Usage{"read-object <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  //! [read object] [START storage_download_file]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name) {
    gcs::ObjectReadStream stream = client.ReadObject(bucket_name, object_name);

    int count = 0;
    std::string line;
    while (std::getline(stream, line, '\n')) {
      ++count;
    }

    std::cout << "The object has " << count << " lines" << std::endl;
  }
  //! [read object] [END storage_download_file]
  (std::move(client), bucket_name, object_name);
}

void ReadObjectRange(google::cloud::storage::Client client, int& argc,
                     char* argv[]) {
  if (argc != 5) {
    throw Usage{"read-object-range <bucket-name> <object-name> <start> <end>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto start = std::stoll(ConsumeArg(argc, argv));
  auto end = std::stoll(ConsumeArg(argc, argv));
  //! [read object range] [START storage_download_byte_range]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
      std::int64_t start, std::int64_t end) {
    gcs::ObjectReadStream stream = client.ReadObject(bucket_name, object_name,
        gcs::ReadRange(start, end));

    int count = 0;
    std::string line;
    while (std::getline(stream, line, '\n')) {
      std::cout << line << "\n";
      ++count;
    }

    std::cout << "The requested range has " << count << " lines" << std::endl;
  }
  //! [read object range] [END storage_download_byte_range]
  (std::move(client), bucket_name, object_name, start, end);
}

void DeleteObject(google::cloud::storage::Client client, int& argc,
                  char* argv[]) {
  if (argc != 3) {
    throw Usage{"delete-object <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  //! [delete object] [START storage_delete_file]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name) {
    google::cloud::Status status =
        client.DeleteObject(bucket_name, object_name);

    if (!status.ok()) {
      std::cerr << "Error deleting object " << object_name << " in bucket "
                << bucket_name << ", status=" << status << std::endl;
      return;
    }

    std::cout << "Deleted " << object_name << " in bucket " << bucket_name
              << std::endl;
  }
  //! [delete object] [END storage_delete_file]
  (std::move(client), bucket_name, object_name);
}

void WriteObject(google::cloud::storage::Client client, int& argc,
                 char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "write-object <bucket-name> <object-name> <target-object-line-count>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto desired_line_count = std::stol(ConsumeArg(argc, argv));

  //! [write object]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     long desired_line_count) {
    std::string const text = "Lorem ipsum dolor sit amet";
    gcs::ObjectWriteStream stream =
        client.WriteObject(bucket_name, object_name);

    for (int lineno = 0; lineno != desired_line_count; ++lineno) {
      // Add 1 to the counter, because it is conventional to number lines
      // starting at 1.
      stream << (lineno + 1) << ": " << text << "\n";
    }

    stream.Close();

    StatusOr<gcs::ObjectMetadata> metadata = std::move(stream).metadata();
    if (!metadata) {
      std::cerr << "Error while writing to object " << object_name
                << " in bucket " << bucket_name
                << ", status=" << metadata.status() << std::endl;
      return;
    }
    std::cout << "Successfully wrote to object " << metadata->name()
              << " its size is: " << metadata->size()
              << "\nFull metadata: " << *metadata << std::endl;
  }
  //! [write object]
  (std::move(client), bucket_name, object_name, desired_line_count);
}

void WriteLargeObject(google::cloud::storage::Client client, int& argc,
                      char* argv[]) {
  if (argc != 4) {
    throw Usage{"write-large-object <bucket-name> <object-name> <size-in-MiB>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  int object_size_in_MiB = std::stoi(ConsumeArg(argc, argv));

  //! [write large object]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     int object_size_in_MiB) {
    // We want random-looking data, but we do not care if the data has a lot of
    // entropy, so do not bother with a complex initialization of the PRNG seed.
    std::mt19937_64 gen;
    auto generate_line = [&gen]() -> std::string {
      std::string const chars =
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ012345789";
      std::string line(128, '\n');
      std::uniform_int_distribution<std::size_t> uni(0, chars.size() - 1);
      std::generate_n(line.begin(), 127, [&] { return chars.at(uni(gen)); });
      return line;
    };

    // Each line is 128 bytes, so the number of lines is:
    long const kMiB = 1024L * 1024;
    long const line_count = object_size_in_MiB * kMiB / 128;

    gcs::ObjectWriteStream stream = client.WriteObject(
        bucket_name, object_name, gcs::IfGenerationMatch(0), gcs::Fields(""));
    std::generate_n(std::ostream_iterator<std::string>(stream), line_count,
                    generate_line);
  }
  //! [write large object]
  (std::move(client), bucket_name, object_name, object_size_in_MiB);
}

void StartResumableUpload(google::cloud::storage::Client client, int& argc,
                          char* argv[]) {
  if (argc != 3) {
    throw Usage{"start-resumable-upload <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);

  //! [start resumable upload]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name) {
    gcs::ObjectWriteStream stream = client.WriteObject(
        bucket_name, object_name, gcs::NewResumableUploadSession());
    std::cout << "Created resumable upload: " << stream.resumable_session_id()
              << std::endl;
    // As it is customary in C++, the destructor automatically closes the
    // stream, that would finish the upload and create the object. For this
    // example we want to restore the session as-if the application had crashed,
    // where no destructors get called.
    stream << "This data will not get uploaded, it is too small" << std::endl;
    std::move(stream).Suspend();
  }
  //! [start resumable upload]
  (std::move(client), bucket_name, object_name);
}

void ResumeResumableUpload(google::cloud::storage::Client client, int& argc,
                           char** argv) {
  if (argc != 4) {
    throw Usage{
        "resume-resumable-upload <bucket-name> <object-name> <session-id>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto session_id = ConsumeArg(argc, argv);

  //! [resume resumable upload]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string session_id) {
    // Restore a resumable upload stream, the library automatically queries the
    // state of the upload and discovers the next expected byte.
    gcs::ObjectWriteStream stream =
        client.WriteObject(bucket_name, object_name,
                           gcs::RestoreResumableUploadSession(session_id));
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
    if (!metadata) {
      std::cerr << "Error writing object " << object_name << " in bucket "
                << bucket_name << ", status=" << metadata.status() << std::endl;
      return;
    }

    std::cout << "Upload completed, the new object metadata is: " << *metadata
              << std::endl;
  }
  //! [resume resumable upload]
  (std::move(client), bucket_name, object_name, session_id);
}

void UploadFile(google::cloud::storage::Client client, int& argc,
                char* argv[]) {
  if (argc != 4) {
    throw Usage{"upload-file <file-name> <bucket-name> <object-name>"};
  }
  auto file_name = ConsumeArg(argc, argv);
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);

  //! [upload file] [START storage_upload_file]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string file_name, std::string bucket_name,
     std::string object_name) {
    // Note that the client library automatically computes a hash on the
    // client-side to verify data integrity during transmission.
    StatusOr<gcs::ObjectMetadata> object_metadata = client.UploadFile(
        file_name, bucket_name, object_name, gcs::IfGenerationMatch(0));

    if (!object_metadata) {
      std::cerr << "Error uploading file " << file_name << " to bucket "
                << bucket_name << " as object " << object_name
                << ", status=" << object_metadata.status() << std::endl;
      return;
    }

    std::cout << "Uploaded " << file_name << " to object "
              << object_metadata->name() << " in bucket "
              << object_metadata->bucket()
              << "\nFull metadata: " << *object_metadata << std::endl;
  }
  //! [upload file] [END storage_upload_file]
  (std::move(client), file_name, bucket_name, object_name);
}

void UploadFileResumable(google::cloud::storage::Client client, int& argc,
                         char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "upload-file-resumable <file-name> <bucket-name> <object-name>"};
  }
  auto file_name = ConsumeArg(argc, argv);
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);

  //! [upload file resumable]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string file_name, std::string bucket_name,
     std::string object_name) {
    // Note that the client library automatically computes a hash on the
    // client-side to verify data integrity during transmission.
    StatusOr<gcs::ObjectMetadata> object_metadata = client.UploadFile(
        file_name, bucket_name, object_name, gcs::IfGenerationMatch(0),
        gcs::NewResumableUploadSession());

    if (!object_metadata) {
      std::cerr << "Error uploading file " << file_name << " to bucket "
                << bucket_name << " as object " << object_name
                << ", status=" << object_metadata.status() << std::endl;
      return;
    }

    std::cout << "Uploaded " << file_name << " to object "
              << object_metadata->name() << " in bucket "
              << object_metadata->bucket()
              << "\nFull metadata: " << *object_metadata << std::endl;
  }
  //! [upload file resumable]
  (std::move(client), file_name, bucket_name, object_name);
}

void DownloadFile(google::cloud::storage::Client client, int& argc,
                  char* argv[]) {
  if (argc != 4) {
    throw Usage{"download-file <bucket-name> <object-name> <file-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto file_name = ConsumeArg(argc, argv);

  //! [download file]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string file_name) {
    google::cloud::Status status =
        client.DownloadToFile(bucket_name, object_name, file_name);

    if (!status.ok()) {
      std::cerr << "Error downloading object " << object_name << " in bucket "
                << bucket_name << " to file " << file_name
                << ", status=" << status << std::endl;
      return;
    }

    std::cout << "Downloaded " << object_name << " to " << file_name
              << std::endl;
  }
  //! [download file]
  (std::move(client), bucket_name, object_name, file_name);
}

void UpdateObjectMetadata(google::cloud::storage::Client client, int& argc,
                          char* argv[]) {
  if (argc != 5) {
    throw Usage{
        "update-object-metadata <bucket-name> <object-name> <key> <value>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto key = ConsumeArg(argc, argv);
  auto value = ConsumeArg(argc, argv);
  //! [update object metadata] [START storage_set_metadata]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string key, std::string value) {
    StatusOr<gcs::ObjectMetadata> object_metadata =
        client.GetObjectMetadata(bucket_name, object_name);

    if (!object_metadata) {
      std::cerr << "Error getting object metadata for object " << object_name
                << " in bucket " << bucket_name
                << ", status=" << object_metadata.status() << std::endl;
      return;
    }

    gcs::ObjectMetadata desired = *object_metadata;
    desired.mutable_metadata().emplace(key, value);

    StatusOr<gcs::ObjectMetadata> updated =
        client.UpdateObject(bucket_name, object_name, desired,
                            gcs::Generation(object_metadata->generation()));

    if (!updated) {
      std::cerr << "Error updating object metadata for object "
                << object_metadata->name() << " in bucket "
                << object_metadata->bucket() << ", status=" << updated.status()
                << std::endl;
      return;
    }

    std::cout << "Object updated. The full metadata after the update is: "
              << *updated << std::endl;
  }
  //! [update object metadata] [END storage_set_metadata]
  (std::move(client), bucket_name, object_name, key, value);
}

void PatchObjectDeleteMetadata(google::cloud::storage::Client client, int& argc,
                               char* argv[]) {
  if (argc != 4) {
    throw Usage{"update-object-metadata <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto key = ConsumeArg(argc, argv);
  //! [patch object delete metadata]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string key) {
    StatusOr<gcs::ObjectMetadata> original =
        client.GetObjectMetadata(bucket_name, object_name);

    if (!original) {
      std::cerr << "Error getting object metadata for object " << object_name
                << " in bucket " << bucket_name
                << ", status=" << original.status() << std::endl;
      return;
    }

    gcs::ObjectMetadata desired = *original;
    desired.mutable_metadata().erase(key);

    StatusOr<gcs::ObjectMetadata> updated =
        client.PatchObject(bucket_name, object_name, *original, desired);

    if (!updated) {
      std::cerr << "Error updating object metadata for object "
                << original->name() << " in bucket " << original->bucket()
                << ", status=" << updated.status() << std::endl;
      return;
    }

    std::cout << "Object updated. The full metadata after the update is: "
              << *updated << std::endl;
  }
  //! [patch object delete metadata]
  (std::move(client), bucket_name, object_name, key);
}

void PatchObjectContentType(google::cloud::storage::Client client, int& argc,
                            char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "update-object-metadata <bucket-name> <object-name> <content-type>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto content_type = ConsumeArg(argc, argv);
  //! [patch object content type]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string content_type) {
    StatusOr<gcs::ObjectMetadata> updated = client.PatchObject(
        bucket_name, object_name,
        gcs::ObjectMetadataPatchBuilder().SetContentType(content_type));

    if (!updated) {
      std::cerr << "Error patching object metadata for object " << object_name
                << " in bucket " << bucket_name
                << ", status=" << updated.status() << std::endl;
      return;
    }

    std::cout << "Object updated. The full metadata after the update is: "
              << *updated << std::endl;
  }
  //! [patch object content type]
  (std::move(client), bucket_name, object_name, content_type);
}

void MakeObjectPublic(google::cloud::storage::Client client, int& argc,
                      char* argv[]) {
  if (argc != 3) {
    throw Usage{"make-object-public <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  //! [make object public] [START storage_make_public]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name) {
    StatusOr<gcs::ObjectMetadata> updated = client.PatchObject(
        bucket_name, object_name, gcs::ObjectMetadataPatchBuilder(),
        gcs::PredefinedAcl::PublicRead());

    if (!updated) {
      std::cerr << "Error patching object metadata for object " << object_name
                << " in bucket " << bucket_name
                << ", status=" << updated.status() << std::endl;
      return;
    }

    std::cout << "Object updated. The full metadata after the update is: "
              << *updated << std::endl;
  }
  //! [make object public] [END storage_make_public]
  (std::move(client), bucket_name, object_name);
}

void ReadObjectUnauthenticated(google::cloud::storage::Client client, int& argc,
                               char* argv[]) {
  if (argc != 3) {
    throw Usage{"read-object-unauthenticated <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  //! [read object unauthenticated]
  namespace gcs = google::cloud::storage;
  [](std::string bucket_name, std::string object_name) {
    // Create a client that does not authenticate with the server.
    gcs::Client client{gcs::oauth2::CreateAnonymousCredentials()};

    // Read an object, the object must have been made public.
    gcs::ObjectReadStream stream = client.ReadObject(bucket_name, object_name);

    int count = 0;
    std::string line;
    while (std::getline(stream, line, '\n')) {
      ++count;
    }
    std::cout << "The object has " << count << " lines" << std::endl;
  }
  //! [read object unauthenticated]
  (bucket_name, object_name);
}

void GenerateEncryptionKey(google::cloud::storage::Client client, int& argc,
                           char* argv[]) {
  if (argc != 1) {
    throw Usage{"generate-encryption-key"};
  }
  //! [generate encryption key] [START storage_generate_encryption_key]
  // Create a pseudo-random number generator (PRNG), this is included for
  // demonstration purposes only. You should consult your security team about
  // best practices to initialize PRNG. In particular, you should verify that
  // the C++ library and operating system provide enough entropy to meet the
  // security policies in your organization.

  // Use the Mersenne-Twister Engine in this example:
  //   https://en.cppreference.com/w/cpp/numeric/random/mersenne_twister_engine
  // Any C++ PRNG can be used below, the choice is arbitrary.
  using GeneratorType = std::mt19937_64;

  // Create the default random device to fetch entropy.
  std::random_device rd;

  // Compute how much entropy we need to initialize the GeneratorType:
  constexpr auto kRequiredEntropyWords =
      GeneratorType::state_size *
      (GeneratorType::word_size / std::numeric_limits<unsigned int>::digits);

  // Capture the entropy bits into a vector.
  std::vector<unsigned long> entropy(kRequiredEntropyWords);
  std::generate(entropy.begin(), entropy.end(), [&rd] { return rd(); });

  // Create the PRNG with the fetched entropy.
  std::seed_seq seed(entropy.begin(), entropy.end());

  // initialized with enough entropy such that the encryption keys are not
  // predictable. Note that the default constructor for all the generators in
  // the C++ standard library produce predictable keys.
  std::mt19937_64 gen(seed);

  namespace gcs = google::cloud::storage;
  gcs::EncryptionKeyData data = gcs::CreateKeyFromGenerator(gen);

  std::cout << "Base64 encoded key = " << data.key << "\n"
            << "Base64 encoded SHA256 of key = " << data.sha256 << std::endl;
  //! [generate encryption key] [END storage_generate_encryption_key]
}

void WriteEncryptedObject(google::cloud::storage::Client client, int& argc,
                          char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "write-encrypted-object <bucket-name> <object-name>"
        " <base64-encoded-aes256-key>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto base64_aes256_key = ConsumeArg(argc, argv);
  //! [insert encrypted object] [START storage_upload_encrypted_file]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string base64_aes256_key) {
    StatusOr<gcs::ObjectMetadata> object_metadata = client.InsertObject(
        bucket_name, object_name, "top secret",
        gcs::EncryptionKey::FromBase64Key(base64_aes256_key));

    if (!object_metadata) {
      std::cerr << "Error inserting object " << object_name << " in bucket "
                << bucket_name << ", status=" << object_metadata.status()
                << std::endl;
      return;
    }

    std::cout << "The object " << object_metadata->name()
              << " was created in bucket " << object_metadata->bucket()
              << "\nFull metadata: " << *object_metadata << std::endl;
  }
  //! [insert encrypted object] [END storage_upload_encrypted_file]
  (std::move(client), bucket_name, object_name, base64_aes256_key);
}

void ReadEncryptedObject(google::cloud::storage::Client client, int& argc,
                         char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "read-encrypted-object <bucket-name> <object-name>"
        " <base64-encoded-aes256-key>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto base64_aes256_key = ConsumeArg(argc, argv);
  //! [read encrypted object] [START storage_download_encrypted_file]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string base64_aes256_key) {
    gcs::ObjectReadStream stream =
        client.ReadObject(bucket_name, object_name,
                          gcs::EncryptionKey::FromBase64Key(base64_aes256_key));

    std::string data(std::istreambuf_iterator<char>{stream}, {});
    std::cout << "The object contents are: " << data << std::endl;
  }
  //! [read encrypted object] [END storage_download_encrypted_file]
  (std::move(client), bucket_name, object_name, base64_aes256_key);
}

void ComposeObject(google::cloud::storage::Client client, int& argc,
                   char* argv[]) {
  if (argc < 4) {
    throw Usage{
        "compose-object <bucket-name> <destination-object-name>"
        " <object_1> ..."};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto destination_object_name = ConsumeArg(argc, argv);
  std::vector<google::cloud::storage::ComposeSourceObject> compose_objects;
  while (argc > 1) {
    compose_objects.push_back({ConsumeArg(argc, argv)});
  }
  //! [compose object] [START storage_compose_file]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name,
     std::string destination_object_name,
     std::vector<gcs::ComposeSourceObject> compose_objects) {
    StatusOr<gcs::ObjectMetadata> composed_object = client.ComposeObject(
        bucket_name, compose_objects, destination_object_name);

    if (!composed_object) {
      std::cerr << "Error composing objects in bucket " << bucket_name
                << ", status=" << composed_object.status() << std::endl;
      return;
    }

    std::cout << "Composed new object " << composed_object->name()
              << " in bucket " << composed_object->bucket()
              << "\nFull metadata: " << *composed_object << std::endl;
  }
  //! [compose object] [END storage_compose_file]
  (std::move(client), bucket_name, destination_object_name,
   std::move(compose_objects));
}

void ComposeObjectFromEncryptedObjects(google::cloud::storage::Client client,
                                       int& argc, char* argv[]) {
  if (argc < 5) {
    throw Usage{
        "compose-object-from-encrypted-objects <bucket-name>"
        " <destination-object-name> <base64-encoded-aes256-key>"
        " <object_1> ..."};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto destination_object_name = ConsumeArg(argc, argv);
  auto base64_aes256_key = ConsumeArg(argc, argv);
  std::vector<google::cloud::storage::ComposeSourceObject> compose_objects;
  while (argc > 1) {
    compose_objects.push_back({ConsumeArg(argc, argv)});
  }
  //! [compose object from encrypted objects]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name,
     std::string destination_object_name, std::string base64_aes256_key,
     std::vector<gcs::ComposeSourceObject> compose_objects) {
    StatusOr<gcs::ObjectMetadata> composed_object = client.ComposeObject(
        bucket_name, compose_objects, destination_object_name,
        gcs::EncryptionKey::FromBase64Key(base64_aes256_key));

    if (!composed_object) {
      std::cerr << "Error composing objects in bucket " << bucket_name
                << ", status=" << composed_object.status() << std::endl;
      return;
    }

    std::cout << "Composed new object " << composed_object->name()
              << " in bucket " << composed_object->bucket()
              << "\nFull metadata: " << *composed_object << std::endl;
  }
  //! [compose object from encrypted objects]
  (std::move(client), bucket_name, destination_object_name, base64_aes256_key,
   std::move(compose_objects));
}

void WriteObjectWithKmsKey(google::cloud::storage::Client client, int& argc,
                           char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "write-object-with-kms-key <bucket-name> <object-name>"
        " <kms-key-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto kms_key_name = ConsumeArg(argc, argv);

  //! [write object with kms key] [START storage_upload_with_kms_key]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string kms_key_name) {
    gcs::ObjectWriteStream stream = client.WriteObject(
        bucket_name, object_name, gcs::KmsKeyName(kms_key_name));

    // Line numbers start at 1.
    for (int lineno = 1; lineno <= 10; ++lineno) {
      stream << lineno << ": placeholder text for CMEK example.\n";
    }

    stream.Close();

    StatusOr<gcs::ObjectMetadata> metadata = std::move(stream).metadata();
    if (!metadata) {
      std::cerr << "Error while writing to object " << object_name
                << " in bucket " << bucket_name
                << ", status=" << metadata.status() << std::endl;
      return;
    }
    std::cout << "Successfully wrote to object " << metadata->name()
              << " its size is: " << metadata->size()
              << "\nFull metadata: " << *metadata << std::endl;
  }
  //! [write object with kms key] [END storage_upload_with_kms_key]
  (std::move(client), bucket_name, object_name, kms_key_name);
}

void RewriteObject(google::cloud::storage::Client client, int& argc,
                   char* argv[]) {
  if (argc != 5) {
    throw Usage{
        "rewrite-object <source-bucket-name> <source-object-name>"
        " <destination-bucket-name> <destination-object-name>"};
  }
  auto source_bucket_name = ConsumeArg(argc, argv);
  auto source_object_name = ConsumeArg(argc, argv);
  auto destination_bucket_name = ConsumeArg(argc, argv);
  auto destination_object_name = ConsumeArg(argc, argv);
  //! [rewrite object]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string source_bucket_name,
     std::string source_object_name, std::string destination_bucket_name,
     std::string destination_object_name) {
    StatusOr<gcs::ObjectMetadata> object_metadata =
        client.RewriteObjectBlocking(source_bucket_name, source_object_name,
                                     destination_bucket_name,
                                     destination_object_name);

    if (!object_metadata) {
      std::cerr << "Cannot rewrite object " << source_object_name
                << " in bucket " << source_bucket_name
                << ", status=" << object_metadata.status() << std::endl;
      return;
    }

    std::cout << "Rewrote object " << destination_object_name
              << " Metadata: " << *object_metadata << std::endl;
  }
  //! [rewrite object]
  (std::move(client), source_bucket_name, source_object_name,
   destination_bucket_name, destination_object_name);
}

void RewriteObjectNonBlocking(google::cloud::storage::Client client, int& argc,
                              char* argv[]) {
  if (argc != 5) {
    throw Usage{
        "rewrite-object-non-blocking <source-bucket-name> <source-object-name>"
        " <destination-bucket-name> <destination-object-name>"};
  }
  auto source_bucket_name = ConsumeArg(argc, argv);
  auto source_object_name = ConsumeArg(argc, argv);
  auto destination_bucket_name = ConsumeArg(argc, argv);
  auto destination_object_name = ConsumeArg(argc, argv);
  //! [rewrite object non blocking]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
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
                std::cerr << "Error during rewrite: " << progress.status()
                          << std::endl;
                return;
              }

              std::cout << "Rewrote " << progress->total_bytes_rewritten << "/"
                        << progress->object_size << std::endl;
            });

    if (!object_metadata) {
      return;  // No need to report the error, already done in the callback.
    }

    std::cout << "Rewrote object " << object_metadata->name() << " in bucket "
              << object_metadata->bucket()
              << "\nFull Metadata: " << *object_metadata << std::endl;
  }
  //! [rewrite object non blocking]
  (std::move(client), source_bucket_name, source_object_name,
   destination_bucket_name, destination_object_name);
}

void RewriteObjectToken(google::cloud::storage::Client client, int& argc,
                        char* argv[]) {
  if (argc != 5) {
    throw Usage{
        "rewrite-object-token <source-bucket-name> <source-object-name>"
        " <destination-bucket-name> <destination-object-name>"};
  }
  auto source_bucket_name = ConsumeArg(argc, argv);
  auto source_object_name = ConsumeArg(argc, argv);
  auto destination_bucket_name = ConsumeArg(argc, argv);
  auto destination_object_name = ConsumeArg(argc, argv);

  //! [rewrite object token]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string source_bucket_name,
     std::string source_object_name, std::string destination_bucket_name,
     std::string destination_object_name) {
    gcs::ObjectRewriter rewriter = client.RewriteObject(
        source_bucket_name, source_object_name, destination_bucket_name,
        destination_object_name, gcs::MaxBytesRewrittenPerCall(1024 * 1024));

    StatusOr<gcs::RewriteProgress> progress = rewriter.Iterate();

    if (!progress) {
      std::cerr << "Error during rewrite: " << progress.status() << std::endl;
      return;
    }

    if (progress->done) {
      std::cout << "The rewrite completed immediately, no token to resume later"
                << std::endl;
      return;
    }
    std::cout << "Rewrite in progress, token " << rewriter.token() << std::endl;
  }
  //! [rewrite object token]
  (std::move(client), source_bucket_name, source_object_name,
   destination_bucket_name, destination_object_name);
}

void RewriteObjectResume(google::cloud::storage::Client client, int& argc,
                         char* argv[]) {
  if (argc != 6) {
    throw Usage{
        "rewrite-object-resume <source-bucket-name> <source-object-name>"
        " <destination-bucket-name> <destination-object-name> <token>"};
  }
  auto source_bucket_name = ConsumeArg(argc, argv);
  auto source_object_name = ConsumeArg(argc, argv);
  auto destination_bucket_name = ConsumeArg(argc, argv);
  auto destination_object_name = ConsumeArg(argc, argv);
  auto rewrite_token = ConsumeArg(argc, argv);

  //! [rewrite object resume]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
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
                std::cerr << "Error during rewrite: " << progress.status()
                          << std::endl;
                return;
              }
              std::cout << "Rewrote " << progress->total_bytes_rewritten << "/"
                        << progress->object_size << std::endl;
            });

    if (!object_metadata) {
      return;  // No need to report the error, already done in the callback.
    }

    std::cout << "Rewrote object " << object_metadata->name() << " in bucket "
              << object_metadata->bucket()
              << "\nFull Metadata: " << *object_metadata << std::endl;
  }
  //! [rewrite object resume]
  (std::move(client), source_bucket_name, source_object_name,
   destination_bucket_name, destination_object_name, rewrite_token);
}

void RotateEncryptionKey(google::cloud::storage::Client client, int& argc,
                         char* argv[]) {
  if (argc != 5) {
    throw Usage{
        "rotate-encryption-key <bucket-name> <object-name>"
        " <old-encryption-key> <new-encryption-key>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto old_key_base64 = ConsumeArg(argc, argv);
  auto new_key_base64 = ConsumeArg(argc, argv);

  //! [rotate encryption key] [START storage_rotate_encryption_key]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string old_key_base64, std::string new_key_base64) {
    StatusOr<gcs::ObjectMetadata> object_metadata =
        client.RewriteObjectBlocking(
            bucket_name, object_name, bucket_name, object_name,
            gcs::SourceEncryptionKey::FromBase64Key(old_key_base64),
            gcs::EncryptionKey::FromBase64Key(new_key_base64));

    if (!object_metadata) {
      std::cerr << "Error rotating key on object " << object_name
                << ", status=" << object_metadata.status() << std::endl;
    }

    std::cout << "Rotated key on object " << object_metadata->name()
              << " in bucket " << object_metadata->bucket()
              << "\nFull Metadata: " << *object_metadata << std::endl;
  }
  //! [rotate encryption key] [END storage_rotate_encryption_key]
  (std::move(client), bucket_name, object_name, old_key_base64, new_key_base64);
}

void RenameObject(google::cloud::storage::Client client, int& argc,
                  char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "rename-object <bucket-name> <old-object-name> <new-object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto old_object_name = ConsumeArg(argc, argv);
  auto new_object_name = ConsumeArg(argc, argv);

  //! [rename object] [START storage_move_file]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string old_object_name,
     std::string new_object_name) {
    StatusOr<gcs::ObjectMetadata> object_metadata =
        client.RewriteObjectBlocking(bucket_name, old_object_name, bucket_name,
                                     new_object_name);

    if (!object_metadata) {
      std::cerr << "Error renaming object " << old_object_name
                << ", status=" << object_metadata.status() << std::endl;
      return;
    }

    google::cloud::Status status =
        client.DeleteObject(bucket_name, old_object_name);

    if (!status.ok()) {
      std::cerr << "Error deleting original object " << old_object_name
                << " in bucket " << bucket_name << ", status=" << status
                << std::endl;
      return;
    }
    std::cout << "Renamed " << old_object_name << " to " << new_object_name
              << " in bucket " << bucket_name << std::endl;
  }
  //! [rename object] [END storage_move_file]
  (std::move(client), bucket_name, old_object_name, new_object_name);
}

void SetObjectEventBasedHold(google::cloud::storage::Client client, int& argc,
                             char* argv[]) {
  if (argc != 3) {
    throw Usage{"set-object-event-based-hold <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  //! [set event based hold] [START storage_set_event_based_hold]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name) {
    StatusOr<gcs::ObjectMetadata> original =
        client.GetObjectMetadata(bucket_name, object_name);

    if (!original) {
      std::cerr << "Error getting object metadata for object " << object_name
                << " in bucket " << bucket_name
                << ", status=" << original.status() << std::endl;
      return;
    }

    StatusOr<gcs::ObjectMetadata> updated = client.PatchObject(
        bucket_name, object_name,
        gcs::ObjectMetadataPatchBuilder().SetEventBasedHold(true),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!updated) {
      std::cerr << "Error patching object metadata for object " << object_name
                << " in bucket " << bucket_name
                << ", status=" << updated.status() << std::endl;
      return;
    }

    std::cout << "The event hold for object " << updated->name()
              << " in bucket " << updated->bucket() << " is "
              << (updated->event_based_hold() ? "enabled" : "disabled")
              << std::endl;
  }
  //! [set event based hold] [END storage_set_event_based_hold]
  (std::move(client), bucket_name, object_name);
}

void ReleaseObjectEventBasedHold(google::cloud::storage::Client client,
                                 int& argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"release-object-event-based-hold <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  //! [release event based hold] [START storage_release_event_based_hold]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name) {
    StatusOr<gcs::ObjectMetadata> original =
        client.GetObjectMetadata(bucket_name, object_name);

    if (!original) {
      std::cerr << "Error getting object metadata for object " << object_name
                << " in bucket " << bucket_name
                << ", status=" << original.status() << std::endl;
      return;
    }

    StatusOr<gcs::ObjectMetadata> updated = client.PatchObject(
        bucket_name, object_name,
        gcs::ObjectMetadataPatchBuilder().SetEventBasedHold(false),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!updated) {
      std::cerr << "Error patching object metadata for object " << object_name
                << " in bucket " << bucket_name
                << ", status=" << updated.status() << std::endl;
      return;
    }

    std::cout << "The event hold for object " << updated->name()
              << " in bucket " << updated->bucket() << " is "
              << (updated->event_based_hold() ? "enabled" : "disabled")
              << std::endl;
  }
  //! [release event based hold] [END storage_release_event_based_hold]
  (std::move(client), bucket_name, object_name);
}

void SetObjectTemporaryHold(google::cloud::storage::Client client, int& argc,
                            char* argv[]) {
  if (argc != 3) {
    throw Usage{"set-object-temporary-hold <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  //! [set temporary hold] [START storage_set_temporary_hold]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name) {
    StatusOr<gcs::ObjectMetadata> original =
        client.GetObjectMetadata(bucket_name, object_name);

    if (!original) {
      std::cerr << "Error getting object metadata for object " << object_name
                << " in bucket " << bucket_name
                << ", status=" << original.status() << std::endl;
      return;
    }

    StatusOr<gcs::ObjectMetadata> updated = client.PatchObject(
        bucket_name, object_name,
        gcs::ObjectMetadataPatchBuilder().SetTemporaryHold(true),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!updated) {
      std::cerr << "Error patching object metadata for object " << object_name
                << " in bucket " << bucket_name
                << ", status=" << updated.status() << std::endl;
      return;
    }

    std::cout << "The temporary hold for object " << updated->name()
              << " in bucket " << updated->bucket() << " is "
              << (updated->temporary_hold() ? "enabled" : "disabled")
              << std::endl;
  }
  //! [set temporary hold] [END storage_set_temporary_hold]
  (std::move(client), bucket_name, object_name);
}

void ReleaseObjectTemporaryHold(google::cloud::storage::Client client,
                                int& argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"release-object-temporary-hold <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  //! [release temporary hold] [START storage_release_temporary_hold]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name) {
    StatusOr<gcs::ObjectMetadata> original =
        client.GetObjectMetadata(bucket_name, object_name);

    if (!original) {
      std::cerr << "Error getting object metadata for object " << object_name
                << " in bucket " << bucket_name
                << ", status=" << original.status() << std::endl;
      return;
    }

    StatusOr<gcs::ObjectMetadata> updated = client.PatchObject(
        bucket_name, object_name,
        gcs::ObjectMetadataPatchBuilder().SetTemporaryHold(false),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!updated) {
      std::cerr << "Error patching object metadata for object " << object_name
                << " in bucket " << bucket_name
                << ", status=" << updated.status() << std::endl;
      return;
    }

    std::cout << "The temporary hold for object " << updated->name()
              << " in bucket " << updated->bucket() << " is "
              << (updated->temporary_hold() ? "enabled" : "disabled")
              << std::endl;
  }
  //! [release temporary hold] [END storage_release_temporary_hold]
  (std::move(client), bucket_name, object_name);
}

void CreateGetSignedUrl(google::cloud::storage::Client client, int& argc,
                        char* argv[]) {
  if (argc != 3) {
    throw Usage{"create-get-signed-url <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  //! [sign url] [START storage_generate_signed_url]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name) {
    StatusOr<std::string> signed_url = client.CreateV2SignedUrl(
        "GET", std::move(bucket_name), std::move(object_name),
        gcs::ExpirationTime(std::chrono::system_clock::now() +
                            std::chrono::minutes(15)));

    if (!signed_url) {
      std::cerr << "Error creating signed URL, status=" << signed_url.status()
                << std::endl;
      return;
    }

    std::cout << "The signed url is: " << *signed_url << "\n\n"
              << "You can use this URL with any user agent, for example:\n"
              << "curl '" << *signed_url << "'" << std::endl;
  }
  //! [sign url] [END storage_generate_signed_url]
  (std::move(client), bucket_name, object_name);
}

void CreatePutSignedUrl(google::cloud::storage::Client client, int& argc,
                        char* argv[]) {
  if (argc != 3) {
    throw Usage{"create-put-signed-url <bucket-name> <object-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  //! [create put signed url]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name) {
    StatusOr<std::string> signed_url = client.CreateV2SignedUrl(
        "PUT", std::move(bucket_name), std::move(object_name),
        gcs::ExpirationTime(std::chrono::system_clock::now() +
                            std::chrono::minutes(15)),
        gcs::ContentType("application/octet-stream"));

    if (!signed_url) {
      std::cerr << "Error creating signed URL, status=" << signed_url.status()
                << std::endl;
      return;
    }

    std::cout << "The signed url is: " << *signed_url << "\n\n"
              << "You can use this URL with any user agent, for example:\n"
              << "curl -X PUT -H 'Content-Type: application/octet-stream'"
              << " --upload-file my-file '" << *signed_url << "'" << std::endl;
  }
  //! [create put signed url]
  (std::move(client), bucket_name, object_name);
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  // Create a client to communicate with Google Cloud Storage.
  google::cloud::StatusOr<google::cloud::storage::Client> client =
      google::cloud::storage::Client::CreateDefaultClient();
  if (!client) {
    std::cerr << "Failed to create Storage Client, status=" << client.status()
              << std::endl;
    return 1;
  }

  using CommandType =
      std::function<void(google::cloud::storage::Client, int&, char*[])>;
  std::map<std::string, CommandType> commands = {
      {"list-objects", &ListObjects},
      {"insert-object", &InsertObject},
      {"insert-object-strict-idempotency", &InsertObjectStrictIdempotency},
      {"insert-object-modified-retry", &InsertObjectModifiedRetry},
      {"insert-object-multipart", &InsertObjectMultipart},
      {"copy-object", &CopyObject},
      {"copy-encrypted-object", &CopyEncryptedObject},
      {"get-object-metadata", &GetObjectMetadata},
      {"read-object", &ReadObject},
      {"read-object-range", &ReadObjectRange},
      {"delete-object", &DeleteObject},
      {"write-object", &WriteObject},
      {"write-large-object", &WriteLargeObject},
      {"start-resumable-upload", &StartResumableUpload},
      {"resume-resumable-upload", &ResumeResumableUpload},
      {"upload-file", &UploadFile},
      {"upload-file-resumable", &UploadFileResumable},
      {"download-file", &DownloadFile},
      {"update-object-metadata", &UpdateObjectMetadata},
      {"patch-object-delete-metadata", &PatchObjectDeleteMetadata},
      {"patch-object-content-type", &PatchObjectContentType},
      {"make-object-public", &MakeObjectPublic},
      {"read-object-unauthenticated", &ReadObjectUnauthenticated},
      {"generate-encryption-key", &GenerateEncryptionKey},
      {"write-encrypted-object", &WriteEncryptedObject},
      {"read-encrypted-object", &ReadEncryptedObject},
      {"compose-object", &ComposeObject},
      {"compose-object-from-encrypted-objects",
       &ComposeObjectFromEncryptedObjects},
      {"write-object-with-kms-key", &WriteObjectWithKmsKey},
      {"rewrite-object", &RewriteObject},
      {"rewrite-object-non-blocking", &RewriteObjectNonBlocking},
      {"rewrite-object-token", &RewriteObjectToken},
      {"rewrite-object-resume", &RewriteObjectResume},
      {"rotate-encryption-key", &RotateEncryptionKey},
      {"rename-object", &RenameObject},
      {"set-event-based-hold", &SetObjectEventBasedHold},
      {"release-event-based-hold", &ReleaseObjectEventBasedHold},
      {"set-temporary-hold", &SetObjectTemporaryHold},
      {"release-temporary-hold", &ReleaseObjectTemporaryHold},
      {"create-get-signed-url", &CreateGetSignedUrl},
      {"create-put-signed-url", &CreatePutSignedUrl},
  };
  for (auto&& kv : commands) {
    try {
      int fake_argc = 0;
      kv.second(*client, fake_argc, argv);
    } catch (Usage const& u) {
      command_usage += "    ";
      command_usage += u.msg;
      command_usage += "\n";
    }
  }

  if (argc < 2) {
    PrintUsage(argc, argv, "Missing command");
    return 1;
  }

  std::string const command = ConsumeArg(argc, argv);
  auto it = commands.find(command);
  if (commands.end() == it) {
    PrintUsage(argc, argv, "Unknown command: " + command);
    return 1;
  }

  it->second(*client, argc, argv);

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argc, argv, ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
