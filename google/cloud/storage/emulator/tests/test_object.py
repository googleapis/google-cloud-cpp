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

"""Tests for the Object class (see gcs/object.py)."""

import json
import unittest

import gcs
import utils

from google.cloud.storage_v1.proto import storage_pb2 as storage_pb2
from google.cloud.storage_v1.proto import storage_resources_pb2 as resources_pb2
from google.cloud.storage_v1.proto.storage_resources_pb2 import CommonEnums


class TestObject(unittest.TestCase):
    def setUp(self):
        request = storage_pb2.InsertBucketRequest(bucket={"name": "bucket"})
        bucket, _ = gcs.bucket.Bucket.init(request, "")
        self.bucket = bucket

    def test_init_media(self):
        request = utils.common.FakeRequest(
            args={"name": "object"}, data=b"12345678", headers={}, environ={}
        )
        blob, _ = gcs.object.Object.init_media(request, self.bucket.metadata)
        self.assertEqual(blob.metadata.name, "object")
        self.assertEqual(blob.media, b"12345678")

    def test_init_multipart(self):
        request = utils.common.FakeRequest(
            args={},
            headers={"content-type": "multipart/related; boundary=foo_bar_baz"},
            data=b'--foo_bar_baz\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n{"name": "object", "metadata": {"key": "value"}}\r\n--foo_bar_baz\r\nContent-Type: image/jpeg\r\n\r\n123456789\r\n--foo_bar_baz--\r\n',
            environ={},
        )
        blob, _ = gcs.object.Object.init_multipart(request, self.bucket.metadata)
        self.assertEqual(blob.metadata.name, "object")
        self.assertEqual(blob.media, b"123456789")
        self.assertEqual(blob.metadata.metadata["key"], "value")
        self.assertEqual(blob.metadata.content_type, "image/jpeg")

    __REST_FIELDS_KEY_ONLY = [
        "owner",
        "timeCreated",
        "timeDeleted",
        "timeStorageClassUpdated",
        "updated",
    ]

    def test_grpc_to_rest(self):
        # Make sure that object created by `gRPC` works with `REST`'s request.
        spec = storage_pb2.InsertObjectSpec(
            resource=resources_pb2.Object(
                name="test-object-name",
                bucket="bucket",
                metadata={"label0": "value0"},
                cache_control="no-cache",
                content_disposition="test-value",
                content_encoding="test-value",
                content_language="test-value",
                content_type="octet-stream",
                storage_class="regional",
                customer_encryption=resources_pb2.Object.CustomerEncryption(
                    encryption_algorithm="AES", key_sha256="123456"
                ),
                # TODO(#6982) - add these fields when moving to storage/v2
                #   custom_time=utils.common.rest_rfc3339_to_proto("2021-08-01T12:00:00Z"),
                event_based_hold={"value": True},
                kms_key_name="test-value",
                retention_expiration_time=utils.common.rest_rfc3339_to_proto(
                    "2022-01-01T00:00:00Z"
                ),
                temporary_hold=True,
                time_deleted=utils.common.rest_rfc3339_to_proto("2021-06-01T00:00:00Z"),
                time_storage_class_updated=utils.common.rest_rfc3339_to_proto(
                    "2021-07-01T00:00:00Z"
                ),
            )
        )
        request = storage_pb2.StartResumableWriteRequest(insert_object_spec=spec)
        upload = gcs.holder.DataHolder.init_resumable_grpc(
            request, self.bucket.metadata, ""
        )
        blob, _ = gcs.object.Object.init(
            upload.request, upload.metadata, b"123456789", upload.bucket, False, ""
        )
        self.assertDictEqual(blob.rest_only, {})
        self.assertEqual(blob.metadata.bucket, "bucket")
        self.assertEqual(blob.metadata.name, "test-object-name")
        self.assertEqual(blob.media, b"123456789")

        # `REST` GET

        rest_metadata = blob.rest_metadata()
        self.assertEqual(rest_metadata["bucket"], "bucket")
        self.assertEqual(rest_metadata["name"], "test-object-name")
        self.assertIsNone(blob.metadata.metadata.get("method"))
        # Verify the ObjectAccessControl entries have the desired fields
        acl = rest_metadata.pop("acl", None)
        self.assertIsNotNone(acl)
        for entry in acl:
            self.assertEqual(entry.pop("kind", None), "storage#objectAccessControl")
            self.assertEqual(entry.pop("bucket", None), "bucket")
            self.assertEqual(entry.pop("object", None), "test-object-name")
            self.assertIsNotNone(entry.pop("entity", None))
            self.assertIsNotNone(entry.pop("role", None))
            # Verify the remaining keys are a subset of the expected keys
            self.assertLessEqual(
                set(entry.keys()),
                set(
                    [
                        "id",
                        "selfLink",
                        "generation",
                        "email",
                        "entityId",
                        "domain",
                        "projectTeam",
                        "etag",
                    ]
                ),
            )
        # Some fields we only care that they exist.
        for key in self.__REST_FIELDS_KEY_ONLY:
            self.assertIsNotNone(rest_metadata.pop(key, None), msg="key=%s" % key)
        # Some fields we need to manually extract to check their values
        generation = rest_metadata.pop("generation", None)
        self.assertIsNotNone(generation)
        self.assertEqual(
            "bucket/o/test-object-name#" + generation, rest_metadata.pop("id")
        )
        self.maxDiff = None
        self.assertDictEqual(
            rest_metadata,
            {
                "kind": "storage#object",
                "bucket": "bucket",
                "name": "test-object-name",
                "cacheControl": "no-cache",
                "contentDisposition": "test-value",
                "contentEncoding": "test-value",
                "contentLanguage": "test-value",
                "contentType": "octet-stream",
                "eventBasedHold": True,
                "crc32c": "4waSgw==",
                "customerEncryption": {
                    "encryptionAlgorithm": "AES",
                    "keySha256": "123456",
                },
                "kmsKeyName": "test-value",
                "md5Hash": "JfnnlDI7RTiF9RgfG2JNCw==",
                "metadata": {
                    "label0": "value0",
                    # The emulator adds useful annotations
                    "x_emulator_upload": "resumable",
                    "x_emulator_no_crc32c": "true",
                    "x_emulator_no_md5": "true",
                    "x_testbench_upload": "resumable",
                    "x_testbench_no_crc32c": "true",
                    "x_testbench_no_md5": "true",
                },
                "metageneration": "1",
                "retentionExpirationTime": "2022-01-01T00:00:00Z",
                "size": "9",
                "storageClass": "regional",
                "temporaryHold": True,
            },
        )

        # `REST` PATCH

        request = utils.common.FakeRequest(
            args={}, data=json.dumps({"metadata": {"method": "rest"}})
        )
        blob.patch(request, None)
        self.assertEqual(blob.metadata.metadata["method"], "rest")

    def test_rest_to_grpc(self):
        # Make sure that object created by `REST` works with `gRPC`'s request.
        metadata = {
            "bucket": "bucket",
            "name": "test-object-name",
            "metadata": {"method": "rest", "label0": "value0"},
            "cacheControl": "no-cache",
            "contentDisposition": "test-value",
            "contentEncoding": "test-value",
            "contentLanguage": "test-value",
            "contentType": "application/octet-stream",
            "eventBasedHold": True,
            "customerEncryption": {"encryptionAlgorithm": "AES", "keySha256": "123456"},
            "kmsKeyName": "test-value",
            "retentionExpirationTime": "2022-01-01T00:00:00Z",
            "temporaryHold": True,
            # These are a bit artificial, but good to test the
            # emulator preserves valid fields.
            "timeDeleted": "2021-07-01T01:02:03Z",
            "timeStorageClassUpdated": "2021-07-01T02:03:04Z",
            "storageClass": "regional",
        }
        boundary = "test_separator_deadbeef"
        payload = (
            ("--" + boundary + "\r\n").join(
                [
                    "",
                    # object metadata "part"
                    "\r\n".join(
                        [
                            "Content-Type: application/json; charset=UTF-8",
                            "",
                            json.dumps(metadata),
                            "",
                        ]
                    ),
                    # object media "part"
                    "\r\n".join(
                        [
                            "Content-Type: application/octet-stream",
                            "Content-Length: 9",
                            "",
                            "123456789",
                            "",
                        ]
                    ),
                ]
            )
            + "--"
            + boundary
            + "--\r\n"
        )
        request = utils.common.FakeRequest(
            args={},
            headers={"content-type": "multipart/related; boundary=" + boundary},
            data=payload.encode("UTF-8"),
            environ={},
        )
        blob, _ = gcs.object.Object.init_multipart(request, self.bucket.metadata)
        self.assertEqual(blob.metadata.bucket, "bucket")
        self.assertEqual(blob.metadata.name, "test-object-name")
        self.assertEqual(blob.media, b"123456789")
        self.assertEqual(blob.metadata.metadata["method"], "rest")
        rest_metadata = blob.rest_metadata()
        # Verify the ObjectAccessControl entries have the desired fields
        acl = rest_metadata.pop("acl", None)
        self.assertIsNotNone(acl)
        for entry in acl:
            self.assertEqual(entry.pop("kind", None), "storage#objectAccessControl")
            self.assertEqual(entry.pop("bucket", None), "bucket")
            self.assertEqual(entry.pop("object", None), "test-object-name")
            self.assertIsNotNone(entry.pop("entity", None))
            self.assertIsNotNone(entry.pop("role", None))
            # Verify the remaining keys are a subset of the expected keys
            self.assertLessEqual(
                set(entry.keys()),
                set(
                    [
                        "id",
                        "selfLink",
                        "generation",
                        "email",
                        "entityId",
                        "domain",
                        "projectTeam",
                        "etag",
                    ]
                ),
            )
        # Some fields we only care that they exist.
        for key in self.__REST_FIELDS_KEY_ONLY:
            self.assertIsNotNone(rest_metadata.pop(key, None), msg="key=%s" % key)
        # Some fields we need to manually extract to check their values
        generation = rest_metadata.pop("generation", None)
        self.assertIsNotNone(generation)
        self.assertEqual(
            "bucket/o/test-object-name#" + generation, rest_metadata.pop("id")
        )
        self.maxDiff = None
        self.assertDictEqual(
            rest_metadata,
            {
                "kind": "storage#object",
                "bucket": "bucket",
                "name": "test-object-name",
                "cacheControl": "no-cache",
                "contentDisposition": "test-value",
                "contentEncoding": "test-value",
                "contentLanguage": "test-value",
                "contentType": "application/octet-stream",
                "eventBasedHold": True,
                "crc32c": "4waSgw==",
                "customerEncryption": {
                    "encryptionAlgorithm": "AES",
                    "keySha256": "123456",
                },
                "kmsKeyName": "test-value",
                "md5Hash": "JfnnlDI7RTiF9RgfG2JNCw==",
                "metadata": {
                    "label0": "value0",
                    "method": "rest",
                    "x_emulator_upload": "multipart",
                    "x_testbench_upload": "multipart",
                },
                "metageneration": "1",
                "retentionExpirationTime": "2022-01-01T00:00:00Z",
                "size": "9",
                "storageClass": "regional",
                "temporaryHold": True,
            },
        )

        # `grpc` PATCH

        request = storage_pb2.PatchObjectRequest(
            bucket="bucket",
            object="object",
            metadata={"metadata": {"method": "grpc"}},
            update_mask={"paths": ["metadata"]},
        )
        blob.patch(request, "")
        self.assertEqual(blob.metadata.bucket, "bucket")
        self.assertEqual(blob.metadata.name, "test-object-name")
        self.assertEqual(blob.media, b"123456789")
        self.assertEqual(blob.metadata.metadata["method"], "grpc")

    def test_update_and_patch(self):
        # Because Object's `update` and `patch` are similar to Bucket'ones, we only
        # want to make sure that REST `UPDATE` and `PATCH` does not throw any exception.

        request = utils.common.FakeRequest(
            args={},
            headers={"content-type": "multipart/related; boundary=foo_bar_baz"},
            data=b'--foo_bar_baz\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n{"name": "object", "metadata": {"method": "rest"}}\r\n--foo_bar_baz\r\nContent-Type: image/jpeg\r\n\r\n123456789\r\n--foo_bar_baz--\r\n',
            environ={},
        )
        blob, _ = gcs.object.Object.init_multipart(request, self.bucket.metadata)
        self.assertEqual(blob.metadata.name, "object")
        self.assertEqual(blob.metadata.metadata["method"], "rest")
        self.assertEqual(blob.metadata.content_type, "image/jpeg")

        request = utils.common.FakeRequest(
            args={}, data=json.dumps({"metadata": {"method": "rest_update"}})
        )
        blob.update(request, None)
        self.assertEqual(blob.metadata.metadata["method"], "rest_update")
        # Modifiable fields will be replaced by default value when updating
        self.assertEqual(blob.metadata.content_type, "")

        request = utils.common.FakeRequest(
            args={}, data=json.dumps({"metadata": {"method": "rest_patch"}})
        )
        blob.patch(request, None)
        self.assertEqual(blob.metadata.metadata["method"], "rest_patch")


if __name__ == "__main__":
    unittest.main()
