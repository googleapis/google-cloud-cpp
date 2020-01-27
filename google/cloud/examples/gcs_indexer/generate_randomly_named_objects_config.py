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

"""Creates a k8s config to populate buckets."""

import argparse
import jinja2 as j2
import os
import time

template = j2.Template("""
apiVersion: batch/v1
kind: Job
metadata:
  name: generate-randomly-named-objects-{{timestamp}}
spec:
  parallelism: {{parallelism}}
  completions: {{task_count}}
  template:
    metadata:
      name: generate-randomly-named-objects-1
    spec:
      restartPolicy: OnFailure
      volumes:
        - name: service-account-key
          secret:
            secretName: service-account-key
      containers:
        - name: gcs-indexer-tools
          image: gcr.io/{{project_id}}/gcs-indexer-tools:latest
          imagePullPolicy: Always
          command: [
              '/r/generate_randomly_named_objects',
              '--object-count={{objects_per_task}}',
              '--task-count={{generator_threads}}',
              '--use-hash-prefix={{use_hash_prefix}}',
              '--bucket={{bucket}}'
          ]
          resources:
            requests:
              cpu: '250m'
              memory: '50Mi'
          volumeMounts:
            - name: service-account-key
              mountPath: /var/secrets/service-account-key
          env:
            - name: GOOGLE_APPLICATION_CREDENTIALS
              value: /var/secrets/service-account-key/key.json
""")


def check_positive(value):
    as_int = int(value)
    if as_int <= 0:
        raise argparse.ArgumentTypeError(
            '%s is not a valid positive integer value' % value)
    return as_int


parser = argparse.ArgumentParser()
parser.add_argument('--object-count', default=1_000_000, type=check_positive,
                    help='the number of objects created by the k8s')
parser.add_argument('--task-count', default=10, type=check_positive,
                    help='the number of parallel tasks')
parser.add_argument('--max-parallelism', default=10, type=check_positive,
                    help='the maximum number of parallel tasks')
parser.add_argument('--bucket', type=str, required=True,
                    help='the GCS bucket where the objects are created')
parser.add_argument('--use-hash-prefix', default=True, type=bool,
                    help='use a hash prefix for each object name')
parser.add_argument('--generator_threads', default=8, type=check_positive,
                    help='the number of threads used by the generator')
parser.add_argument('--project',
                    default=os.environ.get('GOOGLE_CLOUD_PROJECT'),
                    type=str, required=True,
                    help='configure the Google Cloud Project')
args = parser.parse_args()

object_count = args.object_count
task_count = args.task_count
objects_per_task = max(int(object_count / task_count), 1)
parallelism = min(task_count, args.max_parallelism)
print(template.render(bucket=args.bucket, object_count=objects_per_task,
                      objects_per_task=objects_per_task,
                      generator_threads=args.generator_threads,
                      use_hash_prefix=args.use_hash_prefix,
                      project_id=args.project, parallelism=parallelism,
                      task_count=task_count, timestamp=int(time.time())))
