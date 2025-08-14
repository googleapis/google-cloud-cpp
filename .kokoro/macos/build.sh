#!/bin/bash
# Fail on any error.
set -e
# Display commands being run.
set -x

# The code is checked out at ${KOKORO_ARTIFACTS_DIR}/github/google-cloud-cpp
# We need to navigate to this directory to run the build.
cd "${KOKORO_ARTIFACTS_DIR}/github/google-cloud-cpp"

# Run the existing build script. You can add specific parameters here
# to match the GHA jobs, for example, to specify a CMake or Bazel build.
ci/build.sh
