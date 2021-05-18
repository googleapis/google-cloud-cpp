#!/usr/bin/env python3
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

"""Unit test for utils"""

import json
import unittest

import gcs
import utils

from werkzeug.test import create_environ
from werkzeug.wrappers import Request

from google.cloud.storage_v1.proto import storage_pb2 as storage_pb2
from google.cloud.storage_v1.proto import storage_resources_pb2 as resources_pb2
from google.cloud.storage_v1.proto.storage_resources_pb2 import CommonEnums
from google.protobuf import json_format


class TestBucket(unittest.TestCase):
    def test_init_grpc(self):
        request = storage_pb2.InsertBucketRequest(bucket={"name": "bucket"})
        bucket, projection = gcs.bucket.Bucket.init(request, "")
        self.assertEqual(bucket.metadata.name, "bucket")
        self.assertEqual(projection, CommonEnums.Projection.NO_ACL)
        self.assertListEqual(
            list(bucket.metadata.acl),
            utils.acl.compute_predefined_bucket_acl(
                "bucket", CommonEnums.PredefinedBucketAcl.BUCKET_ACL_PROJECT_PRIVATE, ""
            ),
        )
        self.assertListEqual(
            list(bucket.metadata.default_object_acl),
            utils.acl.compute_predefined_default_object_acl(
                "bucket", CommonEnums.PredefinedObjectAcl.OBJECT_ACL_PROJECT_PRIVATE, ""
            ),
        )

        # WITH ACL
        request = storage_pb2.InsertBucketRequest(
            bucket={
                "name": "bucket",
                "acl": utils.acl.compute_predefined_bucket_acl(
                    "bucket",
                    CommonEnums.PredefinedBucketAcl.BUCKET_ACL_AUTHENTICATED_READ,
                    "",
                ),
            }
        )
        bucket, projection = gcs.bucket.Bucket.init(request, "")
        self.assertEqual(bucket.metadata.name, "bucket")
        self.assertEqual(projection, CommonEnums.Projection.FULL)
        self.assertEqual(
            list(bucket.metadata.acl),
            utils.acl.compute_predefined_bucket_acl(
                "bucket",
                CommonEnums.PredefinedBucketAcl.BUCKET_ACL_AUTHENTICATED_READ,
                "",
            ),
        )
        self.assertListEqual(
            list(bucket.metadata.default_object_acl),
            utils.acl.compute_predefined_default_object_acl(
                "bucket", CommonEnums.PredefinedObjectAcl.OBJECT_ACL_PROJECT_PRIVATE, ""
            ),
        )

        # WITH PREDEFINED ACL
        request = storage_pb2.InsertBucketRequest(
            bucket={"name": "bucket"},
            predefined_acl=CommonEnums.PredefinedBucketAcl.BUCKET_ACL_PUBLIC_READ_WRITE,
        )
        bucket, projection = gcs.bucket.Bucket.init(request, "")
        self.assertEqual(bucket.metadata.name, "bucket")
        self.assertEqual(projection, CommonEnums.Projection.NO_ACL)
        self.assertEqual(
            list(bucket.metadata.acl),
            utils.acl.compute_predefined_bucket_acl(
                "bucket",
                CommonEnums.PredefinedBucketAcl.BUCKET_ACL_PUBLIC_READ_WRITE,
                "",
            ),
        )
        self.assertListEqual(
            list(bucket.metadata.default_object_acl),
            utils.acl.compute_predefined_default_object_acl(
                "bucket", CommonEnums.PredefinedObjectAcl.OBJECT_ACL_PROJECT_PRIVATE, ""
            ),
        )

        # WITH ACL AND PREDEFINED ACL
        request = storage_pb2.InsertBucketRequest(
            bucket={
                "name": "bucket",
                "acl": utils.acl.compute_predefined_bucket_acl(
                    "bucket",
                    CommonEnums.PredefinedBucketAcl.BUCKET_ACL_AUTHENTICATED_READ,
                    "",
                ),
            },
            predefined_acl=CommonEnums.PredefinedBucketAcl.BUCKET_ACL_PRIVATE,
        )
        bucket, projection = gcs.bucket.Bucket.init(request, "")
        self.assertEqual(bucket.metadata.name, "bucket")
        self.assertEqual(projection, CommonEnums.Projection.FULL)
        self.assertEqual(
            list(bucket.metadata.acl),
            utils.acl.compute_predefined_bucket_acl(
                "bucket",
                CommonEnums.PredefinedBucketAcl.BUCKET_ACL_AUTHENTICATED_READ,
                "",
            ),
        )
        self.assertListEqual(
            list(bucket.metadata.default_object_acl),
            utils.acl.compute_predefined_default_object_acl(
                "bucket", CommonEnums.PredefinedObjectAcl.OBJECT_ACL_PROJECT_PRIVATE, ""
            ),
        )

    def test_grpc_to_rest(self):
        request = storage_pb2.InsertBucketRequest(bucket={"name": "bucket"})
        bucket, projection = gcs.bucket.Bucket.init(request, "")
        self.assertEqual(bucket.metadata.name, "bucket")

        # `REST` GET

        rest_metadata = bucket.rest()
        self.assertEqual(rest_metadata["name"], "bucket")
        self.assertIsNone(bucket.metadata.labels.get("method"))

        # `REST` PATCH

        request = utils.common.FakeRequest(
            args={}, data=json.dumps({"labels": {"method": "rest"}})
        )
        bucket.patch(request, None)
        self.assertEqual(bucket.metadata.labels["method"], "rest")

    def test_init_rest(self):
        request = utils.common.FakeRequest(args={}, data=json.dumps({"name": "bucket"}))
        bucket, projection = gcs.bucket.Bucket.init(request, None)
        self.assertEqual(bucket.metadata.name, "bucket")
        self.assertEqual(projection, "noAcl")

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


if __name__ == "__main__":
    unittest.main()
