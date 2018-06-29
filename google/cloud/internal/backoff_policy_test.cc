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

#include "google/cloud/internal/backoff_policy.h"
#include <gmock/gmock.h>
#include <chrono>
#include <vector>

using google::cloud::internal::ExponentialBackoffPolicy;
using ms = std::chrono::milliseconds;

/// @test A simple test for the ExponentialBackoffPolicy.
TEST(ExponentialBackoffPolicy, Simple) {
  ExponentialBackoffPolicy tested(ms(10), ms(100), 2.0);

  auto delay = tested.OnCompletion();
  EXPECT_LE(ms(10), delay);
  EXPECT_GE(ms(20), delay);
  delay = tested.OnCompletion();
  EXPECT_LE(ms(20), delay);
  EXPECT_GE(ms(40), delay);
  delay = tested.OnCompletion();
  EXPECT_LE(ms(40), delay);
  EXPECT_GE(ms(80), delay);
  delay = tested.OnCompletion();
  EXPECT_LE(ms(50), delay);
  EXPECT_GE(ms(100), delay);
}

/// @test Verify that the scaling factor is validated.
TEST(ExponentialBackoffPolicy, ValidateScaling) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ExponentialBackoffPolicy(ms(10), ms(50), 0.0),
               std::invalid_argument);
  EXPECT_THROW(ExponentialBackoffPolicy(ms(10), ms(50), 1.0),
               std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ExponentialBackoffPolicy(ms(10), ms(50), 0.0),
                            "exceptions are disabled");
  EXPECT_DEATH_IF_SUPPORTED(ExponentialBackoffPolicy(ms(10), ms(50), 1.0),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that less common arguments work.
TEST(ExponentialBackoffPolicy, DifferentParameters) {
  ExponentialBackoffPolicy tested(ms(100), std::chrono::seconds(10), 1.5);

  auto delay = tested.OnCompletion();
  EXPECT_LE(ms(100), delay) << "delay=" << delay.count() << "ms";
  EXPECT_GE(ms(200), delay) << "delay=" << delay.count() << "ms";
  delay = tested.OnCompletion();
  EXPECT_LE(ms(150), delay) << "delay=" << delay.count() << "ms";
  EXPECT_GE(ms(300), delay) << "delay=" << delay.count() << "ms";
  delay = tested.OnCompletion();
  EXPECT_LE(ms(225), delay) << "delay=" << delay.count() << "ms";
  EXPECT_GE(ms(450), delay) << "delay=" << delay.count() << "ms";
}

/// @test Test cloning for ExponentialBackoffPolicy.
TEST(ExponentialBackoffPolicy, Clone) {
  ExponentialBackoffPolicy original(ms(10), ms(50), 2.0);
  auto tested = original.clone();

  auto delay = tested->OnCompletion();
  EXPECT_LE(ms(10), delay);
  EXPECT_GE(ms(20), delay);
  delay = tested->OnCompletion();
  EXPECT_LE(ms(20), delay);
  EXPECT_GE(ms(40), delay);
  delay = tested->OnCompletion();
  EXPECT_LE(ms(25), delay);
  EXPECT_GE(ms(50), delay);
  delay = tested->OnCompletion();
  EXPECT_LE(ms(25), delay);
  EXPECT_GE(ms(50), delay);
}

/// @test Test for testing randomness for 2 objects of
/// ExponentialBackoffPolicy such that no two clients have same sleep time.
TEST(ExponentialBackoffPolicy, Randomness) {
  ExponentialBackoffPolicy test_object1(ms(10), ms(1500), 2.0);
  ExponentialBackoffPolicy test_object2(ms(10), ms(1500), 2.0);
  std::vector<int> output1, output2;

  auto delay = test_object1.OnCompletion();
  EXPECT_LE(ms(10), delay);
  EXPECT_GE(ms(20), delay);
  test_object2.OnCompletion();
  EXPECT_LE(ms(10), delay);
  EXPECT_GE(ms(20), delay);

  for (int i = 0; i != 100; ++i) {
    output1.push_back(test_object1.OnCompletion().count());
    output2.push_back(test_object2.OnCompletion().count());
  }
  EXPECT_NE(output1, output2);
}
