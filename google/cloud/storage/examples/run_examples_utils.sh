#!/usr/bin/env bash
#
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

set -eu

if [ -z "${PROJECT_ROOT+x}" ]; then
  readonly PROJECT_ROOT="$(cd "$(dirname $0)/../../../.."; pwd)"
fi
source "${PROJECT_ROOT}/ci/colors.sh"

# If an example fails, this is set to 1 and the program exits with failure.
EXIT_STATUS=0

################################################
# Run a list of examples in a given program
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   program_name: the name of the program to run.
#   examples: a string with the list of examples to run, separated by commas
#   *: the base arguments that all commands need.
# Returns:
#   None
################################################
run_program_examples() {
  if [ $# -lt 3 ]; then
    echo "Usage: run_all_examples <program_name> [example ...]"
    exit 1
  fi

  local program_path=$1
  local example_list=$(echo $2 | tr ',' ' ')
  shift 2
  local base_arguments=$*

  local program_name=$(basename ${program_path})

  if [ ! -x ${program_name} ]; then
    echo "${COLOR_YELLOW}[  SKIPPED ]${COLOR_RESET}" \
        " ${program_name} is not compiled"
    return
  fi
  log="$(mktemp -t "storage_samples.XXXXXX")"
  for example in ${example_list}; do
    echo    "${COLOR_GREEN}[ RUN      ]${COLOR_RESET}" \
        "${program_name} ${example} running"
    #
    # Magic list of examples that need additional arguments.
    #
    case ${example} in
        list-buckets)
            arguments=""
            export GOOGLE_CLOUD_PROJECT="${PROJECT_ID}"
            ;;
        list-buckets-for-project)
            arguments="${PROJECT_ID}"
            ;;
        insert-object)
            arguments="${base_arguments} a-short-string-to-put-in-the-object"
            ;;
        write-object)
            arguments="${base_arguments} 100000"
            ;;
        create-object-acl)
            arguments="${base_arguments} allAuthenticatedUsers READER"
            ;;
        get-object-acl)
            arguments="${base_arguments} allAuthenticatedUsers"
            ;;
        update-object-acl)
            arguments="${base_arguments} allAuthenticatedUsers OWNER"
            ;;
        delete-object-acl)
            arguments="${base_arguments} allAuthenticatedUsers"
            ;;
        *)
            arguments="${base_arguments}"
            ;;
    esac
    echo ${program_path} ${example} ${arguments} >"${log}"
    set +e
    ${program_path} ${example} ${arguments} >>"${log}" 2>&1 </dev/null
    if [ $? = 0 ]; then
      echo  "${COLOR_GREEN}[       OK ]${COLOR_RESET}" \
          "${program_name} ${example}"
      continue
    else
      EXIT_STATUS=1
      echo    "${COLOR_RED}[    ERROR ]${COLOR_RESET}" \
          "${program_name} ${example}"
      echo
      echo "================ [begin ${log}] ================"
      cat "${log}"
      echo "================ [end ${log}] ================"
      if [ -f "testbench.log" ]; then
        echo "================ [begin testbench.log ================"
        cat "testbench.log"
        echo "================ [end testbench.log ================"
      fi
    fi
    set -e
  done

  echo "${COLOR_GREEN}[ RUN      ]${COLOR_RESET}" \
      "${program_name} (no command) running"
  set +e
  ${program_path} >"${log}" 2>&1 </dev/null
  # Note the inverted test, this is supposed to exit with 1.
  if [ $? != 0 ]; then
    echo "${COLOR_GREEN}[       OK ]${COLOR_RESET}" \
        "${program_name}" "(no command)"
  else
    EXIT_STATUS=1
    echo   "${COLOR_RED}[    ERROR ]${COLOR_RESET}" \
        "${program_name}" "(no command)"
    echo
    echo "================ [begin ${log}] ================"
    cat "${log}"
    echo "================ [end ${log}] ================"
  fi
  set -e
  /bin/rm -f "${log}"
}

################################################
# Run all Bucket examples.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   bucket_name: the name of the bucket to run the examples against.
# Returns:
#   None
################################################
run_all_bucket_examples() {
  local bucket_name=$1
  shift

  # The list of commands in the storage_bucket_samples program that we will
  # test. Currently get-metadata assumes that $bucket_name is already created.
  readonly BUCKET_EXAMPLES_COMMANDS=$(tr '\n' ',' <<_EOF_
list-buckets
list-buckets-for-project
get-bucket-metadata
list-objects
_EOF_
)

  run_program_examples ./storage_bucket_samples \
      "${BUCKET_EXAMPLES_COMMANDS}" \
      "${bucket_name}"
}

################################################
# Run all Object examples.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   bucket_name: the name of the bucket to run the examples against.
# Returns:
#   None
################################################
run_all_object_examples() {
  local bucket_name=$1
  shift

  # The list of commands in the storage_bucket_samples program that we will
  # test. Currently get-metadata assumes that $bucket_name is already created.
  readonly OBJECT_EXAMPLES_COMMANDS=$(tr '\n' ',' <<_EOF_
insert-object
get-object-metadata
read-object
write-object
delete-object
_EOF_
)

  local object_name="object-$(date +%s)"

  run_program_examples ./storage_object_samples \
      "${OBJECT_EXAMPLES_COMMANDS}" \
      "${bucket_name}" \
      "${object_name}"
}

################################################
# Run all Object examples.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   bucket_name: the name of the bucket to run the examples against.
# Returns:
#   None
################################################
run_all_object_acl_examples() {
  local bucket_name=$1
  shift

  # The list of commands in the storage_bucket_samples program that we will
  # test. Currently get-metadata assumes that $bucket_name is already created.
  readonly OBJECT_ACL_COMMANDS=$(tr '\n' ',' <<_EOF_
list-object-acl
create-object-acl
get-object-acl
update-object-acl
delete-object-acl
_EOF_
)

  # First create an object for the example:
  local object_name="object-$(date +%s)"
  if [ ! -x ./storage_object_samples ]; then
    echo "${COLOR_YELLOW}[  SKIPPED ]${COLOR_RESET}" \
        " storage_object_samples is not compiled"
    return
  fi
  log="$(mktemp -t "storage_object_acl_setup.XXXXXX")"
  set +e
  ./storage_object_samples insert-object \
      "${bucket_name}" \
      "${object_name}" \
      "some-contents-does-not-matter-what" >${log} 2>&1
  if [ $? != 0 ]; then
    EXIT_STATUS=1
    echo   "${COLOR_RED}[    ERROR ]${COLOR_RESET}" \
        " cannot create test object"
    echo "================ [begin ${log}] ================"
    cat "${log}"
    echo "================ [end ${log}] ================"
    set -e
    return
  fi
  set -e

  run_program_examples ./storage_object_acl_samples \
      "${OBJECT_ACL_COMMANDS}" \
      "${bucket_name}" \
      "${object_name}"

  ./storage_object_samples delete-object \
      "${bucket_name}" \
      "${object_name}" >${log} 2>&1
  if [ $? != 0 ]; then
    EXIT_STATUS=1
    echo   "${COLOR_RED}[    ERROR ]${COLOR_RESET}" \
        " cannot delete test object"
    echo "================ [begin ${log}] ================"
    cat "${log}"
    echo "================ [end ${log}] ================"
  fi
  set -e
}

################################################
# Run all the examples.
# Globals:
#   BUCKET_NAME: the name of the bucket to use in the examples.
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   None
# Returns:
#   None
################################################
run_all_storage_examples() {
  echo "${COLOR_GREEN}[ ======== ]${COLOR_RESET}" \
      " Running Google Cloud Storage Examples"
  run_all_bucket_examples "${BUCKET_NAME}"
  run_all_object_examples "${BUCKET_NAME}"
  run_all_object_acl_examples "${BUCKET_NAME}"
  echo "${COLOR_GREEN}[ ======== ]${COLOR_RESET}" \
      " Google Cloud Storage Examples Finished"
  if [ "${EXIT_STATUS}" = "0" ]; then
    TESTBENCH_DUMP_LOG=no
  fi
  exit ${EXIT_STATUS}
}
