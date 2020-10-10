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

"""Utils related to generation and pre-condition"""

import grpc
import utils

# === EXTRACT INFORMATION === #


def extract_precondition(request, is_meta, is_source, context):
    match_field, not_match_field = "", ""
    if is_meta:
        match_field = (
            "ifMetagenerationMatch" if not is_source else "ifSourceMetagenerationMatch"
        )
        not_match_field = (
            "ifMetagenerationNotMatch"
            if not is_source
            else "ifSourceMetagenerationNotMatch"
        )
    else:
        match_field = (
            "ifGenerationMatch" if not is_source else "ifSourceGenerationMatch"
        )
        not_match_field = (
            "ifGenerationNotMatch" if not is_source else "ifSourceGenerationNotMatch"
        )
    match, not_match = None, None
    if context is not None:
        match_field = utils.common.to_snake_case(match_field)
        not_match_field = utils.common.to_snake_case(not_match_field)
        match = (
            getattr(request, match_field, None).value
            if request.HasField(match_field)
            else None
        )
        not_match = (
            getattr(request, not_match_field, None).value
            if request.HasField(not_match_field)
            else None
        )
    else:
        match = (
            int(request.args.get(match_field)) if match_field in request.args else None
        )
        not_match = (
            int(request.args.get(not_match_field))
            if not_match_field in request.args
            else None
        )
    return match, not_match


def extract_generation(request, is_source, context):
    if context is not None:
        extract_field = "generation" if not is_source else "source_generation"
        return getattr(request, extract_field, 0)
    else:
        extract_field = "generation" if not is_source else "sourceGeneration"
        return int(request.args.get(extract_field, 0))


# === CHECK === #


def check_precondition(generation, match, not_match, is_meta, context):
    msg = "generation" if not is_meta else "metageneration"
    if generation is not None and not_match is not None and not_match == generation:
        utils.error.generic(
            "Precondition Failed (%s = %d vs %s_not_match = %d)"
            % (msg, generation, msg, not_match),
            412,
            grpc.StatusCode.FAILED_PRECONDITION,
            context,
        )
    if generation is not None and match is not None and match != generation:
        utils.error.generic(
            "Precondition Failed (%s = %d vs %s_match = %d)"
            % (msg, generation, msg, match),
            412,
            grpc.StatusCode.FAILED_PRECONDITION,
            context,
        )
