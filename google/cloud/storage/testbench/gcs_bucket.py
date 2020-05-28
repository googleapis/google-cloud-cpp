#!/usr/bin/env python
# Copyright 2018 Google Inc.
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

import base64
import error_response
import flask
import gcs_object
import json
import re
import testbench_utils
import time


class GcsBucket(object):
    """Represent a GCS Bucket."""

    def __init__(self, gcs_url, name):
        self.name = name
        self.gcs_url = gcs_url
        self.metadata = {
            "metageneration": "0",
            "name": self.name,
            "location": "US",
            "storageClass": "STANDARD",
            "etag": "XYZ=",
            "labels": {"foo": "bar", "baz": "qux"},
            "owner": {"entity": "project-owners-123456789", "entityId": ""},
        }
        self.notification_id = "1"
        self.notifications = {}
        self.iam_version = 1
        self.counter = 1
        self.iam_bindings = []
        self.resumable_uploads = {}
        # Update the derived metadata attributes (e.g.: id, kind, selfLink)
        self.update_from_metadata({})
        self.insert_acl(
            testbench_utils.canonical_entity_name("project-owners-123456789"), "OWNER"
        )
        self.insert_acl(
            testbench_utils.canonical_entity_name("project-editors-123456789"), "OWNER"
        )
        self.insert_acl(
            testbench_utils.canonical_entity_name("project-viewers-123456789"), "READER"
        )
        self.insert_default_object_acl(
            testbench_utils.canonical_entity_name("project-owners-123456789"), "OWNER"
        )
        self.insert_default_object_acl(
            testbench_utils.canonical_entity_name("project-editors-123456789"), "OWNER"
        )
        self.insert_default_object_acl(
            testbench_utils.canonical_entity_name("project-viewers-123456789"), "READER"
        )

    def increase_metageneration(self):
        """Increase the current metageneration number."""
        new = str(int(self.metadata.get("metageneration", "0")) + 1)
        self.metadata["metageneration"] = new

    def versioning_enabled(self):
        """Return True if versioning is enabled for this Bucket."""
        v = self.metadata.get("versioning", None)
        if v is None:
            return False
        return v.get("enabled", False)

    @classmethod
    def _remove_non_writable_keys(cls, metadata):
        """Remove the keys from metadata (an update or patch) that are not
         writable.

         Both `Buckets: patch` and `Buckets: update` either ignore non-writable
         keys or return 400 if the key does not match the current value. In
         the testbench we simply always ignore them, to make life easier.

         :param metadata:dict a dictionary representing a patch or
             update to the metadata.
         :return metadata but with only any non-writable keys removed.
         :rtype: dict
         """
        writeable_keys = {
            "acl",
            "billing",
            "cors",
            "defaultObjectAcl",
            "encryption",
            "labels",
            "lifecycle",
            "location",
            "logging",
            "retentionPolicy",
            "storageClass",
            "versioning",
            "website",
            "iamConfiguration",
        }
        non_writeable_keys = []
        for key in metadata.keys():
            if key not in writeable_keys:
                non_writeable_keys.append(key)
        for key in non_writeable_keys:
            metadata.pop(key, None)
        return metadata

    def _adjust_field_patch(self, patch, field):
        """Add missing fields (such as lockedTme) to a UniformBucketLevelAccess
        or BucketPolicyOnly patch.

        :param patch:dict a dictionary of metadata values.
        :param field: one of 'uniformBucketLevelAccess' or 'bucketPolicyOnly'
        """
        field_was_enabled = False
        if self.metadata.get("iamConfiguration"):
            field_value = self.metadata.get("iamConfiguration").get(field)
            if field_value:
                field_was_enabled = field_value.get("enabled")
        config = patch.get("iamConfiguration")
        if config is not None:
            if config.get(field):
                field_enabled = config.get(field).get("enabled")
                if not field_was_enabled and field_enabled:
                    # Set the locked time (arbitrarily) to 7 days from now.
                    locked_time = time.gmtime(time.time() + 7 * 24 * 3600)
                    modified_field = {
                        "lockedTime": time.strftime("%Y-%m-%dT%H:%M:%SZ", locked_time),
                        "enabled": field_enabled,
                    }
                    config[field] = modified_field

    def update_from_metadata(self, metadata):
        """Update from a metadata dictionary.

        :param metadata:dict a dictionary with new metadata values.
        """
        retention_policy = metadata.get("retentionPolicy")
        if retention_policy:
            # Ignore any values set for 'isLocked' or 'effectiveTime'.
            retention_policy.pop("isLocked", None)
            now = time.gmtime(time.time())
            timestamp = time.strftime("%Y-%m-%dT%H:%M:%SZ", now)
            retention_policy["effectiveTime"] = timestamp
            metadata["retentionPolicy"] = retention_policy
        self._adjust_field_patch(self.metadata, "uniformBucketLevelAccess")
        self._adjust_field_patch(self.metadata, "bucketPolicyOnly")
        tmp = self.metadata.copy()
        metadata = GcsBucket._remove_non_writable_keys(metadata)
        tmp.update(metadata)
        tmp["name"] = tmp.get("name", self.name)
        tmp.update(
            {
                "id": self.name,
                "kind": "storage#bucket",
                "selfLink": self.gcs_url + self.name,
                "projectNumber": "123456789",
                "timeCreated": "2018-05-19T19:31:14Z",
                "updated": "2018-05-19T19:31:24Z",
            }
        )
        self.metadata = tmp
        self.increase_metageneration()

    def apply_patch(self, patch):
        """Update from a metadata dictionary.

        :param patch:dict a dictionary with metadata changes.
        """
        patch = GcsBucket._remove_non_writable_keys(patch)
        retention_policy = patch.get("retentionPolicy")
        if retention_policy:
            # Ignore any values set for 'isLocked' or 'effectiveTime'.
            retention_policy.pop("isLocked", None)
            now = time.gmtime(time.time())
            timestamp = time.strftime("%Y-%m-%dT%H:%M:%SZ", now)
            retention_policy["effectiveTime"] = timestamp
            patch["retentionPolicy"] = retention_policy
        self._adjust_field_patch(self.metadata, "uniformBucketLevelAccess")
        self._adjust_field_patch(self.metadata, "bucketPolicyOnly")
        patched = testbench_utils.json_api_patch(
            self.metadata, patch, recurse_on={"labels"}
        )
        self.metadata = patched
        self.increase_metageneration()

    def check_preconditions(self, request):
        """Verify that the preconditions in request are met.

        :param request:flask.Request the contents of the HTTP request.
        :rtype:NoneType
        :raises:ErrorResponse if the request does not pass the preconditions,
            for example, the request has a `ifMetagenerationMatch` restriction
            that is not met.
        """

        metageneration_match = request.args.get("ifMetagenerationMatch")
        metageneration_not_match = request.args.get("ifMetagenerationNotMatch")
        metageneration = self.metadata.get("metageneration")

        if (
            metageneration_not_match is not None
            and metageneration_not_match == metageneration
        ):
            raise error_response.ErrorResponse(
                "Precondition Failed (metageneration = %s)" % metageneration,
                status_code=412,
            )

        if metageneration_match is not None and metageneration_match != metageneration:
            raise error_response.ErrorResponse(
                "Precondition Failed (metageneration = %s)" % metageneration,
                status_code=412,
            )

    def create_acl_entry(self, entity, role):
        """Return an ACL entry for the given entity and role.

        :param entity: str the user, group or email granted permissions.
        :param role: str the name of the permissions (READER, WRITER, OWNER).
        :return: the canonical entity name and the ACL entry.
        :rtype: (str,dict)
        """
        entity = testbench_utils.canonical_entity_name(entity)
        email = ""
        if entity.startswith("user-"):
            email = entity.replace("user-", "", 1)
        return (
            entity,
            {
                "bucket": self.name,
                "email": email,
                "entity": entity,
                "etag": self.metadata.get("etag", "XYZ="),
                "id": self.metadata.get("id", "") + "/" + entity,
                "kind": "storage#bucketAccessControl",
                "role": role,
                "selfLink": self.metadata.get("selfLink") + "/acl/" + entity,
            },
        )

    def insert_acl(self, entity, role):
        """Insert (or update) a new BucketAccessControl entry for this bucket.

        :param entity:str the name of the entity to insert.
        :param role:str the new role
        :return: the dictionary representing the new AccessControl metadata.
        :rtype: dict
        """
        entity, entry = self.create_acl_entry(entity, role)
        # Replace or insert the entry.
        indexed = testbench_utils.index_acl(self.metadata.get("acl", []))
        indexed[entity] = entry
        self.metadata["acl"] = list(indexed.values())
        return entry

    def delete_acl(self, entity):
        """
        Delete a single BucketAccessControl entry from this bucket.

        :param entity:str the name of the entity.
        :rtype:NoneType
        """
        entity = testbench_utils.canonical_entity_name(entity)
        indexed = testbench_utils.index_acl(self.metadata.get("acl", []))
        indexed.pop(entity)
        self.metadata["acl"] = list(indexed.values())

    def get_acl(self, entity):
        """Get a single BucketAccessControl entry from this bucket.

        :param entity:str the name of the entity.
        :return: with the contents of the BucketAccessControl.
        :rtype: dict
        """
        entity = testbench_utils.canonical_entity_name(entity)
        for acl in self.metadata.get("acl", []):
            if acl.get("entity", "") == entity:
                return acl
        raise error_response.ErrorResponse(
            "Entity %s not found in object %s" % (entity, self.name)
        )

    def update_acl(self, entity, role):
        """Update a single BucketAccessControl entry in this bucket.

        :param entity:str the name of the entity.
        :param role:str the new role for the entity.
        :return: with the contents of the BucketAccessControl.
        :rtype: dict
        """
        return self.insert_acl(entity, role)

    def insert_default_object_acl(self, entity, role):
        """Insert (or update) a new default ObjectAccessControl entry for this
        bucket.

        :param entity:str the name of the entity to insert.
        :param role:str the new role
        :return: the dictionary representing the new ObjectAccessControl.
        :rtype: dict
        """
        entity = testbench_utils.canonical_entity_name(entity)
        email = ""
        if entity.startswith("user-"):
            email = email.replace("user-", "", 1)
        # Replace or insert the entry.
        indexed = testbench_utils.index_acl(self.metadata.get("defaultObjectAcl", []))
        indexed[entity] = {
            "bucket": self.name,
            "email": email,
            "entity": entity,
            "etag": self.metadata.get("etag", "XYZ="),
            "id": self.metadata.get("id", "") + "/" + entity,
            "kind": "storage#objectAccessControl",
            "role": role,
            "selfLink": self.metadata.get("selfLink") + "/acl/" + entity,
        }
        self.metadata["defaultObjectAcl"] = list(indexed.values())
        return indexed[entity]

    def delete_default_object_acl(self, entity):
        """Delete a single default ObjectAccessControl entry from this bucket.

        :param entity:str the name of the entity.
        :rtype:NoneType
        """
        entity = testbench_utils.canonical_entity_name(entity)
        indexed = testbench_utils.index_acl(self.metadata.get("defaultObjectAcl", []))
        indexed.pop(entity)
        self.metadata["defaultObjectAcl"] = list(indexed.values())

    def get_default_object_acl(self, entity):
        """Get a single default ObjectAccessControl entry from this Bucket.

        :param entity:str the name of the entity.
        :return: with the contents of the BucketAccessControl.
        :rtype: dict
        """
        entity = testbench_utils.canonical_entity_name(entity)
        for acl in self.metadata.get("defaultObjectAcl", []):
            if acl.get("entity", "") == entity:
                return acl
        raise error_response.ErrorResponse(
            "Entity %s not found in object %s" % (entity, self.name)
        )

    def update_default_object_acl(self, entity, role):
        """Update a single default ObjectAccessControl entry in this Bucket.

        :param entity:str the name of the entity.
        :param role:str the new role for the entity.
        :return: with the contents of the ObjectAccessControl.
        :rtype: dict
        """
        return self.insert_default_object_acl(entity, role)

    def list_notifications(self):
        """List the notifications associated with this Bucket.

        :return: with the notification definitions.
        :rtype: list[dict]
        """
        return list(self.notifications.values())

    def insert_notification(self, request):
        """
        Insert a new notification into this Bucket.

        :param request:flask.Request the HTTP request contents.
        :return: the new notification value.
        :rtype:dict
        """
        notification_id = "notification-%s" % self.notification_id
        link = "%s/b/%s/notificationConfigs/%s" % (
            self.gcs_url,
            self.name,
            notification_id,
        )
        self.notification_id = str(int(self.notification_id) + 1)
        notification = json.loads(request.data)
        notification.update(
            {
                "id": notification_id,
                "selfLink": link,
                "etag": "XYZ=",
                "kind": "storage#notification",
            }
        )
        self.notifications[notification_id] = notification
        return notification

    def delete_notification(self, notification_id):
        """Delete a notification in this Bucket.

        :param notification_id:str the id of the notification.
        :rtype:NoneType
        """
        if self.notifications.get(notification_id) is None:
            raise error_response.ErrorResponse(
                "Notification %d not found in %s" % (notification_id, self.name),
                status_code=404,
            )
        del self.notifications[notification_id]

    def get_notification(self, notification_id):
        """
        Get the details of a given notification in this Bucket.

        :param notification_id:str the id of the notification.
        :return: the details of the notification.
        :rtype: dict
        """
        details = self.notifications.get(notification_id)
        if details is None:
            raise error_response.ErrorResponse(
                "Notification %d not found in %s" % (notification_id, self.name),
                status_code=404,
            )
        return details

    @classmethod
    def _append_acl_members_to_binding(cls, role, members, bindings):
        """Add ACL members into IAM bindings."""
        found = False
        for binding in bindings:
            if binding.get("role") == role and not binding.get("condition"):
                found = True
                binding.setdefault("members", [])
                for member in members:
                    binding["members"].append(member)
                break
        if not found:
            bindings.append({"role": role, "members": members})
        return bindings

    def iam_policy_as_json(self):
        """Get the current IamPolicy in the right format for JSON."""
        role_mapping = {
            "READER": "roles/storage.legacyBucketReader",
            "WRITER": "roles/storage.legacyBucketWriter",
            "OWNER": "roles/storage.legacyBucketOwner",
        }
        copy_of_bindings = self.iam_bindings.copy()
        if self.metadata.get("acl") is not None:
            # Store the ACLs as IamBindings
            for entry in self.metadata.get("acl", []):
                legacy_role = entry.get("role")
                if legacy_role is None or entry.get("entity") is None:
                    raise error_response.ErrorResponse(
                        "Invalid ACL entry", status_code=500
                    )
                role = role_mapping.get(legacy_role)
                if role is None:
                    raise error_response.ErrorResponse(
                        "Invalid legacy role %s" % legacy_role, status_code=500
                    )
                copy_of_bindings = GcsBucket._append_acl_members_to_binding(
                    role, [entry.get("entity")], copy_of_bindings
                )
        policy = {
            "kind": "storage#policy",
            "resourceId": "projects/_/buckets/%s" % self.name,
            "bindings": copy_of_bindings,
            "etag": base64.b64encode(bytearray(str(self.counter), "utf-8")).decode(
                "utf-8"
            ),
            "version": self.iam_version,
        }
        return policy

    def get_iam_policy(self, request):
        """Get the IamPolicy associated with this Bucket.

        :param request: flask.Request the http request.
        :return: the IamPolicy as a dictionary, ready for JSON encoding.
        :rtype: dict
        """
        self.check_preconditions(request)
        return self.iam_policy_as_json()

    def set_iam_policy(self, request):
        """Set the IamPolicy associated with this Bucket.

        :param request: flask.Request the original http request.
        :return: the IamPolicy as a dictionary, ready for JSON encoding.
        :rtype: dict
        """
        self.check_preconditions(request)
        current_etag = base64.b64encode(bytearray(str(self.counter), "utf-8")).decode(
            "utf-8"
        )
        if request.headers.get(
            "if-match"
        ) is not None and current_etag != request.headers.get("if-match"):
            raise error_response.ErrorResponse(
                "Mismatched ETag has %s" % current_etag, status_code=412
            )
        if request.headers.get(
            "if-none-match"
        ) is not None and current_etag == request.headers.get("if-none-match"):
            raise error_response.ErrorResponse(
                "Mismatched ETag has %s" % current_etag, status_code=412
            )

        policy = json.loads(request.data)
        if policy.get("bindings") is None:
            raise error_response.ErrorResponse('Missing "bindings" field')

        new_acl = []
        new_iam_bindings = []
        role_mapping = {
            "roles/storage.legacyBucketReader": "READER",
            "roles/storage.legacyBucketWriter": "WRITER",
            "roles/storage.legacyBucketOwner": "OWNER",
        }
        for binding in policy.get("bindings"):
            role = binding.get("role")
            members = binding.get("members")
            condition = binding.get("condition")
            if role is None or members is None:
                raise error_response.ErrorResponse('Missing "role" or "members" fields')
            if role_mapping.get(role) is None:
                new_binding = {"role": role, "members": members}
                if condition:
                    new_binding["condition"] = condition
                new_iam_bindings.append(new_binding)
            else:
                for m in members:
                    legacy_role = role_mapping.get(role)
                    _, entry = self.create_acl_entry(entity=m, role=legacy_role)
                    new_acl.append(entry)
        self.metadata["acl"] = new_acl
        self.iam_bindings = new_iam_bindings
        if policy.get("version") is None:
            self.iam_version = 1
        else:
            self.iam_version = policy.get("version")
        self.counter = self.counter + 1
        return self.iam_policy_as_json()

    def test_iam_permissions(self, request):
        """Test the IAM permissions for the current user.

        Because we do not want to implement a full simulator for IAM, we simply
        return the permissions matching 'storage.*'

        :param request: flask.Request the current http request.
        :return: formatted for `Buckets: testIamPermissions`
        :rtype: dict
        """
        result = {"kind": "storage#testIamPermissionsResponse", "permissions": []}
        for p in request.args.getlist("permissions"):
            if p.startswith("storage."):
                result["permissions"].append(p)
        return result

    def lock_retention_policy(self, request):
        """Set the IamPolicy associated with this Bucket.

        :param request: flask.Request the current http request.
        :return: None
        """
        metageneration = request.args.get("ifMetagenerationMatch")
        if metageneration is None:
            raise error_response.ErrorResponse(
                "Missing ifMetagenerationMatch parameter", status_code=400
            )
        if metageneration != self.metadata.get("metageneration"):
            raise error_response.ErrorResponse(
                "Precondition Failed (metageneration = %s)" % metageneration,
                status_code=412,
            )
        retention_policy = self.metadata.get("retentionPolicy")
        if retention_policy is None:
            raise error_response.ErrorResponse(
                "Precondition Failed, bucket does not have a retention policy to lock",
                status_code=412,
            )
        retention_policy["isLocked"] = True
        self.metadata["retentionPolicy"] = retention_policy
        self.increase_metageneration()

    def create_resumable_upload(self, upload_url, request):
        """Capture the details for a resumable upload.

        :param upload_url: str the base URL for uploads.
        :param request: flask.Request the original http request.
        :return: the HTTP response to send back.
        """
        x_upload_content_type = request.headers.get(
            "x-upload-content-type", "application/octet-stream"
        )
        x_upload_content_length = request.headers.get("x-upload-content-length")
        expected_bytes = None
        if x_upload_content_length:
            expected_bytes = int(x_upload_content_length)

        if request.args.get("name") is not None and len(request.data):
            raise error_response.ErrorResponse(
                "The name argument is only supported for empty payloads",
                status_code=400,
            )
        if len(request.data):
            metadata = json.loads(request.data)
        else:
            metadata = {"name": request.args.get("name")}

        if metadata.get("name") is None:
            raise error_response.ErrorResponse(
                "Missing object name argument", status_code=400
            )
        metadata.setdefault("contentType", x_upload_content_type)
        upload = {
            "metadata": metadata,
            "instructions": request.headers.get("x-goog-testbench-instructions"),
            "fields": request.args.get("fields"),
            "next_byte": 0,
            "expected_bytes": expected_bytes,
            "object_name": metadata.get("name"),
            "media": b"",
            "done": False,
        }
        # Capture the preconditions, including those that are None.
        for precondition in [
            "ifGenerationMatch",
            "ifGenerationNotMatch",
            "ifMetagenerationMatch",
            "ifMetagenerationNotMatch",
        ]:
            upload[precondition] = request.args.get(precondition)
        upload_id = base64.b64encode(bytearray(metadata.get("name"), "utf-8")).decode(
            "utf-8"
        )
        self.resumable_uploads[upload_id] = upload
        location = "%s?uploadType=resumable&upload_id=%s" % (upload_url, upload_id)
        response = flask.make_response("")
        response.headers["Location"] = location
        return response

    def receive_upload_chunk(self, gcs_url, request):
        """Receive a new upload chunk.

        :param gcs_url: str the base URL for the service.
        :param request: flask.Request the original http request.
        :return: the HTTP response.
        """
        upload_id = request.args.get("upload_id")
        if upload_id is None:
            raise error_response.ErrorResponse(
                "Missing upload_id in resumable_upload_chunk", status_code=400
            )
        upload = self.resumable_uploads.get(upload_id)
        if upload is None:
            raise error_response.ErrorResponse(
                "Cannot find resumable upload %s" % upload_id, status_code=404
            )
        # Be gracious in what you accept, if the Content-Range header is not
        # set we assume it is a good header and it is the end of the file.
        next_byte = upload["next_byte"]
        begin = next_byte
        end = next_byte + len(request.data)
        total = end
        final_chunk = False
        content_range = request.headers.get("content-range")
        if content_range is not None:
            if content_range.startswith("bytes */*"):
                # This is just a query to resume an upload, if it is done, return
                # the completed upload payload and an empty range header.
                response = flask.make_response(upload.get("payload", ""))
                if next_byte > 1 and not upload["done"]:
                    response.headers["Range"] = "bytes=0-%d" % (next_byte - 1)
                response.status_code = 200 if upload["done"] else 308
                return response
            match = re.match("bytes \*/(\\*|[0-9]+)", content_range)
            if match:
                if match.group(1) == "*":
                    total = 0
                else:
                    total = int(match.group(1))
                    final_chunk = True
            else:
                match = re.match("bytes ([0-9]+)-([0-9]+)/(\\*|[0-9]+)", content_range)
                if not match:
                    raise error_response.ErrorResponse(
                        "Invalid Content-Range in upload %s" % content_range,
                        status_code=400,
                    )
                begin = int(match.group(1))
                end = int(match.group(2))
                if match.group(3) == "*":
                    total = 0
                else:
                    total = int(match.group(3))
                    final_chunk = True

                if begin != next_byte:
                    raise error_response.ErrorResponse(
                        "Mismatched data range, expected data at %d, got %d"
                        % (next_byte, begin),
                        status_code=400,
                    )
                if len(request.data) != end - begin + 1:
                    raise error_response.ErrorResponse(
                        "Mismatched data range (%d) vs. content-length (%d)"
                        % (end - begin + 1, len(request.data)),
                        status_code=400,
                    )

        upload["media"] = upload.get("media", b"") + request.data
        next_byte = len(upload.get("media", ""))
        upload["next_byte"] = next_byte
        response_payload = ""
        if final_chunk and next_byte >= total:
            expected_bytes = upload["expected_bytes"]
            if expected_bytes is not None and expected_bytes != total:
                raise error_response.ErrorResponse(
                    "X-Upload-Content-Length"
                    "validation failed. Expected=%d, got %d." % (expected_bytes, total)
                )
            upload["done"] = True
            object_name = upload.get("object_name")
            object_path, blob = testbench_utils.get_object(
                self.name, object_name, gcs_object.GcsObject(self.name, object_name)
            )
            # Release a few resources to control memory usage.
            original_metadata = upload.pop("metadata", None)
            media = upload.pop("media", None)
            blob.check_preconditions_by_value(
                upload.get("ifGenerationMatch"),
                upload.get("ifGenerationNotMatch"),
                upload.get("ifMetagenerationMatch"),
                upload.get("ifMetagenerationNotMatch"),
            )
            if upload.pop("instructions", None) == "inject-upload-data-error":
                media = testbench_utils.corrupt_media(media)
            revision = blob.insert_resumable(gcs_url, request, media, original_metadata)
            response_payload = testbench_utils.filter_fields_from_response(
                upload.get("fields"), revision.metadata
            )
            upload["payload"] = response_payload
            testbench_utils.insert_object(object_path, blob)

        response = flask.make_response(response_payload)
        if next_byte == 0:
            response.headers["Range"] = "bytes=0-0"
        else:
            response.headers["Range"] = "bytes=0-%d" % (next_byte - 1)
        if upload.get("done", False):
            response.status_code = 200
        else:
            response.status_code = 308
        return response
