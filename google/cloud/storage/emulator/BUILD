# Copyright 2020 Google LLC
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

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

py_test(
    name = "test_gcs",
    size = "small",
    srcs = ["tests/test_gcs.py"] + glob(["gcs/*.py"]) + glob(["utils/*.py"]),
    imports = ["."],
    main = "tests/test_gcs.py",
    tags = ["manual"],
)

py_test(
    name = "test_utils",
    size = "small",
    srcs = ["tests/test_utils.py"] + glob(["gcs/*.py"]) + glob(["utils/*.py"]),
    imports = ["."],
    main = "tests/test_utils.py",
    tags = ["manual"],
)
