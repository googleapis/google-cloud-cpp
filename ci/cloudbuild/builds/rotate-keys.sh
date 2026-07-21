#!/bin/bash
#
# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
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
# integration tests, such as GCS, Bigtable, and Observability (see lib/integration.sh).
# The idea is to have p12 and json keys stored in the
# gs://cloud-cpp-testing-resources-secrets bucket that are named for each
# month. For example, there may be a key named `key-2021-04.p12` and `observability-key-2026-07.json`.
#
# We need to create the keys for the next month sometime before that month
# starts, and we need to delete the old keys sometime after we're sure they're
# not being used by any in-progress builds anymore.
#
# The strategy is to:
# 1. Make sure a keyfile exists for the *current* month.
# 2. Make sure a keyfile exists for the month that's, say, 2 weeks in the future.
# 3. Delete any keyfile from the month that's, say, 45 days ago.
# 4. Delete any key that's over 90 days old.
#
# We can schedule this script to run every night or weekly and it will create
# keys as necessary and delete old ones.

set -euo pipefail

source "$(dirname "$0")/../../lib/init.sh"
source module /ci/lib/io.sh

bucket="gs://cloud-cpp-testing-resources-secrets"

# Configuration format: "SA_EMAIL|KEY_PREFIX|FILE_TYPES"
accounts_config=(
  "storage-key-file-sa@cloud-cpp-testing-resources.iam.gserviceaccount.com|key|json p12"
  "observability-sa@cloud-cpp-testing-resources.iam.gserviceaccount.com|observability-key|json"
)

for entry in "${accounts_config[@]}"; do
  IFS="|" read -r sa_email prefix filetypes <<<"${entry}"

  io::log_h2 "Current service account keys for ${sa_email}"
  gcloud iam service-accounts keys list \
    --iam-account="${sa_email}" \
    --managed-by=user \
    --format="table(CREATED_AT, EXPIRES_AT)"

  io::log_h2 "Checking for the expected active keys (${prefix})"
  active_key_bases=(
    "${prefix}-$(date +"%Y-%m")"
    "${prefix}-$(date +"%Y-%m" --date="now + 2 weeks")"
  )
  for key_base in "${active_key_bases[@]}"; do
    for filetype in ${filetypes}; do
      bucket_path="${bucket}/${key_base}.${filetype}"
      io::log "Checking for active key at ${bucket_path}"
      if ! gcloud storage objects list --stat --fetch-encrypted-object-hashes "${bucket_path}"; then
        io::log "Not found. Creating ${bucket_path}"
        gcloud iam service-accounts keys create - \
          --iam-account="${sa_email}" \
          --key-file-type="${filetype}" |
          gcloud storage cp - "${bucket_path}"
      fi
    done
  done

  io::log_h2 "Checking for stale keyfiles (${prefix})"
  stale_key_base="${prefix}-$(date +"%Y-%m" --date="now - 45 days")"
  for filetype in ${filetypes}; do
    bucket_path="${bucket}/${stale_key_base}.${filetype}"
    io::log "Checking for stale key at ${bucket_path}"
    if gcloud storage objects list --stat --fetch-encrypted-object-hashes "${bucket_path}"; then
      io::log "Removing ${bucket_path}"
      gcloud storage rm "${bucket_path}"
    fi
  done

  io::log_h2 "Checking for keys over 90-days old for ${sa_email}"
  args=(
    "--iam-account=${sa_email}"
    "--managed-by=user"
    "--filter=CREATED_AT<-p90d"
    "--format=value(KEY_ID)"
  )
  old_keys=$(gcloud iam service-accounts keys list "${args[@]}")
  for old_key in ${old_keys}; do
    io::log "Deleting key: ${old_key}"
    gcloud iam service-accounts keys delete "${old_key}" --iam-account="${sa_email}" --quiet
  done
  echo
done
