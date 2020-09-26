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

import re
import scalpl
import types

re_remove_index = re.compile(r":[0-9]+|^[0-9]+")
re_split_fields = re.compile(r"[a-zA-Z0-9]*\(.*\)|[^,]+")

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
    result = []
    for field in re_split_fields.findall(fields):
        if "(" not in field and ")" not in field:
            result.append(field)
        else:
            prefix = field[0 : field.find("(")]
            subfields = field[len(prefix) + 1 : -1]
            items = parse_fields(subfields)
            for item in items:
                result.append(prefix + "." + item)
    return result


def filter_response_rest(response, projection, fields):
    if fields is not None:
        fields.replace(" ", "")
        fields.replace("/", ".")
        fields = parse_fields(fields)
    deleted_keys = set()
    for key in nested_key(response):
        simplfied_key = remove_index(key)
        if projection == "noAcl":
            if simplfied_key == "owner" or simplfied_key == "items.owner":
                deleted_keys.add(key)
            if simplfied_key == "acl" or simplfied_key == "items.acl":
                deleted_keys.add(key)
            if (
                simplfied_key == "defaultObjectAcl"
                or simplfied_key == "items.defaultObjectAcl"
            ):
                deleted_keys.add(key)
        if fields is not None:
            if simplfied_key not in fields:
                deleted_keys.add(key)
    proxy = scalpl.Cut(response)
    for key in deleted_keys:
        del proxy[key]
    return proxy.data


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
