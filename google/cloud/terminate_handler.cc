// Copyright 2018 Google LLC
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

#include "google/cloud/terminate_handler.h"
#include <iostream>
#include <mutex>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {

class TerminateFunction {
 public:
  explicit TerminateFunction(TerminateHandler f) : f_(std::move(f)) {}

  TerminateHandler Get() {
    std::lock_guard<std::mutex> l(m_);
    return f_;
  }

  TerminateHandler Set(TerminateHandler f) {
    std::lock_guard<std::mutex> l(m_);
    f.swap(f_);
    return f;
  }

 private:
  std::mutex m_;
  TerminateHandler f_;
};

TerminateFunction& GetTerminateHolder() {
  static TerminateFunction f([](char const* msg) {
    std::cerr << "Aborting because exceptions are disabled: " << msg << "\n";
    std::abort();
  });
  return f;
}

}  // anonymous namespace

TerminateHandler SetTerminateHandler(TerminateHandler f) {
  return GetTerminateHolder().Set(std::move(f));
}

TerminateHandler GetTerminateHandler() { return GetTerminateHolder().Get(); }

[[noreturn]] void Terminate(char const* msg) {
  GetTerminateHolder().Get()(msg);
  std::cerr << "Aborting because the installed terminate handler returned. "
               "Error details: "
            << msg << "\n";
  std::abort();
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
