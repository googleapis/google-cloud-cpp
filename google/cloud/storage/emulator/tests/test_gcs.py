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

import gcs
import pytest
import utils

from google.cloud.storage_v1.proto import storage_pb2 as storage_pb2
from google.protobuf import json_format


class TestBucket:
    def test_init_grpc(self):
        request = storage_pb2.InsertBucketRequest(bucket={"name": "bucket"})
        bucket, projection = gcs.bucket.Bucket.init(request, "")
        assert bucket.metadata.name == "bucket"
        assert projection == 1
        assert list(bucket.metadata.acl) == utils.acl.compute_predefined_bucket_acl(
            "bucket", 3, ""
        )
        assert list(
            bucket.metadata.default_object_acl
        ) == utils.acl.compute_predefined_default_object_acl("bucket", 5, "")

        # WITH ACL
        request = storage_pb2.InsertBucketRequest(
            bucket={
                "name": "bucket",
                "acl": utils.acl.compute_predefined_bucket_acl("bucket", 1, ""),
            }
        )
        bucket, projection = gcs.bucket.Bucket.init(request, "")
        assert bucket.metadata.name == "bucket"
        assert projection == 2
        assert list(bucket.metadata.acl) == utils.acl.compute_predefined_bucket_acl(
            "bucket", 1, ""
        )
        assert list(
            bucket.metadata.default_object_acl
        ) == utils.acl.compute_predefined_default_object_acl("bucket", 5, "")

        # WITH PREDEFINED ACL
        request = storage_pb2.InsertBucketRequest(
            bucket={"name": "bucket"}, predefined_acl=5
        )
        bucket, projection = gcs.bucket.Bucket.init(request, "")
        assert bucket.metadata.name == "bucket"
        assert projection == 1
        assert list(bucket.metadata.acl) == utils.acl.compute_predefined_bucket_acl(
            "bucket", 5, ""
        )
        assert list(
            bucket.metadata.default_object_acl
        ) == utils.acl.compute_predefined_default_object_acl("bucket", 5, "")

        # WITH ACL AND PREDEFINED ACL
        request = storage_pb2.InsertBucketRequest(
            bucket={
                "name": "bucket",
                "acl": utils.acl.compute_predefined_bucket_acl("bucket", 1, ""),
            },
            predefined_acl=2,
        )
        bucket, projection = gcs.bucket.Bucket.init(request, "")
        assert bucket.metadata.name == "bucket"
        assert projection == 2
        assert list(bucket.metadata.acl) == utils.acl.compute_predefined_bucket_acl(
            "bucket", 1, ""
        )
        assert list(
            bucket.metadata.default_object_acl
        ) == utils.acl.compute_predefined_default_object_acl("bucket", 5, "")

    def test_init_rest(self):
        request = utils.common.FakeRequest(args={}, data=json.dumps({"name": "bucket"}))
        bucket, projection = gcs.bucket.Bucket.init(request, None)
        assert bucket.metadata.name == "bucket"
        assert projection == "noAcl"

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
        assert bucket.metadata.name == "bucket"
        assert projection == "full"
        assert list(bucket.metadata.acl) == utils.acl.compute_predefined_bucket_acl(
            "bucket", "authenticatedRead", None
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
        assert bucket.metadata.labels.get("init") == "true"
        assert bucket.metadata.labels.get("patch") == "false"
        assert bucket.metadata.labels.get("method") is None
        assert bucket.metadata.website.main_page_suffix == ""
        assert bucket.metadata.website.not_found_page == "notfound.html"

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
        assert bucket.metadata.labels.get("init") is None
        assert bucket.metadata.labels.get("patch") == "true"
        assert bucket.metadata.labels.get("method") == "grpc"
        assert bucket.metadata.website.main_page_suffix == "bucket"
        # `update_mask` does not update `website.not_found_page`
        assert bucket.metadata.website.not_found_page == "notfound.html"

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
        assert bucket.metadata.name == "bucket"
        # REST can update a part of map field.
        assert bucket.metadata.labels.get("init") is None
        assert bucket.metadata.labels.get("patch") == "true"
        assert bucket.metadata.labels.get("method") == "rest"
        assert bucket.metadata.website.main_page_suffix == "bucket"
        assert bucket.metadata.website.not_found_page == "404.html"

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
        assert acl.entity == entity
        assert acl.email == "bucket.acl@example.com"
        assert acl.role == "READER"

        request = storage_pb2.PatchBucketAccessControlRequest(
            bucket="bucket", entity=entity, bucket_access_control={"role": "OWNER"}
        )
        bucket.patch_acl(request, entity, "")
        acl = bucket.get_acl(entity, "")
        assert acl.entity == entity
        assert acl.email == "bucket.acl@example.com"
        assert acl.role == "OWNER"

        bucket.delete_acl(entity, "")
        for acl in bucket.metadata.acl:
            assert acl.entity != entity

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
        assert acl.entity == entity
        assert acl.email == "bucket.default_object_acl@example.com"
        assert acl.role == "READER"

        request = storage_pb2.PatchDefaultObjectAccessControlRequest(
            bucket="bucket", entity=entity, object_access_control={"role": "OWNER"}
        )
        bucket.patch_default_object_acl(request, entity, "")
        acl = bucket.get_default_object_acl(entity, "")
        assert acl.entity == entity
        assert acl.email == "bucket.default_object_acl@example.com"
        assert acl.role == "OWNER"

        bucket.delete_default_object_acl(entity, "")
        for acl in bucket.metadata.default_object_acl:
            assert acl.entity != entity


class TestObject:
    def testInitMedia(self):
        request = storage_pb2.InsertBucketRequest(bucket={"name": "bucket"})
        bucket, projection = gcs.bucket.Bucket.init(request, "")
        request = utils.common.FakeRequest(
            args={"name": "object"}, data=b"12345678", headers={}
        )
        blob, _ = gcs.object.Object.init_media(request, bucket.metadata)
        assert blob.metadata.name == "object"
        assert blob.media == b"12345678"

    def testInitMultiPart(self):
        request = storage_pb2.InsertBucketRequest(bucket={"name": "bucket"})
        bucket, _ = gcs.bucket.Bucket.init(request, "")
        request = utils.common.FakeRequest(
            args={},
            headers={"content-type": "multipart/related; boundary=foo_bar_baz"},
            data=b'--foo_bar_baz\r\nContent-Type: application/json; charset=UTF-8\r\n{"name": "object", "metadata": {"key": "value"}}\r\n--foo_bar_baz\r\nContent-Type: image/jpeg\r\n123456789\r\n--foo_bar_baz--\r\n',
        )
        blob, _ = gcs.object.Object.init_multipart(request, bucket.metadata)
        assert blob.metadata.name == "object"
        assert blob.media == b"123456789"
        assert blob.metadata.metadata["key"] == "value"


def run():
    pytest.main(["-v"])
