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
import random
import re


def validate_bucket_name(bucket_name):
    """Return True if bucket_name is a valid bucket name.

    Bucket naming requirements are described in:

    https://cloud.google.com/storage/docs/naming

    Note that this function does not verify domain bucket names:

    https://cloud.google.com/storage/docs/domain-name-verification

    :param bucket_name:str the name to validate.
    :rtype: bool
    """
    valid = True
    if "." in bucket_name:
        valid &= len(bucket_name) <= 222
        valid &= all([len(part) <= 63 for part in bucket_name.split(".")])
    else:
        valid &= len(bucket_name) <= 63
    valid &= re.match("^[a-z0-9][a-z0-9._\\-]+[a-z0-9]$", bucket_name) is not None
    valid &= not bucket_name.startswith("goog")
    valid &= re.search("g[0o][0o]g[1l][e3]", bucket_name) is None
    valid &= (
        re.match("^[0-9]{1,3}[.][0-9]{1,3}[.][0-9]{1,3}[.][0-9]{1,3}$", bucket_name)
        is None
    )
    return valid


def canonical_entity_name(entity):
    """Convert entity names to their canonical form.

    Some entities (notably project-<team>-) have more than one name, for
    example the project-owners-<project_id> entities are called
    project-owners-<project_number> internally.  This function
    :param entity:str convert this entity to its canonical name.
    :return: the name in canonical form.
    :rtype:str
    """
    if entity == "allUsers" or entity == "allAuthenticatedUsers":
        return entity
    if entity.startswith("project-owners-"):
        entity = "project-owners-123456789"
    if entity.startswith("project-editors-"):
        entity = "project-editors-123456789"
    if entity.startswith("project-viewers-"):
        entity = "project-viewers-123456789"
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
        indexed[e["entity"]] = e
    return indexed


def filter_fields_from_response(fields, response):
    """Format the response as a JSON string, using any filtering included in
    the request.

    :param fields:str the value of the `fields` parameter in the original
        request.
    :param response:dict a dictionary to be formatted as a JSON string.
    :return: the response formatted as a string.
    :rtype:str
    """
    if fields is None:
        return json.dumps(response)
    tmp = {}
    # TODO(#1037) - support full filter expressions
    for key in fields.split(","):
        if key in response:
            tmp[key] = response[key]
    return json.dumps(tmp)


def filtered_response(request, response):
    """Format the response as a JSON string, using any filtering included in
    the request.

    :param request:flask.Request the original HTTP request.
    :param response:dict a dictionary to be formatted as a JSON string.
    :return: the response formatted as a string.
    :rtype:str
    """
    fields = request.args.get("fields")
    return filter_fields_from_response(fields, response)


def raise_csek_error(code=400):
    msg = "Missing a SHA256 hash of the encryption key, or it is not"
    msg += " base64 encoded, or it does not match the encryption key."
    link = "https://cloud.google.com/storage/docs/encryption#customer-supplied_encryption_keys"
    error = {
        "error": {
            "errors": [
                {
                    "domain": "global",
                    "reason": "customerEncryptionKeySha256IsInvalid",
                    "message": msg,
                    "extendedHelp": link,
                }
            ],
            "code": code,
            "message": msg,
        }
    }
    raise error_response.ErrorResponse(json.dumps(error), status_code=code)


def validate_customer_encryption_headers(
    key_header_value, hash_header_value, algo_header_value
):
    """Verify that the encryption headers are internally consistent.

    :param key_header_value: str the value of the x-goog-*-key header
    :param hash_header_value: str the value of the x-goog-*-key-sha256 header
    :param algo_header_value: str the value of the x-goog-*-key-algorithm header
    :rtype: NoneType
    """
    try:
        if algo_header_value is None or algo_header_value != "AES256":
            raise error_response.ErrorResponse(
                "Invalid or missing algorithm %s for CSEK" % algo_header_value,
                status_code=400,
            )

        key = base64.standard_b64decode(key_header_value)
        if key is None or len(key) != 256 / 8:
            raise_csek_error()

        h = hashlib.sha256()
        h.update(key)
        expected = base64.standard_b64encode(h.digest()).decode("utf-8")
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
    for key, value in patch.items():
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
    if request.environ.get("HTTP_TRANSFER_ENCODING", "") == "chunked":
        return request.environ.get("wsgi.input").read()
    return request.data


def corrupt_media(media):
    """Return a randomly modified version of a string.

    :param media:bytes a string (typically some object media) to be modified.
    :return: a string that is slightly different than media.
    :rtype: str
    """
    # Deal with the boundary condition.
    if not media:
        return bytearray(random.sample("abcdefghijklmnopqrstuvwxyz", 1), "utf-8")
    return b"B" + media[1:] if media[0:1] == b"A" else b"A" + media[1:]


# Define the collection of Buckets indexed by <bucket_name>
GCS_BUCKETS = dict()


def lookup_bucket(bucket_name):
    """Lookup a bucket by name in the global collection.

    :param bucket_name:str the name of the Bucket.
    :return: the bucket matching the name.
    :rtype:GcsBucket
    :raises:ErrorResponse if the bucket is not found.
    """
    bucket = GCS_BUCKETS.get(bucket_name)
    if bucket is None:
        raise error_response.ErrorResponse(
            "Bucket %s not found" % bucket_name, status_code=404
        )
    return bucket


def has_bucket(bucket_name):
    """Return True if the bucket already exists in the global collection."""
    return GCS_BUCKETS.get(bucket_name) is not None


def insert_bucket(bucket_name, bucket):
    """Insert (or replace) a new bucket into the global collection.

    :param bucket_name:str the name of the bucket.
    :param bucket:GcsBucket the bucket to insert.
    """
    GCS_BUCKETS[bucket_name] = bucket


def delete_bucket(bucket_name):
    """Delete a bucket from the global collection."""
    GCS_BUCKETS.pop(bucket_name)


def all_buckets():
    """Return a key,value iterator for all the buckets in the global collection.

    :rtype:dict[str, GcsBucket]
    """
    return GCS_BUCKETS.items()


# Define the collection of GcsObjects indexed by <bucket_name>/o/<object_name>
GCS_OBJECTS = dict()


def lookup_object(bucket_name, object_name):
    """Lookup an object by name in the global collection.

    :param bucket_name:str the name of the Bucket that contains the object.
    :param object_name:str the name of the Object.
    :return: tuple the object path and the object.
    :rtype: (str,GcsObject)
    :raises:ErrorResponse if the object is not found.
    """
    object_path, gcs_object = get_object(bucket_name, object_name, None)
    if gcs_object is None:
        raise error_response.ErrorResponse(
            "Object %s in %s not found" % (object_name, bucket_name), status_code=404
        )
    return object_path, gcs_object


def get_object(bucket_name, object_name, default_value):
    """Find an object in the global collection, return a default value if not
    found.

    :param bucket_name:str the name of the Bucket that contains the object.
    :param object_name:str the name of the Object.
    :param default_value:GcsObject the default value returned if the object is
        not found.
    :return: tuple the object path and the object.
    :rtype: (str,GcsObject)
    """
    object_path = bucket_name + "/o/" + object_name
    return object_path, GCS_OBJECTS.get(object_path, default_value)


def insert_object(object_path, value):
    """Insert an object to the global collection."""
    GCS_OBJECTS[object_path] = value


def delete_object(object_path):
    """Delete an object from the global collection."""
    GCS_OBJECTS.pop(object_path)


def all_objects():
    """Return a key,value iterator for all the objects in the global collection.

    :rtype:dict[str, GcsBucket]
    """
    return GCS_OBJECTS.items()


def parse_part(multipart_upload_part):
    """Parse a portion of a multipart breaking out the headers and payload.

    :param multipart_upload_part:str a portion of the multipart upload body.
    :return: a tuple with the headers and the payload.
    :rtype: (dict, str)
    """
    headers = dict()
    index = 0
    next_line = multipart_upload_part.find(b"\r\n", index)
    while next_line != index:
        header_line = multipart_upload_part[index:next_line]
        key, value = header_line.split(b": ", 2)
        # This does not work for repeated headers, but we do not expect
        # those in the testbench.
        headers[key.decode("utf-8")] = value.decode("utf-8")
        index = next_line + 2
        next_line = multipart_upload_part.find(b"\r\n", index)
    return headers, multipart_upload_part[next_line + 2 :]


def parse_multi_part(request):
    """Parse a multi-part request

    :param request:flask.Request multipart request.
    :return: a tuple with the resource, media_headers and the media_body.
    :rtype: (dict, dict, str)
    """
    content_type = request.headers.get("content-type")
    if content_type is None or not content_type.startswith("multipart/related"):
        raise error_response.ErrorResponse(
            "Missing or invalid content-type header in multipart upload"
        )
    _, _, boundary = content_type.partition("boundary=")
    if boundary is None:
        raise error_response.ErrorResponse(
            "Missing boundary (%s) in content-type header in multipart upload"
            % boundary
        )

    boundary = bytearray(boundary, "utf-8")
    marker = b"--" + boundary + b"\r\n"
    body = extract_media(request)
    parts = body.split(marker)
    # parts[0] is the empty string, `multipart` should start with the boundary
    # parts[1] is the JSON resource object part, with some headers
    resource_headers, resource_body = parse_part(parts[1])
    # parts[2] is the media, with some headers
    media_headers, media_body = parse_part(parts[2])
    end = media_body.find(b"\r\n--" + boundary + b"--\r\n")
    if end == -1:
        raise error_response.ErrorResponse(
            "Missing end marker (--%s--) in media body" % boundary
        )
    media_body = media_body[:end]
    resource = json.loads(resource_body)

    return resource, media_headers, media_body
