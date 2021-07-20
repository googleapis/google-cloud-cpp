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
#
# Usage: build.sh [options] [build-name]
#
#   Options:
#     --distro=<name>      The distro name to use
#     --docker-clean       Delete the build output directories
#     -t|--trigger         The trigger file to extract the build name and distro
#     -l|--local           Run the build in the local environment
#     -d|--docker          Run the build in a local docker
#     -s|--docker-shell    Run a shell in the build's docker container
#     -p|--project=<name>  The Cloud Project ID to use
#     -h|--help            Print this help message
#
# This script runs named builds, where builds are defined as a build script
# (which lives in `ci/cloudbuild/builds/`) and a distro (which is defined in
# `ci/cloudbuild/dockerfiles/<distro>.Dockerfile`). Trigger files (which live
# in `ci/cloudbuild/triggers/`) associate build scripts with the distro they're
# intended to run on. For example, the "asan-pr" build is defined in the
# `ci/cloudbuild/triggers/asan-pr.yaml` file, which specifies that the
# `ci/cloudbuild/builds/asan.sh` script should be run on the
# `ci/cloudbuild/dockerfiles/fedora.Dockerfile` image. There are a couple ways
# to specify builds:
#
# 1. Explicitly name the distro and build script. For example:
#    $ build.sh --distro fedora asan
#
# 2. Name the trigger file, which contains the distro and build script:
#    $ build.sh --trigger asan-pr
#
# Note: command-line flags may be specified with or without an equals sign
# (e.g. `-t=foo` is the same as `-t foo`), and in any order.
#
# Usage examples:
#
#   Runs the asan build from the asan-pr.yaml file on GCB
#   $ build.sh -t asan-pr
#
#   Runs the asan build from the asan-pr.yaml file in docker
#   $ build.sh -t asan-pr --docker
#
#   Opens a shell in the docker container for the asan-pr.yaml build
#   NOTE: The `-s` flag is useful for debugging builds.
#   $ build.sh -t asan-pr --docker-shell
#   $ build.sh -t asan-pr --docker -s  # equivalent
#   $ build.sh -t asan-pr -s  # equivalent
#
#   Runs the asan build from the asan-pr.yaml file in the local environment
#   $ build.sh -t asan-pr --local
#
#   Runs builds/cmake-install.sh script in the demo-centos-7.Dockerfile
#   $ build.sh cmake-install --distro demo-centos-7 --docker
#
#   Runs the asan/integration tests in the cloud-cpp-testing-resources project
#   $ build.sh -t asan-pr --project cloud-cpp-testing-resources
#
#   Runs the checkers in your local docker
#   NOTE: This is a good way to format your code and check for style issues.
#   $ build.sh -t checkers-pr --docker
#
# Note: Builds with the `--docker` flag inherit some (but not all) environment
# variables from the calling process, such as USE_BAZEL_VERSION
# (https://github.com/bazelbuild/bazelisk), CODECOV_TOKEN
# (https://codecov.io/), and every variable starting with GOOGLE_CLOUD_.

set -euo pipefail

source "$(dirname "$0")/../lib/init.sh"
source module ci/lib/io.sh
cd "${PROJECT_ROOT}"

function print_usage() {
  # Extracts the usage from the file comment starting at line 17.
  sed -n '17,/^$/s/^# \?//p' "${PROGRAM_PATH}"
}

# Use getopt to parse and normalize all the args.
PARSED="$(getopt -a \
  --options="p:t:ldsh" \
  --longoptions="distro:,project:,trigger:,local,docker,docker-shell,docker-clean,help" \
  --name="${PROGRAM_NAME}" \
  -- "$@")"
eval set -- "${PARSED}"

DISTRO_FLAG=""
PROJECT_FLAG=""
TRIGGER_FLAG=""
CLEAN_FLAG="false"
LOCAL_FLAG="false"
DOCKER_FLAG="false"
SHELL_FLAG="false"
while true; do
  case "$1" in
  --distro)
    DISTRO_FLAG="$2"
    shift 2
    ;;
  -p | --project)
    PROJECT_FLAG="$2"
    shift 2
    ;;
  -t | --trigger)
    TRIGGER_FLAG="$2"
    shift 2
    ;;
  -l | --local)
    LOCAL_FLAG="true"
    shift
    ;;
  -d | --docker)
    DOCKER_FLAG="true"
    shift
    ;;
  -s | --docker-shell)
    DOCKER_FLAG="true"
    SHELL_FLAG="true"
    shift
    ;;
  --docker-clean)
    DOCKER_FLAG="true"
    CLEAN_FLAG="true"
    shift
    ;;
  -h | --help)
    print_usage
    exit 0
    ;;
  --)
    shift
    break
    ;;
  esac
done
readonly PROJECT_FLAG

# If `--trigger=name` was specified, use the _BUILD_NAME and _DISTRO in the
# trigger file as defaults.
BUILD_NAME="${1:-}"
if [[ -n "${TRIGGER_FLAG}" ]]; then
  trigger_file="${PROGRAM_DIR}/triggers/${TRIGGER_FLAG}.yaml"
  if [[ ! -r "${trigger_file}" ]]; then
    io::log_red "Cannot open ${trigger_file}"
    exit 1
  fi
  : "${BUILD_NAME:="$(grep _BUILD_NAME "${trigger_file}" | awk '{print $2}')"}"
  : "${DISTRO_FLAG:="$(grep _DISTRO "${trigger_file}" | awk '{print $2}')"}"
fi
readonly BUILD_NAME
readonly DISTRO_FLAG

if [[ -z "${BUILD_NAME}" ]]; then
  io::log_red "No build name specified. Specify a build name or use --trigger"
  print_usage
  exit 1
fi

# Sets some env vars that usually come from GCB, but need to be set explicitly
# when doing --local or --docker builds. See also `cloudbuild.yaml` and
# https://cloud.google.com/build/docs/configuring-builds/substitute-variable-values
: "${TRIGGER_TYPE:=manual}"
: "${BRANCH_NAME:=$(git branch --show-current)}"
: "${COMMIT_SHA:=$(git rev-parse HEAD)}"
CODECOV_TOKEN="$(tr -d '[:space:]' <<<"${CODECOV_TOKEN:-}")"
LOG_LINKER_PAT="$(tr -d '[:space:]' <<<"${LOG_LINKER_PAT:-}")"

export TRIGGER_TYPE
export BRANCH_NAME
export COMMIT_SHA
export CODECOV_TOKEN
export LOG_LINKER_PAT

# --local is the most fundamental build mode, in that all other builds
# eventually call this one. For example, a --docker build will build the
# specified docker image, then in a container from that image it will run the
# --local build. Similarly, the GCB build will submit the build to GCB, which
# will call the --local build.
if [[ "${LOCAL_FLAG}" = "true" ]]; then
  test -n "${DISTRO_FLAG}" && io::log_red "Local build ignoring --distro=${DISTRO_FLAG}"
  if [[ "${DOCKER_FLAG}" = "true" ]]; then
    io::log_red "Only one of --local or --docker may be specified"
    print_usage
    exit 1
  fi

  # Prints links to the log files for the current build. These can be useful to
  # copy-n-paste into issues. The GCB link will require auth but the raw link
  # may be publicly accessible.
  io::log_h1 "Log Links"
  printf "GCB: %s\n" "${CONSOLE_LOG_URL:-none}"
  printf "Raw: %s\n" "${RAW_LOG_URL:-none}"

  function mem_total() {
    awk '$1 == "MemTotal:" {printf "%0.2f GiB", $2/1024/1024}' /proc/meminfo
  }
  function google_time() {
    # Extracts the time that Google thinks it is.
    curl -sI google.com | sed -n 's/Date: \(.*\)\r/\1/p'
  }
  io::log_h1 "Machine Info"
  printf "%10s %s\n" "host:" "$(date -u --rfc-3339=seconds)"
  printf "%10s %s\n" "google:" "$(date -ud "$(google_time)" --rfc-3339=seconds)"
  printf "%10s %s\n" "kernel:" "$(uname -v)"
  printf "%10s %s\n" "os:" "$(grep PRETTY_NAME /etc/os-release)"
  printf "%10s %s\n" "nproc:" "$(nproc)"
  printf "%10s %s\n" "mem:" "$(mem_total)"
  printf "%10s %s\n" "term:" "${TERM-}"
  printf "%10s %s\n" "gcc:" "$(gcc --version 2>&1 | head -1)"
  printf "%10s %s\n" "clang:" "$(clang --version 2>&1 | head -1)"
  printf "%10s %s\n" "cc:" "$(cc --version 2>&1 | head -1)"
  io::log_h1 "Starting local build: ${BUILD_NAME}"
  readonly TIMEFORMAT="==> 🕑 ${BUILD_NAME} completed in %R seconds"
  time "${PROGRAM_DIR}/builds/${BUILD_NAME}.sh"
  exit
fi

if [[ -z "${DISTRO_FLAG}" ]]; then
  io::log_red "No distro specified. Use --distro or --trigger"
  print_usage
  exit 1
fi

# Uses docker to locally build the specified image and run the build command.
# Docker builds store their outputs on the host system in `build-out/`.
if [[ "${DOCKER_FLAG}" = "true" ]]; then
  io::log_h1 "Starting docker build: ${BUILD_NAME}"
  out_dir="${PROJECT_ROOT}/build-out/${DISTRO_FLAG}-${BUILD_NAME}"
  out_home="${out_dir}/h"
  out_cmake="${out_dir}/cmake-out"
  if [[ "${CLEAN_FLAG}" = "true" ]]; then
    io::log_yellow "Removing build output directory:"
    du -sh "${out_dir}"
    rm -rf "${out_dir}"
  fi
  # Creates the directories that docker will mount as a volumes, otherwise they
  # will be created by the docker daemon as root-owned directories.
  mkdir -p "${out_cmake}" "${out_home}/.config/gcloud"
  image="gcb-${DISTRO_FLAG}:latest"
  io::log_h2 "Building docker image: ${image}"
  io::run \
    docker build -t "${image}" "--build-arg=NCPU=$(nproc)" \
    -f "ci/cloudbuild/dockerfiles/${DISTRO_FLAG}.Dockerfile" ci
  io::log_h2 "Starting docker container: ${image}"
  run_flags=(
    "--interactive"
    "--tty=$([[ -t 0 ]] && echo true || echo false)"
    "--rm"
    "--network=bridge"
    "--user=$(id -u):$(id -g)"
    "--env=PS1=docker:${DISTRO_FLAG}\$ "
    "--env=USER=$(id -un)"
    "--env=TERM=$([[ -t 0 ]] && echo "${TERM:-dumb}" || echo dumb)"
    "--env=TZ=UTC0"
    "--env=CI_LIB_IO_FIRST_TIMESTAMP=${CI_LIB_IO_FIRST_TIMESTAMP:-}"
    "--env=CODECOV_TOKEN=${CODECOV_TOKEN:-}"
    "--env=BRANCH_NAME=${BRANCH_NAME}"
    "--env=COMMIT_SHA=${COMMIT_SHA}"
    "--env=TRIGGER_TYPE=${TRIGGER_TYPE:-}"
    "--env=USE_BAZEL_VERSION=${USE_BAZEL_VERSION:-}"
    # Mounts an empty volume over "build-out" to isolate builds from each
    # other. Doesn't affect GCB builds, but it helps our local docker builds.
    "--volume=/workspace/build-out"
    "--volume=${PROJECT_ROOT}:/workspace:Z"
    "--workdir=/workspace"
    "--volume=${out_cmake}:/workspace/cmake-out:Z"
    "--volume=${out_home}:/h:Z"
    "--env=HOME=/h"
    # Makes the host's gcloud credentials visible inside the docker container,
    # which we need for integration tests.
    "--volume=${HOME}/.config/gcloud:/h/.config/gcloud:Z"
  )
  # All GOOGLE_CLOUD_* env vars will be passed to the docker container.
  for e in $(env); do
    if [[ "${e}" = GOOGLE_CLOUD_* ]]; then
      io::log_yellow "Exporting to docker environment: ${e}"
      run_flags+=("--env=${e}")
    fi
  done
  cmd=(ci/cloudbuild/build.sh --local "${BUILD_NAME}")
  if [[ "${SHELL_FLAG}" = "true" ]]; then
    printf "To run the build manually:\n  "
    printf " %q" "${cmd[@]}"
    printf "\n\n"
    cmd=("bash" "--norc") # some distros have rc files that override our PS1
  fi
  io::run docker run "${run_flags[@]}" "${image}" "${cmd[@]}"
  exit
fi

# Surface invalid arguments early rather than waiting for GCB to fail.
if [ ! -r "${PROGRAM_DIR}/dockerfiles/${DISTRO_FLAG}.Dockerfile" ]; then
  io::log_red "Unknown distro: ${DISTRO_FLAG}"
  print_usage
  exit 1
elif [ ! -x "${PROGRAM_DIR}/builds/${BUILD_NAME}.sh" ]; then
  io::log_red "Unknown build name: ${BUILD_NAME}"
  print_usage
  exit 1
fi

# Uses Google Cloud build to run the specified build.
io::log_h1 "Starting cloud build: ${BUILD_NAME}"
project="${PROJECT_FLAG:-$(gcloud config get-value project 2>/dev/null)}"
account="$(gcloud config get-value account 2>/dev/null)"
# The cloudbuild.yaml file expects certain "secrets" to be present in the
# project's "Secret Manager". This is true for our main production project, but
# for personal projects we may need to create them (with empty strings).
if [[ "${project}" != "cloud-cpp-testing-resources" ]]; then
  for secret in "CODECOV_TOKEN" "LOG_LINKER_PAT"; do
    if ! gcloud --project "${project}" secrets describe "${secret}" >/dev/null; then
      io::log_yellow "Adding missing secret ${secret} to ${project}"
      echo | gcloud --project "${project}" secrets create "${secret}" --data-file=-
    fi
  done
fi
subs=("_DISTRO=${DISTRO_FLAG}")
subs+=("_BUILD_NAME=${BUILD_NAME}")
subs+=("_TRIGGER_SOURCE=manual-${account}")
subs+=("_PR_NUMBER=") # Must be empty or a number, and this is not a PR
subs+=("_LOGS_BUCKET=${project}_cloudbuild")
subs+=("BRANCH_NAME=${BRANCH_NAME}")
subs+=("COMMIT_SHA=${COMMIT_SHA}")
printf "Substitutions:\n"
printf "  %s\n" "${subs[@]}"
args=(
  "--config=ci/cloudbuild/cloudbuild.yaml"
  "--substitutions=$(printf "%s," "${subs[@]}")"
  "--project=${project}"
  # This value must match the workerPool configured in cloudbuild.yaml
  "--region=us-east1"
)
io::run gcloud builds submit "${args[@]}" .
