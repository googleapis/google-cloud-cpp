#!/bin/bash

if [[ -z "${PROJECT_ROOT+x}" ]]; then
  readonly PROJECT_ROOT="$(cd "$(dirname "$0")/../.."; pwd)"
fi
source "${PROJECT_ROOT}/ci/travis/linux-config.sh"
source "${PROJECT_ROOT}/ci/define-dump-log.sh"

echo "================================================================"
NCPU=$(nproc)
export NCPU
cd "${PROJECT_ROOT}"
echo "Building with ${NCPU} cores $(date) on ${PWD}."


echo "================================================================"
echo "Capture Docker version to troubleshoot $(date)."
sudo docker version
echo "================================================================"

echo "================================================================"
echo "Running the full build $(date)."
# When running on Travis the build gets a tty, and docker can produce nicer
# output in that case, but on Kokoro the script does not get a tty, and Docker
# terminates the program if we pass the `-it` flag in that case.
interactive_flag="-t"
if [[ -t 0 ]]; then
  interactive_flag="-it"
fi

readonly BUILD_HOME="cmake-out/home/${IMAGE}${suffix}"

mkdir -p "${BUILD_HOME}"

sudo docker run \
     --cap-add SYS_PTRACE \
     "${interactive_flag}" \
     --env DISTRO="${DISTRO}" \
     --env DISTRO_VERSION="${DISTRO_VERSION}" \
     --env CXX="${CXX}" \
     --env CC="${CC}" \
     --env NCPU="${NCPU:-2}" \
     --env CHECK_STYLE="${CHECK_STYLE:-}" \
     --env BAZEL_CONFIG="${BAZEL_CONFIG:-}" \
     --env RUN_INTEGRATION_TESTS="${RUN_INTEGRATION_TESTS:-}" \
     --env TERM="${TERM:-dumb}" \
     --env HOME="/h" \
     --env USER="${USER}" \
     --user "${UID:-0}" \
     --volume "${PWD}":/v \
     --volume "${PWD}/${BUILD_HOME}:/h" \
     --volume "${KOKORO_GFILE_DIR:-/dev/shm}":/c \
     --workdir /v \
     "${IMAGE}:tip" \
     "/v/ci/kokoro/build-in-docker-bazel.sh"

