#!/usr/bin/env python
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
"""Standalone helpers for the Google Cloud Storage test bench."""

import base64
import error_response
import hashlib
import json


def canonical_entity_name(entity):
    """Convert entity names to their canonical form.

    Some entities (notably project-<team>-) have more than one name, for
    example the project-owners-<project_id> entities are called
    project-owners-<project_number> internally.  This function
    :param entity:str convert this entity to its canonical name.
    :return: the name in canonical form.
    :rtype:str
    """
    if entity == 'allUsers' or entity == 'allAuthenticatedUsers':
        return entity
    if entity.startswith('project-owners-'):
        entity = 'project-owners-123456789'
    if entity.startswith('project-editors-'):
        entity = 'project-editors-123456789'
    if entity.startswith('project-viewers-'):
        entity = 'project-viewers-123456789'
    return entity.lower()


def index_acl(acl):
    """Return a ACL as a dictionary indexed by the 'entity' values of the ACL.

    We represent ACLs as lists of dictionaries, that makes it easy to convert
    them to JSON objects. When changing them though, we need to make sure there
    is a single element in the list for each `entity` value, so it is convenient
    to convert the list to a dictionary (indexed by `entity`) of dictionaries.
    This function performs that conversion.

    :param acl:list of dict
    :return: the ACL indexed by the entity of each entry.
    :rtype:dict
    """
    # This can be expressed by a comprehension but turns out to be less
    # readable in that form.
    indexed = dict()
    for e in acl:
        indexed[e['entity']] = e
    return indexed


def filtered_response(request, response):
    """Format the response as a JSON string, using any filtering included in
    the request.

    :param request:flask.Request the original HTTP request.
    :param response:dict a dictionary to be formatted as a JSON string.
    :return: the response formatted as a string.
    :rtype:str
    """
    fields = request.args.get('fields')
    if fields is None:
        return json.dumps(response)
    tmp = {}
    # TODO(#1037) - support full filter expressions
    for key in fields.split(','):
        if key in response:
            tmp[key] = response[key]
    return json.dumps(tmp)


def raise_csek_error(code=400):
    msg = 'Missing a SHA256 hash of the encryption key, or it is not'
    msg += ' base64 encoded, or it does not match the encryption key.'
    link = 'https://cloud.google.com/storage/docs/encryption#customer-supplied_encryption_keys'
    error = {
        "error": {
            "errors": [{
                "domain": "global",
                "reason": "customerEncryptionKeySha256IsInvalid",
                "message": msg,
                "extendedHelp": link,
            }],
            "code":
            code,
            "message":
            msg,
        }
    }
    raise error_response.ErrorResponse(json.dumps(error), status_code=code)


def validate_customer_encryption_headers(key_header_value, hash_header_value,
                                         algo_header_value):
    """Verify that the encryption headers are internally consistent.

    :param key_header_value: str the value of the x-goog-*-key header
    :param hash_header_value: str the value of the x-goog-*-key-sha256 header
    :param algo_header_value: str the value of the x-goog-*-key-algorithm header
    :rtype: NoneType
    """
    try:
        if algo_header_value is None or algo_header_value != 'AES256':
            raise error_response.ErrorResponse(
                'Invalid or missing algorithm %s for CSEK' % algo_header_value,
                status_code=400)

        key = base64.standard_b64decode(key_header_value)
        if key is None or len(key) != 256 / 8:
            raise_csek_error()

        h = hashlib.sha256()
        h.update(key)
        expected = base64.standard_b64encode(h.digest())
        if hash_header_value is None or expected != hash_header_value:
            raise_csek_error()
    except error_response.ErrorResponse:
        # error_response.ErrorResponse indicates that the request was invalid, just pass
        # that exception through.
        raise
    except Exception:
        # Many of the functions above may raise, convert those to an
        # error_response.ErrorResponse with the right format.
        raise_csek_error()


def json_api_patch(original, patch, recurse_on=set({})):
    """Patch a dictionary using the JSON API semantics.

    Patches are applied using the following algorithm:
    - patch is a dictionary representing a JSON object. JSON `null` values are
      represented by None).
    - For fields that are not in `recursive_fields`:
      - If patch contains {field: None} the field is erased from `original`.
      - Otherwise `patch[field]` replaces `original[field]`.
    - For fields that are in `recursive_fields`:
      - If patch contains {field: None} the field is erased from `original`.
      - If patch contains {field: {}} the field is left untouched in `original`,
        note that if the field does not exist in original this means it is not
        created.
      - Otherwise patch[field] is treated as a patch and applied to
        `original[field]`, potentially creating the new field.

    :param original:dict the dictionary to patch
    :param patch:dict the patch to apply. Elements pointing to None are removed,
        other elements are replaced.
    :param recurse_on:set of strings, the names of fields for which the patch
        is applied recursively.
    :return: the updated dictionary
    :rtype:dict
    """
    tmp = original.copy()
    for key, value in patch.iteritems():
        if value is None:
            tmp.pop(key, None)
        elif key not in recurse_on:
            tmp[key] = value
        elif len(value) != 0:
            tmp[key] = json_api_patch(original.get(key, {}), value)
    return tmp


def extract_media(request):
    """Extract the media from a flask Request.

    To avoid race conditions when using greenlets we cannot perform I/O in the
    constructor of GcsObjectVersion, or in any of the operations that modify
    the state of the service.  Because sometimes the media is uploaded with
    chunked encoding, we need to do I/O before finishing the GcsObjectVersion
    creation. If we do this I/O after the GcsObjectVersion creation started,
    the the state of the application may change due to other I/O.

    :param request:flask.Request the HTTP request.
    :return: the full media of the request.
    :rtype: str
    """
    if request.environ.get('HTTP_TRANSFER_ENCODING', '') == 'chunked':
        return request.environ.get('wsgi.input').read()
    return request.data
