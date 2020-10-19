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

import gcs
import utils
import simdjson
import os
from google.cloud.storage_v1.proto import storage_pb2 as storage_pb2


class Database:
    def __init__(self, buckets):
        self.buckets = buckets

    @classmethod
    def init(cls):
        return cls({})

    # === BUCKET === #

    def __check_bucket_metageneration(self, request, bucket, context):
        metageneration = bucket.metadata.metageneration
        match, not_match = utils.generation.extract_precondition(
            request, True, False, context
        )
        utils.generation.check_precondition(
            metageneration, match, not_match, True, context
        )

    def __get_bucket_without_generation(self, bucket_name, context):
        bucket = self.buckets.get(bucket_name)
        if bucket is None:
            utils.error.notfound("Bucket %s" % bucket_name, context)
        return bucket

    def insert_bucket(self, request, bucket, context):
        self.buckets[bucket.metadata.name] = bucket

    def get_bucket(self, request, bucket_name, context):
        bucket = self.__get_bucket_without_generation(bucket_name, context)
        self.__check_bucket_metageneration(request, bucket, context)
        return bucket

    def list_bucket(self, request, project_id, context):
        if project_id is None or project_id.endswith("-"):
            utils.error.invalid("Project id %s" % project_id, context)
        return self.buckets.values()

    def delete_bucket(self, request, bucket_name, context):
        bucket = self.get_bucket(request, bucket_name, context)
        del self.buckets[bucket.metadata.name]

    def insert_test_bucket(self, context):
        bucket_name = os.environ.get(
            "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME", "bucket"
        )
        if self.buckets.get(bucket_name) is None:
            if context is not None:
                request = storage_pb2.InsertBucketRequest(bucket={"name": bucket_name})
            else:
                request = utils.common.FakeRequest(
                    args={}, data=simdjson.dumps({"name": bucket_name})
                )
            bucket_test, _ = gcs.bucket.Bucket.init(request, context)
            self.insert_bucket(request, bucket_test, context)
            bucket_test.metadata.metageneration = 4
            bucket_test.metadata.versioning.enabled = True
