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

FROM alpine:edge AS devtools

RUN apk update && apk add build-base gcc g++
RUN apk update && apk add libc-dev
RUN apk update && apk add cmake ninja
RUN apk update && apk add curl git perl unzip tar zip
RUN apk update && apk add grpc-dev grpc grpc-cli protoc protobuf-dev
RUN apk update && apk add c-ares-dev

FROM devtools AS build

WORKDIR /home/build
COPY . /home/build
RUN cmake -S . -B .build -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    && cmake --build .build

FROM alpine:edge AS deploy
RUN apk update && apk add grpc protobuf c-ares
WORKDIR /r
COPY --from=build /home/build/.build/hello_world_grpc /r

ENTRYPOINT /r/hello_world_grpc
