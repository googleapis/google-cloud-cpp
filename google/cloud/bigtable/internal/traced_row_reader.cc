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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/bigtable/internal/traced_row_reader.h"
#include "google/cloud/internal/opentelemetry.h"

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

class TracedRowReader : public bigtable_internal::RowReaderImpl {
 public:
  explicit TracedRowReader(
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
      bigtable::RowReader reader)
      : span_(std::move(span)),
        reader_(std::move(reader)),
        it_(reader_.begin()) {}

  ~TracedRowReader() override {
    // It is ok not to iterate the full range. We should still end our span.
    (void)End(Status());
  }

  /// Skips remaining rows and invalidates current iterator.
  void Cancel() override {
    span_->AddEvent("gl-cpp.cancel");
    reader_.Cancel();
  };

  absl::variant<Status, bigtable::Row> Advance() override {
    if (it_ == reader_.end()) return End(Status());
    auto row = *it_;
    ++it_;
    if (!row) return End(std::move(row).status());
    return *row;
  }

 private:
  Status End(Status const& s) { return internal::EndSpan(*span_, s); }

  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
  bigtable::RowReader reader_;
  bigtable::RowReader::iterator it_;
};

}  // namespace

bigtable::RowReader MakeTracedRowReader(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
    bigtable::RowReader reader) {
  auto impl =
      std::make_shared<TracedRowReader>(std::move(span), std::move(reader));
  return bigtable_internal::MakeRowReader(std::move(impl));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
