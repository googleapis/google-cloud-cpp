// Copyright 2023 Google LLC
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
#if 0
#include "google/cloud/testing_util/status_matchers.h"
#include "google/protobuf/util/json_util.h"
#include <generator/integration_tests/discovery.pb.h>
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Eq;

auto constexpr kJson = R"""(
{
  "kind": "compute#autoscalerAggregatedList",
  "id": "projects/cloud-cpp-testing-resources/aggregated/autoscalers",
  "items": {
    "regions/us-central1": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/us-central1' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/us-central1"
          }
        ]
      }
    },
    "regions/us-central2": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/us-central2' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/us-central2"
          }
        ]
      }
    },
    "regions/europe-west1": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/europe-west1' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/europe-west1"
          }
        ]
      }
    },
    "regions/us-west1": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/us-west1' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/us-west1"
          }
        ]
      }
    },
    "regions/asia-east1": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/asia-east1' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/asia-east1"
          }
        ]
      }
    },
    "regions/us-east1": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/us-east1' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/us-east1"
          }
        ]
      }
    },
    "regions/asia-northeast1": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/asia-northeast1' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/asia-northeast1"
          }
        ]
      }
    },
    "regions/asia-southeast1": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/asia-southeast1' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/asia-southeast1"
          }
        ]
      }
    },
    "regions/us-east4": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/us-east4' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/us-east4"
          }
        ]
      }
    },
    "regions/australia-southeast1": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/australia-southeast1' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/australia-southeast1"
          }
        ]
      }
    },
    "regions/europe-west2": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/europe-west2' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/europe-west2"
          }
        ]
      }
    },
    "regions/europe-west3": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/europe-west3' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/europe-west3"
          }
        ]
      }
    },
    "regions/southamerica-east1": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/southamerica-east1' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/southamerica-east1"
          }
        ]
      }
    },
    "regions/asia-south1": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/asia-south1' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/asia-south1"
          }
        ]
      }
    },
    "regions/northamerica-northeast1": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/northamerica-northeast1' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/northamerica-northeast1"
          }
        ]
      }
    },
    "regions/europe-west4": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/europe-west4' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/europe-west4"
          }
        ]
      }
    },
    "regions/europe-north1": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/europe-north1' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/europe-north1"
          }
        ]
      }
    },
    "regions/us-west2": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/us-west2' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/us-west2"
          }
        ]
      }
    },
    "regions/asia-east2": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/asia-east2' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/asia-east2"
          }
        ]
      }
    },
    "regions/europe-west6": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/europe-west6' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/europe-west6"
          }
        ]
      }
    },
    "regions/asia-northeast2": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/asia-northeast2' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/asia-northeast2"
          }
        ]
      }
    },
    "regions/asia-northeast3": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/asia-northeast3' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/asia-northeast3"
          }
        ]
      }
    },
    "regions/us-west3": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/us-west3' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/us-west3"
          }
        ]
      }
    },
    "regions/us-west4": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/us-west4' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/us-west4"
          }
        ]
      }
    },
    "regions/asia-southeast2": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/asia-southeast2' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/asia-southeast2"
          }
        ]
      }
    },
    "regions/europe-central2": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/europe-central2' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/europe-central2"
          }
        ]
      }
    },
    "regions/northamerica-northeast2": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/northamerica-northeast2' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/northamerica-northeast2"
          }
        ]
      }
    },
    "regions/asia-south2": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/asia-south2' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/asia-south2"
          }
        ]
      }
    },
    "regions/australia-southeast2": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/australia-southeast2' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/australia-southeast2"
          }
        ]
      }
    },
    "regions/southamerica-west1": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/southamerica-west1' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/southamerica-west1"
          }
        ]
      }
    },
    "regions/us-east7": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/us-east7' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/us-east7"
          }
        ]
      }
    },
    "regions/europe-west8": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/europe-west8' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/europe-west8"
          }
        ]
      }
    },
    "regions/europe-west9": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/europe-west9' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/europe-west9"
          }
        ]
      }
    },
    "regions/us-east5": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/us-east5' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/us-east5"
          }
        ]
      }
    },
    "regions/europe-southwest1": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/europe-southwest1' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/europe-southwest1"
          }
        ]
      }
    },
    "regions/us-south1": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/us-south1' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/us-south1"
          }
        ]
      }
    },
    "regions/me-west1": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'regions/me-west1' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "regions/me-west1"
          }
        ]
      }
    },
    "zones/us-central1-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-central1-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-central1-a"
          }
        ]
      }
    },
    "zones/us-central1-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-central1-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-central1-b"
          }
        ]
      }
    },
    "zones/us-central1-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-central1-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-central1-c"
          }
        ]
      }
    },
    "zones/us-central1-f": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-central1-f' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-central1-f"
          }
        ]
      }
    },
    "zones/us-central2-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-central2-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-central2-b"
          }
        ]
      }
    },
    "zones/us-central2-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-central2-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-central2-a"
          }
        ]
      }
    },
    "zones/us-central2-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-central2-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-central2-c"
          }
        ]
      }
    },
    "zones/us-central2-d": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-central2-d' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-central2-d"
          }
        ]
      }
    },
    "zones/europe-west1-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west1-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west1-b"
          }
        ]
      }
    },
    "zones/europe-west1-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west1-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west1-c"
          }
        ]
      }
    },
    "zones/europe-west1-d": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west1-d' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west1-d"
          }
        ]
      }
    },
    "zones/us-west1-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-west1-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-west1-a"
          }
        ]
      }
    },
    "zones/us-west1-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-west1-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-west1-b"
          }
        ]
      }
    },
    "zones/us-west1-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-west1-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-west1-c"
          }
        ]
      }
    },
    "zones/asia-east1-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-east1-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-east1-a"
          }
        ]
      }
    },
    "zones/asia-east1-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-east1-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-east1-b"
          }
        ]
      }
    },
    "zones/asia-east1-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-east1-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-east1-c"
          }
        ]
      }
    },
    "zones/us-east1-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-east1-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-east1-b"
          }
        ]
      }
    },
    "zones/us-east1-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-east1-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-east1-c"
          }
        ]
      }
    },
    "zones/us-east1-d": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-east1-d' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-east1-d"
          }
        ]
      }
    },
    "zones/asia-northeast1-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-northeast1-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-northeast1-a"
          }
        ]
      }
    },
    "zones/asia-northeast1-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-northeast1-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-northeast1-b"
          }
        ]
      }
    },
    "zones/asia-northeast1-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-northeast1-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-northeast1-c"
          }
        ]
      }
    },
    "zones/asia-southeast1-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-southeast1-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-southeast1-a"
          }
        ]
      }
    },
    "zones/asia-southeast1-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-southeast1-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-southeast1-b"
          }
        ]
      }
    },
    "zones/asia-southeast1-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-southeast1-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-southeast1-c"
          }
        ]
      }
    },
    "zones/us-east4-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-east4-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-east4-a"
          }
        ]
      }
    },
    "zones/us-east4-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-east4-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-east4-b"
          }
        ]
      }
    },
    "zones/us-east4-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-east4-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-east4-c"
          }
        ]
      }
    },
    "zones/australia-southeast1-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/australia-southeast1-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/australia-southeast1-c"
          }
        ]
      }
    },
    "zones/australia-southeast1-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/australia-southeast1-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/australia-southeast1-a"
          }
        ]
      }
    },
    "zones/australia-southeast1-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/australia-southeast1-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/australia-southeast1-b"
          }
        ]
      }
    },
    "zones/europe-west2-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west2-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west2-a"
          }
        ]
      }
    },
    "zones/europe-west2-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west2-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west2-b"
          }
        ]
      }
    },
    "zones/europe-west2-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west2-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west2-c"
          }
        ]
      }
    },
    "zones/europe-west3-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west3-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west3-c"
          }
        ]
      }
    },
    "zones/europe-west3-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west3-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west3-a"
          }
        ]
      }
    },
    "zones/europe-west3-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west3-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west3-b"
          }
        ]
      }
    },
    "zones/southamerica-east1-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/southamerica-east1-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/southamerica-east1-a"
          }
        ]
      }
    },
    "zones/southamerica-east1-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/southamerica-east1-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/southamerica-east1-b"
          }
        ]
      }
    },
    "zones/southamerica-east1-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/southamerica-east1-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/southamerica-east1-c"
          }
        ]
      }
    },
    "zones/asia-south1-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-south1-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-south1-b"
          }
        ]
      }
    },
    "zones/asia-south1-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-south1-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-south1-a"
          }
        ]
      }
    },
    "zones/asia-south1-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-south1-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-south1-c"
          }
        ]
      }
    },
    "zones/northamerica-northeast1-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/northamerica-northeast1-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/northamerica-northeast1-a"
          }
        ]
      }
    },
    "zones/northamerica-northeast1-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/northamerica-northeast1-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/northamerica-northeast1-b"
          }
        ]
      }
    },
    "zones/northamerica-northeast1-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/northamerica-northeast1-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/northamerica-northeast1-c"
          }
        ]
      }
    },
    "zones/europe-west4-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west4-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west4-c"
          }
        ]
      }
    },
    "zones/europe-west4-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west4-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west4-b"
          }
        ]
      }
    },
    "zones/europe-west4-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west4-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west4-a"
          }
        ]
      }
    },
    "zones/europe-north1-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-north1-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-north1-b"
          }
        ]
      }
    },
    "zones/europe-north1-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-north1-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-north1-c"
          }
        ]
      }
    },
    "zones/europe-north1-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-north1-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-north1-a"
          }
        ]
      }
    },
    "zones/us-west2-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-west2-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-west2-c"
          }
        ]
      }
    },
    "zones/us-west2-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-west2-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-west2-b"
          }
        ]
      }
    },
    "zones/us-west2-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-west2-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-west2-a"
          }
        ]
      }
    },
    "zones/asia-east2-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-east2-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-east2-c"
          }
        ]
      }
    },
    "zones/asia-east2-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-east2-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-east2-b"
          }
        ]
      }
    },
    "zones/asia-east2-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-east2-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-east2-a"
          }
        ]
      }
    },
    "zones/europe-west6-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west6-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west6-b"
          }
        ]
      }
    },
    "zones/europe-west6-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west6-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west6-c"
          }
        ]
      }
    },
    "zones/europe-west6-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west6-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west6-a"
          }
        ]
      }
    },
    "zones/asia-northeast2-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-northeast2-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-northeast2-b"
          }
        ]
      }
    },
    "zones/asia-northeast2-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-northeast2-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-northeast2-c"
          }
        ]
      }
    },
    "zones/asia-northeast2-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-northeast2-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-northeast2-a"
          }
        ]
      }
    },
    "zones/asia-northeast3-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-northeast3-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-northeast3-a"
          }
        ]
      }
    },
    "zones/asia-northeast3-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-northeast3-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-northeast3-c"
          }
        ]
      }
    },
    "zones/asia-northeast3-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-northeast3-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-northeast3-b"
          }
        ]
      }
    },
    "zones/us-west3-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-west3-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-west3-a"
          }
        ]
      }
    },
    "zones/us-west3-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-west3-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-west3-b"
          }
        ]
      }
    },
    "zones/us-west3-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-west3-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-west3-c"
          }
        ]
      }
    },
    "zones/us-west4-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-west4-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-west4-c"
          }
        ]
      }
    },
    "zones/us-west4-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-west4-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-west4-a"
          }
        ]
      }
    },
    "zones/us-west4-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-west4-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-west4-b"
          }
        ]
      }
    },
    "zones/asia-southeast2-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-southeast2-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-southeast2-a"
          }
        ]
      }
    },
    "zones/asia-southeast2-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-southeast2-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-southeast2-c"
          }
        ]
      }
    },
    "zones/asia-southeast2-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-southeast2-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-southeast2-b"
          }
        ]
      }
    },
    "zones/europe-central2-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-central2-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-central2-b"
          }
        ]
      }
    },
    "zones/europe-central2-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-central2-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-central2-c"
          }
        ]
      }
    },
    "zones/europe-central2-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-central2-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-central2-a"
          }
        ]
      }
    },
    "zones/northamerica-northeast2-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/northamerica-northeast2-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/northamerica-northeast2-b"
          }
        ]
      }
    },
    "zones/northamerica-northeast2-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/northamerica-northeast2-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/northamerica-northeast2-a"
          }
        ]
      }
    },
    "zones/northamerica-northeast2-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/northamerica-northeast2-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/northamerica-northeast2-c"
          }
        ]
      }
    },
    "zones/asia-south2-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-south2-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-south2-a"
          }
        ]
      }
    },
    "zones/asia-south2-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-south2-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-south2-c"
          }
        ]
      }
    },
    "zones/asia-south2-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/asia-south2-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/asia-south2-b"
          }
        ]
      }
    },
    "zones/australia-southeast2-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/australia-southeast2-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/australia-southeast2-a"
          }
        ]
      }
    },
    "zones/australia-southeast2-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/australia-southeast2-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/australia-southeast2-c"
          }
        ]
      }
    },
    "zones/australia-southeast2-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/australia-southeast2-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/australia-southeast2-b"
          }
        ]
      }
    },
    "zones/southamerica-west1-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/southamerica-west1-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/southamerica-west1-a"
          }
        ]
      }
    },
    "zones/southamerica-west1-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/southamerica-west1-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/southamerica-west1-b"
          }
        ]
      }
    },
    "zones/southamerica-west1-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/southamerica-west1-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/southamerica-west1-c"
          }
        ]
      }
    },
    "zones/us-east7-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-east7-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-east7-a"
          }
        ]
      }
    },
    "zones/us-east7-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-east7-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-east7-b"
          }
        ]
      }
    },
    "zones/us-east7-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-east7-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-east7-c"
          }
        ]
      }
    },
    "zones/europe-west8-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west8-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west8-a"
          }
        ]
      }
    },
    "zones/europe-west8-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west8-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west8-b"
          }
        ]
      }
    },
    "zones/europe-west8-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west8-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west8-c"
          }
        ]
      }
    },
    "zones/europe-west9-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west9-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west9-b"
          }
        ]
      }
    },
    "zones/europe-west9-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west9-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west9-a"
          }
        ]
      }
    },
    "zones/europe-west9-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-west9-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-west9-c"
          }
        ]
      }
    },
    "zones/us-east5-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-east5-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-east5-c"
          }
        ]
      }
    },
    "zones/us-east5-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-east5-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-east5-b"
          }
        ]
      }
    },
    "zones/us-east5-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-east5-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-east5-a"
          }
        ]
      }
    },
    "zones/europe-southwest1-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-southwest1-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-southwest1-b"
          }
        ]
      }
    },
    "zones/europe-southwest1-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-southwest1-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-southwest1-a"
          }
        ]
      }
    },
    "zones/europe-southwest1-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/europe-southwest1-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/europe-southwest1-c"
          }
        ]
      }
    },
    "zones/us-south1-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-south1-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-south1-c"
          }
        ]
      }
    },
    "zones/us-south1-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-south1-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-south1-a"
          }
        ]
      }
    },
    "zones/us-south1-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/us-south1-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/us-south1-b"
          }
        ]
      }
    },
    "zones/me-west1-b": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/me-west1-b' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/me-west1-b"
          }
        ]
      }
    },
    "zones/me-west1-a": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/me-west1-a' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/me-west1-a"
          }
        ]
      }
    },
    "zones/me-west1-c": {
      "warning": {
        "code": "NO_RESULTS_ON_PAGE",
        "message": "There are no results for scope 'zones/me-west1-c' on this page.",
        "data": [
          {
            "key": "scope",
            "value": "zones/me-west1-c"
          }
        ]
      }
    }
  },
  "selfLink": "https://www.googleapis.com/compute/v1/projects/cloud-cpp-testing-resources/aggregated/autoscalers"
}

)""";

TEST(DiscoveryTest, AutoscalerAggregatedListJsonToMessage) {
  google::test::discovery::v1::AutoscalerAggregatedList list;
  google::protobuf::util::JsonParseOptions parse_options;
  parse_options.ignore_unknown_fields = true;
  auto result =
      google::protobuf::util::JsonStringToMessage(kJson, &list, parse_options);
  ASSERT_THAT(result, Eq(google::protobuf::util::OkStatus()));
  EXPECT_THAT(
      list.id(),
      Eq("projects/cloud-cpp-testing-resources/aggregated/autoscalers"));
  EXPECT_THAT(list.kind(), Eq("compute#autoscalerAggregatedList"));
  auto items = list.items();
  auto item = items.find("zones/me-west1-c");
  ASSERT_TRUE(item != items.end());
  auto scoped_list = item->second;
  //  EXPECT_THAT(scoped_list.warning().code(), Eq("NO_RESULTS_ON_PAGE"));
  EXPECT_THAT(scoped_list.warning().code(), Eq("NO_RESULTS_ON_PAGE"));
  EXPECT_THAT(
      scoped_list.warning().message(),
      Eq("There are no results for scope 'zones/me-west1-c' on this page."));
  //  auto data = scoped_list.warning().data();
  //  auto datum = data.find("scope");
  //  ASSERT_TRUE(datum != data.end());
  //  EXPECT_THAT(datum->second, Eq("zones/me-west-c"));
}

auto constexpr kBigQueryJson = R"""(
{
  "kind": "bigquery#datasetList",
  "etag": "2IPUO8USB6tBXgOFs6lwmw==",
  "nextPageToken": "covid19_public_forecasts_asia_ne1",
  "datasets": [
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:america_health_rankings",
      "datasetReference": {
        "datasetId": "america_health_rankings",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:austin_311",
      "datasetReference": {
        "datasetId": "austin_311",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:austin_bikeshare",
      "datasetReference": {
        "datasetId": "austin_bikeshare",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:austin_crime",
      "datasetReference": {
        "datasetId": "austin_crime",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:austin_incidents",
      "datasetReference": {
        "datasetId": "austin_incidents",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:austin_waste",
      "datasetReference": {
        "datasetId": "austin_waste",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:baseball",
      "datasetReference": {
        "datasetId": "baseball",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:bbc_news",
      "datasetReference": {
        "datasetId": "bbc_news",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:bitcoin_blockchain",
      "datasetReference": {
        "datasetId": "bitcoin_blockchain",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:blackhole_database",
      "datasetReference": {
        "datasetId": "blackhole_database",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:bls",
      "datasetReference": {
        "datasetId": "bls",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:bls_qcew",
      "datasetReference": {
        "datasetId": "bls_qcew",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:breathe",
      "datasetReference": {
        "datasetId": "breathe",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:broadstreet_adi",
      "datasetReference": {
        "datasetId": "broadstreet_adi",
        "projectId": "bigquery-public-data"
      },
      "labels": {
        "freebqcovid": "freebqcovid"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:catalonian_mobile_coverage",
      "datasetReference": {
        "datasetId": "catalonian_mobile_coverage",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:catalonian_mobile_coverage_eu",
      "datasetReference": {
        "datasetId": "catalonian_mobile_coverage_eu",
        "projectId": "bigquery-public-data"
      },
      "location": "EU"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:census_bureau_acs",
      "datasetReference": {
        "datasetId": "census_bureau_acs",
        "projectId": "bigquery-public-data"
      },
      "labels": {
        "freebqcovid": "freebqcovid"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:census_bureau_construction",
      "datasetReference": {
        "datasetId": "census_bureau_construction",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:census_bureau_international",
      "datasetReference": {
        "datasetId": "census_bureau_international",
        "projectId": "bigquery-public-data"
      },
      "labels": {
        "freebqcovid": "freebqcovid"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:census_bureau_usa",
      "datasetReference": {
        "datasetId": "census_bureau_usa",
        "projectId": "bigquery-public-data"
      },
      "labels": {
        "freebqcovid": "freebqcovid"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:census_opportunity_atlas",
      "datasetReference": {
        "datasetId": "census_opportunity_atlas",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:census_utility",
      "datasetReference": {
        "datasetId": "census_utility",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:cfpb_complaints",
      "datasetReference": {
        "datasetId": "cfpb_complaints",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:chicago_crime",
      "datasetReference": {
        "datasetId": "chicago_crime",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:chicago_taxi_trips",
      "datasetReference": {
        "datasetId": "chicago_taxi_trips",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:clemson_dice",
      "datasetReference": {
        "datasetId": "clemson_dice",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:cloud_storage_geo_index",
      "datasetReference": {
        "datasetId": "cloud_storage_geo_index",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:cms_codes",
      "datasetReference": {
        "datasetId": "cms_codes",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:cms_medicare",
      "datasetReference": {
        "datasetId": "cms_medicare",
        "projectId": "bigquery-public-data"
      },
      "labels": {
        "freebqcovid": "freebqcovid"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:cms_synthetic_patient_data_omop",
      "datasetReference": {
        "datasetId": "cms_synthetic_patient_data_omop",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:country_codes",
      "datasetReference": {
        "datasetId": "country_codes",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:covid19_aha",
      "datasetReference": {
        "datasetId": "covid19_aha",
        "projectId": "bigquery-public-data"
      },
      "labels": {
        "freebqcovid": "freebqcovid"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:covid19_covidtracking",
      "datasetReference": {
        "datasetId": "covid19_covidtracking",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:covid19_ecdc",
      "datasetReference": {
        "datasetId": "covid19_ecdc",
        "projectId": "bigquery-public-data"
      },
      "labels": {
        "freebqcovid": "freebqcovid"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:covid19_ecdc_eu",
      "datasetReference": {
        "datasetId": "covid19_ecdc_eu",
        "projectId": "bigquery-public-data"
      },
      "labels": {
        "freebqcovid": "freebqcovid"
      },
      "location": "EU"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:covid19_genome_sequence",
      "datasetReference": {
        "datasetId": "covid19_genome_sequence",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:covid19_geotab_mobility_impact",
      "datasetReference": {
        "datasetId": "covid19_geotab_mobility_impact",
        "projectId": "bigquery-public-data"
      },
      "labels": {
        "freebqcovid": "freebqcovid"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:covid19_geotab_mobility_impact_eu",
      "datasetReference": {
        "datasetId": "covid19_geotab_mobility_impact_eu",
        "projectId": "bigquery-public-data"
      },
      "labels": {
        "freebqcovid": "freebqcovid"
      },
      "location": "EU"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:covid19_google_mobility",
      "datasetReference": {
        "datasetId": "covid19_google_mobility",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:covid19_google_mobility_eu",
      "datasetReference": {
        "datasetId": "covid19_google_mobility_eu",
        "projectId": "bigquery-public-data"
      },
      "labels": {
        "freebqcovid": "freebqcovid"
      },
      "location": "EU"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:covid19_govt_response",
      "datasetReference": {
        "datasetId": "covid19_govt_response",
        "projectId": "bigquery-public-data"
      },
      "labels": {
        "freebqcovid": "freebqcovid"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:covid19_italy",
      "datasetReference": {
        "datasetId": "covid19_italy",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:covid19_italy_eu",
      "datasetReference": {
        "datasetId": "covid19_italy_eu",
        "projectId": "bigquery-public-data"
      },
      "location": "EU"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:covid19_jhu_csse",
      "datasetReference": {
        "datasetId": "covid19_jhu_csse",
        "projectId": "bigquery-public-data"
      },
      "labels": {
        "freebqcovid": "freebqcovid"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:covid19_jhu_csse_eu",
      "datasetReference": {
        "datasetId": "covid19_jhu_csse_eu",
        "projectId": "bigquery-public-data"
      },
      "labels": {
        "freebqcovid": "freebqcovid"
      },
      "location": "EU"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:covid19_nyt",
      "datasetReference": {
        "datasetId": "covid19_nyt",
        "projectId": "bigquery-public-data"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:covid19_open_data",
      "datasetReference": {
        "datasetId": "covid19_open_data",
        "projectId": "bigquery-public-data"
      },
      "labels": {
        "freebqcovid": "freebqcovid"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:covid19_open_data_eu",
      "datasetReference": {
        "datasetId": "covid19_open_data_eu",
        "projectId": "bigquery-public-data"
      },
      "labels": {
        "freebqcovid": "freebqcovid"
      },
      "location": "EU"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:covid19_public_forecasts",
      "datasetReference": {
        "datasetId": "covid19_public_forecasts",
        "projectId": "bigquery-public-data"
      },
      "labels": {
        "freebqcovid": "freebqcovid"
      },
      "location": "US"
    },
    {
      "kind": "bigquery#dataset",
      "id": "bigquery-public-data:covid19_public_forecasts_asia_ne1",
      "datasetReference": {
        "datasetId": "covid19_public_forecasts_asia_ne1",
        "projectId": "bigquery-public-data"
      },
      "location": "asia-northeast1"
    }
  ]
}

)""";

TEST(DiscoveryTest, DatasetListJsonToMessage) {
  google::test::discovery::v1::DatasetList list;
  google::protobuf::util::JsonParseOptions parse_options;
  parse_options.ignore_unknown_fields = true;
  auto result = google::protobuf::util::JsonStringToMessage(
      kBigQueryJson, &list, parse_options);
  ASSERT_THAT(result, Eq(google::protobuf::util::OkStatus()));
  EXPECT_THAT(list.kind(), Eq("bigquery#datasetList"));
  EXPECT_THAT(list.etag(), Eq("2IPUO8USB6tBXgOFs6lwmw=="));
  EXPECT_THAT(list.next_page_token(), Eq("covid19_public_forecasts_asia_ne1"));
  auto datasets = list.datasets();
  auto num_datasets = datasets.size();
  EXPECT_GT(num_datasets, 0);
  for (auto const& d : datasets) {
    EXPECT_THAT(d.dataset_reference().project_id(), Eq("bigquery-public-data"));
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1
}  // namespace cloud
}  // namespace google
#endif