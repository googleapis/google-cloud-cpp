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

"""Utils to raise error code and abort the server"""

import flask
import grpc
import simdjson


def generic(msg, rest_code, grpc_code, context):
    if context is not None:
        context.abort(grpc_code, msg)
    else:
        flask.abort(flask.make_response(flask.jsonify(msg), rest_code))


def csek(context, rest_code=400, grpc_code=grpc.StatusCode.INVALID_ARGUMENT):
    msg = "Missing a SHA256 hash of the encryption key, or it is not"
    msg += " base64 encoded, or it does not match the encryption key."
    link = "https://cloud.google.com/storage/docs/encryption#customer-supplied_encryption_keys"
    error_msg = {
        "error": {
            "errors": [
                {
                    "domain": "global",
                    "reason": "customerEncryptionKeySha256IsInvalid",
                    "message": msg,
                    "extendedHelp": link,
                }
            ],
            "code": rest_code,
            "message": msg,
        }
    }
    generic(simdjson.dumps(error_msg), rest_code, grpc_code, context)


def invalid(msg, context, rest_code=400, grpc_code=grpc.StatusCode.INVALID_ARGUMENT):
    msg = "%s is invalid." % msg
    generic(msg, rest_code, grpc_code, context)


def mismatch(
    msg,
    expect,
    actual,
    context,
    rest_code=412,
    grpc_code=grpc.StatusCode.FAILED_PRECONDITION,
):
    msg = "%s validation failed. Expected = %s vs Actual = %s." % (
        msg,
        str(expect),
        str(actual),
    )
    generic(msg, rest_code, grpc_code, context)


def missing(name, context, rest_code=400, grpc_code=grpc.StatusCode.INVALID_ARGUMENT):
    msg = "Missing %s." % name
    generic(msg, rest_code, grpc_code, context)


def notfound(name, context, rest_code=404, grpc_code=grpc.StatusCode.NOT_FOUND):
    msg = "%s does not exist." % name
    generic(msg, rest_code, grpc_code, context)
