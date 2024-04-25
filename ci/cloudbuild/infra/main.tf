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

terraform {
  required_providers {
    google = {
      source  = "hashicorp/google"
      version = "~> 5.21.0"
    }
  }
}

provider "google" {
  project = var.project
  region  = var.region
  zone    = var.zone
}

locals {
  # Google Cloud Build installs an application on the GitHub organization or
  # repository. This id is hard-coded here because there is no easy way [^1] to
  # manage that installation via terraform.
  #
  # [^1]: there is a way, described in [Connecting a Gitub host programatically]
  #     but I would not call that "easy". It requires (for example) manually
  #     creating a personally access token (PAT) on GitHub, and storing that
  #     in the Terraform file.
  # [Connecting a Gitub host programatically]: https://cloud.google.com/build/docs/automating-builds/github/connect-repo-github?generation=2nd-gen#terraform
  #
  gcb_app_installation_id = 1168573

  # Google Cloud build stores a secret to access Github in secret manager. The 
  # UI creates a secret name that we record here.
  gcb_secret_name = "projects/${var.project}/secrets/github-github-oauthtoken-e403ad/versions/latest"
}

# Enable Cloud Build and create triggers.
module "services" {
  source  = "./services"
  project = var.project
}

resource "google_cloudbuildv2_connection" "github" {
  project  = var.project
  location = var.region
  name     = "github"

  github_config {
    app_installation_id = local.gcb_app_installation_id
    authorizer_credential {
      oauth_token_secret_version = local.gcb_secret_name
    }
  }
  depends_on = [module.services]
}

resource "google_cloudbuildv2_repository" "cpp-storage-prelaunch" {
  project           = var.project
  location          = var.region
  name              = "googleapis-cpp-storage-prelaunch"
  parent_connection = google_cloudbuildv2_connection.github.name
  remote_uri        = "https://github.com/googleapis/cpp-storage-prelaunch.git"
}

resource "google_cloudbuild_worker_pool" "cpp-pool" {
  name     = "cpp-pool"
  location = "us-central1"
  worker_config {
    disk_size_gb   = 256
    machine_type   = "e2-standard-32"
    no_external_ip = false
  }
}

# This is used to retrieve the project number. The project number is embedded in
# certain P4 (Per-product per-project) service accounts.
data "google_project" "project" {
}

# Create a bucket to store the public-facing logs on GitHub. Though the repo
# is private, we want to be able to see the logs directly on GitHub.
resource "google_storage_bucket" "publiclogs" {
  name = "${var.project}-publiclogs"
  # Use a project with the right exceptions to support public buckets.
  project = "cloud-cpp-community"

  # The bucket configuration.
  location                    = var.region
  storage_class               = "STANDARD"
  uniform_bucket_level_access = true
  # After 90 days the logs are rarely interesting.
  lifecycle_rule {
    condition {
      age = 90
    }
    action {
      type = "Delete"
    }
  }
}

resource "google_storage_bucket_iam_binding" "publiclogs-are-public" {
  bucket = google_storage_bucket.publiclogs.name
  role   = "roles/storage.objectViewer"
  members = [
    "allUsers"
  ]
}

resource "google_storage_bucket_iam_member" "gcb-writes-to-publiclogs" {
  bucket = google_storage_bucket.publiclogs.name
  role   = "roles/storage.admin"
  member = "serviceAccount:${data.google_project.project.number}@cloudbuild.gserviceaccount.com"
}

# We create a bucket to store CI caching artifacts. This greatly accelerates
# C++ builds
resource "google_storage_bucket" "ci-cache" {
  name          = "${var.project}-ci-cache"
  force_destroy = false
  # This prevents Terraform from deleting the bucket. Any plan to do so is
  # rejected. If we really need to delete the bucket we must take additional
  # steps.
  lifecycle {
    prevent_destroy = true
  }

  # The bucket configuration.
  location                    = var.region
  storage_class               = "STANDARD"
  uniform_bucket_level_access = true
  # After 180 days build caches are probably expired, key depedencies like
  # Protobuf or gRPC change more often than this and invalidate most cached
  # objects. 
  lifecycle_rule {
    condition {
      age = 180
    }
    action {
      type = "Delete"
    }
  }
}

resource "google_storage_bucket_iam_member" "gcb-owns-ci-cache" {
  bucket = google_storage_bucket.ci-cache.name
  role   = "roles/storage.admin"
  member = "serviceAccount:${data.google_project.project.number}@cloudbuild.gserviceaccount.com"
}

# Enable Cloud Build and create triggers.
module "triggers" {
  depends_on = [module.services,
    google_storage_bucket.ci-cache,
    google_storage_bucket.publiclogs,
    google_cloudbuildv2_repository.cpp-storage-prelaunch,
  google_cloudbuild_worker_pool.cpp-pool]
  source     = "./triggers"
  project    = var.project
  region     = var.region
  repository = google_cloudbuildv2_repository.cpp-storage-prelaunch.id
}
