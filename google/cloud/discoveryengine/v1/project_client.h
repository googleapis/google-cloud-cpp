// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Generated by the Codegen C++ plugin.
// If you make any local changes, they will be lost.
// source: google/cloud/discoveryengine/v1/project_service.proto

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_DISCOVERYENGINE_V1_PROJECT_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_DISCOVERYENGINE_V1_PROJECT_CLIENT_H

#include "google/cloud/discoveryengine/v1/project_connection.h"
#include "google/cloud/future.h"
#include "google/cloud/no_await_tag.h"
#include "google/cloud/options.h"
#include "google/cloud/polling_policy.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <google/longrunning/operations.grpc.pb.h>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace discoveryengine_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

///
/// Service for operations on the
/// [Project][google.cloud.discoveryengine.v1.Project].
///
/// @par Equality
///
/// Instances of this class created via copy-construction or copy-assignment
/// always compare equal. Instances created with equal
/// `std::shared_ptr<*Connection>` objects compare equal. Objects that compare
/// equal share the same underlying resources.
///
/// @par Performance
///
/// Creating a new instance of this class is a relatively expensive operation,
/// new objects establish new connections to the service. In contrast,
/// copy-construction, move-construction, and the corresponding assignment
/// operations are relatively efficient as the copies share all underlying
/// resources.
///
/// @par Thread Safety
///
/// Concurrent access to different instances of this class, even if they compare
/// equal, is guaranteed to work. Two or more threads operating on the same
/// instance of this class is not guaranteed to work. Since copy-construction
/// and move-construction is a relatively efficient operation, consider using
/// such a copy when using this class from multiple threads.
///
/// [google.cloud.discoveryengine.v1.Project]:
/// @googleapis_reference_link{google/cloud/discoveryengine/v1/project.proto#L33}
///
class ProjectServiceClient {
 public:
  explicit ProjectServiceClient(
      std::shared_ptr<ProjectServiceConnection> connection, Options opts = {});
  ~ProjectServiceClient();

  ///@{
  /// @name Copy and move support
  ProjectServiceClient(ProjectServiceClient const&) = default;
  ProjectServiceClient& operator=(ProjectServiceClient const&) = default;
  ProjectServiceClient(ProjectServiceClient&&) = default;
  ProjectServiceClient& operator=(ProjectServiceClient&&) = default;
  ///@}

  ///@{
  /// @name Equality
  friend bool operator==(ProjectServiceClient const& a,
                         ProjectServiceClient const& b) {
    return a.connection_ == b.connection_;
  }
  friend bool operator!=(ProjectServiceClient const& a,
                         ProjectServiceClient const& b) {
    return !(a == b);
  }
  ///@}

  // clang-format off
  ///
  /// Provisions the project resource. During the
  /// process, related systems will get prepared and initialized.
  ///
  /// Caller must read the [Terms for data
  /// use](https://cloud.google.com/retail/data-use-terms), and optionally
  /// specify in request to provide consent to that service terms.
  ///
  /// @param name  Required. Full resource name of a
  ///  [Project][google.cloud.discoveryengine.v1.Project], such as
  ///  `projects/{project_id_or_number}`.
  /// @param opts Optional. Override the class-level options, such as retry and
  ///     backoff policies.
  /// @return A [`future`] that becomes satisfied when the LRO
  ///     ([Long Running Operation]) completes or the polling policy in effect
  ///     for this call is exhausted. The future is satisfied with an error if
  ///     the LRO completes with an error or the polling policy is exhausted.
  ///     In this case the [`StatusOr`] returned by the future contains the
  ///     error. If the LRO completes successfully the value of the future
  ///     contains the LRO's result. For this RPC the result is a
  ///     [google.cloud.discoveryengine.v1.Project] proto message.
  ///     The C++ class representing this message is created by Protobuf, using
  ///     the [Protobuf mapping rules].
  ///
  /// [Protobuf mapping rules]: https://protobuf.dev/reference/cpp/cpp-generated/
  /// [input iterator requirements]: https://en.cppreference.com/w/cpp/named_req/InputIterator
  /// [Long Running Operation]: https://google.aip.dev/151
  /// [`std::string`]: https://en.cppreference.com/w/cpp/string/basic_string
  /// [`future`]: @ref google::cloud::future
  /// [`StatusOr`]: @ref google::cloud::StatusOr
  /// [`Status`]: @ref google::cloud::Status
  /// [google.cloud.discoveryengine.v1.Project]: @googleapis_reference_link{google/cloud/discoveryengine/v1/project.proto#L33}
  /// [google.cloud.discoveryengine.v1.ProvisionProjectRequest]: @googleapis_reference_link{google/cloud/discoveryengine/v1/project_service.proto#L65}
  ///
  // clang-format on
  future<StatusOr<google::cloud::discoveryengine::v1::Project>>
  ProvisionProject(std::string const& name, Options opts = {});

  // clang-format off
  ///
  /// @copybrief ProvisionProject
  ///
  /// Specifying the [`NoAwaitTag`] immediately returns the
  /// [`google::longrunning::Operation`] that corresponds to the Long Running
  /// Operation that has been started. No polling for operation status occurs.
  ///
  /// [`NoAwaitTag`]: @ref google::cloud::NoAwaitTag
  ///
  // clang-format on
  StatusOr<google::longrunning::Operation> ProvisionProject(
      NoAwaitTag, std::string const& name, Options opts = {});

  // clang-format off
  ///
  /// Provisions the project resource. During the
  /// process, related systems will get prepared and initialized.
  ///
  /// Caller must read the [Terms for data
  /// use](https://cloud.google.com/retail/data-use-terms), and optionally
  /// specify in request to provide consent to that service terms.
  ///
  /// @param request Unary RPCs, such as the one wrapped by this
  ///     function, receive a single `request` proto message which includes all
  ///     the inputs for the RPC. In this case, the proto message is a
  ///     [google.cloud.discoveryengine.v1.ProvisionProjectRequest].
  ///     Proto messages are converted to C++ classes by Protobuf, using the
  ///     [Protobuf mapping rules].
  /// @param opts Optional. Override the class-level options, such as retry and
  ///     backoff policies.
  /// @return A [`future`] that becomes satisfied when the LRO
  ///     ([Long Running Operation]) completes or the polling policy in effect
  ///     for this call is exhausted. The future is satisfied with an error if
  ///     the LRO completes with an error or the polling policy is exhausted.
  ///     In this case the [`StatusOr`] returned by the future contains the
  ///     error. If the LRO completes successfully the value of the future
  ///     contains the LRO's result. For this RPC the result is a
  ///     [google.cloud.discoveryengine.v1.Project] proto message.
  ///     The C++ class representing this message is created by Protobuf, using
  ///     the [Protobuf mapping rules].
  ///
  /// [Protobuf mapping rules]: https://protobuf.dev/reference/cpp/cpp-generated/
  /// [input iterator requirements]: https://en.cppreference.com/w/cpp/named_req/InputIterator
  /// [Long Running Operation]: https://google.aip.dev/151
  /// [`std::string`]: https://en.cppreference.com/w/cpp/string/basic_string
  /// [`future`]: @ref google::cloud::future
  /// [`StatusOr`]: @ref google::cloud::StatusOr
  /// [`Status`]: @ref google::cloud::Status
  /// [google.cloud.discoveryengine.v1.Project]: @googleapis_reference_link{google/cloud/discoveryengine/v1/project.proto#L33}
  /// [google.cloud.discoveryengine.v1.ProvisionProjectRequest]: @googleapis_reference_link{google/cloud/discoveryengine/v1/project_service.proto#L65}
  ///
  // clang-format on
  future<StatusOr<google::cloud::discoveryengine::v1::Project>>
  ProvisionProject(
      google::cloud::discoveryengine::v1::ProvisionProjectRequest const&
          request,
      Options opts = {});

  // clang-format off
  ///
  /// @copybrief ProvisionProject
  ///
  /// Specifying the [`NoAwaitTag`] immediately returns the
  /// [`google::longrunning::Operation`] that corresponds to the Long Running
  /// Operation that has been started. No polling for operation status occurs.
  ///
  /// [`NoAwaitTag`]: @ref google::cloud::NoAwaitTag
  ///
  // clang-format on
  StatusOr<google::longrunning::Operation> ProvisionProject(
      NoAwaitTag,
      google::cloud::discoveryengine::v1::ProvisionProjectRequest const&
          request,
      Options opts = {});

  // clang-format off
  ///
  /// @copybrief ProvisionProject
  ///
  /// This method accepts a `google::longrunning::Operation` that corresponds
  /// to a previously started Long Running Operation (LRO) and polls the status
  /// of the LRO in the background.
  ///
  // clang-format on
  future<StatusOr<google::cloud::discoveryengine::v1::Project>>
  ProvisionProject(google::longrunning::Operation const& operation,
                   Options opts = {});

 private:
  std::shared_ptr<ProjectServiceConnection> connection_;
  Options options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace discoveryengine_v1
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_DISCOVERYENGINE_V1_PROJECT_CLIENT_H
