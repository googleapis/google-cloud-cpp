workspace(name = "google_cloud_cpp")

load("//bazel:workspace0.bzl", "gl_cpp_workspace0")

gl_cpp_workspace0()

# Add aliases for canonical repository names. This provides backward
# compatibility for the legacy WORKSPACE-based builds, which expect the old
# repository names, while allowing the rest of the repository to use the
# modern canonical names required for Bzlmod.
native.bind(name = "googletest", actual = "@com_google_googletest")
native.bind(name = "abseil-cpp", actual = "@com_google_absl")
native.bind(name = "curl", actual = "@com_github_curl_curl")
native.bind(name = "crc32c", actual = "@com_github_google_crc32c")
native.bind(name = "nlohmann_json", actual = "@com_github_nlohmann_json")
native.bind(name = "grpc", actual = "@com_github_grpc_grpc")
native.bind(name = "opentelemetry-cpp", actual = "@io_opentelemetry_cpp")
native.bind(name = "googleapis", actual = "@com_google_googleapis")
native.bind(name = "protobuf", actual = "@com_google_protobuf")

load("//bazel:workspace2.bzl", "gl_cpp_workspace2")

gl_cpp_workspace2()

load("//bazel:workspace3.bzl", "gl_cpp_workspace3")

gl_cpp_workspace3()
