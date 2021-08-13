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
#include "google/cloud/storage/examples/storage_examples_common.h"
#include "google/cloud/internal/getenv.h"
#include <iostream>
#include <thread>

namespace {

void ListNotifications(google::cloud::storage::Client client,
                       std::vector<std::string> const& argv) {
  //! [list notifications] [START storage_list_bucket_notifications]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<std::vector<gcs::NotificationMetadata>> items =
        client.ListNotifications(bucket_name);
    if (!items) throw std::runtime_error(items.status().message());

    std::cout << "Notifications for bucket=" << bucket_name << "\n";
    for (gcs::NotificationMetadata const& notification : *items) {
      std::cout << notification << "\n";
    }
  }
  //! [list notifications] [END storage_list_bucket_notifications]
  (std::move(client), argv.at(0));
}

void CreateNotification(google::cloud::storage::Client client,
                        std::vector<std::string> const& argv) {
  //! [create notification] [START storage_create_bucket_notifications]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& topic_name) {
    StatusOr<gcs::NotificationMetadata> notification =
        client.CreateNotification(bucket_name, topic_name,
                                  gcs::payload_format::JsonApiV1(),
                                  gcs::NotificationMetadata());

    if (!notification) {
      throw std::runtime_error(notification.status().message());
    }

    std::cout << "Successfully created notification " << notification->id()
              << " for bucket " << bucket_name << "\n";
    std::cout << "Full details for the notification:\n"
              << *notification << "\n";
  }
  //! [create notification] [END storage_create_bucket_notifications]
  (std::move(client), argv.at(0), argv.at(1));
}

void GetNotification(google::cloud::storage::Client client,
                     std::vector<std::string> const& argv) {
  //! [get notification] [START storage_print_pubsub_bucket_notification]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& notification_id) {
    StatusOr<gcs::NotificationMetadata> notification =
        client.GetNotification(bucket_name, notification_id);
    if (!notification) {
      throw std::runtime_error(notification.status().message());
    }

    std::cout << "Notification " << notification->id() << " for bucket "
              << bucket_name << "\n";
    if (notification->object_name_prefix().empty()) {
      std::cout << "This notification is sent for all objects in the bucket\n";
    } else {
      std::cout << "This notification is sent only for objects starting with"
                << " the prefix " << notification->object_name_prefix() << "\n";
    }
    std::cout << "Full details for the notification:\n"
              << *notification << "\n";
  }
  //! [get notification] [END storage_print_pubsub_bucket_notification]
  (std::move(client), argv.at(0), argv.at(1));
}

void DeleteNotification(google::cloud::storage::Client client,
                        std::vector<std::string> const& argv) {
  //! [delete notification] [START storage_delete_bucket_notification]
  namespace gcs = ::google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& notification_id) {
    google::cloud::Status status =
        client.DeleteNotification(bucket_name, notification_id);
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "Successfully deleted notification " << notification_id
              << " on bucket " << bucket_name << "\n";
  }
  //! [delete notification] [END storage_delete_bucket_notification]
  (std::move(client), argv.at(0), argv.at(1));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  namespace gcs = ::google::cloud::storage;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_STORAGE_TEST_TOPIC_NAME",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const topic_name = google::cloud::internal::GetEnv(
                              "GOOGLE_CLOUD_CPP_STORAGE_TEST_TOPIC_NAME")
                              .value();
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const bucket_name = examples::MakeRandomBucketName(generator);
  auto client = gcs::Client();

  std::cout << "\nCreating bucket to run the example (" << bucket_name << ")"
            << std::endl;
  (void)client
      .CreateBucketForProject(bucket_name, project_id, gcs::BucketMetadata{})
      .value();
  // In GCS a single project cannot create or delete buckets more often than
  // once every two seconds. We will pause until that time before deleting the
  // bucket.
  auto pause = std::chrono::steady_clock::now() + std::chrono::seconds(2);

  std::cout << "\nRunning ListNotifications() example [1]" << std::endl;
  ListNotifications(client, {bucket_name});

  std::cout << "\nRunning CreateNotification() example" << std::endl;
  CreateNotification(client, {bucket_name, topic_name});

  std::cout << "\nRunning ListNotifications() example [1]" << std::endl;
  ListNotifications(client, {bucket_name});

  // We need to create notifications directly to get their ids and call the
  // other examples.
  std::cout << "\nManually creating a notification [1]" << std::endl;
  auto n1 = client
                .CreateNotification(
                    bucket_name, topic_name, gcs::payload_format::JsonApiV1(),
                    gcs::NotificationMetadata().set_object_name_prefix("foo/"))
                .value();

  std::cout << "\nManually creating a notification [2]" << std::endl;
  auto n2 = client
                .CreateNotification(bucket_name, topic_name,
                                    gcs::payload_format::JsonApiV1(),
                                    gcs::NotificationMetadata())
                .value();

  std::cout << "\nRunning GetNotification() example" << std::endl;
  GetNotification(client, {bucket_name, n1.id()});

  std::cout << "\nRunning GetNotification() example" << std::endl;
  GetNotification(client, {bucket_name, n2.id()});

  std::cout << "\nRunning ListNotifications() example [3]" << std::endl;
  ListNotifications(client, {bucket_name});

  std::cout << "\nRunning DeleteNotification() example [1]" << std::endl;
  DeleteNotification(client, {bucket_name, n1.id()});

  std::cout << "\nRunning DeleteNotification() example [2]" << std::endl;
  DeleteNotification(client, {bucket_name, n2.id()});

  if (!examples::UsingEmulator()) std::this_thread::sleep_until(pause);
  (void)examples::RemoveBucketAndContents(client, bucket_name);
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  examples::Example example({
      examples::CreateCommandEntry("list-notifications", {"<bucket-name>"},
                                   ListNotifications),
      examples::CreateCommandEntry("create-notification",
                                   {"<bucket-name>", "<topic-name>"},
                                   CreateNotification),
      examples::CreateCommandEntry("get-notification",
                                   {"<bucket-name>", "<notification-id>"},
                                   GetNotification),
      examples::CreateCommandEntry("delete-notification",
                                   {"<bucket-name>", "<notification-id>"},
                                   DeleteNotification),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
