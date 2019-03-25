# ~~~
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
# ~~~

# A helper file for IncludeNlohmannJson. We need to download the json.hpp file.
# With old versions of cmake there is no way to disable the untar step from all
# downloads, and we get failures.  This is a custom cmake script to download
# that file.
set(JSON_URL
    "https://github.com/nlohmann/json/releases/download/v3.4.0/json.hpp")
set(
    JSON_SHA256 63da6d1f22b2a7bb9e4ff7d6b255cf691a161ff49532dcc45d398a53e295835f
    )

# Use a function to avoid filling up the global namespace with local variables.
function (_download_json_hpp)
    set(sleep_seconds 2)
    foreach (attempt 1 2 3 4 5)
        math(EXPR sleep_seconds "2 * ${sleep_seconds}")
        if (NOT attempt EQUAL 1)
            message(
                STATUS
                    "Will retry after ${sleep_seconds} seconds (attempt #${attempt})."
                )
            execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep
                                    "${sleep_seconds}")
        endif ()
        file(DOWNLOAD "${JSON_URL}" "${DEST}/json.hpp" STATUS download_status)
        list(GET download_status 0 download_error_code)
        if (download_error_code EQUAL 0)
            break()
        endif ()
    endforeach ()

    if (NOT download_error_code EQUAL 0)
        message(
            FATAL_ERROR
                "Could not download ${JSON_URL} after multiple attempts."
                " status=${download_status}")
    endif ()

    file(SHA256 "${DEST}/json.hpp" actual_json_hpp_sha256)
    if (NOT "${JSON_SHA256}" STREQUAL "${actual_json_hpp_sha256}")
        message(FATAL_ERROR "Mismatch digest for downloaded json.hpp file."
                            "\n  expected hash: [${JSON_SHA256}]"
                            "\n    actual hash: [${actual_json_hpp_sha256}]")
    endif ()
    # Remove the definitions of `operator""_json()` and
    # `operator""_json_pointer`. I know it looks ugly to remove specific lines,
    # but we know the contents of the file exactly (there is a SHA256 hash of it
    # above).
    file(READ "${DEST}/json.hpp" JSON_HPP_CONTENT)
    string(
        REPLACE
            [==[
inline nlohmann::json operator "" _json(const char* s, std::size_t n)
{
    return nlohmann::json::parse(s, s + n);
}
]==]
            "" JSON_HPP_CONTENT "${JSON_HPP_CONTENT}")
    string(REPLACE
            [==[
inline nlohmann::json::json_pointer operator "" _json_pointer(const char* s, std::size_t n)
{
    return nlohmann::json::json_pointer(std::string(s, n));
}
]==]
            ""
            JSON_HPP_CONTENT
            "${JSON_HPP_CONTENT}")

    file(WRITE "${DEST}/json.hpp" "${JSON_HPP_CONTENT}")
endfunction ()

_download_json_hpp()
