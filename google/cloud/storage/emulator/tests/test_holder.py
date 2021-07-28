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

from werkzeug.test import create_environ
from werkzeug.wrappers import Request


class TestHolder(unittest.TestCase):
    def test_init_resumable_rest_incorrect_usage(self):
        bucket_metadata = json.dumps({"name": "bucket-test"})
        environ = create_environ(
            base_url="http://localhost:8080",
            content_length=len(bucket_metadata),
            data=bucket_metadata,
            content_type="application/json",
            method="POST",
        )

        bucket, _ = gcs.bucket.Bucket.init(Request(environ), None)
        bucket = bucket.metadata
        with self.assertRaises(utils.error.RestException) as cm:
            data = "{}"
            environ = create_environ(
                base_url="http://localhost:8080",
                content_length=len(data),
                data=data,
                content_type="application/json",
                method="POST",
            )
            upload = gcs.holder.DataHolder.init_resumable_rest(Request(environ), bucket)
        self.assertEqual(cm.exception.msg, "No object name is invalid.")

        with self.assertRaises(utils.error.RestException) as cm:
            data = ""
            environ = create_environ(
                base_url="http://localhost:8080",
                content_length=len(data),
                data=data,
                content_type="application/json",
                method="POST",
            )
            upload = gcs.holder.DataHolder.init_resumable_rest(Request(environ), bucket)
        self.assertEqual(cm.exception.msg, "No object name is invalid.")

        with self.assertRaises(utils.error.RestException) as cm:
            data = json.dumps({"name": ""})
            environ = create_environ(
                base_url="http://localhost:8080",
                query_string={"name": ""},
                content_length=len(data),
                data=data,
                content_type="application/json",
                method="POST",
            )
            upload = gcs.holder.DataHolder.init_resumable_rest(Request(environ), bucket)
        self.assertEqual(cm.exception.msg, "No object name is invalid.")

        with self.assertRaises(utils.error.RestException) as cm:
            data = json.dumps({"name": "a"})
            environ = create_environ(
                base_url="http://localhost:8080",
                query_string={"name": "b"},
                content_length=len(data),
                data=data,
                content_type="application/json",
                method="POST",
            )
            upload = gcs.holder.DataHolder.init_resumable_rest(Request(environ), bucket)
        self.assertEqual(
            cm.exception.msg,
            "Value 'a' in content does not agree with value 'b'. is invalid.",
        )

        with self.assertRaises(utils.error.RestException) as cm:
            data = json.dumps({"name": ""})
            environ = create_environ(
                base_url="http://localhost:8080",
                query_string={"name": "b"},
                content_length=len(data),
                data=data,
                content_type="application/json",
                method="POST",
            )
            upload = gcs.holder.DataHolder.init_resumable_rest(Request(environ), bucket)
        self.assertEqual(
            cm.exception.msg,
            "Value '' in content does not agree with value 'b'. is invalid.",
        )

        with self.assertRaises(utils.error.RestException) as cm:
            data = json.dumps({"name": "a"})
            environ = create_environ(
                base_url="http://localhost:8080",
                query_string={"name": ""},
                content_length=len(data),
                data=data,
                content_type="application/json",
                method="POST",
            )
            upload = gcs.holder.DataHolder.init_resumable_rest(Request(environ), bucket)
        self.assertEqual(
            cm.exception.msg,
            "Value 'a' in content does not agree with value ''. is invalid.",
        )

    def test_init_resumable_rest_correct_usage(self):
        bucket_metadata = json.dumps({"name": "bucket-test"})
        environ = create_environ(
            base_url="http://localhost:8080",
            content_length=len(bucket_metadata),
            data=bucket_metadata,
            content_type="application/json",
            method="POST",
        )

        bucket, _ = gcs.bucket.Bucket.init(Request(environ), None)
        bucket = bucket.metadata
        data = json.dumps({"name": "a"})
        environ = create_environ(
            base_url="http://localhost:8080",
            query_string={"name": "a"},
            content_length=len(data),
            data=data,
            content_type="application/json",
            method="POST",
        )
        upload = gcs.holder.DataHolder.init_resumable_rest(Request(environ), bucket)

        data = json.dumps({"name": "a"})
        environ = create_environ(
            base_url="http://localhost:8080",
            content_length=len(data),
            data=data,
            content_type="application/json",
            method="POST",
        )
        upload = gcs.holder.DataHolder.init_resumable_rest(Request(environ), bucket)

        environ = create_environ(
            base_url="http://localhost:8080",
            query_string={"name": "a"},
            content_type="application/json",
            method="POST",
        )
        upload = gcs.holder.DataHolder.init_resumable_rest(Request(environ), bucket)

        data = json.dumps({"name": None})
        environ = create_environ(
            base_url="http://localhost:8080",
            query_string={"name": "b"},
            content_length=len(data),
            data=data,
            content_type="application/json",
            method="POST",
        )
        upload = gcs.holder.DataHolder.init_resumable_rest(Request(environ), bucket)

    def test_init_resumable_grpc(self):
        request = storage_pb2.InsertBucketRequest(bucket={"name": "bucket"})
        bucket, _ = gcs.bucket.Bucket.init(request, "")
        bucket = bucket.metadata
        insert_object_spec = storage_pb2.InsertObjectSpec(
            resource={"name": "object", "bucket": "bucket"},
            predefined_acl=CommonEnums.PredefinedObjectAcl.OBJECT_ACL_PROJECT_PRIVATE,
            if_generation_not_match={"value": 1},
            if_metageneration_match={"value": 2},
            if_metageneration_not_match={"value": 3},
            projection=CommonEnums.Projection.FULL,
        )
        request = storage_pb2.InsertObjectRequest(
            insert_object_spec=insert_object_spec, write_offset=0
        )
        upload = gcs.holder.DataHolder.init_resumable_grpc(request, bucket, "")
        # Verify the annotations inserted by the emulator.
        annotations = upload.metadata.metadata
        self.assertGreaterEqual(
            set(["x_emulator_upload", "x_emulator_no_crc32c", "x_emulator_no_md5"]),
            set(annotations.keys()),
        )
        # Clear any annotations created by the emulator
        upload.metadata.metadata.clear()
        self.assertEqual(
            upload.metadata, resources_pb2.Object(name="object", bucket="bucket")
        )
        predefined_acl = utils.acl.extract_predefined_acl(upload.request, False, "")
        self.assertEqual(
            predefined_acl, CommonEnums.PredefinedObjectAcl.OBJECT_ACL_PROJECT_PRIVATE
        )
        match, not_match = utils.generation.extract_precondition(
            upload.request, False, False, ""
        )
        self.assertIsNone(match)
        self.assertEqual(not_match, 1)
        match, not_match = utils.generation.extract_precondition(
            upload.request, True, False, ""
        )
        self.assertEqual(match, 2)
        self.assertEqual(not_match, 3)
        projection = utils.common.extract_projection(upload.request, False, "")
        self.assertEqual(projection, CommonEnums.Projection.FULL)

    def test_init_resumable_rest(self):
        request = storage_pb2.InsertBucketRequest(bucket={"name": "bucket"})
        bucket, _ = gcs.bucket.Bucket.init(request, "")
        bucket = bucket.metadata
        data = json.dumps(
            {
                # Empty payload checksums
                "crc32c": "AAAAAA==",
                "md5Hash": "1B2M2Y8AsgTpgAmY7PhCfg==",
                "name": "test-object-name",
            }
        )
        environ = create_environ(
            base_url="http://localhost:8080",
            content_length=len(data),
            data=data,
            content_type="application/json",
            method="POST",
        )
        upload = gcs.holder.DataHolder.init_resumable_rest(Request(environ), bucket)
        self.assertEqual(upload.metadata.name, "test-object-name")
        self.assertEqual(upload.metadata.crc32c.value, 0)
        self.assertEqual(upload.metadata.md5_hash, "1B2M2Y8AsgTpgAmY7PhCfg==")


if __name__ == "__main__":
    unittest.main()
