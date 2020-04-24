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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_TESTING_TYPES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_TESTING_TYPES_H
/**
 * @file
 *
 * Implement types useful to test the behavior of template classes.
 *
 * Just like a function should be tested with different inputs, template classes
 * should be tested with types that have different characteristics. For example,
 * it is often interesting to test a template with a type that lacks a default
 * constructor. This file implements some types that we have found useful for
 * testing template classes.
 */

#include "google/cloud/log.h"
#include <utility>
#include <vector>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {
/// A class without a default constructor.
class NoDefaultConstructor {
 public:
  NoDefaultConstructor() = delete;
  explicit NoDefaultConstructor(std::string x) : str_(std::move(x)) {}

  std::string str() const { return str_; }

 private:
  friend bool operator==(NoDefaultConstructor const& lhs,
                         NoDefaultConstructor const& rhs);

  std::string str_;
};

inline bool operator==(NoDefaultConstructor const& lhs,
                       NoDefaultConstructor const& rhs) {
  return lhs.str_ == rhs.str_;
}

inline bool operator!=(NoDefaultConstructor const& lhs,
                       NoDefaultConstructor const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

/**
 * A class that counts calls to its constructors.
 */
class Observable {
 public:
  static int default_constructor;
  static int value_constructor;
  static int copy_constructor;
  static int move_constructor;
  static int copy_assignment;
  static int move_assignment;
  static int destructor;

  static void reset_counters() {
    default_constructor = 0;
    value_constructor = 0;
    copy_constructor = 0;
    move_constructor = 0;
    copy_assignment = 0;
    move_assignment = 0;
    destructor = 0;
  }

  Observable() { ++default_constructor; }
  explicit Observable(std::string s) : str_(std::move(s)) {
    ++value_constructor;
  }
  Observable(Observable const& rhs) : str_(rhs.str_) { ++copy_constructor; }
  Observable(Observable&& rhs) noexcept : str_(std::move(rhs.str_)) {
    rhs.str_ = "moved-out";
    ++move_constructor;
  }

  Observable& operator=(Observable const& rhs) {
    str_ = rhs.str_;
    ++copy_assignment;
    return *this;
  }
  Observable& operator=(Observable&& rhs) noexcept {
    str_ = std::move(rhs.str_);
    rhs.str_ = "moved-out";
    ++move_assignment;
    return *this;
  }
  ~Observable() { ++destructor; }

  std::string const& str() const { return str_; }

 private:
  std::string str_;
};

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_TESTING_TYPES_H
