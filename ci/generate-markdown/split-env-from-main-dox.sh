#!/bin/bash
#
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

set -euo pipefail

mapfile -t ga_list < <(cmake -DCMAKE_MODULE_PATH="${PWD}/cmake" -P cmake/print-ga-libraries.cmake 2>&1)

for library in "${ga_list[@]}"; do
  maindox="google/cloud/${library}/doc/main.dox"
  envdox="google/cloud/${library}/doc/environment-variables.dox"
  if [[ ! -r "${maindox}" ]]; then
    echo "Skip ${library}, no main.dox"
    continue
  fi
  if ! grep -q '^## Environment Variables' "${maindox}"; then
    echo "Skip ${library}, no Environment Variables section in main.dox"
    continue
  fi
  echo "Processing ${library}"
  (
    cat <<_EOF_
/*!

@page ${library}-env Environment Variables

A number of environment variables can be used to configure the behavior of
the library.  There are also functions to configure this behavior in code. The
environment variables are convenient when troubleshooting problems.

@section ${library}-env-endpoint Endpoint Overrides

_EOF_
    sed -e '1,/^## Environment Variables/d' \
      -e '/^<!-- inject-endpoint-env-vars-end -->/,$d' \
      "${maindox}"
    sed "s/@library@/${library}/" <<'_EOF_'
<!-- inject-endpoint-env-vars-end -->

@section @library@-logging Logging

`GOOGLE_CLOUD_CPP_ENABLE_TRACING=rpc`: turns on tracing for most gRPC
calls. The library injects an additional Stub decorator that prints each gRPC
request and response.  Unless you have configured you own logging backend,
you should also set `GOOGLE_CLOUD_CPP_ENABLE_CLOG` to produce any output on
the program's console.

@see google::cloud::TracingComponentsOption

`GOOGLE_CLOUD_CPP_TRACING_OPTIONS=...\`: modifies the behavior of gRPC tracing,
including whether messages will be output on multiple lines, or whether
string/bytes fields will be truncated.

@see google::cloud::GrpcTracingOptionsOption

`GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes`: turns on logging in the library, basically
the library always "logs" but the logging infrastructure has no backend to
actually print anything until the application sets a backend or they set this
environment variable.

@see google::cloud::LogBackend
@see google::cloud::LogSink

@section @library@-env-project Setting the Default Project

`GOOGLE_CLOUD_PROJECT=...`: is used in examples and integration tests to
configure the GCP project. This has no effect in the library.

*/
_EOF_
  ) >"${envdox}"
  sed -n '1,/^## Environment Variables/p;/^## Error Handling/,$p' "${maindox}" |
    sed -e '/^## Environment Variables/d' |
    sponge "${maindox}"
done
