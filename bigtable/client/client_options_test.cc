// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "bigtable/client/client_options.h"

#include <gmock/gmock.h>

TEST(ClientOptionsTest, ClientOptionsDefaultSettings) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  EXPECT_EQ("bigtable.googleapis.com", client_options_object.endpoint());
  EXPECT_EQ(typeid(grpc::GoogleDefaultCredentials()),
            typeid(client_options_object.credentials()));
}

TEST(ClientOptionsTest, ClientOptionsCustomEndpoint) {
  setenv("BIGTABLE_EMULATOR_HOST", "testendpoint.googleapis.com", 1);
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  EXPECT_EQ("testendpoint.googleapis.com", client_options_object.endpoint());
  EXPECT_EQ(typeid(grpc::InsecureChannelCredentials()),
            typeid(client_options_object.credentials()));
  unsetenv("BIGTABLE_EMULATOR_HOST");
}

TEST(ClientOptionsTest, EditEndpoint) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object =
      client_options_object.SetEndpoint("customendpoint.com");
  EXPECT_EQ("customendpoint.com", client_options_object.endpoint());
}

TEST(ClientOptionsTest, EditCredentials) {
  bigtable::ClientOptions client_options_object = bigtable::ClientOptions();
  client_options_object =
      client_options_object.SetCredentials(grpc::InsecureChannelCredentials());
  EXPECT_EQ(typeid(grpc::InsecureChannelCredentials()),
            typeid(client_options_object.credentials()));
}
