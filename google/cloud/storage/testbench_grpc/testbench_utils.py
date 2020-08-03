#!/usr/bin/env python
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
"""Standalone helpers for the GCS+gRPC plugin test bench."""

import base64

# Define the collection of Buckets indexed by <bucket_name>
GCS_BUCKETS = dict()


def has_bucket(bucket_name):
    return GCS_BUCKETS.get(bucket_name) is not None


def insert_bucket(bucket):
    GCS_BUCKETS[bucket.name] = bucket
    return bucket


def all_buckets():
    return GCS_BUCKETS.items()


def get_bucket(bucket_name, bucket=None):
    return GCS_BUCKETS.get(bucket_name, bucket)


def delete_bucket(name):
    del GCS_BUCKETS[name]


# Define the collection of Objects indexed by <bucket_name>/o/<object_name>
GCS_OBJECTS = dict()


def make_object_wrapper(object_value, content, finish_write=False):
    return {
        "object": object_value,
        "content": content,
        "crc32c": object_value.crc32c,
        "md5_hash": object_value.md5_hash,
        "finish_write": finish_write,
    }


def insert_object(bucket_name, object_name, object_wrapper):
    GCS_OBJECTS[bucket_name + "/o/" + object_name] = object_wrapper
    return object_wrapper["object"]


def get_object(bucket_name, object_name, object_value=None):
    return GCS_OBJECTS.get(bucket_name + "/o/" + object_name, object_value)


def has_object(bucket_name, object_name):
    return GCS_OBJECTS.get(bucket_name + "/o/" + object_name) is not None


def delete_object(bucket_name, object_name):
    del GCS_OBJECTS[bucket_name + "/o/" + object_name]


# Define the collection of upload_id indexed by <upload_id>
GCS_UPLOAD_IDS = dict()


def make_upload_wrapper(upload_id, bucket_name, object_name, content, complete=False):
    return {
        "upload_id": upload_id,
        "content": content,
        "complete": complete,
        "bucket_name": bucket_name,
        "object_name": object_name,
    }


def insert_upload(bucket_name, object_name):
    upload_id = base64.b64encode(
        bytearray(bucket_name + "/o/" + object_name, "utf-8")
    ).decode("utf-8")
    GCS_UPLOAD_IDS[upload_id] = make_upload_wrapper(
        upload_id, bucket_name, object_name, bytearray("", "utf-8")
    )
    return upload_id


def get_upload(upload_id, value=None):
    return GCS_UPLOAD_IDS.get(upload_id, value)


def has_upload(upload_id):
    return GCS_UPLOAD_IDS.get(upload_id) is not None


def delete_upload(upload_id):
    del GCS_UPLOAD_IDS[upload_id]
