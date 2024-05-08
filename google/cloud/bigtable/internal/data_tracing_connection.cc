// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/internal/data_tracing_connection.h"
#include "google/cloud/internal/opentelemetry.h"
#include "traced_row_reader.h"

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
namespace {

std::vector<bigtable::FailedMutation> EndBulkApplySpan(
    opentelemetry::trace::Span& span, std::size_t total_mutations,
    std::vector<bigtable::FailedMutation> failures) {
  span.SetStatus(failures.empty() ? opentelemetry::trace::StatusCode::kOk
                                  : opentelemetry::trace::StatusCode::kError);
  span.SetAttribute("gcloud.bigtable.failed_mutations",
                    static_cast<std::uint32_t>(failures.size()));
  span.SetAttribute(
      "gcloud.bigtable.successful_mutations",
      static_cast<std::uint32_t>(total_mutations - failures.size()));
  span.End();
  return failures;
}

StatusOr<std::pair<bool, bigtable::Row>> EndReadRowSpan(
    opentelemetry::trace::Span& span,
    StatusOr<std::pair<bool, bigtable::Row>> result) {
  if (result) span.SetAttribute("gcloud.bigtable.row_found", result->first);
  return internal::EndSpan(span, std::move(result));
}

class DataTracingConnection : public bigtable::DataConnection {
 public:
  ~DataTracingConnection() override = default;

  explicit DataTracingConnection(
      std::shared_ptr<bigtable::DataConnection> child)
      : child_(std::move(child)) {}

  Options options() override { return child_->options(); }

  Status Apply(std::string const& table_name,
               bigtable::SingleRowMutation mut) override {
    auto span = internal::MakeSpan("bigtable::Table::Apply");
    auto scope = opentelemetry::trace::Scope(span);
    return internal::EndSpan(*span, child_->Apply(table_name, std::move(mut)));
  }

  future<Status> AsyncApply(std::string const& table_name,
                            bigtable::SingleRowMutation mut) override {
    auto span = internal::MakeSpan("bigtable::Table::AsyncApply");
    internal::OTelScope scope(span);
    return internal::EndSpan(std::move(span),
                             child_->AsyncApply(table_name, std::move(mut)));
  }

  std::vector<bigtable::FailedMutation> BulkApply(
      std::string const& table_name, bigtable::BulkMutation mut) override {
    auto span = internal::MakeSpan("bigtable::Table::BulkApply");
    auto scope = opentelemetry::trace::Scope(span);
    auto total_mutations = mut.size();
    return EndBulkApplySpan(*span, total_mutations,
                            child_->BulkApply(table_name, std::move(mut)));
  }

  future<std::vector<bigtable::FailedMutation>> AsyncBulkApply(
      std::string const& table_name, bigtable::BulkMutation mut) override {
    auto span = internal::MakeSpan("bigtable::Table::AsyncBulkApply");
    internal::OTelScope scope(span);
    auto total_mutations = mut.size();
    return child_->AsyncBulkApply(table_name, std::move(mut))
        .then([s = std::move(span), total_mutations](auto f) {
          return EndBulkApplySpan(*s, total_mutations, f.get());
        })
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent()](
                  auto f) {
          auto t = f.get();
          internal::DetachOTelContext(oc);
          return t;
        });
  }

  bigtable::RowReader ReadRowsFull(bigtable::ReadRowsParams params) override {
    auto span = internal::MakeSpan("bigtable::Table::ReadRows");
    auto scope = opentelemetry::trace::Scope(span);
    auto reader = child_->ReadRowsFull(std::move(params));
    return MakeTracedRowReader(std::move(span), std::move(reader));
  }

  StatusOr<std::pair<bool, bigtable::Row>> ReadRow(
      std::string const& table_name, std::string row_key,
      bigtable::Filter filter) override {
    auto span = internal::MakeSpan("bigtable::Table::ReadRow");
    auto scope = opentelemetry::trace::Scope(span);
    return EndReadRowSpan(*span, child_->ReadRow(table_name, std::move(row_key),
                                                 std::move(filter)));
  }

  StatusOr<bigtable::MutationBranch> CheckAndMutateRow(
      std::string const& table_name, std::string row_key,
      bigtable::Filter filter, std::vector<bigtable::Mutation> true_mutations,
      std::vector<bigtable::Mutation> false_mutations) override {
    auto span = internal::MakeSpan("bigtable::Table::CheckAndMutateRow");
    auto scope = opentelemetry::trace::Scope(span);
    return internal::EndSpan(
        *span, child_->CheckAndMutateRow(
                   table_name, std::move(row_key), std::move(filter),
                   std::move(true_mutations), std::move(false_mutations)));
  }

  future<StatusOr<bigtable::MutationBranch>> AsyncCheckAndMutateRow(
      std::string const& table_name, std::string row_key,
      bigtable::Filter filter, std::vector<bigtable::Mutation> true_mutations,
      std::vector<bigtable::Mutation> false_mutations) override {
    auto span = internal::MakeSpan("bigtable::Table::AsyncCheckAndMutateRow");
    internal::OTelScope scope(span);
    return internal::EndSpan(
        std::move(span),
        child_->AsyncCheckAndMutateRow(
            table_name, std::move(row_key), std::move(filter),
            std::move(true_mutations), std::move(false_mutations)));
  }

  StatusOr<std::vector<bigtable::RowKeySample>> SampleRows(
      std::string const& table_name) override {
    auto span = internal::MakeSpan("bigtable::Table::SampleRows");
    auto scope = opentelemetry::trace::Scope(span);
    return internal::EndSpan(*span, child_->SampleRows(table_name));
  }

  future<StatusOr<std::vector<bigtable::RowKeySample>>> AsyncSampleRows(
      std::string const& table_name) override {
    auto span = internal::MakeSpan("bigtable::Table::AsyncSampleRows");
    internal::OTelScope scope(span);
    return internal::EndSpan(std::move(span),
                             child_->AsyncSampleRows(table_name));
  }

  StatusOr<bigtable::Row> ReadModifyWriteRow(
      google::bigtable::v2::ReadModifyWriteRowRequest request) override {
    auto span = internal::MakeSpan("bigtable::Table::ReadModifyWriteRow");
    auto scope = opentelemetry::trace::Scope(span);
    return internal::EndSpan(*span,
                             child_->ReadModifyWriteRow(std::move(request)));
  }

  future<StatusOr<bigtable::Row>> AsyncReadModifyWriteRow(
      google::bigtable::v2::ReadModifyWriteRowRequest request) override {
    auto span = internal::MakeSpan("bigtable::Table::AsyncReadModifyWriteRow");
    internal::OTelScope scope(span);
    return internal::EndSpan(
        std::move(span), child_->AsyncReadModifyWriteRow(std::move(request)));
  }

  void AsyncReadRows(std::string const& table_name,
                     std::function<future<bool>(bigtable::Row)> on_row,
                     std::function<void(Status)> on_finish,
                     bigtable::RowSet row_set, std::int64_t rows_limit,
                     bigtable::Filter filter) override {
    auto span = internal::MakeSpan("bigtable::Table::AsyncReadRows");
    internal::OTelScope scope(span);
    auto traced_on_finish =
        [span, on_finish,
         oc = opentelemetry::context::RuntimeContext::GetCurrent()](
            Status const& status) {
          internal::DetachOTelContext(oc);
          return on_finish(internal::EndSpan(*span, status));
        };
    child_->AsyncReadRows(table_name, std::move(on_row),
                          std::move(traced_on_finish), std::move(row_set),
                          std::move(rows_limit), std::move(filter));
  }

  future<StatusOr<std::pair<bool, bigtable::Row>>> AsyncReadRow(
      std::string const& table_name, std::string row_key,
      bigtable::Filter filter) override {
    auto span = internal::MakeSpan("bigtable::Table::AsyncReadRow");
    internal::OTelScope scope(span);
    return child_
        ->AsyncReadRow(table_name, std::move(row_key), std::move(filter))
        .then([s = std::move(span)](auto f) {
          return EndReadRowSpan(*s, f.get());
        })
        .then([oc = opentelemetry::context::RuntimeContext::GetCurrent()](
                  auto f) {
          auto t = f.get();
          internal::DetachOTelContext(oc);
          return t;
        });
  }

 private:
  std::shared_ptr<bigtable::DataConnection> child_;
};
}  // namespace

std::shared_ptr<bigtable::DataConnection> MakeDataTracingConnection(
    std::shared_ptr<bigtable::DataConnection> conn) {
  return std::make_shared<DataTracingConnection>(std::move(conn));
}

#else

std::shared_ptr<bigtable::DataConnection> MakeDataTracingConnection(
    std::shared_ptr<bigtable::DataConnection> conn) {
  return conn;
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
