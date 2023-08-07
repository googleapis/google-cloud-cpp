// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! [all]
#include "google/cloud/datastore/v1/datastore_client.h"
#include <iostream>

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace datastore = ::google::cloud::datastore_v1;
  auto client =
      datastore::DatastoreClient(datastore::MakeDatastoreConnection());

  auto const* project_id = argv[1];

  google::datastore::v1::Key key;
  key.mutable_partition_id()->set_project_id(project_id);
  auto& path = *key.add_path();
  path.set_kind("Task");
  path.set_name("sampletask1");

  google::datastore::v1::Mutation mutation;
  auto& upsert = *mutation.mutable_upsert();
  *upsert.mutable_key() = key;
  google::datastore::v1::Value value;
  value.set_string_value("Buy milk");
  upsert.mutable_properties()->insert({"description", std::move(value)});

  auto put = client.Commit(
      project_id, google::datastore::v1::CommitRequest::NON_TRANSACTIONAL,
      {mutation});
  if (!put) throw std::move(put).status();

  std::cout << "Saved " << key.DebugString() << " " << put->DebugString()
            << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
//! [all]
