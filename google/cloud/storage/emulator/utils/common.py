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

re_remove_index = re.compile(r"\[\d+\]+|^[0-9]+")

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
                deleted_keys.add("items.owner")
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
                    if simplfied_key.startswith(field):
                        maybe_delete = False
                        break
                if maybe_delete:
                    deleted_keys.add(key)
    proxy = scalpl.Cut(response)
    print("delete: ", deleted_keys)
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
