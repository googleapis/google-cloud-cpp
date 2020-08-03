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


def insert_object(value, data):
    GCS_OBJECTS[value.bucket + "/o/" + value.name] = {"object": value, "data": data}
    return value


def get_object(bucket_name, object_name, value=None):
    return GCS_OBJECTS.get(bucket_name + "/o/" + object_name, value)


def has_object(bucket_name, object_name):
    return GCS_OBJECTS.get(bucket_name + "/o/" + object_name) is not None


def delete_object(bucket_name, object_name):
    del GCS_OBJECTS[bucket_name + "/o/" + object_name]
