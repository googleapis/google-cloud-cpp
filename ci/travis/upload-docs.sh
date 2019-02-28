#!/usr/bin/env bash
#
# Copyright 2017 Google Inc.
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

set -eu

if [[ -z "${PROJECT_ROOT+x}" ]]; then
  readonly PROJECT_ROOT="$(cd "$(dirname $0)/../.."; pwd)"
fi
source "${PROJECT_ROOT}/ci/travis/linux-config.sh"

if [ "${TRAVIS_PULL_REQUEST}" != "false" ]; then
  echo "Skipping document generation as it is disabled for pull requests."
  exit 0
fi

if [ "${GENERATE_DOCS:-}" != "yes" ]; then
  echo "Skipping document generation as it is disabled for this build."
  exit 0
fi

subdir=""
case "${TRAVIS_BRANCH:-}" in
    master)
      subdir="latest"
      ;;
    v[0-9]\.*)
      subdir=$(echo "${TRAVIS_BRANCH:-}" | sed -r -e 's/^v//' -e 's/^([0-9]+).([0-9]+).x/\1.\2.0/')
      ;;
    *)
      echo "Skipping document generation as it is only used in master and release branches."
      exit 0
      ;;
esac

# Clone the gh-pages branch into a staging directory.
readonly REPO_URL=$(git config remote.origin.url)
if [ -d github-io-staging ]; then
  if [ ! -d github-io-staging/.git ]; then
    echo "github-io-staging exists but it is not a git repository."
    exit 1
  fi
  (cd github-io-staging && git checkout gh-pages && git pull)
else
  git clone -b gh-pages "${REPO_URL}" github-io-staging
fi

output="${PROJECT_ROOT}/docs-output"

# Copy the build results.
mkdir -p "${output}" || echo "${output} already exists"
cp -r "${BUILD_OUTPUT}/google/cloud/bigtable/html/." "${output}/bigtable"
cp -r "${BUILD_OUTPUT}/google/cloud/html/." "${output}/common"
cp -r "${BUILD_OUTPUT}/google/cloud/firestore/html/." "${output}/firestore"
cp -r "${BUILD_OUTPUT}/google/cloud/storage/html/." "${output}/storage"

cd github-io-staging
if [ "${subdir}" != "latest" ]; then
  cp -r latest/css "${output}"
  cp -r latest/img "${output}"
  cp -r latest/js "${output}"
fi
cp -r ../doc/landing/index.html "${output}"

if git diff --quiet HEAD; then
  echo "Skipping documentation upload as there are no differences to upload."
  exit 0
fi
