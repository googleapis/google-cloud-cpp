// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/trace/trace_client.h"
#include "google/cloud/project.h"
#include <google/protobuf/util/time_util.h>
#include <iostream>
#include <random>
#include <thread>

std::string RandomHexDigits(std::mt19937_64& gen, int count) {
  auto const digits = std::string{"0123456789abcdef"};
  std::string sample;
  std::generate_n(std::back_inserter(sample), count, [&] {
    auto n = digits.size() - 1;
    return digits[std::uniform_int_distribution<std::size_t>(0, n)(gen)];
  });
  return sample;
}

int main(int argc, char* argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " project-id\n";
    return 1;
  }

  namespace trace = ::google::cloud::trace;
  namespace v2 = ::google::devtools::cloudtrace::v2;
  using ::google::protobuf::util::TimeUtil;

  auto client = trace::TraceServiceClient(trace::MakeTraceServiceConnection());

  // Create a span ID using some random hex digits.
  auto gen = std::mt19937_64(std::random_device{}());
  v2::Span span;
  auto span_id = RandomHexDigits(gen, 16);
  span.set_name(std::string{"projects/"} + argv[1] + "/traces/" +
                RandomHexDigits(gen, 32) + "/spans/" + span_id);
  span.set_span_id(std::move(span_id));
  *span.mutable_start_time() = TimeUtil::GetCurrentTime();
  // Simulate a call using a small sleep
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  *span.mutable_end_time() = TimeUtil::GetCurrentTime();

  auto response = client.CreateSpan(span);
  if (!response) throw std::move(response).status();
  std::cout << response->DebugString() << "\n";

  return 0;
} catch (google::cloud::Status const& status) {
  std::cerr << "google::cloud::Status thrown: " << status << "\n";
  return 1;
}
