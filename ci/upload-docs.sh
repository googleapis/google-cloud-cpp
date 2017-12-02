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

if [ "${TRAVIS_PULL_REQUEST}" != "false" ]; then
    echo "Skipping code generation as it is disabled for pull requests."
    exit 0
fi

if [ "${GENERATE_DOCS:-}" != "yes" ]; then
    echo "Skipping document generation as it is disabled for this build."
    exit 0
fi

# TODO(coryan) - change the branch name to "master" before sending the PR.
if [ "${TRAVIS_BRANCH:-}" != "issue-18-generate-doxygen-docs" ]; then
    echo "Skipping document generation as it is disabled for non-master directories."
    exit 0
fi

# The usual way to host documentation in ${GIT_NAME}.github.io/${PROJECT_NAME}
# is to create a branch (gh-pages) and post the documentation in that branch.
# We first do some general git configuration:

# Clone the gh-pages branch into the doc/html subdirectory.
REPO_URL=$(git config remote.origin.url)
REPO_REF=${REPO_URL/https:\/\/}
git clone -b gh-pages "${REPO_URL}" doc/html

# Remove any previous content of the branch.  We will recover any unmodified
# files in a second.
(cd doc/html && git rm -qfr . || exit 0)

# Copy the build results out of the docker image:
readonly IMAGE="cached-${DISTRO}-${DISTRO_VERSION}"
sudo docker run --volume "$PWD/doc:/d" --rm -it "${IMAGE}:tip" cp -r /var/tmp/build/gccpp/bigtable/doc/html /d

cd doc/html
git config user.name "Travis Build Robot"
git config user.email "nobody@users.noreply.github.com"
git add --all .
git commit -q -m"Automatically generated documentation" || exit 0

if [ -z "${GH_TOKEN:-}" ]; then
    echo "Skipping documentation upload as GH_TOKEN is not configured."
    exit 0
fi
git push https://${GH_TOKEN}@${REPO_REF} gh-pages
