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

#include "google/cloud/spanner/internal/transaction_impl.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/spanner/transaction.h"
#include "google/cloud/internal/port_platform.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <chrono>
#include <ctime>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

using ::google::spanner::v1::TransactionSelector;
using ::testing::IsNull;
using ::testing::NotNull;

class KeySet {};
class ResultSet {};

// A fake `google::cloud::spanner::Client`, supporting a single `Read()`
// operation that does nothing but track the expected transaction callbacks.
class Client {
 public:
  enum class Mode {
    kReadSucceeds,  // and assigns a transaction ID
    kReadFailsAndTxnRemainsBegin,
    kReadFailsAndTxnInvalidated,
  };

  explicit Client(Mode mode) : mode_(mode) {}

  // Set the `read_timestamp` we expect to see, and the `session_id` and
  // `txn_id` we want to use during the upcoming `Read()` calls.
  void Reset(Timestamp read_timestamp, std::string session_id,
             std::string txn_id) {
    read_timestamp_ = read_timestamp;
    session_id_ = std::move(session_id);
    txn_id_ = std::move(txn_id);
    std::unique_lock<std::mutex> lock(mu_);
    valid_visits_ = 0;
  }

  // Return the number of valid visitations made to the transaction during a
  // completed set of `Read()` calls.
  int ValidVisits() {
    std::unique_lock<std::mutex> lock(mu_);
    return valid_visits_;
  }

  // User-visible read operation.
  ResultSet Read(Transaction txn, std::string const& table, KeySet const& keys,
                 std::vector<std::string> const& columns) {
    auto read = [this, &table, &keys, &columns](
                    SessionHolder& session,
                    StatusOr<TransactionSelector>& selector,
                    std::int64_t seqno) {
      return this->Read(session, selector, seqno, table, keys, columns);
    };
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    try {
#endif
      return internal::Visit(std::move(txn), std::move(read));
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    } catch (char const*) {
      return {};
    }
#endif
  }

 private:
  ResultSet Read(SessionHolder& session,
                 StatusOr<TransactionSelector>& selector, std::int64_t seqno,
                 std::string const& table, KeySet const& keys,
                 std::vector<std::string> const& columns);

  Mode mode_;
  Timestamp read_timestamp_;
  std::string session_id_;
  std::string txn_id_;
  std::mutex mu_;
  std::int64_t begin_seqno_{0};  // GUARDED_BY(mu_)
  int valid_visits_;             // GUARDED_BY(mu_)
};

// Transaction callback.  Normally we would use the TransactionSelector
// to make a StreamingRead() RPC, and then, if the selector was a `begin`,
// switch the selector to use the allocated transaction ID.  Here we use
// the pre-assigned transaction ID after checking the read timestamp.
ResultSet Client::Read(SessionHolder& session,
                       StatusOr<TransactionSelector>& selector,
                       std::int64_t seqno, std::string const&, KeySet const&,
                       std::vector<std::string> const&) {
  // when we mark a transaction invalid, we use this Status.
  const Status failed_txn_status(StatusCode::kInternal, "Bad transaction");

  bool fail_with_throw = false;
  if (!selector) {
    std::unique_lock<std::mutex> lock(mu_);
    switch (mode_) {
      case Mode::kReadSucceeds:  // visits never valid
      case Mode::kReadFailsAndTxnRemainsBegin:
        break;
      case Mode::kReadFailsAndTxnInvalidated:
        EXPECT_EQ(selector.status(), failed_txn_status);
        ++valid_visits_;
        fail_with_throw = (valid_visits_ % 2 == 0);
        break;
    }
  } else if (selector->has_begin()) {
    EXPECT_THAT(session, IsNull());
    if (selector->begin().has_read_only() &&
        selector->begin().read_only().has_read_timestamp()) {
      auto const& proto = selector->begin().read_only().read_timestamp();
      if (MakeTimestamp(proto).value() == read_timestamp_ && seqno > 0) {
        std::unique_lock<std::mutex> lock(mu_);
        switch (mode_) {
          case Mode::kReadSucceeds:  // first visit valid
            if (valid_visits_ == 0) ++valid_visits_;
            break;
          case Mode::kReadFailsAndTxnRemainsBegin:  // visits always valid
          case Mode::kReadFailsAndTxnInvalidated:
            ++valid_visits_;
            fail_with_throw = (valid_visits_ % 2 == 0);
            break;
        }
        if (valid_visits_ != 0) begin_seqno_ = seqno;
      }
    }
    switch (mode_) {
      case Mode::kReadSucceeds:
        // `begin` -> `id`, calls now parallelized
        session = internal::MakeDissociatedSessionHolder(session_id_);
        selector->set_id(txn_id_);
        break;
      case Mode::kReadFailsAndTxnRemainsBegin:
        // leave as `begin`, calls stay serialized
        break;
      case Mode::kReadFailsAndTxnInvalidated:
        // `begin` -> `error`, calls now parallelized
        selector = failed_txn_status;
        break;
    }
  } else {
    if (selector->id() == txn_id_) {
      EXPECT_THAT(session, NotNull());
      EXPECT_EQ(session_id_, session->session_name());
      std::unique_lock<std::mutex> lock(mu_);
      switch (mode_) {
        case Mode::kReadSucceeds:  // non-initial visits valid
          if (valid_visits_ != 0 && seqno > begin_seqno_) ++valid_visits_;
          break;
        case Mode::kReadFailsAndTxnRemainsBegin:  // visits never valid
        case Mode::kReadFailsAndTxnInvalidated:
          break;
      }
    }
  }
  if (fail_with_throw) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    throw "1202 Program Alarm - Executive Overflow - No VAC Areas.";
#endif
  }
  // kReadSucceeds -v- kReadFailsAnd* is all about whether we assign a
  // transaction ID, not about what we return here (which is never used).
  return {};
}

// Call `client->Read()` from multiple threads in the context of a single,
// read-only transaction with an exact-staleness timestamp, and return the
// number of valid visitations to that transaction (should be `n_threads`).
int MultiThreadedRead(int n_threads, Client* client, std::time_t read_time,
                      std::string const& session_id,
                      std::string const& txn_id) {
  Timestamp read_timestamp =
      MakeTimestamp(std::chrono::system_clock::from_time_t(read_time)).value();
  client->Reset(read_timestamp, session_id, txn_id);

  Transaction::ReadOnlyOptions opts(read_timestamp);
  Transaction txn(opts);

  // Unused Read() parameters.
  std::string const table{};
  KeySet const keys{};
  std::vector<std::string> const columns{};

  std::promise<void> ready_promise;
  std::shared_future<void> ready_future(ready_promise.get_future());
  auto read = [&](std::promise<void>* started) {
    started->set_value();
    ready_future.wait();                      // wait for go signal
    client->Read(txn, table, keys, columns);  // ignore ResultSet
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < n_threads; ++i) {
    std::promise<void> started;
    threads.emplace_back(read, &started);
    started.get_future().wait();  // wait until thread running
  }
  ready_promise.set_value();  // go!
  for (auto& thread : threads) {
    thread.join();
  }

  return client->ValidVisits();  // should be n_threads
}

TEST(InternalTransaction, ReadSucceeds) {
  Client client(Client::Mode::kReadSucceeds);
  EXPECT_EQ(1, MultiThreadedRead(1, &client, 1562359982, "sess0", "txn0"));
  EXPECT_EQ(64, MultiThreadedRead(64, &client, 1562360571, "sess1", "txn1"));
  EXPECT_EQ(128, MultiThreadedRead(128, &client, 1562361252, "sess2", "txn2"));
}

TEST(InternalTransaction, ReadFailsAndTxnRemainsBegin) {
  Client client(Client::Mode::kReadFailsAndTxnRemainsBegin);
  EXPECT_EQ(1, MultiThreadedRead(1, &client, 1562359982, "sess0", "txn0"));
  EXPECT_EQ(64, MultiThreadedRead(64, &client, 1562360571, "sess1", "txn1"));
  EXPECT_EQ(128, MultiThreadedRead(128, &client, 1562361252, "sess2", "txn2"));
}

TEST(InternalTransaction, ReadFailsAndTxnInvalidated) {
  Client client(Client::Mode::kReadFailsAndTxnInvalidated);
  EXPECT_EQ(1, MultiThreadedRead(1, &client, 1562359982, "sess0", "txn0"));
  EXPECT_EQ(64, MultiThreadedRead(64, &client, 1562360571, "sess1", "txn1"));
  EXPECT_EQ(128, MultiThreadedRead(128, &client, 1562361252, "sess2", "txn2"));
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
