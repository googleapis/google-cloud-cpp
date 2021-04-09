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
# ## Run builds using Google Cloud Build ##
#
# This script runs the builds defined in `ci/cloudbuild/builds/` on the local
# machine (if `--local` is specified), in the local docker (if `--docker` is
# specified), or in the cloud using Google Cloud Build (the default). A single
# argument indicating the build name is required. The distro indicates the
# `<distro>.Dockerfile` to use for the build (ignored if using `--local`). The
# distro is looked up from the specified build's trigger file
# (`ci/cloudbuild/triggers/<build-name>.yaml`), but the distro can be
# overridden with the optional `--distro=<arg>` flag.
#
# Usage: build.sh [options] <build-name>
#
#   Options:
#     --distro=<name>      The distro name to use
#     -l|--local           Run the build in the local environment
#     -d|--docker          Run the build in a local docker
#     -s|--docker-shell    Run a shell in the build's docker container
#     -p|--project=<name>  The Cloud Project ID to use
#     -h|--help            Print this help message
#
#   Build names:

set -euo pipefail

source "$(dirname "$0")/../lib/init.sh"
source module ci/lib/io.sh
cd "${PROJECT_ROOT}"

function print_usage() {
  # Extracts the usage from the file comment starting at line 17.
  sed -n '17,/^$/s/^# \?//p' "${PROGRAM_PATH}"
  basename -s .sh ci/cloudbuild/builds/*.sh | sort | xargs printf "    %s\n"
}

# Use getopt to parse and normalize all the args.
PARSED="$(getopt -a \
  --options="p:ldsh" \
  --longoptions="distro:,project:,local,docker,docker-shell,help" \
  --name="${PROGRAM_NAME}" \
  -- "$@")"
eval set -- "${PARSED}"

DISTRO_FLAG=""
PROJECT_FLAG=""
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

if (($# != 1)); then
  echo "Must specify exactly one build name"
  print_usage
  exit 1
fi
readonly BUILD_NAME="$1"

# --local is the most fundamental build mode, in that all other builds
# eventually call this one. For example, a --docker build will build the
# specified docker image, then in a container from that image it will run the
# --local build. Similarly, the GCB build will submit the build to GCB, which
# will call the --local build.
if [[ "${LOCAL_FLAG}" = "true" ]]; then
  test -n "${DISTRO_FLAG}" && io::log_red "Local build ignoring --distro=${DISTRO_FLAG}"
  if [[ "${DOCKER_FLAG}" = "true" ]]; then
    echo "Only one of --local or --docker may be specified"
    print_usage
    exit 1
  fi
  function mem_total() {
    awk '$1 == "MemTotal:" {printf "%0.2f GiB", $2/1024/1024}' /proc/meminfo
  }
  function google_time() {
    # Extracts the time that Google thinks it is.
    curl -sI google.com | grep "^Date:" | cut -f2- -d:
  }
  io::log_h1 "Machine Info"
  printf "%10s %s\n" "host:" "$(date -u --rfc-3339=seconds)"
  printf "%10s %s\n" "google:" "$(date -ud "$(google_time)" --rfc-3339=seconds)"
  printf "%10s %s\n" "kernel:" "$(uname -v)"
  printf "%10s %s\n" "os:" "$(grep PRETTY_NAME /etc/os-release)"
  printf "%10s %s\n" "nproc:" "$(nproc)"
  printf "%10s %s\n" "mem:" "$(mem_total)"
  printf "%10s %s\n" "gcc:" "$(gcc --version 2>&1 | head -1)"
  printf "%10s %s\n" "clang:" "$(clang --version 2>&1 | head -1)"
  printf "%10s %s\n" "cc:" "$(cc --version 2>&1 | head -1)"
  printf "%10s %s\n" "cmake:" "$(cmake --version 2>&1 | head -1)"
  printf "%10s %s\n" "bazel:" "$(bazel --version 2>&1 | head -1)"
  io::log_h1 "Starting local build: ${BUILD_NAME}"
  readonly TIMEFORMAT="==> 🕑 ${BUILD_NAME} completed in %R seconds"
  time "${PROGRAM_DIR}/builds/${BUILD_NAME}.sh"
  exit
fi

# If --distro wasn't specified, look it up from the build's trigger file.
if [[ -z "${DISTRO_FLAG}" ]]; then
  trigger_file="ci/cloudbuild/triggers/${BUILD_NAME}-ci.yaml"
  DISTRO_FLAG="$(grep _DISTRO "${trigger_file}" | awk '{print $2}' || true)"
  if [[ -z "${DISTRO_FLAG}" ]]; then
    echo "Missing --distro=<arg>, and none found in ${trigger_file}"
    print_usage
    exit 1
  fi
fi
readonly DISTRO_FLAG

# Uses docker to locally build the specified image and run the build command.
# Docker builds store their outputs on the host system in `build-out/`.
if [[ "${DOCKER_FLAG}" = "true" ]]; then
  io::log_h1 "Starting docker build: ${BUILD_NAME}"
  out_dir="${PROJECT_ROOT}/build-out/${DISTRO_FLAG}-${BUILD_NAME}"
  out_home="${out_dir}/h"
  out_cmake="${out_dir}/cmake-out"
  mkdir -p "${out_home}" "${out_cmake}"
  image="gcb-${DISTRO_FLAG}:latest"
  io::log_h2 "Building docker image: ${image}"
  docker build -t "${image}" "--build-arg=NCPU=$(nproc)" \
    -f "ci/cloudbuild/${DISTRO_FLAG}.Dockerfile" ci
  io::log_h2 "Starting container for ${image} running ${BUILD_NAME}"
  run_flags=(
    "--interactive"
    "--tty"
    "--rm"
    "--user=$(id -u):$(id -g)"
    "--env=USER=$(id -un)"
    "--env=TZ=UTC0"
    # Mounts an empty volume over "build-out" to isolate builds from each
    # other. Doesn't affect GCB builds, but it helps our local docker builds.
    "--volume=/workspace/build-out"
    "--volume=${PROJECT_ROOT}:/workspace:Z"
    "--workdir=/workspace"
    "--volume=${out_cmake}:/workspace/cmake-out:Z"
    "--volume=${out_home}:/h:Z"
    "--env=HOME=/h"
  )
  cmd=(ci/cloudbuild/build.sh --local "${BUILD_NAME}")
  if [[ "${SHELL_FLAG}" = "true" ]]; then
    io::log "Starting shell, to manually run the requested build use:"
    echo "==> ${cmd[*]}"
    cmd=("bash")
  fi
  docker run "${run_flags[@]}" "${image}" "${cmd[@]}"
  exit
fi

# Surface invalid arguments early rather than waiting for GCB to fail.
if [ ! -r "${PROGRAM_DIR}/${DISTRO_FLAG}.Dockerfile" ]; then
  echo "Unknown distro: ${DISTRO_FLAG}"
  print_usage
  exit 1
elif [ ! -x "${PROGRAM_DIR}/builds/${BUILD_NAME}.sh" ]; then
  echo "Unknown build name: ${BUILD_NAME}"
  print_usage
  exit 1
fi

# Uses Google Cloud build to run the specified build.
io::log_h1 "Starting cloud build: ${BUILD_NAME}"
account="$(gcloud config list account --format "value(core.account)")"
subs="_DISTRO=${DISTRO_FLAG}"
subs+=",_BUILD_NAME=${BUILD_NAME}"
subs+=",_CACHE_TYPE=manual-${account}"
io::log "Substitutions ${subs}"
args=(
  "--config=ci/cloudbuild/cloudbuild.yaml"
  "--substitutions=${subs}"
)
if [[ -n "${PROJECT_FLAG}" ]]; then
  args+=("--project=${PROJECT_FLAG}")
fi
gcloud builds submit "${args[@]}" .
