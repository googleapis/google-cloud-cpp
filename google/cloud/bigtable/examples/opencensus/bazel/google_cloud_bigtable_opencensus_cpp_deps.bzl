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

"""Load dependencies needed to compile and test the integration of
 opencensus and google-cloud-cpp client library."""

def google_cloud_bigtable_opencensus_cpp_deps():
    """Loads dependencies need to compile the google-cloud-cpp client library
     and opencensus.

    Application developers can call this function from their WORKSPACE file
    to obtain all the necessary dependencies for google-cloud-cpp, including
    gRPC and its dependencies. This function only loads dependencies that
    have not been previously loaded, allowing application developers to
    override the version of the dependencies they want to use.
    """
    
    if "com_google_absl" not in native.existing_rules():
        native.http_archive(
            name = "com_google_absl",
            strip_prefix = "abseil-cpp-20180600",
            sha256 = "794d483dd9a19c43dc1fbbe284ce8956eb7f2600ef350dac4c602f9b4eb26e90",
            urls = ["https://github.com/abseil/abseil-cpp/archive/20180600.tar.gz"],
        )
 
             
    # Load the googleapis dependency.
    if "com_github_googleapis_googleapis" not in native.existing_rules():
        native.new_http_archive(
            name = "com_github_googleapis_googleapis",
            url = "https://github.com/google/googleapis/archive/6a3277c0656219174ff7c345f31fb20a90b30b97.zip",
            strip_prefix = "googleapis-6a3277c0656219174ff7c345f31fb20a90b30b97",
            sha256 = "82ba91a41fb01305de4e8805c0a9270ed2035007161aa5a4ec60f887a499f5e9",
            build_file = "@com_github_googlecloudplatform_google_cloud_cpp//bazel:googleapis.BUILD",
            workspace_file = "@com_github_googlecloudplatform_google_cloud_cpp//bazel:googleapis.WORKSPACE",
        )
        
        
    # Load gRPC and its dependencies, using a similar pattern to this function.
    # This implictly loads "com_google_protobuf", which we use.
    if "com_github_grpc_grpc" not in native.existing_rules():
        native.http_archive(
            name = "com_github_grpc_grpc",
            strip_prefix = "grpc-1.14.2-pre1",
            urls = [
                "https://github.com/grpc/grpc/archive/v1.14.2-pre1.tar.gz",
                "https://mirror.bazel.build/github.com/grpc/grpc/archive/v1.14.2-pre1.tar.gz",
            ],
            sha256 = "81b5cf28a0531c8ba16f3780214ec0a54050c80003036b5ba71b5345731ddea2",
        )

    # Load OpenCensus and its dependencies.
    if "io_opencensus_cpp" not in native.existing_rules():
        native.http_archive(
            name = "io_opencensus_cpp",
            strip_prefix = "opencensus-cpp-893e0835a45d749221f049d0d167e157b67b6d9c",
            urls = [
                "https://github.com/census-instrumentation/opencensus-cpp/archive"
                + "/893e0835a45d749221f049d0d167e157b67b6d9c.tar.gz",
            ],
            sha256 = "8e2bddd3ea6d747a8c4255c73dcea1b9fcdf1560f3bb9ff96bcb20d4d207235e",
        )
        
    # Load OpenCensus and its dependencies.
    if "com_github_googlecloudplatform_google_cloud_cpp" not in native.existing_rules():
        native.http_archive(
            name = "com_github_googlecloudplatform_google_cloud_cpp",
            strip_prefix = "google-cloud-cpp-0.2.0",
            url = "http://github.com/googlecloudplatform/google-cloud-cpp/archive/v0.2.0.tar.gz",
            sha256 = "5fa6577828e5f949178b13ed0411dd634527c9d2d8f00e433edbd6ef9e42a281",
        )
    



    # We use the cc_proto_library() rule from @com_google_protobuf, which
    # assumes that grpc_cpp_plugin and grpc_lib are in the //external: module
    native.bind(
        name = "grpc_cpp_plugin",
        actual = "@com_github_grpc_grpc//:grpc_cpp_plugin",
    )

    native.bind(
        name = "grpc_lib",
        actual = "@com_github_grpc_grpc//:grpc++",
    )
