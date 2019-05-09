#!/usr/bin/env bash
# Copyright 2019 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -eu

# Run all the admin integration tests against production. Only nightly builds
# should run this script.
#
# The following environment variables must be defined before calling this
# script:
#
# - PROJECT_ID: the id (typically the string form) of a valid GCP project.
# - ZONE_A: the name of a valid GCP zone supporting Cloud Bigtable.
# - ZONE_B: the name of a valid GCP zone supporting Cloud Bigtable, should be
#   different from ZONE_A and should be in the same region.
# - INSTANCE_ID: the ID of an existing Cloud Bigtable instance in ZONE_A.
# - SERVICE_ACCOUNT: a valid service account to test IAM operations.
#

echo
echo "Running bigtable::InstanceAdmin integration test."
./instance_admin_integration_test \
    "${PROJECT_ID}" "${ZONE_A}" "${ZONE_B}" "${SERVICE_ACCOUNT}"

echo
echo "Running bigtable::InstanceAdmin async with futures integration test."
./instance_admin_async_future_integration_test \
    "${PROJECT_ID}" "${ZONE_A}" "${ZONE_B}" "${SERVICE_ACCOUNT}"

echo
echo "Running bigtable::TableAdmin integration test."
./admin_integration_test \
    "${PROJECT_ID}" "${INSTANCE_ID}" "${ZONE_A}" "${ZONE_B}"

echo
echo "Running TableAdmin async with futures integration test."
./admin_async_future_integration_test \
    "${PROJECT_ID}" "${INSTANCE_ID}"  "${ZONE_A}" "${ZONE_B}"
