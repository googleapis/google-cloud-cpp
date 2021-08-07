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
# Usage: build.sh [options]
#
#   Options:
#     --build=<name>       The basename minus suffix of the build script to run
#     --distro=<name>      The distro name to use
#     --docker-clean       Delete the build output directories
#     -t|--trigger         The trigger file to extract the build name and distro
#     -c|--cloud=<project> Run the build in GCB in the given project
#     -l|--local           Run the build in the local environment
#     -d|--docker          Run the build in a local docker (default)
#     -s|--docker-shell    Run a shell in the build's docker container
#     -h|--help            Print this help message
#
#   Note: flags may be specified in any order and with or without an equals
#   sign (e.g. `-t=foo` is the same as `-t foo`)
#
# This tool runs build scripts (named using the --build flag), which live in
# ci/cloudbuild/builds, in certain environments (named using the --distro
# flag), which are defined in ci/cloudbuild/dockerfiles. Each build can be run
# in one of three modes: on the local machine (--local), in the cloud
# (--cloud), or in docker on the local machine (--docker), which is the default
# if no other mode is specified.
#
# A build name and a distro are required. These can be specified with the
# --build and --distro flags, respectively. As a short-hand, the --trigger flag
# can specify the basename (minus suffix) of a file in ci/cloudbuild/triggers,
# and the _BUILD_NAME and _DISTRO will be looked up from that file.
#
# Examples:
#
# 1. Runs the asan build in docker on the local machine
#    $ build.sh -t asan-pr
#    $ build.sh -t asan-pr -d  # Equivalent form
#
# 2. Starts a shell in the asan build's docker container on the local machine
#    $ build.sh -t asan-pr -s
#    $ build.sh -t asan-pr -d -s  # Equivalent form
#
# 3. Runs the asan build in the named Google Cloud Build project
#    $ build.sh -t asan-pr --cloud=cloud-cpp-testing-resources
#
# 4. Runs a local docker build with a completely clean environment with all
#    cached artifacts from previous builds removed
#    $ build.sh -t asan-pr --docker-clean
#
# Advanced Examples:
#
# 1. Runs the ci/cloudbuild/builds/asan.sh script using the
#    ci/cloudbuild/dockerfiles/fedora-34.Dockerfile distro.
#    $ build.sh --build asan --distro fedora-34
#
# Note: Builds with the `--docker` flag inherit some (but not all) environment
# variables from the calling process, such as USE_BAZEL_VERSION
# (https://github.com/bazelbuild/bazelisk), CODECOV_TOKEN
# (https://codecov.io/), and every variable starting with GOOGLE_CLOUD_.
#

set -euo pipefail

source "$(dirname "$0")/../lib/init.sh"
source module ci/lib/io.sh
cd "${PROJECT_ROOT}"

function print_usage() {
  # Extracts the usage from the file comment starting at line 17.
  sed -n '17,/^$/s/^# \?//p' "${PROGRAM_PATH}"
}

function die() {
  io::log_red "$@"
  print_usage
  exit 1
}

# Use getopt to parse and normalize all the args.
PARSED="$(getopt -a \
  --options="t:c:ldsh" \
  --longoptions="build:,distro:,trigger:,cloud:,local,docker,docker-shell,docker-clean,help" \
  --name="${PROGRAM_NAME}" \
  -- "$@")"
eval set -- "${PARSED}"

BUILD_FLAG=""
DISTRO_FLAG=""
TRIGGER_FLAG=""
CLOUD_FLAG=""
CLEAN_FLAG="false"
LOCAL_FLAG="false"
DOCKER_FLAG="false"
SHELL_FLAG="false"
while true; do
  case "$1" in
  --build)
    BUILD_FLAG="$2"
    shift 2
    ;;
  --distro)
    DISTRO_FLAG="$2"
    shift 2
    ;;
  -t | --trigger)
    TRIGGER_FLAG="$2"
    shift 2
    ;;
  -c | --cloud)
    CLOUD_FLAG="$2"
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

# If `--trigger=name` was specified, use the _BUILD_NAME and _DISTRO in the
# trigger file as defaults.
if [[ -n "${TRIGGER_FLAG}" ]]; then
  trigger_file="${PROGRAM_DIR}/triggers/${TRIGGER_FLAG}.yaml"
  test -r "${trigger_file}" || die "Cannot open ${trigger_file}"
  : "${BUILD_FLAG:="$(grep _BUILD_NAME "${trigger_file}" | awk '{print $2}')"}"
  : "${DISTRO_FLAG:="$(grep _DISTRO "${trigger_file}" | awk '{print $2}')"}"
fi

if [[ -z "${BUILD_FLAG}" ]]; then
  die "No build name. Specify --build or --trigger"
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
  if [[ "${DOCKER_FLAG}" = "true" || -n "${CLOUD_FLAG}" ]]; then
    die "Only one of --local, --docker, or --cloud may be specified"
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
  io::log_h1 "Starting local build: ${BUILD_FLAG}"
  readonly TIMEFORMAT="==> 🕑 ${BUILD_FLAG} completed in %R seconds"
  time "${PROGRAM_DIR}/builds/${BUILD_FLAG}.sh"
  exit
fi

if [[ -z "${DISTRO_FLAG}" ]]; then
  die "No distro specified. Use --distro or --trigger"
fi

if [[ -n "${CLOUD_FLAG}" ]]; then
  test "${DOCKER_FLAG}" = "true" && die "Cannot specify --docker and --cloud"
  # Surface invalid arguments early rather than waiting for GCB to fail.
  if [ ! -r "${PROGRAM_DIR}/dockerfiles/${DISTRO_FLAG}.Dockerfile" ]; then
    die "Unknown distro: ${DISTRO_FLAG}"
  elif [ ! -x "${PROGRAM_DIR}/builds/${BUILD_FLAG}.sh" ]; then
    die "Unknown build name: ${BUILD_FLAG}"
  fi

  # Uses Google Cloud build to run the specified build.
  io::log_h1 "Starting cloud build: ${BUILD_FLAG}"
  # The cloudbuild.yaml file expects certain "secrets" to be present in the
  # project's "Secret Manager". This is true for our main production project, but
  # for personal projects we may need to create them (with empty strings).
  if [[ "${CLOUD_FLAG}" != "cloud-cpp-testing-resources" ]]; then
    for secret in "CODECOV_TOKEN" "LOG_LINKER_PAT"; do
      if ! gcloud --project "${CLOUD_FLAG}" secrets describe "${secret}" >/dev/null; then
        io::log_yellow "Adding missing secret ${secret} to ${CLOUD_FLAG}"
        echo | gcloud --project "${CLOUD_FLAG}" secrets create "${secret}" --data-file=-
      fi
    done
  fi
  account="$(gcloud config get-value account 2>/dev/null)"
  subs=("_DISTRO=${DISTRO_FLAG}")
  subs+=("_BUILD_NAME=${BUILD_FLAG}")
  subs+=("_TRIGGER_SOURCE=manual-${account}")
  subs+=("_PR_NUMBER=") # Must be empty or a number, and this is not a PR
  subs+=("_LOGS_BUCKET=${CLOUD_FLAG}_cloudbuild")
  subs+=("BRANCH_NAME=${BRANCH_NAME}")
  subs+=("COMMIT_SHA=${COMMIT_SHA}")
  printf "Substitutions:\n"
  printf "  %s\n" "${subs[@]}"
  args=(
    "--config=ci/cloudbuild/cloudbuild.yaml"
    "--substitutions=$(printf "%s," "${subs[@]}")"
    "--project=${CLOUD_FLAG}"
    # This value must match the workerPool configured in cloudbuild.yaml
    "--region=us-east1"
  )
  io::run gcloud builds submit "${args[@]}" .
fi

# Default to --docker mode since no other mode was specified.
DOCKER_FLAG="true"

# Uses docker to locally build the specified image and run the build command.
if [[ "${DOCKER_FLAG}" = "true" ]]; then
  io::log_h1 "Starting docker build: ${BUILD_FLAG}"
  out_dir="${PROJECT_ROOT}/build-out/${DISTRO_FLAG}-${BUILD_FLAG}"
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
  cmd=(ci/cloudbuild/build.sh --local --build "${BUILD_FLAG}")
  if [[ "${SHELL_FLAG}" = "true" ]]; then
    printf "To run the build manually:\n  "
    printf " %q" "${cmd[@]}"
    printf "\n\n"
    cmd=("bash" "--norc") # some distros have rc files that override our PS1
  fi
  io::run docker run "${run_flags[@]}" "${image}" "${cmd[@]}"
fi
