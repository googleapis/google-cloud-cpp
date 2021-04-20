# Copyright 2021 Google LLC
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

FROM debian:bullseye-slim
ARG NCPU=4

RUN apt-get update && apt-get install -y curl git
WORKDIR /var/tmp
RUN curl -sSL -o gh.deb https://github.com/cli/cli/releases/download/v1.9.1/gh_1.9.1_linux_amd64.deb && \
    dpkg --install gh.deb
