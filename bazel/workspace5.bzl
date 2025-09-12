# Copyright 2023 Google LLC
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

"""Load dependencies needed for google-cloud-cpp development / Phase 4."""

def gl_cpp_workspace5(name = None):
    """Loads dependencies needed to use the google-cloud-cpp libraries.

    Call this function after `//bazel/workspace4:workspace4()`.

    Args:
        name: Unused. It is conventional to provide a `name` argument to all
            workspace functions.
    """
    # This is a placeholder, if we ever need more phases the function already
    # exists.
    native.bind(
        name = "libcrypto",
        actual = "@boringssl//:crypto",
    )

    native.bind(
        name = "libssl",
        actual = "@boringssl//:ssl",
    )
