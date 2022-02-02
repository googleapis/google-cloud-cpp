# ~~~
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
# ~~~

# A CMake script to run the quickstart programs.
#
# This script runs the "quickstart" program for a given library. We use CMake as
# a scripting language because we know CMake to be installed and usable for both
# Windows, Linux, and macOS. At least while building with CMake.
#
# The first two arguments for this script are always `-P` and the name of the
# script itself.
#
# This script receives the path to the quickstart executable in CMAKE_ARGV3.
#
# The remaining arguments to the quickstart are the *names* of N environment
# variables. The script expects that these environment variables will be set by
# the CI system.
#

if (CMAKE_ARGC VERSION_LESS "4")
    message(
        FATAL_ERROR "Usage: cmake -P quickstart-runner.cmake EXE [ARGS ...]")
elseif (CMAKE_ARGC STREQUAL "4")
    execute_process(
        TIMEOUT 300
        RESULT_VARIABLE RET
        OUTPUT_VARIABLE LOG
        ERROR_VARIABLE LOG
        COMMAND "${CMAKE_ARGV3}")
elseif (CMAKE_ARGC STREQUAL "5")
    execute_process(
        TIMEOUT 300
        RESULT_VARIABLE RET
        OUTPUT_VARIABLE LOG
        ERROR_VARIABLE LOG
        COMMAND "${CMAKE_ARGV3}" "$ENV{${CMAKE_ARGV4}}")
elseif (CMAKE_ARGC STREQUAL "6")
    execute_process(
        TIMEOUT 300
        RESULT_VARIABLE RET
        OUTPUT_VARIABLE LOG
        ERROR_VARIABLE LOG
        COMMAND "${CMAKE_ARGV3}" "$ENV{${CMAKE_ARGV4}}" "$ENV{${CMAKE_ARGV5}}")
elseif (CMAKE_ARGC STREQUAL "7")
    execute_process(
        TIMEOUT 300
        RESULT_VARIABLE RET
        OUTPUT_VARIABLE LOG
        ERROR_VARIABLE LOG
        COMMAND "${CMAKE_ARGV3}" "$ENV{${CMAKE_ARGV4}}" "$ENV{${CMAKE_ARGV5}}"
                "$ENV{${CMAKE_ARGV6}}")
elseif (CMAKE_ARGC STREQUAL "8")
    execute_process(
        TIMEOUT 300
        RESULT_VARIABLE RET
        OUTPUT_VARIABLE LOG
        ERROR_VARIABLE LOG
        COMMAND "${CMAKE_ARGV3}" "$ENV{${CMAKE_ARGV4}}" "$ENV{${CMAKE_ARGV5}}"
                "$ENV{${CMAKE_ARGV6}}" "$ENV{${CMAKE_ARGV7}}")
elseif (CMAKE_ARGC STREQUAL "9")
    execute_process(
        TIMEOUT 300
        RESULT_VARIABLE RET
        OUTPUT_VARIABLE LOG
        ERROR_VARIABLE LOG
        COMMAND
            "${CMAKE_ARGV3}" "$ENV{${CMAKE_ARGV4}}" "$ENV{${CMAKE_ARGV5}}"
            "$ENV{${CMAKE_ARGV6}}" "$ENV{${CMAKE_ARGV7}}"
            "$ENV{${CMAKE_ARGV8}}")
else ()
    message(
        FATAL_ERROR
            "Too many arguments (${CMAKE_ARGC}) for quickstart-runner.cmake")
endif ()

if (NOT "${RET}" STREQUAL "0")
    message(FATAL_ERROR "Non-zero exit status (${RET}) running ${CMAKE_ARGV3}\n"
                        "${LOG}")
endif ()
