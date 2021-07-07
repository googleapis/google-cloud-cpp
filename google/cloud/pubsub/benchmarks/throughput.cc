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
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <functional>
#include <numeric>
#include <sstream>
#include <string>

namespace {
namespace pubsub = ::google::cloud::pubsub;
using ::google::cloud::future;
using ::google::cloud::Status;
using ::google::cloud::StatusOr;
using ::google::cloud::testing_util::FormatSize;
using ::google::cloud::testing_util::kMB;
using ::google::cloud::testing_util::kMiB;

auto constexpr kDescription = R"""(
A throughput vs. CPU benchmark for the Cloud Pub/Sub C++ client library.

Measure the throughput for publishers and/or subscribers in the Cloud Pub/Sub
C++ client library.
)""";

struct Config {
  std::string endpoint;
  std::string project_id;
  std::string topic_id;
  std::string subscription_id;

  std::int64_t payload_size = 1024;
  std::chrono::seconds iteration_duration = std::chrono::seconds(5);

  bool publisher = false;
  int publisher_thread_count = 1;
  int publisher_io_threads = 0;
  int publisher_io_channels = 0;
  int publisher_max_batch_size = 1000;
  std::int64_t publisher_max_batch_bytes = 10 * kMB;
  std::int64_t publisher_pending_lwm = 112 * kMiB;
  std::int64_t publisher_pending_hwm = 128 * kMiB;
  std::int64_t publisher_target_messages_per_second = 1200 * 2000;

  bool subscriber = false;
  int subscriber_thread_count = 1;
  int subscriber_io_threads = 0;
  int subscriber_io_channels = 0;
  int subscriber_max_outstanding_messages = 0;
  std::int64_t subscriber_max_outstanding_bytes = 100 * kMiB;
  int subscriber_max_concurrency = 0;

  std::int64_t minimum_samples = 10;
  std::int64_t maximum_samples = (std::numeric_limits<std::int64_t>::max)();
  std::chrono::seconds minimum_runtime = std::chrono::seconds(5);
  std::chrono::seconds maximum_runtime = std::chrono::seconds(300);

  bool show_help = false;
};

void Print(std::ostream& os, Config const&);

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

  Print(std::cout, *config);

  auto const topic = pubsub::Topic(config->project_id, config->topic_id);

  std::cout << "timestamp,elapsed(us),op,iteration,count,msgs/s,bytes,MB/s"
            << std::endl;

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

using ::google::cloud::pubsub_internal::MessageSize;
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

void PrintResult(std::string const& operation, int iteration,
                 std::int64_t count, std::int64_t bytes,
                 Timer::Snapshot const& usage) {
  using std::chrono::duration_cast;
  using std::chrono::microseconds;
  using std::chrono::seconds;

  auto const elapsed_us = duration_cast<microseconds>(usage.elapsed_time);
  auto const mbs =
      absl::StrFormat("%.02f", static_cast<double>(bytes) /
                                   static_cast<double>(elapsed_us.count()));
  auto const msgs =
      absl::StrFormat("%.02f", static_cast<double>(count) * 1000000.0 /
                                   static_cast<double>(elapsed_us.count()));
  std::lock_guard<std::mutex> lk(cout_mu);
  std::cout << Timestamp() << ',' << elapsed_us.count() << ',' << operation
            << ',' << iteration << ',' << count << ',' << msgs << ',' << bytes
            << ',' << mbs << std::endl;
}

pubsub::Publisher CreatePublisher(Config const& config) {
  auto publisher_options =
      pubsub::PublisherOptions{}
          .set_maximum_batch_message_count(config.publisher_max_batch_size)
          .set_maximum_batch_bytes(
              static_cast<std::size_t>(config.publisher_max_batch_bytes));
  auto connection_options =
      pubsub::ConnectionOptions{}.set_channel_pool_domain("Publisher");
  if (!config.endpoint.empty()) {
    connection_options.set_endpoint(config.endpoint);
  }
  if (config.publisher_io_threads != 0) {
    connection_options.set_background_thread_pool_size(
        config.publisher_io_threads);
  }
  if (config.publisher_io_channels != 0) {
    connection_options.set_num_channels(config.publisher_io_channels);
  }

  return pubsub::Publisher(pubsub::MakePublisherConnection(
      pubsub::Topic(config.project_id, config.topic_id),
      std::move(publisher_options), std::move(connection_options)));
}

std::atomic<std::int64_t> send_count{0};
std::atomic<std::int64_t> send_bytes{0};
std::atomic<std::int64_t> ack_count{0};
std::atomic<std::int64_t> ack_bytes{0};
std::atomic<std::int64_t> error_count{0};

/// Run a single thread publishing events
class PublishWorker {
 public:
  PublishWorker(Config config, int id) : config_(std::move(config)), id_(id) {}

  void Shutdown() {
    std::unique_lock<std::mutex> lk(mu_);
    shutdown_ = true;
    cv_.notify_all();
  }

  void Run() {
    auto publisher = CreatePublisher(config_);

    auto gen = google::cloud::internal::DefaultPRNG(std::random_device{}());
    auto const data = google::cloud::internal::Sample(
        gen, static_cast<int>(config_.payload_size), "0123456789");

    using std::chrono::duration_cast;
    using std::chrono::steady_clock;

    // We typically want to send tens of thousands or a million messages
    // per second, but sleeping for just one microsecond (or less) does not
    // work, the sleep call takes about 100us. We pace every kPacingCount
    // messages instead.
    auto constexpr kPacingCount = 8192;
    auto const enable_pacing =
        config_.publisher_target_messages_per_second != 0;
    auto const pacing_period = [&] {
      using std::chrono::microseconds;
      if (config_.publisher_target_messages_per_second == 0) {
        return microseconds(0);
      }
      return microseconds(std::chrono::seconds(1)) * kPacingCount /
             config_.publisher_target_messages_per_second;
    }();

    auto const start = std::chrono::steady_clock::now();
    auto pacing_time = start + pacing_period;
    for (std::int64_t i = 0; NotShutdownAndReady(); ++i) {
      auto const elapsed =
          duration_cast<std::chrono::microseconds>(steady_clock::now() - start);
      auto message = pubsub::MessageBuilder{}
                         .SetAttributes({
                             {"sendTime", std::to_string(elapsed.count())},
                             {"clientId", std::to_string(id_)},
                             {"sequenceNumber", std::to_string(i)},
                         })
                         .SetData(data)
                         .Build();
      auto const bytes = MessageSize(message);
      publisher.Publish(std::move(message))
          .then([this, bytes](future<StatusOr<std::string>> f) {
            ++ack_count;
            ack_bytes.fetch_add(bytes);
            if (!f.get()) ++error_count;
            OnAck();
          });
      ++send_count;
      send_bytes.fetch_add(bytes);
      if (enable_pacing && (i + 1) % kPacingCount == 0) {
        auto const now = steady_clock::now();
        if (now < pacing_time) std::this_thread::sleep_for(pacing_time - now);
        pacing_time = now + pacing_period;
      }
    }
    WaitUntilAllAcked();
  }

  std::int64_t hwm_count() const { return hwm_count_; }
  std::int64_t lwm_count() const { return lwm_count_; }

 private:
  bool NotShutdownAndReady() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [&] { return !blocked_ || shutdown_; });
    if (shutdown_) return false;
    pending_ += config_.payload_size;
    if (pending_ <= config_.publisher_pending_hwm) return true;
    blocked_ = true;
    ++hwm_count_;
    return true;
  }

  void WaitUntilAllAcked() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [&] { return pending_ == 0; });
  }

  void OnAck() {
    std::unique_lock<std::mutex> lk(mu_);
    pending_ -= config_.payload_size;
    if (pending_ == 0) {
      blocked_ = false;
      cv_.notify_all();
      return;
    }
    if (pending_ > config_.publisher_pending_lwm) return;
    if (!blocked_) return;
    ++lwm_count_;
    blocked_ = false;
    cv_.notify_all();
  }

  Config const config_;
  int const id_;
  std::mutex mu_;
  std::condition_variable cv_;
  bool shutdown_ = false;
  bool blocked_ = false;
  std::int64_t pending_ = 0;
  std::int64_t lwm_count_ = 0;
  std::int64_t hwm_count_ = 0;
};

void PublisherTask(Config const& config) {
  std::vector<std::shared_ptr<PublishWorker>> workers;
  int task_id = 0;
  std::generate_n(
      std::back_inserter(workers), config.publisher_thread_count,
      [&] { return std::make_shared<PublishWorker>(config, task_id++); });
  std::vector<std::thread> tasks(workers.size());
  auto activate = [](std::shared_ptr<PublishWorker> const& w) {
    return std::thread{[w] { w->Run(); }};
  };
  std::transform(workers.begin(), workers.end(), tasks.begin(), activate);

  auto const start = std::chrono::steady_clock::now();
  for (int i = 0; !Done(config, i, start); ++i) {
    using std::chrono::steady_clock;
    auto timer = Timer::PerThread();
    auto const start_send_count = send_count.load();
    auto const start_send_bytes = send_bytes.load();
    auto const start_ack_count = ack_count.load();
    auto const start_ack_bytes = ack_bytes.load();
    std::this_thread::sleep_for(config.iteration_duration);
    auto const send_count_last = send_count.load() - start_send_count;
    auto const send_bytes_last = send_bytes.load() - start_send_bytes;
    auto const ack_count_last = ack_count.load() - start_ack_count;
    auto const ack_bytes_last = ack_bytes.load() - start_ack_bytes;
    auto const usage = timer.Sample();
    PrintResult("Pub", i, send_count_last, send_bytes_last, usage);
    PrintResult("Ack", i, ack_count_last, ack_bytes_last, usage);
  }

  for (auto& w : workers) w->Shutdown();
  for (auto& t : tasks) t.join();
  auto const hwm_count = std::accumulate(
      workers.begin(), workers.end(), std::int64_t{0},
      [](std::int64_t a, std::shared_ptr<PublishWorker> const& w) {
        return a + w->hwm_count();
      });
  auto const lwm_count = std::accumulate(
      workers.begin(), workers.end(), std::int64_t{0},
      [](std::int64_t a, std::shared_ptr<PublishWorker> const& w) {
        return a + w->lwm_count();
      });
  std::cout << "# Publisher: error_count=" << error_count
            << ", ack_count=" << ack_count << ", send_count=" << send_count
            << ", hwm_count=" << hwm_count << ", lwm_count=" << lwm_count
            << std::endl;
}

pubsub::Subscriber CreateSubscriber(Config const& config) {
  auto subscriber_options =
      pubsub::SubscriberOptions{}
          .set_max_outstanding_messages(
              config.subscriber_max_outstanding_messages)
          .set_max_outstanding_bytes(config.subscriber_max_outstanding_bytes)
          .set_max_concurrency(config.subscriber_max_concurrency);
  auto connection_options =
      pubsub::ConnectionOptions{}.set_channel_pool_domain("Subscriber");
  if (!config.endpoint.empty()) {
    connection_options.set_endpoint(config.endpoint);
  }
  if (config.subscriber_io_threads != 0) {
    connection_options.set_background_thread_pool_size(
        config.subscriber_io_threads);
  }
  if (config.subscriber_io_channels != 0) {
    connection_options.set_num_channels(config.subscriber_io_channels);
  }

  return pubsub::Subscriber(pubsub::MakeSubscriberConnection(
      pubsub::Subscription(config.project_id, config.subscription_id),
      std::move(subscriber_options), std::move(connection_options)));
}

void SubscriberTask(Config const& config) {
  std::vector<pubsub::Subscriber> subscribers;
  std::generate_n(std::back_inserter(subscribers),
                  config.subscriber_thread_count,
                  [config] { return CreateSubscriber(config); });

  std::atomic<std::int64_t> received_count{0};
  std::atomic<std::int64_t> received_bytes{0};
  auto handler = [&received_count, &received_bytes](pubsub::Message const& m,
                                                    pubsub::AckHandler h) {
    ++received_count;
    received_bytes.fetch_add(MessageSize(m));
    std::move(h).ack();
  };

  std::vector<future<Status>> sessions;
  std::transform(
      subscribers.begin(), subscribers.end(), std::back_inserter(sessions),
      [&handler](pubsub::Subscriber s) { return s.Subscribe(handler); });

  auto const start = std::chrono::steady_clock::now();
  for (int i = 0; !Done(config, i, start); ++i) {
    using std::chrono::steady_clock;
    auto timer = Timer::PerThread();
    auto const start_count = received_count.load();
    auto const start_bytes = received_bytes.load();
    std::this_thread::sleep_for(config.iteration_duration);
    auto const count = received_count.load() - start_count;
    auto const bytes = received_bytes.load() - start_bytes;
    auto const usage = timer.Sample();
    PrintResult("Sub", i, count, bytes, usage);
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

void PrintPublisher(std::ostream& os, Config const& config) {
  os << "\n# Publisher: " << std::boolalpha << config.publisher
     << "\n# Publisher Threads: " << config.publisher_thread_count
     << "\n# Publisher I/O Threads: " << config.publisher_io_threads
     << "\n# Publisher I/O Channels: " << config.publisher_io_channels
     << "\n# Publisher Max Batch Size: " << config.publisher_max_batch_size
     << "\n# Publisher Max Batch Bytes: "
     << FormatSize(config.publisher_max_batch_bytes)
     << "\n# Publisher Pending LWM: "
     << FormatSize(config.publisher_pending_lwm)
     << "\n# Publisher Pending HWM: "
     << FormatSize(config.publisher_pending_hwm)
     << "\n# Publisher Target messages/s: "
     << config.publisher_target_messages_per_second;
}

void PrintSubscriber(std::ostream& os, Config const& config) {
  os << "\n# Subscriber: " << std::boolalpha << config.subscriber
     << "\n# Subscriber Threads: " << config.subscriber_thread_count
     << "\n# Subscriber I/O Threads: " << config.subscriber_io_threads
     << "\n# Subscriber I/O Channels: " << config.subscriber_io_channels
     << "\n# Subscriber Max Outstanding Messages: "
     << config.subscriber_max_outstanding_messages
     << "\n# Subscriber Max Outstanding Bytes: "
     << FormatSize(config.subscriber_max_outstanding_bytes)
     << "\n# Subscriber Max Concurrency: " << config.subscriber_max_concurrency;
}

void Print(std::ostream& os, Config const& config) {
  os << "# Running Cloud Pub/Sub experiment"
     << "\n# Start time: "
     << google::cloud::internal::FormatRfc3339(std::chrono::system_clock::now())
     << "\n# Endpoint: " << config.endpoint
     << "\n# Topic ID: " << config.topic_id
     << "\n# Subscription ID: " << config.subscription_id
     << "\n# Payload Size: " << FormatSize(config.payload_size)
     << "\n# Iteration_Duration: " << config.iteration_duration.count() << "s"
     << "\n# Minimum Samples: " << config.minimum_samples
     << "\n# Maximum Samples: " << config.maximum_samples
     << "\n# Minimum Runtime: " << config.minimum_runtime.count() << "s"
     << "\n# Maximum Runtime: " << config.maximum_runtime.count() << "s";
  if (config.publisher) PrintPublisher(os, config);
  if (config.subscriber) PrintSubscriber(os, config);
  os << std::endl;
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
      {"--endpoint", "use the given endpoint",
       [&options](std::string const& val) { options.endpoint = val; }},
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
       "number of publisher I/O threads, set to 0 to use the library "
       "default",
       [&options](std::string const& val) {
         options.publisher_io_threads = std::stoi(val);
       }},
      {"--publisher-io-channels",
       "number of publisher I/O (gRPC) channels, set to 0 to use the "
       "library "
       "default",
       [&options](std::string const& val) {
         options.publisher_io_channels = std::stoi(val);
       }},
      {"--publisher-max-batch-size", "configure batching parameters",
       [&options](std::string const& val) {
         options.publisher_max_batch_size = std::stoi(val);
       }},
      {"--publisher-max-batch-bytes", "configure batching parameters",
       [&options](std::string const& val) {
         options.publisher_max_batch_bytes = ParseSize(val);
       }},
      {"--publisher-pending-lwm",
       "message generation flow control, maximum size of messages with a "
       "pending ack",
       [&options](std::string const& val) {
         options.publisher_pending_lwm = ParseSize(val);
       }},
      {"--publisher-pending-hwm",
       "message generation flow control, maximum size of messages with a "
       "pending ack",
       [&options](std::string const& val) {
         options.publisher_pending_hwm = ParseSize(val);
       }},
      {"--publisher-target-messages-per-second",
       "limit the number of messages generated per second."
       " If set to 0 this flow control feature is disabled.",
       [&options](std::string const& val) {
         options.publisher_target_messages_per_second = std::stol(val);
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
       "number of subscriber I/O threads, set to 0 to use the library "
       "default",
       [&options](std::string const& val) {
         options.subscriber_io_threads = std::stoi(val);
       }},
      {"--subscriber-io-channels",
       "number of subscriber I/O (gRPC) channels, set to 0 to use the "
       "library "
       "default",
       [&options](std::string const& val) {
         options.subscriber_io_channels = std::stoi(val);
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
  config = ParseArgsImpl({cmd, "--endpoint=test"}, kDescription);
  if (!config) return error("--endpoint");

  return ParseArgsImpl(
      {
          cmd,
          "--project-id=" + GetEnv("GOOGLE_CLOUD_PROJECT").value_or(""),
          "--publisher=true",
          "--publisher-thread-count=1",
          "--publisher-io-threads=1",
          "--publisher-io-channels=1",
          "--publisher-max-batch-size=2",
          "--publisher-max-batch-bytes=1KiB",
          "--publisher-pending-lwm=8MiB",
          "--publisher-pending-hwm=10MiB",
          "--publisher-target-messages-per-second=1000000",
          "--subscriber=true",
          "--subscriber-thread-count=1",
          "--subscriber-io-threads=1",
          "--subscriber-io-channels=1",
          "--subscriber-max-outstanding-messages=0",
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
