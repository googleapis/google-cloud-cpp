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

# Enable Cloud Build and create triggers.
module "services" {
  source  = "./services"
  project = var.project
}

# Creates a number of resources needed to run the GCB (Goole Cloud Build)
# builds. This includes the connection to our GitHub repository, the worker
# pool, and some buckets to store build caches and logs.
module "build" {
  source     = "./build"
  project    = var.project
  region     = var.region
  depends_on = [module.services]
}

# Enable Cloud Build and create triggers.
module "triggers" {
  depends_on = [module.services, module.build]
  source     = "./triggers"
  project    = var.project
  region     = var.region
  repository = module.build.repository
}
