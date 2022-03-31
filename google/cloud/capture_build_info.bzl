# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Define a rule to capture the compile-time options.

Our benchmarks report the compile-time build flags, as these are important to evaluate and compiler benchmark results.
With Bazel we need to create a new rule that captures these flags.  This rule substitutes some magic strings in
`google/cloud/internal/build_info.cc.in` to the values of several interesting compilation parameters.  The substitution
is easy, we simply use a native Bazel function: `ctx.actions.expand_template()`.  Finding the values of the compilation
flags requires digging into the C++ toolchain definition.  This is documented (at a high-level) here:

https://bazel.build/docs/integrating-with-rules-cc

it links to an example here:

https://github.com/bazelbuild/rules_cc/blob/main/examples/my_c_compile/my_c_compile.bzl

I found this fragment particularly useful:

https://github.com/bazelbuild/rules_cc/blob/58f8e026c00a8a20767e3dc669f46ba23bc93bdb/examples/my_c_compile/my_c_compile.bzl#L40-L51
"""

load("@rules_cc//cc:action_names.bzl", "CPP_COMPILE_ACTION_NAME")
load("@rules_cc//cc:toolchain_utils.bzl", "find_cpp_toolchain")

def _capture_build_info_impl(ctx):
    toolchain = find_cpp_toolchain(ctx)
    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = toolchain,
        requested_features = ctx.features,
        unsupported_features = ["module_maps"] + ctx.disabled_features,
    )
    cc_source = ctx.outputs.source_file
    cc_vars = cc_common.create_compile_variables(
        feature_configuration = feature_configuration,
        cc_toolchain = toolchain,
        user_compile_flags = ctx.fragments.cpp.cxxopts + ctx.fragments.cpp.conlyopts,
        source_file = cc_source.path,
    )
    command_line = cc_common.get_memory_inefficient_command_line(
        feature_configuration = feature_configuration,
        action_name = CPP_COMPILE_ACTION_NAME,
        variables = cc_vars,
    )

    # The command_line is a list of strings, the last two are `-c` and `.../*.cc` we want to ignore them
    if len(command_line) > 2:
        command_line = command_line[:-2]
    cc_flags = " ".join(command_line)
    compilation_mode = ctx.var.get("COMPILATION_MODE", "fastbuild")
    ctx.actions.expand_template(
        template = ctx.file.template,
        output = ctx.outputs.source_file,
        substitutions = {
            "@CMAKE_CXX_FLAGS@": "bazel:" + compilation_mode,
            "${CMAKE_CXX_FLAGS_${GOOGLE_CLOUD_CPP_BUILD_TYPE_UPPER}}": cc_flags,
            "@GOOGLE_CLOUD_CPP_BUILD_METADATA@": "",
        },
    )

capture_build_info = rule(
    implementation = _capture_build_info_impl,
    attrs = {
        "template": attr.label(
            allow_single_file = True,
            mandatory = True,
        ),
        "source_file": attr.output(mandatory = True),
        "_cc_toolchain": attr.label(default = Label("@bazel_tools//tools/cpp:current_cc_toolchain")),
    },
    toolchains = ["@bazel_tools//tools/cpp:toolchain_type"],
    fragments = ["cpp"],
)
