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

#include "google/cloud/spanner/internal/transaction_impl.h"

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TransactionImpl::~TransactionImpl() = default;

TransactionImpl::TransactionImpl(
    google::spanner::v1::TransactionSelector selector, bool route_to_leader,
    std::string tag)
    : TransactionImpl(/*session=*/{}, std::move(selector), route_to_leader,
                      std::move(tag), absl::nullopt) {}

TransactionImpl::TransactionImpl(
    TransactionImpl const& impl,
    google::spanner::v1::TransactionSelector selector, bool route_to_leader,
    std::string tag)
    : TransactionImpl(impl.session_, std::move(selector), route_to_leader,
                      std::move(tag),
                      (impl.session_ && impl.session_->is_multiplexed() &&
                       impl.selector_->has_id())
                          ? absl::optional<std::string>(impl.selector_->id())
                          : absl::nullopt) {}

TransactionImpl::TransactionImpl(
    SessionHolder session, google::spanner::v1::TransactionSelector selector,
    bool route_to_leader, std::string tag,
    absl::optional<std::string> multiplexed_session_previous_transaction_id)
    : session_(std::move(session)),
      selector_(std::move(selector)),
      route_to_leader_(route_to_leader),
      tag_(std::move(tag)),
      seqno_(0) {
  state_ = selector_->has_begin() ? State::kBegin : State::kDone;
  // If we're attempting to retry an aborted ReadWrite transaction on a
  // multiplexed session, then propagate the aborted transaction id.
  if (session_ && session_->is_multiplexed() && selector_.ok() &&
      selector_->has_begin() && selector_->begin().has_read_write() &&
      multiplexed_session_previous_transaction_id.has_value()) {
    selector_->mutable_begin()
        ->mutable_read_write()
        ->set_multiplexed_session_previous_transaction_id(
            *multiplexed_session_previous_transaction_id);
  }
}

void TransactionImpl::UpdatePrecommitToken(
    std::unique_lock<std::mutex> const&,
    absl::optional<google::spanner::v1::MultiplexedSessionPrecommitToken>
        token) {
  if (token.has_value() && (!precommit_token_.has_value() ||
                            token->seq_num() > precommit_token_->seq_num())) {
    precommit_token_ = std::move(token);
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
