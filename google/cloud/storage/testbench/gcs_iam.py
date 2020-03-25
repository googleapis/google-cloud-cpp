#!/usr/bin/env python
# Copyright 2019 Google Inc.
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
"""Simulate IAM operations."""

import base64
import error_response
import flask
import json

IAM_HANDLER_PATH = '/iamapi'
iam = flask.Flask(__name__)
iam.debug = True


@iam.route('/projects/-/serviceAccounts/<service_account>:signBlob',
           methods=['POST'])
def sign_blob(service_account):
    """Implement the `projects.serviceAccounts.signBlob` API."""
    payload = json.loads(flask.request.data)
    if payload.get('payload') is None:
        raise error_response.ErrorResponse(
            'Missing payload in the payload', status_code=400)
    try:
        blob = base64.b64decode(payload.get('payload'))
    except TypeError:
        raise error_response.ErrorResponse(
            'payload must be base64-encoded', status_code=400)
    blob = b'signed: ' + blob
    response = {
        'keyId': 'fake-key-id-123',
        'signedBlob': base64.b64encode(blob).decode('utf-8')
    }
    return json.dumps(response)


def get_iam_app():
    return IAM_HANDLER_PATH, iam
