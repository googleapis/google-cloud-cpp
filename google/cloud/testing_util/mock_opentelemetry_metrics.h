// Copyright 2026 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_OPENTELEMETRY_METRICS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_OPENTELEMETRY_METRICS_H

#include "google/cloud/version.h"
#include <gmock/gmock.h>
#include <opentelemetry/common/key_value_iterable_view.h>
#include <opentelemetry/context/context.h>
#include <opentelemetry/metrics/async_instruments.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/sync_instruments.h>
#include <opentelemetry/version.h>
#include <cstdint>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

template <typename T>
class MockHistogram : public opentelemetry::metrics::Histogram<T> {
 public:
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  MOCK_METHOD(void, Record,  // NOLINT(bugprone-exception-escape)
              (T value), (noexcept, override));
  MOCK_METHOD(void, Record,  // NOLINT(bugprone-exception-escape)
              (T, opentelemetry::common::KeyValueIterable const&),
              (noexcept, override));

#endif
  MOCK_METHOD(void, Record,  // NOLINT(bugprone-exception-escape)
              (T value, opentelemetry::context::Context const& context),
              (noexcept, override));
  MOCK_METHOD(void, Record,  // NOLINT(bugprone-exception-escape)
              (T value,
               opentelemetry::common::KeyValueIterable const& attributes,
               opentelemetry::context::Context const& context),
              (noexcept, override));
};

template <typename T>
class MockCounter : public opentelemetry::metrics::Counter<T> {
 public:
  MOCK_METHOD(void, Add,  // NOLINT(bugprone-exception-escape)
              (T value), (noexcept, override));
  MOCK_METHOD(void, Add,  // NOLINT(bugprone-exception-escape)
              (T value, opentelemetry::context::Context const&),
              (noexcept, override));
  MOCK_METHOD(void, Add,  // NOLINT(bugprone-exception-escape)
              (T value, opentelemetry::common::KeyValueIterable const&),
              (noexcept, override));
  MOCK_METHOD(void, Add,  // NOLINT(bugprone-exception-escape)
              (T value, opentelemetry::common::KeyValueIterable const&,
               opentelemetry::context::Context const&),
              (noexcept, override));
};

class MockMeter : public opentelemetry::metrics::Meter {
 public:
  MOCK_METHOD(opentelemetry::nostd::unique_ptr<
                  opentelemetry::metrics::Counter<uint64_t>>,
              CreateUInt64Counter,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));

  MOCK_METHOD(
      opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<double>>,
      CreateDoubleCounter,  // NOLINT(bugprone-exception-escape)
      (opentelemetry::nostd::string_view, opentelemetry::nostd::string_view,
       opentelemetry::nostd::string_view),
      (noexcept, override));

  MOCK_METHOD(
      opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::ObservableInstrument>,
      CreateInt64ObservableCounter,  // NOLINT(bugprone-exception-escape)
      (opentelemetry::nostd::string_view, opentelemetry::nostd::string_view,
       opentelemetry::nostd::string_view),
      (noexcept, override));

  MOCK_METHOD(
      opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::ObservableInstrument>,
      CreateDoubleObservableCounter,  // NOLINT(bugprone-exception-escape)
      (opentelemetry::nostd::string_view, opentelemetry::nostd::string_view,
       opentelemetry::nostd::string_view),
      (noexcept, override));

  MOCK_METHOD(opentelemetry::nostd::unique_ptr<
                  opentelemetry::metrics::Histogram<uint64_t>>,
              CreateUInt64Histogram,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));

  MOCK_METHOD(opentelemetry::nostd::unique_ptr<
                  opentelemetry::metrics::Histogram<double>>,
              CreateDoubleHistogram,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  MOCK_METHOD(
      opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Gauge<int64_t>>,
      CreateInt64Gauge,  // NOLINT(bugprone-exception-escape)
      (opentelemetry::nostd::string_view, opentelemetry::nostd::string_view,
       opentelemetry::nostd::string_view),
      (noexcept, override));

  MOCK_METHOD(
      opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Gauge<double>>,
      CreateDoubleGauge,  // NOLINT(bugprone-exception-escape)
      (opentelemetry::nostd::string_view, opentelemetry::nostd::string_view,
       opentelemetry::nostd::string_view),
      (noexcept, override));

  MOCK_METHOD(uintptr_t,
              RegisterCallback,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::metrics::MultiObservableCallbackPtr, void*,
               opentelemetry::nostd::span<
                   opentelemetry::metrics::ObservableInstrument*>),
              (noexcept, override));

  MOCK_METHOD(void,
              DeregisterCallback,  // NOLINT(bugprone-exception-escape)
              (uintptr_t), (noexcept, override));
#endif

  MOCK_METHOD(opentelemetry::nostd::shared_ptr<
                  opentelemetry::metrics::ObservableInstrument>,
              CreateInt64ObservableGauge,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));

  MOCK_METHOD(opentelemetry::nostd::shared_ptr<
                  opentelemetry::metrics::ObservableInstrument>,
              CreateDoubleObservableGauge,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));

  MOCK_METHOD(opentelemetry::nostd::unique_ptr<
                  opentelemetry::metrics::UpDownCounter<int64_t>>,
              CreateInt64UpDownCounter,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));

  MOCK_METHOD(opentelemetry::nostd::unique_ptr<
                  opentelemetry::metrics::UpDownCounter<double>>,
              CreateDoubleUpDownCounter,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));

  MOCK_METHOD(
      opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::ObservableInstrument>,
      CreateInt64ObservableUpDownCounter,  // NOLINT(bugprone-exception-escape)
      (opentelemetry::nostd::string_view, opentelemetry::nostd::string_view,
       opentelemetry::nostd::string_view),
      (noexcept, override));

  MOCK_METHOD(
      opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::ObservableInstrument>,
      CreateDoubleObservableUpDownCounter,  // NOLINT(bugprone-exception-escape)
      (opentelemetry::nostd::string_view, opentelemetry::nostd::string_view,
       opentelemetry::nostd::string_view),
      (noexcept, override));
};

class MockMeterProvider : public opentelemetry::metrics::MeterProvider {
 public:
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  MOCK_METHOD(opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>,
              GetMeter,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::common::KeyValueIterable const*),
              (noexcept, override));

  MOCK_METHOD(void, RemoveMeter,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));

#else
  MOCK_METHOD(opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>,
              GetMeter,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));
#endif
};

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_OPENTELEMETRY_METRICS_H
