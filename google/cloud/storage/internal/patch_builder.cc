// Copyright 2020 Google LLC
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

#include "google/cloud/storage/internal/patch_builder.h"
#include "google/cloud/storage/internal/patch_builder_details.h"
#include "google/cloud/storage/version.h"
#include "absl/types/optional.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

struct PatchBuilder::Impl {
  Impl() = default;  // NOLINT(bugprone-exception-escape)

  std::string ToString() const {
    if (empty()) {
      return "{}";
    }
    return patch_.dump();
  }

  bool empty() const { return patch_.empty(); }
  void clear() { patch_.clear(); }

  void AddStringField(char const* field_name, std::string const& lhs,
                      std::string const& rhs) {
    if (lhs == rhs) return;
    if (rhs.empty()) {
      patch_[field_name] = nullptr;
      return;
    }
    patch_[field_name] = rhs;
  }

  void AddBoolField(char const* field_name, bool lhs, bool rhs) {
    if (lhs == rhs) return;
    patch_[field_name] = rhs;
  }

  void AddIntField(char const* field_name, std::int32_t lhs, std::int32_t rhs,
                   std::int32_t null_value = 0) {
    return AddIntegerField(field_name, lhs, rhs, null_value);
  }
  void AddIntField(char const* field_name, std::uint32_t lhs, std::uint32_t rhs,
                   std::uint32_t null_value = 0) {
    return AddIntegerField(field_name, lhs, rhs, null_value);
  }
  void AddIntField(char const* field_name, std::int64_t lhs, std::int64_t rhs,
                   std::int64_t null_value = 0) {
    return AddIntegerField(field_name, lhs, rhs, null_value);
  }
  void AddIntField(char const* field_name, std::uint64_t lhs, std::uint64_t rhs,
                   std::uint64_t null_value = 0) {
    return AddIntegerField(field_name, lhs, rhs, null_value);
  }

  void AddSubPatch(char const* field_name, PatchBuilder::Impl const& builder) {
    patch_[field_name] = builder.patch_;
  }

  void RemoveField(char const* field_name) { patch_[field_name] = nullptr; }

  void SetStringField(char const* field_name, std::string const& v) {
    patch_[field_name] = v;
  }

  void SetBoolField(char const* field_name, bool v) { patch_[field_name] = v; }

  void SetIntField(char const* field_name, std::int32_t v) {
    patch_[field_name] = v;
  }

  void SetIntField(char const* field_name, std::uint32_t v) {
    patch_[field_name] = v;
  }

  void SetIntField(char const* field_name, std::int64_t v) {
    patch_[field_name] = v;
  }

  void SetIntField(char const* field_name, std::uint64_t v) {
    patch_[field_name] = v;
  }

  void SetArrayField(char const* field_name,
                     std::vector<nlohmann::json> const& v) {
    patch_[field_name] = v;
  }

  template <typename Integer>
  void AddIntegerField(char const* field_name, Integer lhs, Integer rhs,
                       Integer null_value) {
    if (lhs == rhs) return;
    if (rhs == null_value) {
      patch_[field_name] = nullptr;
      return;
    }
    patch_[field_name] = rhs;
  }

  nlohmann::json patch_;
};

PatchBuilder::PatchBuilder() : pimpl_(std::make_unique<Impl>()) {}
PatchBuilder::~PatchBuilder() = default;

PatchBuilder::PatchBuilder(PatchBuilder const& other)
    : pimpl_(new Impl(*other.pimpl_)) {}

PatchBuilder::PatchBuilder(std::unique_ptr<Impl> impl)
    : pimpl_(std::move(impl)) {}

PatchBuilder& PatchBuilder::operator=(PatchBuilder const& other) {
  *pimpl_ = *other.pimpl_;
  return *this;
}

PatchBuilder::PatchBuilder(PatchBuilder&& rhs) noexcept
    : pimpl_(std::move(rhs.pimpl_)) {}

PatchBuilder& PatchBuilder::operator=(PatchBuilder&& rhs) noexcept {
  pimpl_ = std::move(rhs.pimpl_);
  return *this;
}

bool operator==(PatchBuilder const& a, PatchBuilder const& b) noexcept {
  return a.pimpl_->patch_ == b.pimpl_->patch_;
}

bool operator!=(PatchBuilder const& a, PatchBuilder const& b) noexcept {
  return !(a == b);
}

std::string PatchBuilder::ToString() const { return pimpl_->ToString(); }

bool PatchBuilder::empty() const { return pimpl_->empty(); }
void PatchBuilder::clear() { pimpl_->clear(); }

PatchBuilder& PatchBuilder::AddStringField(char const* field_name,
                                           std::string const& lhs,
                                           std::string const& rhs) {
  pimpl_->AddStringField(field_name, lhs, rhs);
  return *this;
}

PatchBuilder& PatchBuilder::AddBoolField(char const* field_name, bool lhs,
                                         bool rhs) {
  pimpl_->AddBoolField(field_name, lhs, rhs);
  return *this;
}

PatchBuilder& PatchBuilder::AddIntField(char const* field_name,
                                        std::int32_t lhs, std::int32_t rhs,
                                        std::int32_t null_value) {
  pimpl_->AddIntField(field_name, lhs, rhs, null_value);
  return *this;
}
PatchBuilder& PatchBuilder::AddIntField(char const* field_name,
                                        std::uint32_t lhs, std::uint32_t rhs,
                                        std::uint32_t null_value) {
  pimpl_->AddIntField(field_name, lhs, rhs, null_value);
  return *this;
}
PatchBuilder& PatchBuilder::AddIntField(char const* field_name,
                                        std::int64_t lhs, std::int64_t rhs,
                                        std::int64_t null_value) {
  pimpl_->AddIntField(field_name, lhs, rhs, null_value);
  return *this;
}
PatchBuilder& PatchBuilder::AddIntField(char const* field_name,
                                        std::uint64_t lhs, std::uint64_t rhs,
                                        std::uint64_t null_value) {
  pimpl_->AddIntField(field_name, lhs, rhs, null_value);
  return *this;
}

PatchBuilder& PatchBuilder::AddSubPatch(char const* field_name,
                                        PatchBuilder const& builder) {
  pimpl_->AddSubPatch(field_name, *builder.pimpl_);
  return *this;
}

PatchBuilder& PatchBuilder::RemoveField(char const* field_name) {
  pimpl_->RemoveField(field_name);
  return *this;
}

PatchBuilder& PatchBuilder::SetStringField(char const* field_name,
                                           std::string const& v) {
  pimpl_->SetStringField(field_name, v);
  return *this;
}

PatchBuilder& PatchBuilder::SetBoolField(char const* field_name, bool v) {
  pimpl_->SetBoolField(field_name, v);
  return *this;
}

PatchBuilder& PatchBuilder::SetIntField(char const* field_name,
                                        std::int32_t v) {
  pimpl_->SetIntField(field_name, v);
  return *this;
}

PatchBuilder& PatchBuilder::SetIntField(char const* field_name,
                                        std::uint32_t v) {
  pimpl_->SetIntField(field_name, v);
  return *this;
}

PatchBuilder& PatchBuilder::SetIntField(char const* field_name,
                                        std::int64_t v) {
  pimpl_->SetIntField(field_name, v);
  return *this;
}

PatchBuilder& PatchBuilder::SetIntField(char const* field_name,
                                        std::uint64_t v) {
  pimpl_->SetIntField(field_name, v);
  return *this;
}

PatchBuilder& PatchBuilder::SetArrayField(
    char const* field_name, std::string const& json_stringified_object) {
  pimpl_->SetArrayField(field_name,
                        nlohmann::json::parse(json_stringified_object));
  return *this;
}

nlohmann::json const& PatchBuilderDetails::GetPatch(PatchBuilder const& patch) {
  return patch.pimpl_->patch_;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
