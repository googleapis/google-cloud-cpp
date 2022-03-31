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
Define a rule to configure .in template files.

We use custom *.BUILD files for two external dependencies that do not natively support Bazel: crc32c and libcurl.
Both require configuring a `.in` file. This creates a rules to perform this configuration.
"""

def _configure_template_impl(ctx):
    ctx.actions.expand_template(
        template = ctx.file.template,
        output = ctx.outputs.output,
        substitutions = ctx.attr.substitutions,
    )

configure_template = rule(
    implementation = _configure_template_impl,
    attrs = {
        "substitutions": attr.string_dict(default = {}),
        "template": attr.label(
            allow_single_file = True,
            mandatory = True,
        ),
        "output": attr.output(mandatory = True),
    },
)
