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

"""Creates a k8s config to run a single refresh_bucket task."""

import argparse
import jinja2 as j2
import os
import random
import time

template = j2.Template("""
apiVersion: batch/v1
kind: Job
metadata:
  name: refresh-bucket-{{timestamp}}-{{random}}
spec:
  completions: 1
  template:
    metadata:
      name: refresh-bucket-worker
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
              '/r/refresh_bucket',
              '--project={{project_id}}',
              '--instance={{instance}}',
              '--database={{database}}',
              '--reader-threads={{reader_threads}}',
              '--worker-threads={{worker_threads}}',
              {{ prefixes|join(', ') }}
          ]
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
parser.add_argument('--project',
                    default=os.environ.get('GOOGLE_CLOUD_PROJECT'),
                    type=str, required=True,
                    help='configure the Google Cloud Project')
parser.add_argument('--instance',
                    type=str, required=True,
                    help='configure the Cloud Spanner instance id')
parser.add_argument('--database',
                    type=str, required=True,
                    help='configure the Cloud Spanner database id')
parser.add_argument('prefixes', type=str, nargs='+',
                    help='the GCS bucket where the objects are created')
args = parser.parse_args()

reader_threads = 2 if len(args.prefixes) > 1 else 1
worker_threads = 8 if len(args.prefixes) > 1 else 4

print(template.render(project_id=args.project, instance=args.instance, database=args.database,
                      reader_threads=reader_threads, worker_threads=worker_threads,
                      prefixes=["'%s'" % prefix for prefix in args.prefixes],
                      timestamp=int(time.time()), random=random.randint(0, 100000)))
