// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_LOG_WRAPPER_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_LOG_WRAPPER_H_

#include "google/cloud/spanner/version.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/log.h"
#include "google/cloud/status_or.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

template <typename T>
struct IsStatusOr : public std::false_type {};
template <typename T>
struct IsStatusOr<StatusOr<T>> : public std::true_type {};

template <typename T>
struct IsUniquePtr : public std::false_type {};
template <typename T>
struct IsUniquePtr<std::unique_ptr<T>> : public std::true_type {};

template <typename T>
struct IsFutureStatusOr : public std::false_type {};
template <typename T>
struct IsFutureStatusOr<future<StatusOr<T>>> : public std::true_type {};

template <
    typename Functor, typename Request,
    typename Result = google::cloud::internal::invoke_result_t<
        Functor, grpc::ClientContext&, Request const&>,
    typename std::enable_if<std::is_same<Result, google::cloud::Status>::value,
                            int>::type = 0>
Result LogWrapper(Functor&& functor, grpc::ClientContext& context,
                  Request const& request, char const* where) {
  GCP_LOG(DEBUG) << where << "() << " << request.DebugString();
  auto response = functor(context, request);
  GCP_LOG(DEBUG) << where << "() >> status=" << response;
  return response;
}

template <typename Functor, typename Request,
          typename Result = google::cloud::internal::invoke_result_t<
              Functor, grpc::ClientContext&, Request const&>,
          typename std::enable_if<IsStatusOr<Result>::value, int>::type = 0>
Result LogWrapper(Functor&& functor, grpc::ClientContext& context,
                  Request const& request, char const* where) {
  GCP_LOG(DEBUG) << where << "() << " << request.DebugString();
  auto response = functor(context, request);
  if (!response) {
    GCP_LOG(DEBUG) << where << "() >> status=" << response.status();
  } else {
    GCP_LOG(DEBUG) << where << "() >> response=" << response->DebugString();
  }
  return response;
}

template <typename Functor, typename Request,
          typename Result = google::cloud::internal::invoke_result_t<
              Functor, grpc::ClientContext&, Request const&>,
          typename std::enable_if<IsUniquePtr<Result>::value, int>::type = 0>
Result LogWrapper(Functor&& functor, grpc::ClientContext& context,
                  Request const& request, char const* where) {
  GCP_LOG(DEBUG) << where << "() << " << request.DebugString();
  auto response = functor(context, request);
  GCP_LOG(DEBUG) << where << "() >> " << (response ? "not null" : "null")
                 << " stream";
  return response;
}

template <
    typename Functor, typename Request,
    typename Result =
        google::cloud::internal::invoke_result_t<Functor, Request>,
    typename std::enable_if<IsFutureStatusOr<Result>::value, int>::type = 0>
Result LogWrapper(Functor&& functor, Request request, char const* where) {
  GCP_LOG(DEBUG) << where << "() << " << request.DebugString();
  auto response = functor(std::move(request));
  // We cannot log the value of the future, even when it is available, because
  // the value can only be extracted once. But we can log if the future is
  // satisfied.
  auto status = response.wait_for(std::chrono::microseconds(0));
  char const* msg = "a future in an unknown state";
  switch (status) {
    case std::future_status::ready:
      msg = "a ready future";
      break;
    case std::future_status::timeout:
      msg = "an unsatisfied future";
      break;
    case std::future_status::deferred:
      msg = "a deferred future";
      break;
  }
  GCP_LOG(DEBUG) << where << "() >> " << msg;
  return response;
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_LOG_WRAPPER_H_
