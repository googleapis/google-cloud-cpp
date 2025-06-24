// Copyright 2025 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_METRICS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_METRICS_H

#include "google/cloud/bigtable/version.h"
#include "google/cloud/opentelemetry/monitoring_exporter.h"
#include "google/cloud/status.h"
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/sync_instruments.h>
#include <map>
#include <string>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// It will be convenient to have one struct
struct MetricLabels {
  // TODO : consider avoiding copies in the no-CSM case with references
  std::string method;
  std::string streaming;
  std::string client_name;
  std::string client_uid;
  std::string table_name;
  std::string app_profile;
  std::string cluster;
  std::string zone;
  // TODO : what is the format of status? "UNAVAILABLE"? Or an int? who knows.
  std::string status;
};

struct ResourceLabels {
  std::string project_id;
  std::string instance;
  std::string table;
  std::string cluster;
  std::string zone;
};

inline std::ostream& operator<<(std::ostream& os, ResourceLabels const& r) {
  return os << r.project_id << "/" << r.instance << "/" << r.table << "/"
            << r.cluster << "/" << r.zone;
}

struct DataLabels {
  std::string method;
  std::string streaming;
  std::string client_name;
  std::string client_uid;
  std::string app_profile;
  std::string status;
};

// using LabelMap = std::map<std::string, std::string>;
otel::LabelMap IntoMap(MetricLabels labels);
otel::LabelMap IntoMap(ResourceLabels const& r, DataLabels const& d);

std::ostream& operator<<(std::ostream& os, otel::LabelMap const& m);

class Metrics {
 public:
  virtual otel::LabelMap const& labels() const { return empty; }
  virtual void RecordAttemptLatency(double value,
                                    MetricLabels const& labels) const {}
  virtual void RecordOperationLatency(double value,
                                      MetricLabels const& labels) const {}

 private:
  otel::LabelMap empty;
};

std::shared_ptr<Metrics> MakeMetrics();

// end darren's prototype

struct PreCallParams {
  std::chrono::system_clock::time_point attempt_start;
};

struct PostCallParams {
  std::chrono::system_clock::time_point attempt_end;
  google::cloud::Status attempt_status;
  std::string cluster;
  std::string zone;
};

struct FirstResponseParams {
  std::chrono::system_clock::time_point first_response;
};

struct OnDoneParams {
  std::chrono::system_clock::time_point operation_end;
  google::cloud::Status operation_status;
};

class Metric {
 public:
  virtual ~Metric() = 0;
  virtual void PreCall(opentelemetry::context::Context const& context,
                       PreCallParams p) {}
  virtual void PostCall(opentelemetry::context::Context const& context,
                        PostCallParams p) {}
  virtual void FirstResponse(opentelemetry::context::Context const& context,
                             FirstResponseParams p) {}
  virtual void OnDone(opentelemetry::context::Context const& context,
                      OnDoneParams p) {}
};

class AttemptLatency : public Metric {
 public:
  AttemptLatency(
      ResourceLabels resource_labels, DataLabels data_labels,
      std::string const& name, std::string const& version,
      std::shared_ptr<opentelemetry::metrics::MeterProvider> provider)
      : resource_labels_(std::move(resource_labels)),
        data_labels_(std::move(data_labels)),
        provider_(std::move(provider)),
        attempt_latencies_(provider_->GetMeter(name, version)
                               ->CreateDoubleHistogram("attempt_latencies")) {}
  void PreCall(opentelemetry::context::Context const&,
               PreCallParams p) override {
    attempt_start_ = std::move(p.attempt_start);
  }
  void PostCall(opentelemetry::context::Context const& context,
                PostCallParams p) override {
    resource_labels_.cluster = p.cluster;
    resource_labels_.zone = p.zone;
    data_labels_.status = StatusCodeToString(p.attempt_status.code());
    using dmilliseconds = std::chrono::duration<double, std::milli>;
    auto attempt_elapsed = std::chrono::duration_cast<dmilliseconds>(
        p.attempt_end - attempt_start_);
    auto m = IntoMap(resource_labels_, data_labels_);
    //    std::cout << __func__ << ": LabelMap=" << m << std::endl;
    attempt_latencies_->Record(attempt_elapsed.count(), std::move(m), context);
  }

 private:
  ResourceLabels resource_labels_;
  DataLabels data_labels_;
  std::shared_ptr<opentelemetry::metrics::MeterProvider> provider_;
  std::unique_ptr<opentelemetry::metrics::Histogram<double>> attempt_latencies_;
  std::chrono::system_clock::time_point attempt_start_;
};

class OperationLatency : public Metric {
 public:
  OperationLatency(
      ResourceLabels resource_labels, DataLabels data_labels,
      std::string const& name, std::string const& version,
      std::shared_ptr<opentelemetry::metrics::MeterProvider> provider)
      : resource_labels_(std::move(resource_labels)),
        data_labels_(std::move(data_labels)),
        provider_(std::move(provider)),
        operation_latencies_(
            provider_->GetMeter(name, version)
                ->CreateDoubleHistogram("operation_latencies")) {}
  void PreCall(opentelemetry::context::Context const&,
               PreCallParams p) override {
    operation_start_ = std::move(p.attempt_start);
  }

  void PostCall(opentelemetry::context::Context const& context,
                PostCallParams p) override {
    resource_labels_.cluster = p.cluster;
    resource_labels_.zone = p.zone;
    data_labels_.status = StatusCodeToString(p.attempt_status.code());
  }

  void OnDone(opentelemetry::context::Context const& context,
              OnDoneParams p) override {
    data_labels_.status = StatusCodeToString(p.operation_status.code());
    using dmilliseconds = std::chrono::duration<double, std::milli>;
    auto operation_elapsed = std::chrono::duration_cast<dmilliseconds>(
        p.operation_end - operation_start_);
    operation_latencies_->Record(operation_elapsed.count(),
                                 IntoMap(resource_labels_, data_labels_),
                                 context);
  }

 private:
  ResourceLabels resource_labels_;
  DataLabels data_labels_;
  std::shared_ptr<opentelemetry::metrics::MeterProvider> provider_;
  std::unique_ptr<opentelemetry::metrics::Histogram<double>>
      operation_latencies_;
  std::chrono::system_clock::time_point operation_start_;
};

class RetryCount : public Metric {
 public:
  RetryCount(ResourceLabels resource_labels, DataLabels data_labels,
             std::string const& name, std::string const& version,
             std::shared_ptr<opentelemetry::metrics::MeterProvider> provider)
      : resource_labels_(std::move(resource_labels)),
        data_labels_(std::move(data_labels)),
        provider_(std::move(provider)),
        retry_count_(provider_->GetMeter(name, version)
                         ->CreateUInt64Counter("retry_count")) {}
  void PreCall(opentelemetry::context::Context const&, PreCallParams) override {
    retry_count_->Add(1, IntoMap(resource_labels_, data_labels_));
  }

 private:
  ResourceLabels resource_labels_;
  DataLabels data_labels_;
  std::shared_ptr<opentelemetry::metrics::MeterProvider> provider_;
  std::unique_ptr<opentelemetry::metrics::Counter<std::uint64_t>> retry_count_;
};

class FirstResponseLatency : public Metric {
 public:
  FirstResponseLatency(
      ResourceLabels resource_labels, DataLabels data_labels,
      std::string const& name, std::string const& version,
      std::shared_ptr<opentelemetry::metrics::MeterProvider> provider)
      : resource_labels_(std::move(resource_labels)),
        data_labels_(std::move(data_labels)),
        provider_(std::move(provider)),
        first_response_latencies_(
            provider_->GetMeter(name, version)
                ->CreateDoubleHistogram("first_response_latencies")) {}
  void PreCall(opentelemetry::context::Context const&,
               PreCallParams p) override {
    operation_start_ = std::move(p.attempt_start);
  }

  void FirstResponse(opentelemetry::context::Context const& context,
                     FirstResponseParams p) override {
    using dmilliseconds = std::chrono::duration<double, std::milli>;
    auto first_response_elapsed = std::chrono::duration_cast<dmilliseconds>(
        p.first_response - operation_start_);
    first_response_latencies_->Record(first_response_elapsed.count(),
                                      IntoMap(resource_labels_, data_labels_),
                                      context);
  }

 private:
  ResourceLabels resource_labels_;
  DataLabels data_labels_;
  std::shared_ptr<opentelemetry::metrics::MeterProvider> provider_;
  std::unique_ptr<opentelemetry::metrics::Histogram<double>>
      first_response_latencies_;
  std::chrono::system_clock::time_point operation_start_;
};

std::shared_ptr<std::vector<std::shared_ptr<Metric>>> MakeMetricCollection();

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_METRICS_H
