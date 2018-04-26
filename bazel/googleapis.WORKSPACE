# Copyright 2018 Google LLC
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

workspace(name = "com_github_googleapis_googleapis")

http_archive(
    name = "com_github_grpc_grpc",
    strip_prefix = "grpc-1.10.0",
            urls = [
                "https://mirror.bazel.build/github.com/grpc/grpc/archive/v1.10.0.tar.gz",
                "https://github.com/grpc/grpc/archive/v1.10.0.tar.gz",
            ],
            sha256 = "39a73de6fa2a03bdb9c43c89a4283e09880833b3c1976ef3ce3edf45c8cacf72",
)
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
grpc_deps()

