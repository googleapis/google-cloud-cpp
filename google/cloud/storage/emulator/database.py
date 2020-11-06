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

import json
import os

import gcs
import utils

from google.cloud.storage_v1.proto import storage_pb2 as storage_pb2


class Database:
    def __init__(self, buckets, objects, live_generations, uploads, rewrites):
        self.buckets = buckets
        self.objects = objects
        self.live_generations = live_generations
        self.uploads = uploads
        self.rewrites = rewrites

    @classmethod
    def init(cls):
        return cls({}, {}, {}, {}, {})

    def raii(self, grpc_server):
        self.grpc_server = grpc_server

    def __del__(self):
        if hasattr(self, "grpc_server") and self.grpc_server is not None:
            self.grpc_server.stop(None)

    # === BUCKET === #

    def __check_bucket_metageneration(self, request, bucket, context):
        metageneration = bucket.metadata.metageneration
        match, not_match = utils.generation.extract_precondition(
            request, True, False, context
        )
        utils.generation.check_precondition(
            metageneration, match, not_match, True, context
        )

    def get_bucket_without_generation(self, bucket_name, context):
        bucket = self.buckets.get(bucket_name)
        if bucket is None:
            utils.error.notfound("Bucket %s" % bucket_name, context)
        return bucket

    def insert_bucket(self, request, bucket, context):
        self.buckets[bucket.metadata.name] = bucket
        self.objects[bucket.metadata.name] = {}
        self.live_generations[bucket.metadata.name] = {}

    def get_bucket(self, request, bucket_name, context):
        bucket = self.get_bucket_without_generation(bucket_name, context)
        self.__check_bucket_metageneration(request, bucket, context)
        return bucket

    def list_bucket(self, request, project_id, context):
        if project_id is None or project_id.endswith("-"):
            utils.error.invalid("Project id %s" % project_id, context)
        return self.buckets.values()

    def delete_bucket(self, request, bucket_name, context):
        bucket = self.get_bucket(request, bucket_name, context)
        if len(self.live_generations[bucket.metadata.name]) > 0:
            utils.error.invalid("Deleting non-empty bucket", context)
        del self.buckets[bucket.metadata.name]
        del self.objects[bucket.metadata.name]
        del self.live_generations[bucket.metadata.name]

    def insert_test_bucket(self, context):
        bucket_name = os.environ.get(
            "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME", "bucket"
        )
        if self.buckets.get(bucket_name) is None:
            if context is not None:
                request = storage_pb2.InsertBucketRequest(bucket={"name": bucket_name})
            else:
                request = utils.common.FakeRequest(
                    args={}, data=json.dumps({"name": bucket_name})
                )
            bucket_test, _ = gcs.bucket.Bucket.init(request, context)
            self.insert_bucket(request, bucket_test, context)
            bucket_test.metadata.metageneration = 4
            bucket_test.metadata.versioning.enabled = True

    # === OBJECT === #

    def __get_bucket_for_object(self, bucket_name, context):
        bucket = self.objects.get(bucket_name)
        if bucket is None:
            utils.error.notfound("Bucket %s" % bucket_name, context)
        return bucket

    @classmethod
    def __extract_list_object_request(cls, request, context):
        delimiter, prefix, versions = "", "", False
        start_offset, end_offset = "", None
        if context is not None:
            delimiter = request.delimiter
            prefix = request.prefix
            versions = request.versions
        else:
            delimiter = request.args.get("delimiter", "")
            prefix = request.args.get("prefix", "")
            versions = request.args.get("versions", False, type=bool)
            start_offset = request.args.get("startOffset", "")
            end_offset = request.args.get("endOffset")
        return delimiter, prefix, versions, start_offset, end_offset

    def list_object(self, request, bucket_name, context):
        bucket = self.__get_bucket_for_object(bucket_name, context)
        (
            delimiter,
            prefix,
            versions,
            start_offset,
            end_offset,
        ) = self.__extract_list_object_request(request, context)
        items = []
        rest_onlys = []
        prefixes = set()
        for obj in bucket.values():
            generation = obj.metadata.generation
            name = obj.metadata.name
            if not versions and generation != self.live_generations[bucket_name].get(
                name
            ):
                continue
            if name.find(prefix) != 0:
                continue
            if name < start_offset:
                continue
            if end_offset is not None and name >= end_offset:
                continue
            delimiter_index = name.find(delimiter, len(prefix))
            if delimiter != "" and delimiter_index > 0:
                prefixes.add(name[: delimiter_index + 1])
                continue
            items.append(obj.metadata)
            rest_onlys.append(obj.rest_only)
        return items, list(prefixes), rest_onlys

    def check_object_generation(
        self, request, bucket_name, object_name, is_source, context
    ):
        bucket = self.__get_bucket_for_object(bucket_name, context)
        generation = utils.generation.extract_generation(request, is_source, context)
        if generation == 0:
            generation = self.live_generations[bucket_name].get(object_name, 0)
        match, not_match = utils.generation.extract_precondition(
            request, False, is_source, context
        )
        utils.generation.check_precondition(
            generation, match, not_match, False, context
        )
        blob = bucket.get("%s#%d" % (object_name, generation))
        metageneration = blob.metadata.metageneration if blob is not None else None
        match, not_match = utils.generation.extract_precondition(
            request, True, is_source, context
        )
        utils.generation.check_precondition(
            metageneration, match, not_match, True, context
        )
        return blob, generation

    def get_object(self, request, bucket_name, object_name, is_source, context):
        blob, generation = self.check_object_generation(
            request, bucket_name, object_name, is_source, context
        )
        if blob is None:
            if generation == 0:
                utils.error.notfound("Live version of object %s" % object_name, context)
            else:
                utils.error.notfound(
                    "Object %s with generation %d" % (object_name, generation), context
                )
        return blob

    def insert_object(self, request, bucket_name, blob, context):
        self.check_object_generation(
            request, bucket_name, blob.metadata.name, False, context
        )
        bucket = self.__get_bucket_for_object(bucket_name, context)
        name = blob.metadata.name
        generation = blob.metadata.generation
        bucket["%s#%d" % (name, generation)] = blob
        self.live_generations[bucket_name][name] = generation

    def delete_object(self, request, bucket_name, object_name, context):
        _ = self.get_object(request, bucket_name, object_name, False, context)
        generation = utils.generation.extract_generation(request, False, context)
        live_generation = self.live_generations[bucket_name].get(object_name)
        if generation == 0 or live_generation == generation:
            self.live_generations[bucket_name].pop(object_name, None)
        if generation != 0:
            del self.objects[bucket_name]["%s#%d" % (object_name, generation)]

    # === UPLOAD === #

    def get_upload(self, upload_id, context):
        upload = self.uploads.get(upload_id)
        if upload is None:
            utils.error.notfound("Upload %s" % upload_id, context)
        return upload

    def insert_upload(self, upload):
        self.uploads[upload.upload_id] = upload

    def delete_upload(self, upload_id, context):
        self.get_upload(upload_id, context)
        del self.uploads[upload_id]

    # === REWRITE === #

    def get_rewrite(self, token, context):
        rewrite = self.rewrites.get(token)
        if rewrite is None:
            utils.error.notfound(404, "Rewrite %s" % token, context)
        return rewrite

    def insert_rewrite(self, rewrite):
        self.rewrites[rewrite.token] = rewrite

    def delete_rewrite(self, token, context):
        self.get_rewrite(token, context)
        del self.rewrites[token]
