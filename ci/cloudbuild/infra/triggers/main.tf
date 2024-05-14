# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

variable "project" {}
variable "region" {}
variable "repository" {}

locals {
  builds = {
    asan = {
      config = "ci/cloudbuild/cloudbuild.yaml"
      distro = "fedora-latest-bazel"
    }
    checkers = {
      config = "ci/cloudbuild/cloudbuild.yaml"
      distro = "checkers"
    }
    clang-tidy = {
      config = "ci/cloudbuild/cloudbuild.yaml"
      distro = "fedora-latest-cmake"
    }
  }
}

resource "google_cloudbuild_trigger" "pull-request" {
  for_each = tomap(local.builds)
  location = var.region
  name     = "${each.key}-pr"
  filename = each.value.config
  tags     = ["pull-request", "name:${each.key}"]

  repository_event_config {
    repository = var.repository
    pull_request {
      branch          = "^pre-launch-*"
      comment_control = "COMMENTS_ENABLED_FOR_EXTERNAL_CONTRIBUTORS_ONLY"
    }
  }

  substitutions = {
    _BUILD_NAME   = lookup(each.value, "name", "${each.key}")
    _DISTRO       = "${each.value.distro}"
    _SHARD        = lookup(each.value, "shard", "__default__")
    _TRIGGER_TYPE = "pr"
    _POOL_REGION  = var.region
    _POOL_ID      = "cpp-pool"
    _CACHE_BUCKET = "${var.project}-ci-cache"
    _LOGS_BUCKET  = "${var.project}-publiclogs"
  }

  include_build_logs = "INCLUDE_BUILD_LOGS_WITH_STATUS"
}

resource "google_cloudbuild_trigger" "post-merge" {
  for_each = tomap(local.builds)
  location = var.region
  name     = "${each.key}-pm"
  filename = each.value.config
  tags     = ["post-merge", "push", "name:${each.key}"]

  repository_event_config {
    repository = var.repository
    push {
      branch = "^pre-launch-.*"
    }
  }

  substitutions = {
    _BUILD_NAME   = lookup(each.value, "name", "${each.key}")
    _DISTRO       = "${each.value.distro}"
    _SHARD        = lookup(each.value, "shard", "__default__")
    _TRIGGER_TYPE = "ci"
  }

  include_build_logs = "INCLUDE_BUILD_LOGS_WITH_STATUS"
}
