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

import pytest
import utils
import gcs
import simdjson
from google.protobuf import json_format
from google.cloud.storage_v1.proto import storage_pb2 as storage_pb2


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
        request = utils.common.FakeRequest(
            args={}, data=simdjson.dumps({"name": "bucket"})
        )
        bucket, projection = gcs.bucket.Bucket.init(request, None)
        assert bucket.metadata.name == "bucket"
        assert projection == "noAcl"

        request = utils.common.FakeRequest(
            args={},
            data=simdjson.dumps(
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


def run():
    pytest.main(["-v"])
