#!/usr/bin/env bash
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

if [ -z "${PROJECT_ROOT+x}" ]; then
  readonly PROJECT_ROOT="$(cd "$(dirname "$0")/.."; pwd)"
fi
source "${PROJECT_ROOT}/ci/define-dump-log.sh"
source "${PROJECT_ROOT}/ci/colors.sh"

# If an example fails, this is set to 1 and the program exits with failure.
EXIT_STATUS=0

# Define the name of the emulator / testbench log file, if it has not been
# defined already.
EMULATOR_LOG="${EMULATOR_LOG:-}"

################################################
# Run one example in a given program
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
#   EMULATOR_LOG: the name of the emulator logfile.
# Arguments:
#   program_name: the name of the program to run.
#   example: the name of the example.
#   *: the arguments for that example.
# Returns:
#   None
################################################
run_example() {
  if [ $# -lt 2 ]; then
    echo "Usage: run_example <program_name> example [arg1 arg2 ...]"
    exit 1
  fi

  local program_path=$1
  local example=$2
  shift 2
  local -a arguments=( "$@" )
  local program_name
  program_name="$(basename "${program_path}")"

  if [ ! -x "${program_path}" ]; then
    echo "${COLOR_YELLOW}[  SKIPPED ]${COLOR_RESET}" \
        " ${program_name} is not compiled"
    return
  fi
  log="$(mktemp -t "run_example.XXXXXX")"
  echo    "${COLOR_GREEN}[ RUN      ]${COLOR_RESET}" \
      " ${program_name} ${example} running"
  # We use parameter expansion for ${arguments} because set -u doesn't like
  # empty arrays on older versions of Bash (which some of our builds use). The
  # expression ${parameter+word} will expand word only if parameter is not
  # unset.
  echo "${program_path}" "${example}" "${arguments[@]+"${arguments[@]}"}" >"${log}"
  set +e
  "${program_path}" "${example}" "${arguments[@]+"${arguments[@]}"}" >>"${log}" 2>&1 </dev/null
  if [ $? = 0 ]; then
    echo  "${COLOR_GREEN}[       OK ]${COLOR_RESET}" \
        " ${program_name} ${example}"
  else
    EXIT_STATUS=1
    echo    "${COLOR_RED}[    ERROR ]${COLOR_RESET}" \
        " ${program_name} ${example}"
    echo
    dump_log "${log}"
    if [ -f "${EMULATOR_LOG}" ]; then
      dump_log "${EMULATOR_LOG}"
    fi
  fi
  set -e
  /bin/rm -f "${log}"
}

################################################
# Run one example in a given program expecting a failure
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
#   EMULATOR_LOG: the name of the emulator logfile.
# Arguments:
#   program_name: the name of the program to run.
#   example: the name of the example.
#   *: the arguments for that example.
# Returns:
#   None
################################################
run_failure_example() {
  if [[ $# -lt 2 ]]; then
    echo "Usage: run_example <program_name> example [arg1 arg2 ...]"
    exit 1
  fi

  local program_path=$1
  local example=$2
  shift 2
  local -a arguments=( "$@" )
  local program_name
  program_name="$(basename "${program_path}")"

  if [[ ! -x "${program_path}" ]]; then
    echo "${COLOR_YELLOW}[  SKIPPED ]${COLOR_RESET}" \
        " ${program_name} is not compiled"
    return
  fi
  log="$(mktemp -t "run_example.XXXXXX")"
  echo    "${COLOR_GREEN}[ RUN      ]${COLOR_RESET}" \
      " ${program_name} ${example} running"
  # We use parameter expansion for ${arguments} because set -u doesn't like
  # empty arrays on older versions of Bash (which some of our builds use). The
  # expression ${parameter+word} will expand word only if parameter is not
  # unset.
  echo "${program_path}" "${example}" "${arguments[@]+"${arguments[@]}"}" >"${log}"
  set +e
  "${program_path}" "${example}" "${arguments[@]+"${arguments[@]}"}" >>"${log}" 2>&1 </dev/null
  if [[ $? != 0 ]]; then
    echo  "${COLOR_GREEN}[       OK ]${COLOR_RESET}" \
        " ${program_name} ${example}"
  else
    EXIT_STATUS=1
    echo    "${COLOR_RED}[    ERROR ]${COLOR_RESET}" \
        " ${program_name} ${example}"
    echo
    dump_log "${log}"
    if [[ -f "${EMULATOR_LOG}" ]]; then
      dump_log "${EMULATOR_LOG}"
    fi
  fi
  set -e
  /bin/rm -f "${log}"
}

################################################
# Test the Usage messages for an example program.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
#   EMULATOR_LOG: the name of the emulator logfile.
# Arguments:
#   program_name: the name of the program to run.
# Returns:
#   None
################################################
run_example_usage() {
  if [ $# -lt 1 ]; then
    echo "Usage: run_example <program_name>"
    exit 1
  fi

  local program_path="$1"
  shift 1
  local program_name
  program_name="$(basename "${program_path}")"

  if [ ! -x "${program_path}" ]; then
    echo "${COLOR_YELLOW}[  SKIPPED ]${COLOR_RESET}" \
        " ${program_name} is not compiled"
    return
  fi
  log="$(mktemp -t "run_example.XXXXXX")"
  echo    "${COLOR_GREEN}[ RUN      ]${COLOR_RESET}" \
      " ${program_name} running"
  echo "${program_path}" >"${log}"
  set +e
  ${program_path} >>"${log}" 2>&1 </dev/null
  if [ $? != 0 ] && grep -q 'Usage' "${log}"; then
    echo  "${COLOR_GREEN}[       OK ]${COLOR_RESET}" \
        " ${program_name}"
  else
    EXIT_STATUS=1
    echo    "${COLOR_RED}[    ERROR ]${COLOR_RESET}" \
        " ${program_name}"
    echo
    dump_log "${log}"
    if [ -f "${EMULATOR_LOG}" ]; then
      dump_log "${EMULATOR_LOG}"
    fi
  fi
  set -e
  /bin/rm -f "${log}"
}

################################################
# Report the result of running many examples.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
#   EMULATOR_LOG: the name of the emulator logfile.
# Arguments:
#   None
# Returns:
#   None
################################################
exit_example_runner() {
  if [[ "${EXIT_STATUS}" == 0 ]]; then
    echo
    echo "${COLOR_GREEN}All the examples finished successfully.${COLOR_RESET}"
    echo
  else
    echo
    echo "${COLOR_RED}Some of the examples failed.${COLOR_RESET}"
    echo
  fi

  exit "${EXIT_STATUS}"
}
