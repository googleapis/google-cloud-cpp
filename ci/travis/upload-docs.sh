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
  readonly PROJECT_ROOT="$(cd "$(dirname "$0")/../.."; pwd)"
fi

if [ "${BUILD_OUTPUT:-}" == "" ]; then
  source "${PROJECT_ROOT}/ci/travis/linux-config.sh"
fi

if [ "${TRAVIS_PULL_REQUEST:-false}" != "false" ]; then
  echo "Skipping document generation as it is disabled for pull requests."
  exit 0
fi

if [ "${GENERATE_DOCS:-}" != "yes" ]; then
  echo "Skipping document generation as it is disabled for this build."
  exit 0
fi

subdir=""
if [ "${DOCS_SUBDIR:-}" != "" ]; then
  subdir="${DOCS_SUBDIR}"
else
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
fi

# The usual way to host documentation in ${GIT_NAME}.github.io/${PROJECT_NAME}
# is to create a branch (gh-pages) and post the documentation in that branch.
# We first do some general git configuration:

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

# Remove any previous content in the subdirectory used for this release. We will
# recover any unmodified files in a second.
cd github-io-staging
git rm -qfr --ignore-unmatch "${subdir}/google/cloud/bigtable"
git rm -qfr --ignore-unmatch "${subdir}/google/cloud/common"
git rm -qfr --ignore-unmatch "${subdir}/google/cloud/firestore"
git rm -qfr --ignore-unmatch "${subdir}/google/cloud/storage"

# Copy the build results into the gh-pages clone.
mkdir -p "${subdir}" || echo "${subdir} already exists"
cp -r "../${BUILD_OUTPUT}/google/cloud/bigtable/html/." "${subdir}/bigtable"
cp -r "../${BUILD_OUTPUT}/google/cloud/html/." "${subdir}/common"
cp -r "../${BUILD_OUTPUT}/google/cloud/firestore/html/." "${subdir}/firestore"
cp -r "../${BUILD_OUTPUT}/google/cloud/storage/html/." "${subdir}/storage"
if [ "${subdir}" != "latest" ]; then
  cp -r latest/css "${subdir}"
  cp -r latest/img "${subdir}"
  cp -r latest/js "${subdir}"
fi
cp -r ../doc/landing/index.html "${subdir}"

git config user.name "Google Cloud C++ Project Robot"
git config user.email "google-cloud-cpp-bot@users.noreply.github.com"
git add --all "${subdir}"

if git diff --quiet HEAD; then
  echo "Skipping documentation upload as there are no differences to upload."
  exit 0
fi

git commit -q -m"Automatically generated documentation"

if [ "${REPO_URL:0:8}" != "https://" ]; then
  echo "Repository is not in https:// format, attempting push to ${REPO_URL}"
  git push
  exit 0
fi

if [ -z "${GH_TOKEN:-}" ]; then
  echo "Skipping documentation upload as GH_TOKEN is not configured."
  exit 0
fi

readonly REPO_REF=${REPO_URL/https:\/\/}
git push https://"${GH_TOKEN}@${REPO_REF}" gh-pages
