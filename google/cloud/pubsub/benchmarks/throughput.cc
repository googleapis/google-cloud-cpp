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
#include "google/cloud/testing_util/timer.h"
#include "absl/strings/str_format.h"
#include <chrono>
#include <cinttypes>
#include <functional>
#include <sstream>
#include <string>

namespace {
namespace pubsub = ::google::cloud::pubsub;
using ::google::cloud::future;
using ::google::cloud::Status;
using ::google::cloud::StatusOr;
using ::google::cloud::testing_util::FormatSize;
using ::google::cloud::testing_util::kMiB;

auto constexpr kDescription = R"""(
A throughput vs. CPU benchmark for the Cloud Pub/Sub C++ client library.

Measure the throughput for publishers and/or subscribers in the Cloud Pub/Sub
C++ client library.
)""";

struct Config {
  std::string project_id;
  std::string topic_id;
  std::string subscription_id;

  std::int64_t payload_size = 1024;
  std::chrono::seconds iteration_duration = std::chrono::seconds(5);

  bool publisher = false;
  int publisher_thread_count = 1;
  int publisher_io_threads = 0;
  int publisher_max_batch_size = 1000;
  std::int64_t publisher_max_batch_bytes = 10 * kMiB;
  std::int64_t publisher_pending_lwm = 9 * 1000000;
  std::int64_t publisher_pending_hwm = 10 * 1000000;

  bool subscriber = false;
  int subscriber_thread_count = 1;
  int subscriber_io_threads = 0;
  int subscriber_max_outstanding_messages = 0;
  std::int64_t subscriber_max_outstanding_bytes = 100 * kMiB;
  int subscriber_max_concurrency = 0;

  std::int64_t minimum_samples = 10;
  std::int64_t maximum_samples = (std::numeric_limits<std::int64_t>::max)();
  std::chrono::seconds minimum_runtime = std::chrono::seconds(5);
  std::chrono::seconds maximum_runtime = std::chrono::seconds(300);

  bool show_help = false;
};

StatusOr<Config> ParseArgs(std::vector<std::string> args);

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

void PublisherTask(Config const& config);
void SubscriberTask(Config const& config);

}  // namespace

int main(int argc, char* argv[]) {
  auto config = ParseArgs({argv, argv + argc});
  if (!config) {
    std::cerr << "Error parsing command-line arguments\n";
    std::cerr << config.status() << "\n";
    return 1;
  }
  if (config->show_help) return 0;
  auto const configured_topic = config->topic_id;

  auto generator = google::cloud::internal::MakeDefaultPRNG();

  Cleanup cleanup;
  // If there is no pre-defined topic and/or subscription for this test, create
  // them and automatically remove them at the end of the test.
  if (config->topic_id.empty()) {
    pubsub::TopicAdminClient topic_admin(pubsub::MakeTopicAdminConnection());
    config->topic_id = google::cloud::pubsub_testing::RandomTopicId(generator);
    auto topic = pubsub::Topic(config->project_id, config->topic_id);
    auto create = topic_admin.CreateTopic(pubsub::TopicBuilder{topic});
    if (!create) {
      std::cout << "CreateTopic() failed: " << create.status() << "\n";
      return 1;
    }
    cleanup.Defer([topic_admin, topic]() mutable {
      (void)topic_admin.DeleteTopic(topic);
    });
  }

  if (config->subscription_id.empty()) {
    pubsub::SubscriptionAdminClient subscription_admin(
        pubsub::MakeSubscriptionAdminConnection());
    config->subscription_id =
        google::cloud::pubsub_testing::RandomSubscriptionId(generator);
    auto topic = pubsub::Topic(config->project_id, config->topic_id);
    auto subscription =
        pubsub::Subscription(config->project_id, config->subscription_id);
    auto create = subscription_admin.CreateSubscription(topic, subscription);
    if (!create) {
      std::cout << "CreateSubscription() failed: " << create.status() << "\n";
      return 1;
    }
    cleanup.Defer([subscription_admin, subscription]() mutable {
      (void)subscription_admin.DeleteSubscription(subscription);
    });
  }

  std::cout << "# Running Cloud Pub/Sub experiment"
            << "\n# Start time: "
            << google::cloud::internal::FormatRfc3339(
                   std::chrono::system_clock::now())
            << "\n# Topic ID: " << config->topic_id
            << "\n# Subscription ID: " << config->subscription_id
            << "\n# Payload Size: " << FormatSize(config->payload_size)
            << "\n# Iteration_Duration: " << config->iteration_duration.count()
            << "s" << std::boolalpha << "\n# Publisher: " << config->publisher
            << "\n# Publisher Threads: " << config->publisher_thread_count
            << "\n# Publisher I/O Threads: " << config->publisher_io_threads
            << "\n# Publisher Max Batch Size: "
            << config->publisher_max_batch_size
            << "\n# Publisher Max Batch Bytes: "
            << FormatSize(config->publisher_max_batch_bytes) << std::boolalpha
            << "\n# Subscriber: " << config->subscriber
            << "\n# Subscriber Threads: " << config->subscriber_thread_count
            << "\n# Subscriber I/O Threads: " << config->subscriber_io_threads
            << "\n# Subscriber Max Outstanding Messages: "
            << config->subscriber_max_outstanding_messages
            << "\n# Subscriber Max Outstanding Bytes: "
            << FormatSize(config->subscriber_max_outstanding_bytes)
            << "\n# Subscriber Max Concurrency: "
            << config->subscriber_max_concurrency
            << "\n# Minimum Samples: " << config->minimum_samples
            << "\n# Maximum Samples: " << config->maximum_samples
            << "\n# Minimum Runtime: " << config->minimum_runtime.count() << "s"
            << "\n# Maximum Runtime: " << config->maximum_runtime.count() << "s"
            << std::endl;

  auto const topic = pubsub::Topic(config->project_id, config->topic_id);

  std::cout << "Timestamp,Op,Iteration,Count,Bytes,ElapsedUs,MBs" << std::endl;

  std::vector<std::thread> tasks;
  if (config->publisher) {
    tasks.emplace_back(PublisherTask, *config);
  }
  if (config->subscriber) {
    tasks.emplace_back(SubscriberTask, *config);
  }
  for (auto& t : tasks) t.join();

  return 0;
}

namespace {

using ::google::cloud::testing_util::Timer;

std::mutex cout_mu;

bool Done(Config const& config, std::int64_t samples,
          std::chrono::steady_clock::time_point start) {
  auto const now = std::chrono::steady_clock::now();
  if (now >= start + config.maximum_runtime) return true;
  if (samples >= config.maximum_samples) return true;
  if (now < start + config.minimum_runtime) return false;
  return samples >= config.minimum_samples;
}

std::string Timestamp() {
  return google::cloud::internal::FormatRfc3339(
      std::chrono::system_clock::now());
}

void PrintResult(Config const& config, std::string const& operation,
                 int iteration, std::int64_t count, Timer const& usage) {
  using std::chrono::duration_cast;
  using std::chrono::microseconds;
  using std::chrono::seconds;

  auto const elapsed_us = duration_cast<microseconds>(usage.elapsed_time());
  auto const bytes = count * config.payload_size;
  auto const mbs =
      absl::StrFormat("%.02f", static_cast<double>(bytes) /
                                   static_cast<double>(elapsed_us.count()));
  std::lock_guard<std::mutex> lk(cout_mu);
  std::cout << Timestamp() << ',' << operation << ',' << iteration << ','
            << count << ',' << bytes << ',' << elapsed_us.count() << ',' << mbs
            << std::endl;
}

void PublisherTask(Config const& config) {
  auto publisher_options =
      pubsub::PublisherOptions{}
          .set_maximum_batch_message_count(config.publisher_max_batch_size)
          .set_maximum_batch_bytes(
              static_cast<std::size_t>(config.publisher_max_batch_bytes));
  auto connection_options =
      pubsub::ConnectionOptions{}.set_channel_pool_domain("Publisher");
  if (config.publisher_io_threads) {
    connection_options.set_background_thread_pool_size(
        config.publisher_io_threads);
  }

  pubsub::Publisher publisher(pubsub::MakePublisherConnection(
      pubsub::Topic(config.project_id, config.topic_id),
      std::move(publisher_options), std::move(connection_options)));
  auto gen = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const data = google::cloud::internal::Sample(
      gen, static_cast<int>(config.payload_size), "0123456789");

  std::mutex pending_mu;
  std::condition_variable pending_cv;
  std::int64_t pending = 0;
  std::int64_t hwm_count = 0;
  auto const lwm = config.publisher_pending_lwm;
  auto const hwm = config.publisher_pending_hwm;
  auto pending_wait = [&] {
    std::unique_lock<std::mutex> lk(pending_mu);
    ++pending;
    if (pending < hwm) return;
    ++hwm_count;
    pending_cv.wait(lk, [&] { return pending < lwm; });
  };
  auto pending_done = [&] {
    std::unique_lock<std::mutex> lk(pending_mu);
    if (--pending < lwm) pending_cv.notify_all();
  };
  auto pending_nothing = [&] {
    std::unique_lock<std::mutex> lk(pending_mu);
    pending_cv.wait(lk, [&] { return pending == 0; });
  };

  std::atomic<std::int64_t> send_count{0};
  std::atomic<std::int64_t> error_count{0};
  std::atomic<bool> shutdown{false};
  auto worker = [&](int id) {
    using std::chrono::steady_clock;
    using std::chrono::duration_cast;
    auto const start = std::chrono::steady_clock::now();
    for (std::int64_t i = 0; !shutdown.load(); ++i) {
      pending_wait();
      auto const elapsed =
          duration_cast<std::chrono::microseconds>(steady_clock::now() - start);
      publisher
          .Publish(pubsub::MessageBuilder{}
                       .SetAttributes({
                           {"sendTime", std::to_string(elapsed.count())},
                           {"clientId", std::to_string(id)},
                           {"sequenceNumber", std::to_string(i)},
                       })
                       .SetData(data)
                       .Build())
          .then([&](future<StatusOr<std::string>> f) {
            ++send_count;
            if (!f.get()) ++error_count;
            pending_done();
          });
    }
  };
  std::vector<std::thread> workers;
  int task_id = 0;
  std::generate_n(std::back_inserter(workers), config.publisher_thread_count,
                  [&] {
                    return std::thread{worker, task_id++};
                  });
  auto const start = std::chrono::steady_clock::now();
  for (int i = 0; !Done(config, i, start); ++i) {
    using std::chrono::steady_clock;
    Timer usage;
    usage.Start();
    auto const start_count = send_count.load();
    std::this_thread::sleep_for(config.iteration_duration);
    auto const count = send_count.load() - start_count;
    usage.Stop();
    PrintResult(config, "Pub", i, count, usage);
  }
  shutdown.store(true);
  for (auto& w : workers) w.join();
  // Need to wait until all pending callbacks have executed.
  pending_nothing();
  std::cout << "# Publisher: error_count=" << error_count
            << ", hwm_count=" << hwm_count << std::endl;
}

void SubscriberTask(Config const& config) {
  auto subscriber_options =
      pubsub::SubscriberOptions{}
          .set_max_outstanding_messages(
              config.subscriber_max_outstanding_messages)
          .set_max_outstanding_bytes(config.subscriber_max_outstanding_bytes)
          .set_max_concurrency(config.subscriber_max_concurrency);
  auto connection_options =
      pubsub::ConnectionOptions{}.set_channel_pool_domain("Subscriber");
  if (config.subscriber_io_threads != 0) {
    connection_options.set_background_thread_pool_size(
        config.subscriber_io_threads);
  }

  pubsub::Subscriber subscriber(pubsub::MakeSubscriberConnection(
      pubsub::Subscription(config.project_id, config.subscription_id),
      std::move(subscriber_options), std::move(connection_options)));
  std::atomic<std::int64_t> received_count{0};
  auto handler = [&received_count](pubsub::Message const&,
                                   pubsub::AckHandler h) {
    ++received_count;
    std::move(h).ack();
  };

  std::vector<future<Status>> sessions;
  std::generate_n(std::back_inserter(sessions), config.subscriber_thread_count,
                  [&] { return subscriber.Subscribe(handler); });

  auto const start = std::chrono::steady_clock::now();
  for (int i = 0; !Done(config, i, start); ++i) {
    using std::chrono::steady_clock;
    Timer usage;
    usage.Start();
    auto const start_count = received_count.load();
    std::this_thread::sleep_for(config.iteration_duration);
    auto const count = received_count.load() - start_count;
    usage.Stop();
    PrintResult(config, "Sub", i, count, usage);
  }
  for (auto& s : sessions) s.cancel();
  Status last_status;
  std::int64_t last_received_count = 0;
  for (auto& s : sessions) {
    auto status = s.get();
    auto const current = received_count.load();
    if (last_status == status && last_received_count == current) continue;
    last_status = std::move(status);
    last_received_count = current;
    std::lock_guard<std::mutex> lk(cout_mu);
    std::cout << "# status=" << last_status << ", count=" << last_received_count
              << std::endl;
  }
}

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::OptionDescriptor;
using ::google::cloud::testing_util::ParseBoolean;
using ::google::cloud::testing_util::ParseDuration;
using ::google::cloud::testing_util::ParseSize;

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
      {"--subscription-id", "use an existing subscription for the benchmark",
       [&options](std::string const& val) { options.subscription_id = val; }},

      {"--payload-size", "set the size of the message payload",
       [&options](std::string const& val) {
         options.payload_size = ParseSize(val);
       }},
      {"--iteration-duration",
       "measurement interval, report throughput every X seconds",
       [&options](std::string const& val) {
         options.iteration_duration = ParseDuration(val);
       }},

      {"--publisher", "run a publisher in this program",
       [&options](std::string const& val) {
         options.publisher = ParseBoolean(val).value_or(true);
       }},
      {"--publisher-thread-count", "number of publisher tasks",
       [&options](std::string const& val) {
         options.publisher_thread_count = std::stoi(val);
       }},
      {"--publisher-io-threads",
       "number of publisher I/O threads, set to 0 to use the library default",
       [&options](std::string const& val) {
         options.publisher_io_threads = std::stoi(val);
       }},
      {"--publisher-max-batch-size", "configure batching parameters",
       [&options](std::string const& val) {
         options.publisher_max_batch_size = std::stoi(val);
       }},
      {"--publisher-max-batch-bytes", "configure batching parameters",
       [&options](std::string const& val) {
         options.publisher_max_batch_bytes = ParseSize(val);
       }},

      {"--subscriber", "run a subscriber in this program",
       [&options](std::string const& val) {
         options.subscriber = ParseBoolean(val).value_or(true);
       }},
      {"--subscriber-thread-count", "number of subscriber tasks",
       [&options](std::string const& val) {
         options.subscriber_thread_count = std::stoi(val);
       }},
      {"--subscriber-io-threads",
       "number of subscriber I/O threads, set to 0 to use the library default",
       [&options](std::string const& val) {
         options.subscriber_io_threads = std::stoi(val);
       }},
      {"--subscriber-max-outstanding-messages",
       "configure message flow control",
       [&options](std::string const& val) {
         options.subscriber_max_outstanding_messages = std::stoi(val);
       }},
      {"--subscriber-max-outstanding-bytes", "configure message flow control",
       [&options](std::string const& val) {
         options.subscriber_max_outstanding_bytes = ParseSize(val);
       }},
      {"--subscriber-max-concurrency", "configure message flow control",
       [&options](std::string const& val) {
         options.subscriber_max_concurrency = std::stoi(val);
       }},

      {"--minimum-samples", "minimum number of samples to capture",
       [&options](std::string const& val) {
         options.minimum_samples = std::stol(val);
       }},
      {"--maximum-samples", "maximum number of samples to capture",
       [&options](std::string const& val) {
         options.maximum_samples = std::stol(val);
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
  config = ParseArgsImpl({cmd, "--topic-id=test"}, kDescription);
  if (!config) return error("--topic-id");
  config = ParseArgsImpl({cmd, "--subscription-id=test"}, kDescription);
  if (!config) return error("--subscription-id");

  return ParseArgsImpl(
      {
          cmd,
          "--project-id=" + GetEnv("GOOGLE_CLOUD_PROJECT").value_or(""),
          "--publisher=true",
          "--publisher-thread-count=1",
          "--publisher-io-threads=1",
          "--publisher-max-batch-size=2",
          "--publisher-max-batch-bytes=1KiB",
          "--subscriber=true",
          "--subscriber-thread-count=1",
          "--subscriber-io-threads=1",
          "--subscriber-max-outstanding-bytes=100MiB",
          "--subscriber-max-concurrency=1000",
          "--iteration-duration=1s",
          "--payload-size=2KiB",
          "--minimum-samples=1",
          "--maximum-samples=2",
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
