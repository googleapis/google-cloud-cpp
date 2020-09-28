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

"""Implement a class to simulate GCS buckets."""

import datetime
import hashlib
import re

import scalpl
import simdjson

import utils
from google.cloud.storage_v1.proto import storage_resources_pb2 as resources_pb2
from google.iam.v1 import policy_pb2
from google.protobuf import field_mask_pb2, json_format


class Bucket:
    modifiable_fields = [
        "acl",
        "default_object_acl",
        "lifecycle",
        "cors",
        "storage_class",
        "default_event_based_hold",
        "labels",
        "website",
        "versioning",
        "logging",
        "encryption",
        "billing",
        "retention_policy",
        "location_type",
        "iam_configuration",
    ]

    def __init__(self, metadata, notifications, iam_policy):
        self.metadata = metadata
        self.notifications = notifications
        self.iam_policy = iam_policy

    @classmethod
    def __validate_bucket_name(cls, bucket_name, context):
        valid = True
        if "." in bucket_name:
            valid &= len(bucket_name) <= 222
            valid &= all([len(part) <= 63 for part in bucket_name.split(".")])
        else:
            valid &= len(bucket_name) <= 63
            valid &= (
                re.match("^[a-z0-9][a-z0-9._\\-]+[a-z0-9]$", bucket_name) is not None
            )
            valid &= not bucket_name.startswith("goog")
            valid &= re.search("g[0o][0o]g[1l][e3]", bucket_name) is None
        if not valid:
            utils.error.invalid("Bucket name %s" % bucket_name, context)

    @classmethod
    def __preprocess_rest(cls, data):
        proxy = scalpl.Cut(data)
        keys = utils.common.nested_key(data)
        proxy.pop("iamConfiguration.bucketPolicyOnly", None)
        for key in keys:
            if key.endswith("createdBefore"):
                proxy[key] = proxy[key] + "T00:00:00Z"
        return proxy.data

    @classmethod
    def __postprocess_rest(cls, data):
        proxy = scalpl.Cut(data)
        keys = utils.common.nested_key(data)
        for key in keys:
            if key.endswith("createdBefore"):
                proxy[key] = proxy[key].replace("T00:00:00Z", "")
        proxy["kind"] = "storage#bucket"
        return proxy.data

    @classmethod
    def __insert_predefined_acl(cls, metadata, predefined_acl, context):
        if predefined_acl == "" and predefined_acl == 0:
            return
        if metadata.iam_configuration.uniform_bucket_level_access.enabled:
            utils.error.invalid(
                "Predefined ACL with uniform bucket level access enabled", context
            )
        acls = utils.acl.compute_predefined_bucket_acl(
            metadata.name, predefined_acl, context
        )
        del metadata.acl[:]
        metadata.acl.extend(acls)

    @classmethod
    def __insert_predefined_default_object_acl(
        cls, metadata, predefined_default_object_acl, context
    ):
        if predefined_default_object_acl == "" and predefined_default_object_acl == 0:
            return
        if metadata.iam_configuration.uniform_bucket_level_access.enabled:
            utils.error.invalid(
                "Predefined Default Object ACL with uniform bucket level access enabled",
                context,
            )
        acls = utils.acl.compute_predefined_default_object_acl(
            metadata.name, predefined_default_object_acl, context
        )
        del metadata.default_object_acl[:]
        metadata.default_object_acl.extend(acls)

    # === INITIALIZATION === #

    @classmethod
    def init(cls, request, context):
        time_created = datetime.datetime.now()
        metadata = None
        if context is not None:
            metadata = request.bucket
        else:
            metadata = json_format.ParseDict(
                cls.__preprocess_rest(simdjson.loads(request.data)),
                resources_pb2.Bucket(),
            )
        cls.__validate_bucket_name(metadata.name, context)
        default_projection = 1
        if len(metadata.acl) != 0 or len(metadata.default_object_acl) != 0:
            default_projection = 2
        if len(metadata.acl) == 0:
            predefined_acl = utils.acl.extract_predefined_acl(request, False, context)
            if predefined_acl == 0:
                predefined_acl = 3
            elif predefined_acl == "":
                predefined_acl = "projectPrivate"
            cls.__insert_predefined_acl(metadata, predefined_acl, context)
        if len(metadata.default_object_acl) == 0:
            predefined_default_object_acl = utils.acl.extract_predefined_default_object_acl(
                request, context
            )
            if predefined_default_object_acl == 0:
                predefined_default_object_acl = 5
            elif predefined_default_object_acl == "":
                predefined_default_object_acl = "projectPrivate"
            cls.__insert_predefined_default_object_acl(
                metadata, predefined_default_object_acl, context
            )
        metadata.id = metadata.name
        metadata.project_number = int(utils.acl.PROJECT_NUMBER)
        metadata.metageneration = 0
        metadata.etag = hashlib.md5(metadata.name.encode("utf-8")).hexdigest()
        metadata.time_created.FromDatetime(time_created)
        metadata.updated.FromDatetime(time_created)
        metadata.owner.entity = utils.acl.get_project_entity("owners", context)
        metadata.owner.entity_id = hashlib.md5(
            metadata.owner.entity.encode("utf-8")
        ).hexdigest()
        return (
            cls(metadata, [], None),
            utils.common.extract_projection(request, default_projection, context),
        )

    # === IAM === #

    @classmethod
    def init_iam_policy(cls, metadata, context):
        role_mapping = {
            "READER": "roles/storage.legacyBucketReader",
            "WRITER": "roles/storage.legacyBucketWriter",
            "OWNER": "roles/storage.legacyBucketOwner",
        }
        bindings = []
        for entry in metadata.acl:
            legacy_role = entry.role
            if legacy_role is None or entry.entity is None:
                utils.error.invalid("ACL entry", context)
            role = role_mapping.get(legacy_role)
            if role is None:
                utils.error.invalid("Legacy role %s" % legacy_role, context)
            bindings.append(policy_pb2.Binding(role=role, members=[entry.entity]))
        return policy_pb2.Policy(
            version=1,
            bindings=bindings,
            etag=datetime.datetime.now().isoformat().encode("utf-8"),
        )

    # === RESPONSE === #

    def rest(self):
        response = json_format.MessageToDict(self.metadata)
        return Bucket.__postprocess_rest(response)
