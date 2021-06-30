#!/bin/bash
#
# Copyright 2021 Google LLC
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

# ATTENTION: This build is different than most because it's designed to be run
# as a "manual" trigger that's run on a schedule in GCB. For details, see
# https://cloud.google.com/build/docs/automating-builds/create-manual-triggers.
#
# NOTE: This build script will not have a trigger file in the
# `ci/cloudbuild/triggers` directory. "Manual" triggers exist only within the
# GCB UI at the time of this writing. Users with the appropriate access can run
# this build by hand with:
#   `ci/cloudbuild/build.sh --distro fedora rotate-keys --local`
#
# The purpose of this build is to rotate the service-account keys used by some
# integration tests, such as GCS and Bigtable (see lib/integration.sh). The
# idea is to have p12 and json keys stored in the
# gs://cloud-cpp-testing-resources-secrets bucket that are named for each
# month. For example, there may be a key named `key-2021-04.p12`. That key will
# be used for the month of April 2021 only.
#
# We need to create the keys for the next month sometime before that month
# starts, and we need to delete the old keys sometime after we're sure they're
# not being used by any in-progress builds anymore.
#
# The strategy is to:
# 1. Make sure a keyfile exists for the *current* month.
# 2. Make sure a keyfile exists for the month that's, say, 2 weeks in the future
# 3. Delete any keyfile from the month that's, say, 45 days ago
# 4. Delete any key that's over 90 days old.
#
# We can schedule this script to run every night or weekly and it will create
# keys as necessary and delete old ones.

set -eu

source "$(dirname "$0")/../../lib/init.sh"
source module /ci/lib/io.sh

io::log_h2 "Current service account keys"
# Note: For historical reasons this keyfile is named with "storage" in the
# name, though it is used for more than just GCS. One day we may rename this
# service account to something more generic.
account="storage-key-file-sa@cloud-cpp-testing-resources.iam.gserviceaccount.com"
gcloud iam service-accounts keys list \
  --iam-account="${account}" \
  --managed-by=user \
  --format="table(CREATED_AT, EXPIRES_AT)"

io::log_h2 "Checking for the expected active keys"
bucket="gs://cloud-cpp-testing-resources-secrets"
active_key_bases=(
  "key-$(date +"%Y-%m")"
  "key-$(date +"%Y-%m" --date="now + 2 weeks")"
)
for key_base in "${active_key_bases[@]}"; do
  for filetype in "json" "p12"; do
    bucket_path="${bucket}/${key_base}.${filetype}"
    io::log "Checking for active key at ${bucket_path}"
    if ! gsutil -q stat "${bucket_path}"; then
      io::log "Not found. Creating ${bucket_path}"
      gcloud iam service-accounts keys create - \
        --iam-account="${account}" \
        --key-file-type="${filetype}" |
        gsutil cp - "${bucket_path}"
    fi
  done
done

io::log_h2 "Checking for stale keyfiles"
stale_key_base="key-$(date +"%Y-%m" --date="now - 45 days")"
for filetype in "json" "p12"; do
  bucket_path="${bucket}/${stale_key_base}.${filetype}"
  io::log "Checking for stale key at ${bucket_path}"
  if gsutil -q stat "${bucket_path}"; then
    io::log "Removing ${bucket_path}"
    gsutil rm "${bucket_path}"
  fi
done

io::log_h2 "Checking for keys over 90-days old"
args=(
  "--iam-account=${account}"
  "--managed-by=user"
  "--filter=CREATED_AT<-p90d"
  "--format=value(KEY_ID)"
)
for old_key in $(gcloud iam service-accounts keys list "${args[@]}"); do
  io::log "Deleting key: ${old_key}"
  gcloud iam service-accounts keys delete "${old_key}" --iam-account="${account}"
done
echo
