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

"""Common utils"""

import json
import random
import re
import types

import scalpl
import utils

re_remove_index = re.compile(r"\[\d+\]+|^[0-9]+")
content_range_split = re.compile(r"bytes (\*|[0-9]+-[0-9]+)\/(\*|[0-9]+)")

# === STR === #


re_snake_case = re.compile(r"(?<!^)(?=[A-Z])")


def to_snake_case(string):
    return re_snake_case.sub("_", string).lower()


def remove_index(string):
    return re_remove_index.sub("", string)


# === FAKE REQUEST === #


class FakeRequest(types.SimpleNamespace):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)

    @classmethod
    def init_xml(cls, request):
        headers = {
            key.lower(): value
            for key, value in request.headers.items()
            if key.lower().startswith("x-goog-") or key.lower() == "range"
        }
        args = request.args.to_dict()
        args.update(cls.xml_headers_to_json_args(headers))
        return cls(args=args, headers=headers)

    @classmethod
    def xml_headers_to_json_args(cls, headers):
        field_map = {
            "x-goog-if-generation-match": "ifGenerationMatch",
            "x-goog-if-meta-generation-match": "ifMetagenerationMatch",
            "x-goog-acl": "predefinedAcl",
        }
        args = {}
        for field_xml, field_json in field_map.items():
            if field_xml in headers:
                args[field_json] = headers[field_xml]
        return args


# === REST === #


def nested_key(data):
    # This function take a dict and return a list of keys that works with `Scalpl` library.
    if isinstance(data, list):
        keys = []
        for i in range(len(data)):
            result = nested_key(data[i])
            if isinstance(result, list):
                if isinstance(data[i], dict):
                    keys.extend(["[%d].%s" % (i, item) for item in result])
                elif isinstance(data[i], list):
                    keys.extend(["[%d]%s" % (i, item) for item in result])
            elif result == "":
                keys.append("[%d]" % i)
        return keys
    elif isinstance(data, dict):
        keys = []
        for key, value in data.items():
            result = nested_key(value)
            if isinstance(result, list):
                if isinstance(value, dict):
                    keys.extend(["%s.%s" % (key, item) for item in result])
                elif isinstance(value, list):
                    keys.extend(["%s%s" % (key, item) for item in result])
            elif result == "":
                keys.append("%s" % key)
        return keys
    else:
        return ""


def parse_fields(fields):
    # "kind,items(id,name)" -> ["kind", "items.id", "items.name"]
    res = []
    for i, c in enumerate(fields):
        if c != " " and c != ")":
            if c == "/":
                res.append(".")
            else:
                res.append(c)
        elif c == ")":
            childrens_fields = []
            tmp_field = []
            while res:
                if res[-1] != "," and res[-1] != "(":
                    tmp_field.append(res.pop())
                else:
                    childrens_fields.append(tmp_field)
                    tmp_field = []
                    if res.pop() == "(":
                        break
            parent_field = []
            while res and res[-1] != "," and res[-1] != "(":
                parent_field.append(res.pop())
            for i, field in enumerate(childrens_fields):
                res.extend(parent_field[::-1])
                res.append(".")
                while field:
                    res.append(field.pop())
                if i < len(childrens_fields) - 1:
                    res.append(",")
    return "".join(res).split(",")


def filter_response_rest(response, projection, fields):
    if fields is not None:
        fields = parse_fields(fields)
    deleted_keys = set()
    for key in nested_key(response):
        simplfied_key = remove_index(key)
        maybe_delete = True
        if projection == "noAcl":
            maybe_delete = False
            if simplfied_key.startswith("owner"):
                deleted_keys.add("owner")
            elif simplfied_key.startswith("items.owner"):
                deleted_keys.add(key[0 : key.find("owner") + len("owner")])
            elif simplfied_key.startswith("acl"):
                deleted_keys.add("acl")
            elif simplfied_key.startswith("items.acl"):
                deleted_keys.add(key[0 : key.find("acl") + len("acl")])
            elif simplfied_key.startswith("defaultObjectAcl"):
                deleted_keys.add("defaultObjectAcl")
            elif simplfied_key.startswith("items.defaultObjectAcl"):
                deleted_keys.add(
                    key[0 : key.find("defaultObjectAcl") + len("defaultObjectAcl")]
                )
            else:
                maybe_delete = True
        if fields is not None:
            if maybe_delete:
                for field in fields:
                    if field != "" and simplfied_key.startswith(field):
                        maybe_delete = False
                        break
                if maybe_delete:
                    deleted_keys.add(key)
    proxy = scalpl.Cut(response)
    for key in deleted_keys:
        del proxy[key]
    return proxy.data


def parse_multipart(request):
    content_type = request.headers.get("content-type")
    if content_type is None or not content_type.startswith("multipart/related"):
        utils.error.invalid("Content-type header in multipart upload", None)
    _, _, boundary = content_type.partition("boundary=")
    if boundary is None:
        utils.error.missing("boundary in content-type header in multipart upload", None)

    def parse_part(part):
        result = part.split(b"\r\n")
        if result[0] != b"" and result[-1] != b"":
            utils.error.invalid("Multipart %s" % str(part), None)
        result = list(filter(None, result))
        headers = {}
        if len(result) < 2:
            result.append(b"")
        for header in result[:-1]:
            key, value = header.split(b": ")
            headers[key.decode("utf-8")] = value.decode("utf-8")
        return headers, result[-1]

    boundary = boundary.encode("utf-8")
    body = extract_media(request)
    parts = body.split(b"--" + boundary)
    if parts[-1] != b"--\r\n":
        utils.error.missing("end marker (--%s--) in media body" % boundary, None)
    _, resource = parse_part(parts[1])
    metadata = json.loads(resource)
    media_headers, media = parse_part(parts[2])
    return metadata, media_headers, media


def extract_media(request):
    """Extract the media from a flask Request.

    To avoid race conditions when using greenlets we cannot perform I/O in the
    constructor of GcsObject, or in any of the operations that modify the state
    of the service.  Because sometimes the media is uploaded with chunked encoding,
    we need to do I/O before finishing the GcsObject creation. If we do this I/O
    after the GcsObject creation started, the the state of the application may change
    due to other I/O.

    :param request:flask.Request the HTTP request.
    :return: the full media of the request.
    :rtype: str
    """
    if request.environ.get("HTTP_TRANSFER_ENCODING", "") == "chunked":
        return request.environ.get("wsgi.input").read()
    return request.data


# === RESPONSE === #


def extract_projection(request, default, context):
    if context is not None:
        return request.projection if request.projection != 0 else default
    else:
        projection_map = ["noAcl", "full"]
        projection = request.args.get("projection")
        return (
            projection if projection in projection_map else projection_map[default - 1]
        )


# === DATA === #


def corrupt_media(media):
    if not media:
        return bytearray(random.sample("abcdefghijklmnopqrstuvwxyz", 1), "utf-8")
    return b"B" + media[1:] if media[0:1] == b"A" else b"A" + media[1:]
