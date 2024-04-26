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
#     --verbose            Print additional information during the build
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
#    ci/cloudbuild/dockerfiles/fedora-latest-bazel.Dockerfile distro.
#    $ build.sh --build asan --distro fedora-latest-bazel
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
  --longoptions="arch:,build:,distro:,trigger:,cloud:,pool-region:,pool-id:,cache-bucket:,logs-bucket:,local,docker,docker-shell,docker-clean,verbose,help" \
  --name="${PROGRAM_NAME}" \
  -- "$@")"
eval set -- "${PARSED}"

ARCH_FLAG=""
BUILD_FLAG=""
DISTRO_FLAG=""
TRIGGER_FLAG=""
CLOUD_FLAG=""
POOL_REGION_FLAG="us-east1"
POOL_ID_FLAG="google-cloud-cpp-pool"
CACHE_BUCKET_FLAG=""
LOGS_BUCKET_FLAG=""
CLEAN_FLAG="false"
LOCAL_FLAG="false"
DOCKER_FLAG="false"
SHELL_FLAG="false"
: "${VERBOSE_FLAG:=false}"
while true; do
  case "$1" in
    --arch)
      ARCH_FLAG="$2"
      shift 2
      ;;
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
    --pool-region)
      POOL_REGION_FLAG="$2"
      shift 2
      ;;
    --pool-id)
      POOL_ID_FLAG="$2"
      shift 2
      ;;
    --cache-bucket)
      CACHE_BUCKET_FLAG="$2"
      shift 2
      ;;
    --logs-bucket)
      LOGS_BUCKET_FLAG="$2"
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
    --verbose)
      VERBOSE_FLAG="true"
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
  : "${LIBRARIES:="$(grep _LIBRARIES "${trigger_file}" | awk '{print $2}')"}"
  : "${SHARD:="$(grep ' _SHARD:' "${trigger_file}" | awk '{print $2}')"}"
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
: "${LIBRARIES:=all}"
: "${SHARD:=__default__}"
CODECOV_TOKEN="$(tr -d '[:space:]' <<<"${CODECOV_TOKEN:-}")"

export CODECOV_TOKEN
export BRANCH_NAME
export COMMIT_SHA
export TRIGGER_TYPE
export VERBOSE_FLAG
export LIBRARIES

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

  function mem_total() {
    awk '$1 == "MemTotal:" {printf "%0.2f GiB", $2/1024/1024}' /proc/meminfo
  }
  function google_time() {
    # Extracts the time that Google thinks it is.
    curl -sI google.com | sed -n 's/Date: \(.*\)\r/\1/p'
  }
  if [[ "${TRIGGER_TYPE}" != "manual" ]]; then
    # Prints links to the log files for the current build. These can be useful
    # to copy-n-paste into issues. The GCB link will require auth but the raw
    # link may be publicly accessible. In manual builds this information is
    # never useful (both are "none").
    io::log_h1 "Log Links"
    printf "* GCB: %s\n" "${CONSOLE_LOG_URL:-none}"
    printf "* Raw: %s\n" "${RAW_LOG_URL:-none}"
  fi

  if [[ "${TRIGGER_TYPE}" != "manual" || "${VERBOSE_FLAG}" == "true" ]]; then
    # Prints information about the machine, compiler, and gcloud.
    # In manual builds this information is almost never useful.
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
    if type gcloud >/dev/null 2>&1; then
      io::log_h1 "gcloud Versions"
      gcloud --version
    fi
  fi

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
    if ! gcloud --project "${CLOUD_FLAG}" secrets describe "CODECOV_TOKEN" >/dev/null; then
      io::log_yellow "Adding missing secret CODECOV_TOKEN to ${CLOUD_FLAG}"
      echo | gcloud --project "${CLOUD_FLAG}" secrets create "CODECOV_TOKEN" --data-file=-
    fi
  fi
  account="$(gcloud config get-value account 2>/dev/null)"
  subs=("_DISTRO=${DISTRO_FLAG}")
  subs+=("_BUILD_NAME=${BUILD_FLAG}")
  subs+=("_TRIGGER_SOURCE=manual-${account}")
  subs+=("_PR_NUMBER=") # Must be empty or a number, and this is not a PR
  subs+=("_POOL_REGION=${POOL_REGION_FLAG}")
  subs+=("_POOL_ID=${POOL_ID_FLAG}")
  if [[ -n "${CACHE_BUCKET_FLAG}" ]]; then
    subs+=("_CACHE_BUCKET=${CACHE_BUCKET_FLAG}")
  fi
  if [[ -n "${LOGS_BUCKET_FLAG}" ]]; then
    subs+=("_LOGS_BUCKET=${LOGS_BUCKET_FLAG}")
  fi
  subs+=("BRANCH_NAME=${BRANCH_NAME}")
  subs+=("COMMIT_SHA=${COMMIT_SHA}")
  subs+=("_LIBRARIES=${LIBRARIES}")
  subs+=("_SHARD=${SHARD}")
  printf "Substitutions:\n"
  printf "  %s\n" "${subs[@]}"
  args=(
    "--config=ci/cloudbuild/cloudbuild.yaml"
    "--substitutions=$(printf "%s," "${subs[@]}")"
    "--project=${CLOUD_FLAG}"
    # This value must match the workerPool configured in cloudbuild.yaml
    "--region=${POOL_REGION_FLAG}"
  )
  io::run gcloud builds submit "${args[@]}" .
  exit
fi

# Default to --docker mode since no other mode was specified.
DOCKER_FLAG="true"

# Uses docker to locally build the specified image and run the build command.
if [[ "${DOCKER_FLAG}" = "true" ]]; then
  io::log_h1 "Starting docker build: ${BUILD_FLAG}"
  out_dir="${PROJECT_ROOT}/build-out/${DISTRO_FLAG}/${BUILD_FLAG}"
  if [[ -n "${SHARD:-}" ]]; then
    out_dir="${out_dir}/${SHARD}"
  fi
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
  build_flags=(
    -t "${image}"
    "--build-arg=NCPU=$(nproc)"
    -f "ci/cloudbuild/dockerfiles/${DISTRO_FLAG}.Dockerfile"
  )
  if [[ -n "${ARCH_FLAG}" ]]; then
    build_flags+=("--build-arg=ARCH=${ARCH_FLAG}")
  fi
  export DOCKER_BUILDKIT=1
  io::run docker build "${build_flags[@]}" ci
  io::log_h2 "Starting docker container: ${image}"
  run_flags=(
    "--interactive"
    "--tty=$([[ -t 0 ]] && echo true || echo false)"
    "--rm"
    "--network=${DOCKER_NETWORK:-bridge}"
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
    "--env=VERBOSE_FLAG=${VERBOSE_FLAG:-}"
    "--env=USE_BAZEL_VERSION=${USE_BAZEL_VERSION:-}"
    "--env=LIBRARIES=${LIBRARIES:-}"
    "--env=SHARD=${SHARD:-}"
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
    # Makes the generate-libraries build ONLY touch golden files in the
    # generator dir.
    "--env=GENERATE_GOLDEN_ONLY=${GENERATE_GOLDEN_ONLY-}"
    # Makes the generate-libraries build expect new files to be created
    # in protos/google/cloud/${UPDATED_DISCOVERY_DOCUMENT} and
    # in google/cloud/${UPDATED_DISCOVERY_DOCUMENT} and git add such files.
    "--env=UPDATED_DISCOVERY_DOCUMENT=${UPDATED_DISCOVERY_DOCUMENT-}"
  )
  if [[ -r "${HOME}/.cloudcxxrc" ]]; then
    run_flags+=(
      "--volume=${HOME}/.cloudcxxrc:/h/.cloudcxxrc:Z"
    )
  fi
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
