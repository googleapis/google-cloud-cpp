#!/usr/bin/env python3
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

    def test_grpc_to_rest(self):
        # Make sure that object created by `gRPC` works with `REST`'s request.
        spec = storage_pb2.InsertObjectSpec(
            resource={"name": "object", "bucket": "bucket"}
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
        self.assertEqual(blob.metadata.name, "object")
        self.assertEqual(blob.media, b"123456789")

        # `REST` GET

        rest_metadata = blob.rest_metadata()
        self.assertEqual(rest_metadata["bucket"], "bucket")
        self.assertEqual(rest_metadata["name"], "object")
        self.assertIsNone(blob.metadata.metadata.get("method"))

        # `REST` PATCH

        request = utils.common.FakeRequest(
            args={}, data=json.dumps({"metadata": {"method": "rest"}})
        )
        blob.patch(request, None)
        self.assertEqual(blob.metadata.metadata["method"], "rest")

    def test_rest_to_grpc(self):
        # Make sure that object created by `REST` works with `gRPC`'s request.
        request = utils.common.FakeRequest(
            args={},
            headers={"content-type": "multipart/related; boundary=foo_bar_baz"},
            data=b'--foo_bar_baz\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n{"name": "object", "metadata": {"method": "rest"}}\r\n--foo_bar_baz\r\nContent-Type: image/jpeg\r\n\r\n123456789\r\n--foo_bar_baz--\r\n',
            environ={},
        )
        blob, _ = gcs.object.Object.init_multipart(request, self.bucket.metadata)
        self.assertEqual(blob.metadata.bucket, "bucket")
        self.assertEqual(blob.metadata.name, "object")
        self.assertEqual(blob.media, b"123456789")
        self.assertEqual(blob.metadata.metadata["method"], "rest")

        # `grpc` PATCH

        request = storage_pb2.PatchObjectRequest(
            bucket="bucket",
            object="object",
            metadata={"metadata": {"method": "grpc"}},
            update_mask={"paths": ["metadata"]},
        )
        blob.patch(request, "")
        self.assertEqual(blob.metadata.bucket, "bucket")
        self.assertEqual(blob.metadata.name, "object")
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
