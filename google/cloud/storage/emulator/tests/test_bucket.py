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

"""Tests for the Bucket class (see gcs/bucket.py)."""

import json
import unittest

import gcs
import utils

from google.cloud.storage_v1.proto import storage_pb2 as storage_pb2
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


if __name__ == "__main__":
    unittest.main()
