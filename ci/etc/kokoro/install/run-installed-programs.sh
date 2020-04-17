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

CONFIG_DIRECTORY="${KOKORO_GFILE_DIR:-/dev/shm}"
readonly CONFIG_DIRECTORY
if [[ -r "${CONFIG_DIRECTORY}/kokoro-run-key.json" && -r \
  "${PROJECT_ROOT}/ci/etc/integration-tests-config.sh" ]]; then
  source "${PROJECT_ROOT}/ci/etc/integration-tests-config.sh"

  run_args=(
    # Remove the container after running
    "--rm"

    # Set the environment variables for the test program.
    "--env" "GOOGLE_APPLICATION_CREDENTIALS=/c/kokoro-run-key.json"
    "--env" "GOOGLE_CLOUD_PROJECT=${GOOGLE_CLOUD_PROJECT}"

    # Mount the config directory as a volume in `/c`
    "--volume" "${CONFIG_DIRECTORY}:/c"
  )
  readonly NONCE="$(date +%s)-${RANDOM}"

  echo "================================================================"
  echo "$(date -u): Run Bigtable test programs against installed libraries" \
    "${DISTRO}."
  docker run "${run_args[@]}" "${INSTALL_RUN_IMAGE}" \
    "/i/bigtable/quickstart" \
    "${GOOGLE_CLOUD_PROJECT}" \
    "${GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID}" "quickstart"

  echo "================================================================"
  echo "$(date -u): Run Storage test programs against installed libraries" \
    "${DISTRO}."
  docker run "${run_args[@]}" "${INSTALL_RUN_IMAGE}" \
    "/i/storage/quickstart" \
    "${GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME}"
fi
