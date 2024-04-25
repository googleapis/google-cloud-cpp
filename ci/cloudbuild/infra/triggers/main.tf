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
    clang-tidy = {
      config = "ci/cloudbuild/cloudbuild.yaml"
      distro = "fedora-latest-cmake"
      shard  = "__default__"
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
    _BUILD_NAME   = "${each.key}"
    _DISTRO       = "${each.value.distro}"
    _SHARD        = "${each.value.shard}"
    _TRIGGER_TYPE = "pr"
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
    _BUILD_NAME   = "${each.key}"
    _DISTRO       = "${each.value.distro}"
    _SHARD        = "${each.value.shard}"
    _TRIGGER_TYPE = "ci"
  }

  include_build_logs = "INCLUDE_BUILD_LOGS_WITH_STATUS"
}
