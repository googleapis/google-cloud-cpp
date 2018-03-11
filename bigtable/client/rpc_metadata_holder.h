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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_RPC_METADATA_HOLDER_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_RPC_METADATA_HOLDER_H_

#include <bigtable/client/version.h>

#include <grpc++/grpc++.h>
#include <memory>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Define the base class for generating metadata pair. metadata is
 * passed to BigTable and it provides detail of the caller. metadata is
 * in the form of key-value pair and must be set in ClientContext before
 * makeing the RPC all.
 *
 * Following metadata keys are currently supported in c++ client
 *     x-goog-request-params,
 *     google-cloud-resource-prefix
 */
class RPCMetadataPair {
 public:
  RPCMetadataPair() {}

  virtual ~RPCMetadataPair() = default;

  grpc::string get_key() const { return std::move(key_); }

  void set_key(grpc::string key) { key_ = std::move(key); }

  grpc::string get_value() const { return std::move(value_); }

  void set_value(grpc::string value) { value_ = std::move(value); }

 private:
  std::string key_;
  std::string value_;
};

/**
 * Define the enumeration for governing x-goog-request-params metadata value.
 * The value of x-goog-request-params starts with one of the following suffix
 *    "parent=" : Operation in instance, e.g. TableAdmin::CreateTable.
 *    "table_name=" : table_id is known at the time of creation, e.g.
 * Table::Apply.
 *    "name=" : this is used when table|_id is known only in the RPC call, e.g.
 * TableAdmin::GetTable.
 *
 */
enum RPCRequestParamType { kParent, kName, kTableName };

class GoogleCloudResourcePrefix : public RPCMetadataPair {
 public:
  explicit GoogleCloudResourcePrefix(std::string resource_string) {
    set_key(std::move(kKey));
    set_value(std::move(resource_string));
  }

 private:
  std::string const kKey = "google-cloud-resource-prefix";
};

class XGoogleRequestParamas : public RPCMetadataPair {
 public:
  explicit XGoogleRequestParamas(std::string resource_string,
                                 RPCRequestParamType request_param_type) {
    std::string value;
    switch (request_param_type) {
      case RPCRequestParamType::kParent:
        value = kValuePrefixParent;
        value += resource_string;
        break;
      case RPCRequestParamType::kName:
        value = kValuePrefixName;
        value += resource_string;
        break;
      case RPCRequestParamType::kTableName:
        value = kValuePrefixTableName;
        value += resource_string;
        break;
    }

    set_key(std::move(kKey));
    set_value(std::move(value));
  }

  explicit XGoogleRequestParamas(std::string resource_string,
                                 RPCRequestParamType request_param_type,
                                 std::string table_id) {
    std::string value;
    switch (request_param_type) {
      case RPCRequestParamType::kParent:
        value = kValuePrefixParent;
        value += resource_string;
        value += "/tables/" + table_id;
        break;
      case RPCRequestParamType::kName:
        value = kValuePrefixName;
        value += resource_string;
        value += "/tables/" + table_id;
        break;
      case RPCRequestParamType::kTableName:
        value = kValuePrefixTableName;
        value += resource_string;
        value += "/tables/" + table_id;
        break;
    }
    set_key(std::move(kKey));
    set_value(std::move(value));
  }

 private:
  std::string const kKey = "x-goog-request-params";
  static constexpr const char* kValuePrefixParent = "parent=";
  static constexpr const char* kValuePrefixName = "name=";
  static constexpr const char* kValuePrefixTableName = "table_name=";
};

/// RPCMetadataHolder holds supported metadata
class RPCMetadataHolder {
 public:
  /**
   * Constructor with default metadata pair.
   *
   * @param resource_name hierarchical name of resource, including  project id,
   * instance id
   *        and/or table_id.
   * @param request_param_type type to decide prefix for the value of
   * x-goog-request-params
   */
  explicit RPCMetadataHolder(std::string resource_name,
                             RPCRequestParamType request_param_type)
      : resource_name_(resource_name),
        request_param_type_(request_param_type),
        google_cloud_resource_prefix_(resource_name),
        x_google_request_params_(resource_name, request_param_type) {}

  /**
   * Constructor with default metadata pair.
   *
   * @param resource_name hierarchical name of resource, including  project id,
   * instance id
   *        and/or table_id.
   * @param request_param_type type to decide prefix for the value of
   * x-goog-request-params.
   * @param table_id table_id used in RPC call.
   */
  explicit RPCMetadataHolder(std::string resource_name,
                             RPCRequestParamType request_param_type,
                             std::string table_id)
      : resource_name_(resource_name),
        request_param_type_(request_param_type),
        google_cloud_resource_prefix_(resource_name),
        x_google_request_params_(resource_name, request_param_type, table_id) {}

  /**
   * Return a new copy of this object.
   *
   * Typically implemented as
   * @code
   *   return std::unique_ptr<RPCMetadataHolder>(new
   * RPCMetadataHolder(this->resource_name_, this->request_param_type_));
   * @endcode
   */
  std::unique_ptr<RPCMetadataHolder> clone() const;

  /**
   * Return a new copy of this object with modification in metadata,
   * currently modification in x-goog-request-params is supported
   *
   * Typically implemented as
   * @code
   *   return std::unique_ptr<RPCMetadataHolder>(new
   * RPCMetadataHolder(this->resource_name_,
   *                         this->request_param_type_, table_id));
   * @endcode
   *
   */
  std::unique_ptr<RPCMetadataHolder> cloneWithModifications(
      RPCRequestParamType request_param_type, std::string table_id) const;

  // Update the ClientContext for the next call.
  void setup(grpc::ClientContext& context) const;

  GoogleCloudResourcePrefix const get_google_cloud_resource_prefix() {
    return google_cloud_resource_prefix_;
  }

  XGoogleRequestParamas const get_x_google_request_params() {
    return x_google_request_params_;
  }

 private:
  std::string resource_name_;
  RPCRequestParamType request_param_type_;
  GoogleCloudResourcePrefix google_cloud_resource_prefix_;
  XGoogleRequestParamas x_google_request_params_;
};

std::unique_ptr<RPCMetadataHolder> DefaultRPCMetadataHolder(
    std::string resource_name, RPCRequestParamType request_param_type);

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_RPC_METADATA_HOLDER_H_
