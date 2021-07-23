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
import json
import random
import re

import scalpl
import utils

from google.cloud.storage_v1.proto import storage_resources_pb2 as resources_pb2
from google.cloud.storage_v1.proto.storage_resources_pb2 import CommonEnums
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
    rest_only_fields = ["iamConfiguration.publicAccessPrevention"]

    def __init__(self, metadata, notifications, iam_policy, rest_only):
        self.metadata = metadata
        self.notifications = notifications
        self.iam_policy = iam_policy
        self.rest_only = rest_only

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
        proxy.pop("iamConfiguration.bucketPolicyOnly", False)
        for key in keys:
            if key.endswith("createdBefore"):
                proxy[key] = proxy[key] + "T00:00:00Z"
        rest_only = {}
        for field in Bucket.rest_only_fields:
            if field in proxy:
                rest_only[field] = proxy.pop(field)
        return proxy.data, rest_only

    @classmethod
    def __postprocess_rest(cls, data, rest_only):
        proxy = scalpl.Cut(data)
        keys = utils.common.nested_key(data)
        for key in keys:
            if key.endswith("createdBefore"):
                proxy[key] = proxy[key].replace("T00:00:00Z", "")
        proxy["kind"] = "storage#bucket"
        if "acl" in data:
            for entry in data["acl"]:
                entry["kind"] = "storage#bucketAccessControl"
        if "defaultObjectAcl" in data:
            for entry in data["defaultObjectAcl"]:
                entry["kind"] = "storage#objectAccessControl"
        proxy.update(rest_only)
        return proxy.data

    @classmethod
    def __insert_predefined_acl(cls, metadata, predefined_acl, context):
        if (
            predefined_acl == ""
            or predefined_acl
            == CommonEnums.PredefinedBucketAcl.PREDEFINED_BUCKET_ACL_UNSPECIFIED
        ):
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
        if (
            predefined_default_object_acl == ""
            or predefined_default_object_acl
            == CommonEnums.PredefinedObjectAcl.PREDEFINED_OBJECT_ACL_UNSPECIFIED
        ):
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

    @classmethod
    def __enrich_acl(cls, metadata):
        for entry in metadata.acl:
            entry.bucket = metadata.name
        for entry in metadata.default_object_acl:
            entry.bucket = metadata.name

    # === INITIALIZATION === #

    @classmethod
    def init(cls, request, context, rest_only=None):
        time_created = datetime.datetime.now()
        metadata = None
        if context is not None:
            metadata = request.bucket
        else:
            metadata, rest_only = cls.__preprocess_rest(json.loads(request.data))
            metadata = json_format.ParseDict(metadata, resources_pb2.Bucket())
        cls.__validate_bucket_name(metadata.name, context)
        default_projection = CommonEnums.Projection.NO_ACL
        if len(metadata.acl) != 0 or len(metadata.default_object_acl) != 0:
            default_projection = CommonEnums.Projection.FULL
        is_uniform = metadata.iam_configuration.uniform_bucket_level_access.enabled
        metadata.iam_configuration.uniform_bucket_level_access.enabled = False
        if len(metadata.acl) == 0:
            predefined_acl = utils.acl.extract_predefined_acl(request, False, context)
            if (
                predefined_acl
                == CommonEnums.PredefinedBucketAcl.PREDEFINED_BUCKET_ACL_UNSPECIFIED
            ):
                predefined_acl = (
                    CommonEnums.PredefinedBucketAcl.BUCKET_ACL_PROJECT_PRIVATE
                )
            elif predefined_acl == "":
                predefined_acl = "projectPrivate"
            elif is_uniform:
                utils.error.invalid(
                    "Predefined ACL with uniform bucket level access enabled", context
                )
            cls.__insert_predefined_acl(metadata, predefined_acl, context)
        if len(metadata.default_object_acl) == 0:
            predefined_default_object_acl = utils.acl.extract_predefined_default_object_acl(
                request, context
            )
            if (
                predefined_default_object_acl
                == CommonEnums.PredefinedObjectAcl.PREDEFINED_OBJECT_ACL_UNSPECIFIED
            ):
                predefined_default_object_acl = (
                    CommonEnums.PredefinedObjectAcl.OBJECT_ACL_PROJECT_PRIVATE
                )
            elif predefined_default_object_acl == "":
                predefined_default_object_acl = "projectPrivate"
            elif is_uniform:
                utils.error.invalid(
                    "Predefined Default Object ACL with uniform bucket level access enabled",
                    context,
                )
            cls.__insert_predefined_default_object_acl(
                metadata, predefined_default_object_acl, context
            )
        cls.__enrich_acl(metadata)
        metadata.iam_configuration.uniform_bucket_level_access.enabled = is_uniform
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
        if rest_only is None:
            rest_only = {}
        return (
            cls(metadata, {}, cls.__init_iam_policy(metadata, context), rest_only),
            utils.common.extract_projection(request, default_projection, context),
        )

    # === IAM === #

    @classmethod
    def __init_iam_policy(cls, metadata, context):
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

    def get_iam_policy(self, request, context):
        return self.iam_policy

    def set_iam_policy(self, request, context):
        policy = None
        if context is not None:
            policy = request.iam_request.policy
        else:
            data = json.loads(request.data)
            if "iam_request" in data:
                data = data["iam_request"]["policy"]
            data.pop("kind", None)
            data.pop("etag", None)
            data.pop("resourceId", None)
            policy = json_format.ParseDict(data, policy_pb2.Policy())
        self.iam_policy = policy
        self.iam_policy.etag = datetime.datetime.now().isoformat().encode("utf-8")
        return self.iam_policy

    # === METADATA === #

    def __update_metadata(self, source, update_mask):
        if update_mask is None:
            update_mask = field_mask_pb2.FieldMask(paths=Bucket.modifiable_fields)
        update_mask.MergeMessage(source, self.metadata, True, True)
        if self.metadata.versioning.enabled:
            self.metadata.metageneration += 1
        self.metadata.updated.FromDatetime(datetime.datetime.now())

    def update(self, request, context):
        metadata = None
        if context is not None:
            metadata = request.metadata
        else:
            metadata, rest_only = self.__preprocess_rest(json.loads(request.data))
            self.rest_only.update(rest_only)
            metadata = json_format.ParseDict(metadata, resources_pb2.Bucket())
        self.__update_metadata(metadata, None)
        self.__insert_predefined_acl(
            metadata, utils.acl.extract_predefined_acl(request, False, context), context
        )
        self.__insert_predefined_default_object_acl(
            metadata,
            utils.acl.extract_predefined_default_object_acl(request, context),
            context,
        )

    def patch(self, request, context):
        update_mask = field_mask_pb2.FieldMask()
        metadata = None
        if context is not None:
            metadata = request.metadata
            update_mask = request.update_mask
        else:
            data = json.loads(request.data)
            if "labels" in data:
                if data["labels"] is None:
                    self.metadata.labels.clear()
                else:
                    for key, value in data["labels"].items():
                        if value is None:
                            self.metadata.labels.pop(key, None)
                        else:
                            self.metadata.labels[key] = value
            data.pop("labels", None)
            data, rest_only = self.__preprocess_rest(data)
            self.rest_only.update(rest_only)
            metadata = json_format.ParseDict(data, resources_pb2.Bucket())
            paths = set()
            for key in utils.common.nested_key(data):
                key = utils.common.to_snake_case(key)
                head = key
                for i, c in enumerate(key):
                    if c == "." or c == "[":
                        head = key[0:i]
                        break
                if head in Bucket.modifiable_fields:
                    if "[" in key:
                        paths.add(head)
                    else:
                        paths.add(key)
            update_mask = field_mask_pb2.FieldMask(paths=list(paths))
        self.__update_metadata(metadata, update_mask)
        self.__insert_predefined_acl(
            metadata, utils.acl.extract_predefined_acl(request, False, context), context
        )
        self.__insert_predefined_default_object_acl(
            metadata,
            utils.acl.extract_predefined_default_object_acl(request, context),
            context,
        )

    # === ACL === #

    def __search_acl(self, entity, must_exist, context):
        entity = utils.acl.get_canonical_entity(entity)
        for i in range(len(self.metadata.acl)):
            if self.metadata.acl[i].entity == entity:
                return i
        if must_exist:
            utils.error.notfound("ACL %s" % entity, context)

    def __upsert_acl(self, entity, role, context):
        # For simplicity, we treat `insert`, `update` and `patch` ACL the same way.
        index = self.__search_acl(entity, False, context)
        acl = utils.acl.create_bucket_acl(self.metadata.name, entity, role, context)
        if index is not None:
            self.metadata.acl[index].CopyFrom(acl)
            return self.metadata.acl[index]
        else:
            self.metadata.acl.append(acl)
            return acl

    def get_acl(self, entity, context):
        index = self.__search_acl(entity, True, context)
        return self.metadata.acl[index]

    def insert_acl(self, request, context):
        entity, role = "", ""
        if context is not None:
            entity, role = (
                request.bucket_access_control.entity,
                request.bucket_access_control.role,
            )
        else:
            payload = json.loads(request.data)
            entity, role = payload["entity"], payload["role"]
        return self.__upsert_acl(entity, role, context)

    def update_acl(self, request, entity, context):
        role = ""
        if context is not None:
            role = request.bucket_access_control.role
        else:
            payload = json.loads(request.data)
            role = payload["role"]
        return self.__upsert_acl(entity, role, context)

    def patch_acl(self, request, entity, context):
        role = ""
        if context is not None:
            role = request.bucket_access_control.role
        else:
            payload = json.loads(request.data)
            role = payload["role"]
        return self.__upsert_acl(entity, role, context)

    def delete_acl(self, entity, context):
        del self.metadata.acl[self.__search_acl(entity, True, context)]

    # === DEFAULT OBJECT ACL === #

    def __search_default_object_acl(self, entity, must_exist, context):
        entity = utils.acl.get_canonical_entity(entity)
        for i in range(len(self.metadata.default_object_acl)):
            if self.metadata.default_object_acl[i].entity == entity:
                return i
        if must_exist:
            utils.error.notfound("Default Object ACL %s" % entity, context)

    def __upsert_default_object_acl(self, entity, role, context):
        # For simplicity, we treat `insert`, `update` and `patch` Default Object ACL the same way.
        index = self.__search_default_object_acl(entity, False, context)
        acl = utils.acl.create_default_object_acl(
            self.metadata.name, entity, role, context
        )
        if index is not None:
            self.metadata.default_object_acl[index].CopyFrom(acl)
            return self.metadata.default_object_acl[index]
        else:
            self.metadata.default_object_acl.append(acl)
            return acl

    def get_default_object_acl(self, entity, context):
        index = self.__search_default_object_acl(entity, True, context)
        return self.metadata.default_object_acl[index]

    def insert_default_object_acl(self, request, context):
        entity, role = "", ""
        if context is not None:
            entity, role = (
                request.object_access_control.entity,
                request.object_access_control.role,
            )
        else:
            payload = json.loads(request.data)
            entity, role = payload["entity"], payload["role"]
        return self.__upsert_default_object_acl(entity, role, context)

    def update_default_object_acl(self, request, entity, context):
        role = ""
        if context is not None:
            role = request.object_access_control.role
        else:
            payload = json.loads(request.data)
            role = payload["role"]
        return self.__upsert_default_object_acl(entity, role, context)

    def patch_default_object_acl(self, request, entity, context):
        role = ""
        if context is not None:
            role = request.object_access_control.role
        else:
            payload = json.loads(request.data)
            role = payload["role"]
        return self.__upsert_default_object_acl(entity, role, context)

    def delete_default_object_acl(self, entity, context):
        del self.metadata.default_object_acl[
            self.__search_default_object_acl(entity, True, context)
        ]

    # === NOTIFICATIONS === #

    def insert_notification(self, request, context):
        notification = {
            "kind": "storage#notification",
            "id": "notification-%d" % random.getrandbits(16),
        }
        data = json.loads(request.data)
        for required_key in {"topic", "payload_format"}:
            value = data.pop(required_key, None)
            if value is not None:
                notification[required_key] = value
            else:
                utils.error.invalid(
                    "Missing field in notification %s" % required_key, context
                )
        for key in {"event_types", "custom_attributes", "object_name_prefix"}:
            value = data.pop(key, None)
            if value is not None:
                notification[key] = value
        self.notifications[notification["id"]] = notification
        return notification

    def get_notification(self, notification_id, context):
        return self.notifications[notification_id]

    def delete_notification(self, notification_id, context):
        del self.notifications[notification_id]

    def list_notifications(self, context):
        response = {"kind": "storage#notifications", "items": []}
        for notification in self.notifications.values():
            response["items"].append(notification)
        return response

    # === RESPONSE === #

    def rest(self):
        response = json_format.MessageToDict(self.metadata)
        return Bucket.__postprocess_rest(response, self.rest_only)
