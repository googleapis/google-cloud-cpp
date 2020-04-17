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

run_all_installed_quickstart_programs() {
  CONFIG_DIRECTORY="${KOKORO_GFILE_DIR:-/dev/shm}"
  readonly CONFIG_DIRECTORY
  if [[ ! -r "${CONFIG_DIRECTORY}/kokoro-run-key.json" ]]; then
    return 0
  fi
  source "${PROJECT_ROOT}/ci/colors.sh"
  source "${PROJECT_ROOT}/ci/etc/integration-tests-config.sh"
  source "${PROJECT_ROOT}/ci/etc/quickstart-config.sh"

  local run_args=(
    # Remove the container after running
    "--rm"

    # Set the environment variables for the test program.
    "--env" "GOOGLE_APPLICATION_CREDENTIALS=/c/kokoro-run-key.json"
    "--env" "GOOGLE_CLOUD_PROJECT=${GOOGLE_CLOUD_PROJECT}"

    # Mount the config directory as a volume in `/c`
    "--volume" "${CONFIG_DIRECTORY}:/c"
  )

  local errors=""
  for library in $(quickstart_libraries); do
    echo
    log_yellow "Running ${library}'s quickstart program for ${DISTRO}"
    local args=()
    while IFS="" read -r line; do
      args+=("${line}")
    done < <(quickstart_arguments "${library}")
    if ! docker run "${run_args[@]}" "${INSTALL_RUN_IMAGE}" \
      "/i/${library}/quickstart" "${args[@]}"; then
      log_red "Error running the quickstart program"
      errors="${errors} ${library}"
    fi
  done

  echo
  if [[ -z "${errors}" ]]; then
    log_green "All quickstart builds were successful"
    return 0
  fi

  log_red "Build failed for ${errors:1}"
  return 1
}

run_all_installed_quickstart_programs
