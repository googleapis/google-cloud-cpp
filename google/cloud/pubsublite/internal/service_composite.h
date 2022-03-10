// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_SERVICE_COMPOSITE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_SERVICE_COMPOSITE_H

#include "google/cloud/pubsublite/internal/futures.h"
#include "google/cloud/pubsublite/internal/service.h"
#include <mutex>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

class ServiceComposite : public Service {
 public:
  template <class... ServiceT>
  explicit ServiceComposite(ServiceT*... dependencies)
      : dependencies_{dependencies...} {}

  future<Status> Start() override {
    future<Status> start_future;
    std::vector<future<Status>> dependency_futures;

    {
      std::lock_guard<std::mutex> g{mu_};
      for (Service* dependency : dependencies_) {
        dependency_futures.push_back(dependency->Start());
      }
      start_future = status_promise_->get_future();
    }

    for (auto& dependency_future : dependency_futures) {
      dependency_future.then([this](future<Status> status_future) {
        Status s = status_future.get();
        if (!s.ok()) Abort(std::move(s));
      });
    }

    return start_future;
  }

  /**
   * This will add a `Service` dependency for the current object to manage. It
   * is only added if the current object hasn't been `Shutdown` yet.
   * @param dependency
   */
  void AddServiceObject(Service* dependency) {
    future<Status> start_future;
    {
      // under lock to guarantee atomicity of being added to `dependencies_` and
      // `Start` being called so `Start` called on dependency if and only if
      // `Shutdown` will be called on dependency
      std::lock_guard<std::mutex> g{mu_};
      if (shutdown_) return;
      dependencies_.push_back(dependency);
      start_future = dependency->Start();
    }
    start_future.then([this](future<Status> status_future) {
      Status s = status_future.get();
      if (!s.ok()) Abort(std::move(s));
    });
  }

  /**
   * @note Can be safely called more than once.
   */
  void Abort(Status s) {
    assert(!s.ok());
    promise<Status> start_promise{null_promise_t{}};
    {
      std::lock_guard<std::mutex> g{mu_};
      if (!status_promise_) return;
      start_promise = std::move(*status_promise_);
      status_promise_.reset();
      final_status_ = s;
    }
    start_promise.set_value(std::move(s));
  }

  /**
   *
   * @return a `Status` of `kOk` if and only if the current lifecycle is in the
   * running phase
   */
  Status status() {
    std::lock_guard<std::mutex> g{mu_};
    return final_status_;
  }

  future<void> Shutdown() override {
    {
      std::lock_guard<std::mutex> g{mu_};
      if (shutdown_) return make_ready_future();
      shutdown_ = true;
    }
    Abort(Status{StatusCode::kAborted, "`Shutdown` called"});
    AsyncRoot root;
    future<void> root_future = root.get_future();

    std::lock_guard<std::mutex> g{mu_};
    for (auto* const dependency : dependencies_) {
      root_future = root_future.then(ChainFuture(dependency->Shutdown()));
    }
    return root_future;
  }

 private:
  std::mutex mu_;

  std::vector<Service*> dependencies_;  // ABSL_GUARDED_BY(mu_)
  bool shutdown_ = false;               // ABSL_GUARDED_BY(mu_)
  absl::optional<promise<Status>> status_promise_{
      promise<Status>{}};  // ABSL_GUARDED_BY(mu_)
  Status final_status_;    // ABSL_GUARDED_BY(mu_)
};

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_SERVICE_COMPOSITE_H
