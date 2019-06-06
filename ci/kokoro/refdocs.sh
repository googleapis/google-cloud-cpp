#!/usr/bin/env bash

set -euo pipefail

# For docuploader to work
export LC_ALL=C.UTF-8
export LANG=C.UTF-8

if [[ -z "${CREDENTIALS:-}" ]]; then
  CREDENTIALS=${KOKORO_KEYSTORE_DIR}/73713_docuploader_service_account
fi

STAGING_BUCKET="docs-staging"

BUILD_DIR="cmake-out"

# work from the git root directory
cd "$(dirname "$0")/../../"

# Try to determine the branch
if [[ -z "${KOKORO_GITHUB_COMMIT:-}" ]]; then
  echo "KOKORO_GITHUB_COMMIT not found, aborting..."
  exit 1
else
  BRANCH=`git branch --no-color --contains ${KOKORO_GITHUB_COMMIT} \
    | grep -v HEAD | head -1`
  # Trim it with echo
  BRANCH=`echo ${BRANCH}`
  echo "branch detected: ${BRANCH}"
fi

# install docuploader
python3 -m pip install gcp-docuploader

# Build doxygen docs
cmake -H. "-B${BUILD_DIR}" \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=OFF \
  -DCMAKE_BUILD_TYPE=Debug \
  -DGOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER=package \
  -DGOOGLE_CLOUD_CPP_GEN_DOCS_FOR_GOOGLEAPIS_DEV=on
cmake --build "${BUILD_DIR}" -- -j `nproc`
cmake --build "${BUILD_DIR}" --target install
cmake --build "${BUILD_DIR}" --target doxygen-docs

upload_docs() {
  DOCS_NAME="${1}"
  DOCS_DIR="${2}"
  if [ ! -d "${DOCS_DIR}" ]; then
    echo "Document not found at: ${DOCS_DIR}"
    return 1
  fi
  pushd "${DOCS_DIR}"
  if [[ "${BRANCH}" == "master" ]]; then
    DOCS_VERSION="master"
  else
    # Extract the version from index.html
    DOCS_VERSION=`sed -n 's:.*<span id="projectnumber">\(.*\)</span>.*:\1:p' \
      index.html`
  fi

  # Create docs metadata
  python3 -m docuploader create-metadata \
    --name "${DOCS_NAME}" \
    --version "${DOCS_VERSION}" \
    --language cpp
  cat docs.metadata

  # Upload the docs
  python3 -m docuploader upload . \
    --credentials "${CREDENTIALS}" \
    --staging-bucket "${STAGING_BUCKET}"
  popd
  return 0
}

upload_docs "google-cloud-common" "${BUILD_DIR}/google/cloud/html"
upload_docs "google-cloud-bigtable" "${BUILD_DIR}/google/cloud/bigtable/html"
upload_docs "google-cloud-storage" "${BUILD_DIR}/google/cloud/storage/html"
upload_docs "google-cloud-firestore" "${BUILD_DIR}/google/cloud/firestore/html"
