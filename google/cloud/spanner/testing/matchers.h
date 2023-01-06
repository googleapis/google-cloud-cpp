// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_MATCHERS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_MATCHERS_H

#include "google/cloud/spanner/internal/session.h"
#include "google/cloud/spanner/transaction.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include <gmock/gmock.h>
#include <cstdint>
#include <memory>

namespace google {
namespace cloud {
namespace spanner_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Verifies a `Transaction` has the expected Session and ID/tag.
MATCHER_P3(
    HasSessionAndTransaction, session_id, transaction_id, transaction_tag,
    "Verifies a Transaction has the expected Session and Transaction IDs") {
  return spanner_internal::Visit(
      arg, [&](spanner_internal::SessionHolder& session,
               StatusOr<google::spanner::v1::TransactionSelector>& s,
               spanner_internal::TransactionContext const& ctx) {
        bool result = true;
        if (!session) {
          *result_listener << "Session ID missing (expected " << session_id
                           << ")";
          result = false;
        } else if (session->session_name() != session_id) {
          *result_listener << "Session ID mismatch: " << session->session_name()
                           << " != " << session_id;
          result = false;
        }
        if (!s) {
          *result_listener << "Transaction ID missing (expected "
                           << transaction_id << " but found status "
                           << s.status() << ")";

          result = false;
        } else if (s->id() != transaction_id) {
          *result_listener << "Transaction ID mismatch: " << s->id()
                           << " != " << transaction_id;
          result = false;
        }
        if (ctx.tag != transaction_tag) {
          *result_listener << "Transaction tag mismatch: " << ctx.tag
                           << " != " << transaction_tag;
          result = false;
        }
        return result;
      });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_MATCHERS_H
