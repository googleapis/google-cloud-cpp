// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/publisher.h"
#include "google/cloud/pubsub/subscriber.h"
#include "google/cloud/pubsub/subscription_admin_client.h"
#include "google/cloud/pubsub/testing/random_names.h"
#include "google/cloud/pubsub/topic_admin_client.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/command_line_parsing.h"
#include "absl/memory/memory.h"
#include <chrono>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace {
namespace pubsub = ::google::cloud::pubsub;
using ::google::cloud::future;
using ::google::cloud::Status;
using ::google::cloud::StatusOr;

auto constexpr kDescription = R"""(
An endurance test for the Cloud Pub/Sub C++ client library.

This experiment is largely a torture test for the library. The objective is to
detect bugs that escape unit and integration tests. Such tests are typically
short-lived and predictable, so we write a test that is long-lived and
unpredictable to find problems that would go otherwise unnoticed.

The test creates a number of threads publishing messages and a number of
subscription sessions. Periodically these publishers and subscriptions are
replaced with new ones.

For flow control purposes, the benchmark keeps a limited number of messages in
flight.
)""";

struct Config {
  std::string project_id;
  std::string topic_id;

  std::int64_t pending_lwm = 10 * 1000;
  std::int64_t pending_hwm = 100 * 1000;

  int publisher_count = 4;
  int subscription_count = 4;
  int session_count = 8;

  std::int64_t minimum_samples = 30 * 1000;
  std::int64_t maximum_samples = (std::numeric_limits<std::int64_t>::max)();
  std::chrono::seconds minimum_runtime = std::chrono::seconds(5);
  std::chrono::seconds maximum_runtime = std::chrono::seconds(300);

  bool show_help = false;
};

StatusOr<Config> ParseArgs(std::vector<std::string> args);

class ExperimentFlowControl {
 public:
  ExperimentFlowControl(int subscription_count, std::int64_t lwm,
                        std::int64_t hwm)
      : subscription_count_(subscription_count), lwm_(lwm), hwm_(hwm) {}

  pubsub::Message GenerateMessage(int task);
  void Published(bool success);
  void Received(pubsub::Message const&);
  void Shutdown();

  std::vector<std::chrono::microseconds> ClearSamples() {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<std::chrono::microseconds> tmp;
    tmp.swap(samples_);
    return tmp;
  }

  void Debug(std::ostream& os) const {
    std::lock_guard<std::mutex> lk(mu_);
    os << "subscription_count=" << subscription_count_ << ", lwm=" << lwm_
       << ", hwm=" << hwm_ << ", pending=" << pending_
       << ", sent=" << sent_count_ << ", received=" << received_count_
       << ", expected=" << expected_count_ << ", failures=" << failures_
       << ", overflow=" << overflow_ << ", shutdown=" << shutdown_
       << ", samples.size()=" << samples_.size();
  }

  std::int64_t SentCount() const {
    std::lock_guard<std::mutex> lk(mu_);
    return sent_count_;
  }

  std::int64_t ExpectedCount() const {
    std::lock_guard<std::mutex> lk(mu_);
    return expected_count_;
  }

  std::int64_t ReceivedCount() const {
    std::lock_guard<std::mutex> lk(mu_);
    return received_count_;
  }

  std::int64_t WaitReceivedCount(std::int64_t count) {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [&] { return received_count_ >= count; });
    return received_count_;
  }

 private:
  int const subscription_count_;
  std::int64_t const lwm_;
  std::int64_t const hwm_;

  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::int64_t pending_ = 0;
  std::int64_t sent_count_ = 0;
  std::int64_t expected_count_ = 0;
  std::int64_t received_count_ = 0;
  std::int64_t failures_ = 0;
  bool overflow_ = false;
  bool shutdown_ = false;
  std::vector<std::chrono::microseconds> samples_;
};

bool ExperimentCompleted(Config const& config,
                         ExperimentFlowControl const& flow_control,
                         std::chrono::steady_clock::time_point start);

void PublisherTask(Config const& config, ExperimentFlowControl& flow_control,
                   int task);

class Cleanup {
 public:
  Cleanup() = default;
  ~Cleanup() {
    for (auto i = actions_.rbegin(); i != actions_.rend(); ++i) (*i)();
  }
  void Defer(std::function<void()> f) { actions_.push_back(std::move(f)); }

 private:
  std::vector<std::function<void()>> actions_;
};

}  // namespace

int main(int argc, char* argv[]) {
  auto config = ParseArgs({argv, argv + argc});
  if (!config) {
    std::cerr << "Error parsing command-line arguments\n";
    std::cerr << config.status() << "\n";
    return 1;
  }
  if (config->show_help) return 0;

  pubsub::TopicAdminClient topic_admin(pubsub::MakeTopicAdminConnection());
  pubsub::SubscriptionAdminClient subscription_admin(
      pubsub::MakeSubscriptionAdminConnection());

  auto generator = google::cloud::internal::MakeDefaultPRNG();

  auto const configured_topic = config->topic_id;

  // If there is no pre-defined topic for this test, create one and
  // automatically remove it at the end of the test.
  std::function<void()> delete_topic = [] {};
  if (config->topic_id.empty()) {
    config->topic_id = google::cloud::pubsub_testing::RandomTopicId(generator);
    auto topic = pubsub::Topic(config->project_id, config->topic_id);
    auto create = topic_admin.CreateTopic(pubsub::TopicBuilder{topic});
    if (!create) {
      std::cout << "CreateTopic() failed: " << create.status() << "\n";
      return 1;
    }
    delete_topic = [topic_admin, topic]() mutable {
      (void)topic_admin.DeleteTopic(topic);
    };
  }

  std::cout << "# Running Cloud Pub/Sub experiment"
            << "\n# Start time: "
            << google::cloud::internal::FormatRfc3339(
                   std::chrono::system_clock::now())
            << "\n# Configured topic: " << configured_topic
            << "\n# Actual topic: " << config->topic_id
            << "\n# Flow Control LWM: " << config->pending_lwm
            << "\n# Flow Control HWM: " << config->pending_hwm
            << "\n# Publisher Count: " << config->publisher_count
            << "\n# Subscription Count: " << config->subscription_count
            << "\n# Session Count: " << config->session_count
            << "\n# Minimum Samples: " << config->minimum_samples
            << "\n# Maximum Samples: " << config->maximum_samples
            << "\n# Minimum Runtime: " << config->minimum_runtime.count() << "s"
            << "\n# Maximum Runtime: " << config->maximum_runtime.count() << "s"
            << std::endl;

  auto const topic = pubsub::Topic(config->project_id, config->topic_id);
  auto const subscriptions = [&] {
    std::vector<pubsub::Subscription> subscriptions;
    for (int i = 0; i != config->subscription_count; ++i) {
      auto sub = pubsub::Subscription(
          config->project_id,
          google::cloud::pubsub_testing::RandomSubscriptionId(generator));
      auto create = subscription_admin.CreateSubscription(topic, sub);
      if (!create) continue;
      subscriptions.push_back(std::move(sub));
    }
    return subscriptions;
  }();
  if (subscriptions.empty()) {
    std::cerr << "Could not create any subscriptions\n";
    return 1;
  }

  ExperimentFlowControl flow_control(config->subscription_count,
                                     config->pending_lwm, config->pending_hwm);

  auto handler = [&](pubsub::Message const& m, pubsub::AckHandler h) {
    std::move(h).ack();
    flow_control.Received(m);
  };

  std::vector<std::unique_ptr<pubsub::Subscriber>> subscribers;
  std::vector<future<google::cloud::Status>> sessions;
  for (auto i = 0; i != config->session_count; ++i) {
    auto const& subscription = subscriptions[i % subscriptions.size()];
    auto subscriber = absl::make_unique<pubsub::Subscriber>(
        pubsub::MakeSubscriberConnection(subscription));
    sessions.push_back(subscriber->Subscribe(handler));
    subscribers.push_back(std::move(subscriber));
  }
  auto cleanup_sessions = [&] {
    for (auto& s : sessions) s.cancel();
    for (auto& s : sessions) s.wait_for(std::chrono::seconds(3));
    sessions.clear();
  };

  auto tasks = [&] {
    std::vector<std::thread> tasks(config->publisher_count);
    int task_id = 0;
    std::generate(tasks.begin(), tasks.end(), [&] {
      return std::thread{PublisherTask, std::cref(*config),
                         std::ref(flow_control), task_id++};
    });
    return tasks;
  }();

  auto ts = [] {
    return google::cloud::internal::FormatRfc3339(
        std::chrono::system_clock::now());
  };

  auto divide_duration = [](std::chrono::microseconds duration, int count,
                            std::chrono::microseconds min,
                            std::chrono::microseconds max) {
    auto const d = duration / count;
    if (d < min) return min;
    if (d > max) return max;
    return d;
  };

  auto const cycle =
      divide_duration(config->minimum_runtime, 50, std::chrono::seconds(10),
                      std::chrono::seconds(60));

  auto const report_interval =
      divide_duration(config->minimum_runtime, 100, std::chrono::seconds(5),
                      std::chrono::seconds(15));

  auto const start = std::chrono::steady_clock::now();
  auto report_deadline = start + report_interval;

  std::uniform_int_distribution<std::size_t> session_selector(
      0, sessions.size() - 1);
  std::cout << "Timestamp,RunningCount,Count,Min,Max,Average(us)\n";

  Cleanup cleanup;
  cleanup.Defer(delete_topic);
  for (auto const& sub : subscriptions) {
    cleanup.Defer([subscription_admin, sub]() mutable {
      (void)subscription_admin.DeleteSubscription(sub);
    });
  }

  while (!ExperimentCompleted(*config, flow_control, start)) {
    std::this_thread::sleep_for(cycle);
    auto const idx = session_selector(generator);
    sessions[idx].cancel();
    sessions[idx] = subscribers[idx]->Subscribe(handler);

    auto const now = std::chrono::steady_clock::now();
    if (now >= report_deadline) {
      report_deadline = now + report_interval;
      auto const samples = flow_control.ClearSamples();
      if (samples.empty()) {
        std::cout << "# " << ts() << ',';
        flow_control.Debug(std::cout);
        std::cout << std::endl;
      } else {
        auto const p = std::minmax_element(samples.begin(), samples.end());
        auto const sum = std::accumulate(samples.begin(), samples.end(),
                                         std::chrono::microseconds{0});
        auto const mean = sum / samples.size();
        auto const received_count = flow_control.ReceivedCount();
        std::cout << ts() << ',' << received_count << ',' << samples.size()
                  << ',' << p.first->count() << ',' << p.second->count() << ','
                  << mean.count() << std::endl;
      }
    }
  }

  flow_control.Shutdown();
  for (auto& t : tasks) t.join();
  auto const sent_count = flow_control.SentCount();
  std::cout << "# " << ts() << " - sent: " << sent_count << " messages"
            << std::endl;

  auto const expected = flow_control.ExpectedCount();
  // Wait until about 95% of the messages are received.
  for (int p : {50, 60, 70, 80, 90, 95, 98, 99}) {
    auto const received_count =
        flow_control.WaitReceivedCount(expected * p / 100);
    std::cout << "# " << ts() << " - received at least " << p << "% ["
              << received_count << " / " << expected
              << "] of the expected messages" << std::endl;
  }

  std::cout << "# " << ts() << " - received: " << flow_control.ReceivedCount()
            << " messages" << std::endl;
  cleanup_sessions();
  std::cout << "# " << ts() << " - received: " << flow_control.ReceivedCount()
            << " messages" << std::endl;

  return 0;
}

namespace {

pubsub::Message ExperimentFlowControl::GenerateMessage(int task) {
  std::unique_lock<std::mutex> lk(mu_);
  cv_.wait(lk, [&] { return shutdown_ || !overflow_; });
  ++pending_;
  ++sent_count_;
  if (pending_ + (expected_count_ - received_count_) >= hwm_) overflow_ = true;
  auto const ts = std::to_string(
      std::chrono::steady_clock::now().time_since_epoch().count());
  if (shutdown_) {
    return pubsub::MessageBuilder{}
        .SetData("shutdown:" + std::to_string(task))
        .SetAttributes({{"timestamp", ts}})
        .Build();
  }
  return pubsub::MessageBuilder{}
      .SetData("task:" + std::to_string(task))
      .SetAttributes({{"timestamp", ts}})
      .Build();
}

void ExperimentFlowControl::Published(bool success) {
  std::unique_lock<std::mutex> lk(mu_);
  --pending_;
  if (success) {
    expected_count_ += subscription_count_;
  } else {
    ++failures_;
  }
  if ((expected_count_ - received_count_) >= hwm_) overflow_ = true;
}

void ExperimentFlowControl::Received(pubsub::Message const& m) {
  auto const now = std::chrono::steady_clock::now();
  auto const message_timestamp = [&m] {
    std::chrono::steady_clock::duration ts{0};
    for (auto const& kv : m.attributes()) {
      if (kv.first == "timestamp") {
        ts = std::chrono::steady_clock::duration(std::stoll(kv.second));
        break;
      }
    }
    return std::chrono::steady_clock::time_point{} + ts;
  }();
  auto const elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
      now - message_timestamp);
  std::unique_lock<std::mutex> lk(mu_);
  ++received_count_;
  samples_.push_back(elapsed);
  if (expected_count_ - received_count_ > lwm_) return;
  overflow_ = false;
  cv_.notify_all();
}

void ExperimentFlowControl::Shutdown() {
  std::unique_lock<std::mutex> lk(mu_);
  shutdown_ = true;
  cv_.notify_all();
}

bool ExperimentCompleted(Config const& config,
                         ExperimentFlowControl const& flow_control,
                         std::chrono::steady_clock::time_point start) {
  auto const now = std::chrono::steady_clock::now();
  auto const samples = flow_control.ReceivedCount();
  if (now >= start + config.maximum_runtime) return true;
  if (samples >= config.maximum_samples) return true;
  if (now < start + config.minimum_runtime) return false;
  return samples >= config.minimum_samples;
}

void PublisherTask(Config const& config, ExperimentFlowControl& flow_control,
                   int task) {
  auto make_publisher = [config, task] {
    auto const topic = pubsub::Topic(config.project_id, config.topic_id);
    return pubsub::Publisher(pubsub::MakePublisherConnection(
        topic, {},
        pubsub::ConnectionOptions{}.set_channel_pool_domain(
            "publisher:" + std::to_string(task))));
  };
  auto publisher = make_publisher();

  using clock = std::chrono::steady_clock;
  auto const start = clock::now();
  auto next_refresh = start + std::chrono::seconds(30);
  future<void> last_publish;
  while (!ExperimentCompleted(config, flow_control, start)) {
    auto ts = clock::now();
    if (ts >= next_refresh) {
      next_refresh = ts + std::chrono::seconds(30);
      publisher.Flush();
      auto d = std::move(last_publish);
      if (d.valid()) d.get();
      publisher = make_publisher();
    }
    auto message = flow_control.GenerateMessage(task);
    auto const shutdown = message.data().rfind("shutdown:", 0) == 0;
    last_publish = publisher.Publish(std::move(message))
                       .then([&flow_control](future<StatusOr<std::string>> f) {
                         flow_control.Published(f.get().ok());
                       });
    if (shutdown) break;
  }
  publisher.Flush();
  if (last_publish.valid()) last_publish.get();
}

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::OptionDescriptor;
using ::google::cloud::testing_util::ParseDuration;

google::cloud::StatusOr<Config> ParseArgsImpl(std::vector<std::string> args,
                                              std::string const& description) {
  Config options;
  options.project_id = GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  bool show_help = false;
  bool show_description = false;

  std::vector<OptionDescriptor> desc{
      {"--help", "print usage information",
       [&show_help](std::string const&) { show_help = true; }},
      {"--description", "print benchmark description",
       [&show_description](std::string const&) { show_description = true; }},
      {"--project-id", "use the given project id for the benchmark",
       [&options](std::string const& val) { options.project_id = val; }},
      {"--topic-id", "use an existing topic for the benchmark",
       [&options](std::string const& val) { options.topic_id = val; }},
      {"--pending-lwm", "flow control from publisher to subscriber",
       [&options](std::string const& val) {
         options.pending_lwm = std::stol(val);
       }},
      {"--pending-hwm", "flow control from publisher to subscriber",
       [&options](std::string const& val) {
         options.pending_hwm = std::stol(val);
       }},
      {"--publisher-count", "number of publishing threads",
       [&options](std::string const& val) {
         options.publisher_count = std::stoi(val);
       }},
      {"--subscription-count", "number of subscriptions",
       [&options](std::string const& val) {
         options.subscription_count = std::stoi(val);
       }},
      {"--session-count", "number of subscription sessions",
       [&options](std::string const& val) {
         options.session_count = std::stoi(val);
       }},
      {"--minimum-samples", "minimum number of samples to capture",
       [&options](std::string const& val) {
         options.minimum_samples = std::stoi(val);
       }},
      {"--maximum-samples", "maximum number of samples to capture",
       [&options](std::string const& val) {
         options.maximum_samples = std::stoi(val);
       }},
      {"--minimum-runtime", "run for at least this time",
       [&options](std::string const& val) {
         options.minimum_runtime = ParseDuration(val);
       }},
      {"--maximum-runtime", "run for at most this time",
       [&options](std::string const& val) {
         options.maximum_runtime = ParseDuration(val);
       }},
  };
  auto const usage = BuildUsage(desc, args[0]);
  auto unparsed = OptionsParse(desc, args);

  if (show_description) {
    std::cout << description << "\n\n";
  }

  if (show_help) {
    std::cout << usage << "\n";
    options.show_help = true;
    return options;
  }

  if (options.project_id.empty()) {
    return google::cloud::Status(google::cloud::StatusCode::kInvalidArgument,
                                 "missing or empty --project-id option");
  }

  return options;
}

google::cloud::StatusOr<Config> SelfTest(std::string const& cmd) {
  auto error = [](std::string m) {
    return google::cloud::Status(google::cloud::StatusCode::kUnknown,
                                 std::move(m));
  };
  for (auto const& var : {"GOOGLE_CLOUD_PROJECT"}) {
    auto const value = GetEnv(var).value_or("");
    if (!value.empty()) continue;
    std::ostringstream os;
    os << "The environment variable " << var << " is not set or empty";
    return error(std::move(os).str());
  }
  auto config = ParseArgsImpl({cmd, "--help"}, kDescription);
  if (!config || !config->show_help) return error("--help parsing");
  config = ParseArgsImpl({cmd, "--description", "--help"}, kDescription);
  if (!config || !config->show_help) return error("--description parsing");
  config = ParseArgsImpl({cmd, "--project-id="}, kDescription);
  if (config) return error("--project-id validation");
  config = ParseArgsImpl({cmd, "--topic-id=test-topic"}, kDescription);
  if (!config) return error("--topic-id");

  return ParseArgsImpl(
      {
          cmd,
          "--project-id=" + GetEnv("GOOGLE_CLOUD_PROJECT").value_or(""),
          "--publisher-count=1",
          "--subscription-count=1",
          "--pending-lwm=8000",
          "--pending-hwm=10000",
          "--session-count=1",
          "--minimum-samples=1",
          "--maximum-samples=10",
          "--minimum-runtime=0s",
          "--maximum-runtime=2s",
      },
      kDescription);
}

google::cloud::StatusOr<Config> ParseArgs(std::vector<std::string> args) {
  bool auto_run =
      GetEnv("GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES").value_or("") == "yes";
  if (auto_run) return SelfTest(args[0]);
  return ParseArgsImpl(std::move(args), kDescription);
}

}  // namespace
