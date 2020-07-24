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

#include "google/cloud/spanner/transaction.h"
#include "google/cloud/spanner/internal/session.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

TEST(TransactionOptions, Construction) {
  Timestamp read_timestamp{};
  std::chrono::nanoseconds staleness{};

  Transaction::ReadOnlyOptions strong;
  Transaction::ReadOnlyOptions exact_ts(read_timestamp);
  Transaction::ReadOnlyOptions exact_dur(staleness);

  Transaction::ReadWriteOptions none;

  Transaction::SingleUseOptions su_strong(strong);
  Transaction::SingleUseOptions su_exact_ts(exact_ts);
  Transaction::SingleUseOptions su_exact_dur(exact_dur);
  Transaction::SingleUseOptions su_bounded_ts(read_timestamp);
  Transaction::SingleUseOptions su_bounded_dur(staleness);
}

TEST(Transaction, RegularSemantics) {
  Transaction::ReadOnlyOptions strong;
  Transaction a(strong);
  Transaction b = MakeReadOnlyTransaction();
  EXPECT_NE(a, b);

  Transaction c = b;
  EXPECT_EQ(c, b);
  EXPECT_NE(c, a);

  c = a;
  EXPECT_EQ(c, a);
  EXPECT_NE(c, b);

  Transaction d(c);
  EXPECT_EQ(d, c);
  EXPECT_EQ(d, a);

  Transaction::ReadWriteOptions none;
  Transaction e(none);
  Transaction f = MakeReadWriteTransaction();
  EXPECT_NE(e, f);

  Transaction g = f;  // NOLINT(performance-unnecessary-copy-initialization)
  EXPECT_EQ(g, f);
  EXPECT_NE(g, e);

  Transaction h = internal::MakeSingleUseTransaction(strong);
  Transaction i = internal::MakeSingleUseTransaction(strong);
  EXPECT_NE(h, i);

  Transaction j = i;  // NOLINT(performance-unnecessary-copy-initialization)
  EXPECT_EQ(j, i);
  EXPECT_NE(j, h);
}

TEST(Transaction, Visit) {
  Transaction a = MakeReadOnlyTransaction();
  std::int64_t a_seqno;
  internal::Visit(
      a, [&a_seqno](internal::SessionHolder& /*session*/,
                    StatusOr<google::spanner::v1::TransactionSelector>& s,
                    std::int64_t seqno) {
        EXPECT_TRUE(s->has_begin());
        EXPECT_TRUE(s->begin().has_read_only());
        s->set_id("test-txn-id");
        a_seqno = seqno;
        return 0;
      });
  internal::Visit(
      a, [a_seqno](internal::SessionHolder& /*session*/,
                   StatusOr<google::spanner::v1::TransactionSelector>& s,
                   std::int64_t seqno) {
        EXPECT_EQ("test-txn-id", s->id());
        EXPECT_GT(seqno, a_seqno);
        return 0;
      });
}

TEST(Transaction, SessionAffinity) {
  auto a_session = internal::MakeDissociatedSessionHolder("SessionAffinity");
  Transaction a = MakeReadWriteTransaction();
  internal::Visit(
      a, [&a_session](internal::SessionHolder& session,
                      StatusOr<google::spanner::v1::TransactionSelector>& s,
                      std::int64_t) {
        EXPECT_FALSE(session);
        EXPECT_TRUE(s->has_begin());
        session = a_session;
        s->set_id("a-txn-id");
        return 0;
      });
  Transaction b = MakeReadWriteTransaction(a);
  internal::Visit(
      b, [&a_session](internal::SessionHolder& session,
                      StatusOr<google::spanner::v1::TransactionSelector>& s,
                      std::int64_t) {
        EXPECT_EQ(a_session, session);  // session affinity
        EXPECT_TRUE(s->has_begin());    // but a new transaction
        return 0;
      });
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
