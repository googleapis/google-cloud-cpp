#!/usr/bin/env bash
# Record the current commit hash.
git -C github/google-cloud-cpp log -1 --format=%H >commit-hash.txt
