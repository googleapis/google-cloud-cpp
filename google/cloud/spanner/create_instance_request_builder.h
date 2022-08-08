// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_CREATE_INSTANCE_REQUEST_BUILDER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_CREATE_INSTANCE_REQUEST_BUILDER_H

#include "google/cloud/spanner/instance.h"
#include "google/cloud/spanner/version.h"
#include <google/protobuf/util/message_differencer.h>
#include <google/spanner/admin/instance/v1/spanner_instance_admin.pb.h>
#include <map>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * CreateInstanceRequestBuilder is a builder class for
 * `google::spanner::admin::instance::v1::CreateInstanceRequest`
 *
 * This is useful when calling the `InstanceAdminClient::CreateInstance()`
 * function.
 *
 * @par Example
 * @snippet samples.cc create-instance
 */
class CreateInstanceRequestBuilder {
 public:
  // Copy and move.
  CreateInstanceRequestBuilder(CreateInstanceRequestBuilder const&) = default;
  CreateInstanceRequestBuilder(CreateInstanceRequestBuilder&&) = default;
  CreateInstanceRequestBuilder& operator=(CreateInstanceRequestBuilder const&) =
      default;
  CreateInstanceRequestBuilder& operator=(CreateInstanceRequestBuilder&&) =
      default;

  friend bool operator==(CreateInstanceRequestBuilder const& a,
                         CreateInstanceRequestBuilder const& b) noexcept {
    return google::protobuf::util::MessageDifferencer::Equivalent(a.request_,
                                                                  b.request_);
  }
  friend bool operator!=(CreateInstanceRequestBuilder const& a,
                         CreateInstanceRequestBuilder const& b) noexcept {
    return !(a == b);
  }

  /**
   * Constructor requires Instance and Cloud Spanner instance config name.
   * The display_name is set to a default value of in.instance_id().
   */
  CreateInstanceRequestBuilder(Instance const& in, std::string config) {
    request_.set_parent(in.project().FullName());
    request_.set_instance_id(in.instance_id());
    request_.mutable_instance()->set_name(in.FullName());
    request_.mutable_instance()->set_display_name(in.instance_id());
    request_.mutable_instance()->set_config(std::move(config));
  }

  CreateInstanceRequestBuilder& SetDisplayName(std::string display_name) & {
    request_.mutable_instance()->set_display_name(std::move(display_name));
    return *this;
  }

  CreateInstanceRequestBuilder&& SetDisplayName(std::string display_name) && {
    request_.mutable_instance()->set_display_name(std::move(display_name));
    return std::move(*this);
  }

  CreateInstanceRequestBuilder& SetNodeCount(int node_count) & {
    request_.mutable_instance()->set_node_count(node_count);
    return *this;
  }

  CreateInstanceRequestBuilder&& SetNodeCount(int node_count) && {
    request_.mutable_instance()->set_node_count(node_count);
    return std::move(*this);
  }

  CreateInstanceRequestBuilder& SetProcessingUnits(int processing_units) & {
    request_.mutable_instance()->set_processing_units(processing_units);
    return *this;
  }

  CreateInstanceRequestBuilder&& SetProcessingUnits(int processing_units) && {
    request_.mutable_instance()->set_processing_units(processing_units);
    return std::move(*this);
  }

  CreateInstanceRequestBuilder& SetLabels(
      std::map<std::string, std::string> const& labels) & {
    for (auto const& pair : labels) {
      request_.mutable_instance()->mutable_labels()->insert(
          {pair.first, pair.second});
    }
    return *this;
  }

  CreateInstanceRequestBuilder&& SetLabels(
      std::map<std::string, std::string> const& labels) && {
    for (auto const& pair : labels) {
      request_.mutable_instance()->mutable_labels()->insert(
          {pair.first, pair.second});
    }
    return std::move(*this);
  }

  google::spanner::admin::instance::v1::CreateInstanceRequest& Build() & {
    // Preserve original behavior of defaulting node_count to 1.
    if (request_.instance().processing_units() == 0) {
      if (request_.instance().node_count() == 0) {
        request_.mutable_instance()->set_node_count(1);
      }
    }
    return request_;
  }
  google::spanner::admin::instance::v1::CreateInstanceRequest&& Build() && {
    // Preserve original behavior of defaulting node_count to 1.
    if (request_.instance().processing_units() == 0) {
      if (request_.instance().node_count() == 0) {
        request_.mutable_instance()->set_node_count(1);
      }
    }
    return std::move(request_);
  }

 private:
  google::spanner::admin::instance::v1::CreateInstanceRequest request_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_CREATE_INSTANCE_REQUEST_BUILDER_H
