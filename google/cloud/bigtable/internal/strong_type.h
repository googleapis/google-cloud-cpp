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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_STRONG_TYPE_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_STRONG_TYPE_H_

#include "google/cloud/bigtable/version.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * A simple implementation of the strong type C++ idiom.
 *
 * In many cases the same class or type (`int`, `double`, `std::string`) is used
 * to represent very different things. It is desirable to create a strong type
 * that wraps the basic type to avoid common mistakes in an API, such as
 * passing parameters in the wrong order. For example, consider a function that
 * takes a distance and speed parameters:
 * @code
 *  double f(double speed, double distance);
 * @endcode
 *
 * This function can used (by mistake), as:
 * @code
 * double my_distance = ..;
 * double my_speed = ...;
 * double x = f(my_distance, my_speed);
 * @endcode
 *
 * One can avoid those mistakes by using a `StrongType`:
 * @code
 * using Distance = StrongType<double, struct DistanceTag>;
 * using Speed = StrongType<double, struct SpeedTag>;
 * double f(Speed speed, Distance distance);
 * @endcode
 *
 * @tparam T the type wrapped by the strong type.
 * @tparam Parameter a formal parameter to create different instantiations of
 * `StrongType<T, ...>`.
 */
template <typename T, typename Parameter>
class StrongType {
 public:
  explicit StrongType(T const& value) : value_(value) {}
  explicit StrongType(T&& value) : value_(std::move(value)) {}
  T& get() { return value_; }
  T const& get() const { return value_; }

 private:
  T value_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_STRONG_TYPE_H_
