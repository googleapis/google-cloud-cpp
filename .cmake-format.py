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

tab_size = 4
separate_ctrl_name_with_space = True

additional_commands = {
    "externalproject_add": {
        "flags": [
        ],
        "kwargs": {
            "BUILD_COMMAND": "+",
            "BUILD_BYPRODUCTS": "+",
            "COMMAND": "+",
            "CONFIGURE_COMMAND": "+",
            "DEPENDS": "+",
            "DOWNLOAD_COMMAND": "+",
            "EXCLUDE_FROM_ALL": 1,
            "INSTALL_COMMAND": "+",
            "INSTALL_DIR": 1,
            "LOG_BUILD": 1,
            "LOG_CONFIGURE": 1,
            "LOG_DOWNLOAD": 1,
            "LOG_INSTALL": 1,
            "PREFIX": 1,
            "URL": 1,
            "URL_HASH": 1,
        }
    }
}
