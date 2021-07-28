#!/usr/bin/env python3
#
# Copyright 2021 Google LLC
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

"""Tests for the Bucket class (see gcs/bucket.py)."""

import json
import unittest

import gcs
import utils

from google.cloud.storage_v1.proto import storage_pb2 as storage_pb2
from google.cloud.storage_v1.proto.storage_resources_pb2 import CommonEnums
from google.protobuf import json_format


class TestBucket(unittest.TestCase):
    def test_init_rest(self):
        metadata = {
            "name": "test-bucket-name",
            "defaultEventBasedHold": True,
            "retentionPolicy": {
                "retentionPeriod": "90",
                "effectiveTime": "2021-09-01T02:03:04Z",
                "isLocked": True,
            },
            "acl": [{"entity": "allAuthenticatedUsers", "role": "READER"}],
            "defaultObjectAcl": [{"entity": "allAuthenticatedUsers", "role": "READER"}],
            "iamConfiguration": {
                "publicAccessPrevention": "enforced",
                "uniformBucketLevelAccess": {
                    "enabled": True,
                    "lockedTime": "2023-01-01T01:02:03Z",
                },
            },
            "encryption": {"defaultKmsKeyName": "test-only-value-"},
            "location": "us-central1",
            "locationType": "REGIONAL",
            "website": {"mainPageSuffix": "html", "notFoundPage": "404.html"},
            "logging": {
                "logBucket": "test-only-value",
                "logObjectPrefix": "test-prefix/",
            },
            "versioning": {"enabled": True},
            "cors": [{"method": ["POST"]}],
            "lifecycle": {
                "rule": [
                    {
                        "action": {"type": "delete"},
                        "condition": {
                            "age": 60,
                            "createdBefore": "2023-08-01",
                            # TODO(#6982) - these cannot be tested until we move to storage v2/
                            #   "customTimeBefore": "2024-08-01",
                            #   "daysSinceCustomTime": 90,
                            #   "daysSinceNoncurrentTime": 30,
                            #   "noncurrentTimeBefore": "2021-10-01",
                            "isLive": True,
                            "matchesStorageClass": ["STANDARD"],
                            "numNewerVersions": 42,
                        },
                    }
                ]
            },
            "labels": {"label0": "value0"},
            "storageClass": "regional",
            "billing": {"requesterPays": True},
        }
        request = utils.common.FakeRequest(args={}, data=json.dumps(metadata))
        bucket, projection = gcs.bucket.Bucket.init(request, None)
        bucket_rest = bucket.rest()
        # Verify the BucketAccessControl entries have the desired fields
        metadata.pop("acl")
        acl = bucket_rest.pop("acl", None)
        self.assertLessEqual({"allAuthenticatedUsers"}, {e["entity"] for e in acl})
        self.assertIsNotNone(acl)
        for entry in acl:
            self.assertEqual(entry.pop("kind", None), "storage#bucketAccessControl")
            self.assertEqual(entry.pop("bucket", None), "test-bucket-name")
            self.assertIsNotNone(entry.pop("entity", None))
            self.assertIsNotNone(entry.pop("role", None))
            # Verify the remaining keys are a subset of the expected keys
            self.assertLessEqual(
                set(entry.keys()),
                {
                    "id",
                    "selfLink",
                    "email",
                    "entityId",
                    "domain",
                    "projectTeam",
                    "etag",
                },
            )
        # Verify the BucketAccessControl entries have the desired fields
        metadata.pop("defaultObjectAcl")
        default_object_acl = bucket_rest.pop("defaultObjectAcl", None)
        self.assertIsNotNone(default_object_acl)
        self.assertLessEqual(
            set(["allAuthenticatedUsers"]),
            set([e["entity"] for e in default_object_acl]),
        )
        for entry in default_object_acl:
            self.assertEqual(entry.pop("kind", None), "storage#objectAccessControl")
            self.assertEqual(entry.pop("bucket", None), "test-bucket-name")
            self.assertIsNotNone(entry.pop("entity", None))
            self.assertIsNotNone(entry.pop("role", None))
            # Verify the remaining keys are a subset of the expected keys
            self.assertLessEqual(
                set(entry.keys()),
                set(
                    [
                        "id",
                        "selfLink",
                        "email",
                        "entityId",
                        "domain",
                        "projectTeam",
                        "etag",
                    ]
                ),
            )
        # Some fields are inserted by `Bucket.init()`, we want to verify they
        # exist and have the right value.
        expected_new_fields = {"kind": "storage#bucket", "id": "test-bucket-name"}
        actual_new_fields = {
            k: bucket_rest.pop(k, None) for k, _ in expected_new_fields.items()
        }
        self.assertDictEqual(expected_new_fields, actual_new_fields)
        # Some fields are inserted by `Bucket.init()`, we want to verify they are
        # present, but their value is not that interesting.
        for key in ["timeCreated", "updated", "owner", "projectNumber", "etag"]:
            self.assertIsNotNone(bucket_rest.pop(key, None), msg="key=%s" % key)
        self.maxDiff = None
        self.assertDictEqual(metadata, bucket_rest)
        self.assertEqual(projection, "full")

        request = utils.common.FakeRequest(
            args={},
            data=json.dumps(
                {
                    "name": "bucket",
                    "acl": [
                        json_format.MessageToDict(acl)
                        for acl in utils.acl.compute_predefined_bucket_acl(
                            "bucket", "authenticatedRead", None
                        )
                    ],
                }
            ),
        )
        bucket, projection = gcs.bucket.Bucket.init(request, None)
        self.assertEqual(bucket.metadata.name, "bucket")
        self.assertEqual(projection, "full")
        self.assertEqual(
            list(bucket.metadata.acl),
            utils.acl.compute_predefined_bucket_acl(
                "bucket", "authenticatedRead", None
            ),
        )

    def test_patch(self):
        # Updating requires a full metadata so we don't test it here.
        request = storage_pb2.InsertBucketRequest(
            bucket={
                "name": "bucket",
                "labels": {"init": "true", "patch": "false"},
                "website": {"not_found_page": "notfound.html"},
            }
        )
        bucket, projection = gcs.bucket.Bucket.init(request, "")
        self.assertEqual(bucket.metadata.labels.get("init"), "true")
        self.assertEqual(bucket.metadata.labels.get("patch"), "false")
        self.assertIsNone(bucket.metadata.labels.get("method"))
        self.assertEqual(bucket.metadata.website.main_page_suffix, "")
        self.assertEqual(bucket.metadata.website.not_found_page, "notfound.html")

        request = storage_pb2.PatchBucketRequest(
            bucket="bucket",
            metadata={
                "labels": {"patch": "true", "method": "grpc"},
                "website": {"main_page_suffix": "bucket", "not_found_page": "404"},
            },
            update_mask={"paths": ["labels", "website.main_page_suffix"]},
        )
        bucket.patch(request, "")
        # GRPC can not update a part of map field.
        self.assertIsNone(bucket.metadata.labels.get("init"))
        self.assertEqual(bucket.metadata.labels.get("patch"), "true")
        self.assertEqual(bucket.metadata.labels.get("method"), "grpc")
        self.assertEqual(bucket.metadata.website.main_page_suffix, "bucket")
        # `update_mask` does not update `website.not_found_page`
        self.assertEqual(bucket.metadata.website.not_found_page, "notfound.html")

        request = utils.common.FakeRequest(
            args={},
            data=json.dumps(
                {
                    "name": "new_bucket",
                    "labels": {"method": "rest"},
                    "website": {"notFoundPage": "404.html"},
                }
            ),
        )
        bucket.patch(request, None)
        # REST should only update modifiable field.
        self.assertEqual(bucket.metadata.name, "bucket")
        # REST can update a part of map field.
        self.assertIsNone(bucket.metadata.labels.get("init"))
        self.assertEqual(bucket.metadata.labels.get("patch"), "true")
        self.assertEqual(bucket.metadata.labels.get("method"), "rest")
        self.assertEqual(bucket.metadata.website.main_page_suffix, "bucket")
        self.assertEqual(bucket.metadata.website.not_found_page, "404.html")

        # We want to make sure REST `UPDATE` does not throw any exception.
        request = utils.common.FakeRequest(
            args={}, data=json.dumps({"labels": {"method": "rest_update"}})
        )
        bucket.update(request, None)
        self.assertEqual(bucket.metadata.labels["method"], "rest_update")

    def test_acl(self):
        # Both REST and GRPC share almost the same implementation so we only test GRPC here.
        entity = "user-bucket.acl@example.com"
        request = storage_pb2.InsertBucketRequest(bucket={"name": "bucket"})
        bucket, projection = gcs.bucket.Bucket.init(request, "")

        request = storage_pb2.InsertBucketAccessControlRequest(
            bucket="bucket", bucket_access_control={"entity": entity, "role": "READER"}
        )
        bucket.insert_acl(request, "")

        acl = bucket.get_acl(entity, "")
        self.assertEqual(acl.entity, entity)
        self.assertEqual(acl.email, "bucket.acl@example.com")
        self.assertEqual(acl.role, "READER")

        request = storage_pb2.PatchBucketAccessControlRequest(
            bucket="bucket", entity=entity, bucket_access_control={"role": "OWNER"}
        )
        bucket.patch_acl(request, entity, "")
        acl = bucket.get_acl(entity, "")
        self.assertEqual(acl.entity, entity)
        self.assertEqual(acl.email, "bucket.acl@example.com")
        self.assertEqual(acl.role, "OWNER")

        bucket.delete_acl(entity, "")
        with self.assertRaises(Exception):
            bucket.get_acl(entity, None)

    def test_default_object_acl(self):
        # Both REST and GRPC share almost the same implementation so we only test GRPC here.
        entity = "user-bucket.default_object_acl@example.com"
        request = storage_pb2.InsertBucketRequest(bucket={"name": "bucket"})
        bucket, projection = gcs.bucket.Bucket.init(request, "")

        request = storage_pb2.InsertDefaultObjectAccessControlRequest(
            bucket="bucket", object_access_control={"entity": entity, "role": "READER"}
        )
        bucket.insert_default_object_acl(request, "")

        acl = bucket.get_default_object_acl(entity, "")
        self.assertEqual(acl.entity, entity)
        self.assertEqual(acl.email, "bucket.default_object_acl@example.com")
        self.assertEqual(acl.role, "READER")

        request = storage_pb2.PatchDefaultObjectAccessControlRequest(
            bucket="bucket", entity=entity, object_access_control={"role": "OWNER"}
        )
        bucket.patch_default_object_acl(request, entity, "")
        acl = bucket.get_default_object_acl(entity, "")
        self.assertEqual(acl.entity, entity)
        self.assertEqual(acl.email, "bucket.default_object_acl@example.com")
        self.assertEqual(acl.role, "OWNER")

        bucket.delete_default_object_acl(entity, "")
        with self.assertRaises(Exception):
            bucket.get_default_object_acl(entity, None)

    def test_notification(self):
        metadata = {
            "name": "test-bucket-name",
            "location": "us-central1",
            "locationType": "REGIONAL",
            "storageClass": "regional",
        }
        request = utils.common.FakeRequest(args={}, data=json.dumps(metadata))
        bucket, _ = gcs.bucket.Bucket.init(request, None)

        expected = []
        for topic in ["test-topic-1", "test-topic-2"]:
            request = utils.common.FakeRequest(
                args={},
                data=json.dumps({"topic": topic, "payload_format": "JSON_API_V1"}),
            )
            notification = bucket.insert_notification(request, None)
            self.assertEqual(notification["topic"], topic)

            get_result = bucket.get_notification(notification["id"], None)
            self.assertEqual(notification, get_result)
            expected.append(notification)

        list_result = bucket.list_notifications(None)
        self.assertDictEqual(
            list_result, {"kind": "storage#notifications", "items": expected}
        )
        for n in expected:
            bucket.delete_notification(n["id"], None)


if __name__ == "__main__":
    unittest.main()
