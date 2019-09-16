#!/usr/bin/env bash
# Copyright 2019 Google LLC
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

set -eu

sed -e "s/@PROJECT_ID@/${PROJECT_ID}/" \
    -e "s/@ID@/$(date +%s)-${RANDOM}/" \
    -e "s/@SRC_BUCKET_NAME@/${SRC_BUCKET_NAME}/" \
    -e "s/@DST_BUCKET_NAME@/${DST_BUCKET_NAME}/" <<'_EOF_'
apiVersion: batch/v1
kind: Job
metadata:
  name: regression-@ID@
  labels:
    app: read-object-stall-regression
spec:
  parallelism: 500
  completions: 500
  template:
    metadata:
      annotations:
        # https://kubernetes.io/docs/tutorials/clusters/apparmor/
        container.apparmor.security.beta.kubernetes.io/read-object-stall-regression: "unconfined"
    spec:
      restartPolicy: OnFailure
      activeDeadlineSeconds: 42300
      volumes:
        - name: google-cloud-key
          secret:
            secretName: service-account-key
        - name: proc
          hostPath:
            path: /proc
      securityContext:
        runAsUser: 0
        fsGroup: 0
      containers:
        - name: read-object-stall-regression
          image: gcr.io/@PROJECT_ID@/read-object-stall-regression:latest
          imagePullPolicy: Always
          args: [
            '/r/read_object_stall_test',
            '@SRC_BUCKET_NAME@',
            '@DST_BUCKET_NAME@',
            '4.0',
            '32',
            '--gtest_filter=ReadObjectStallTest.Streaming',
            '--gtest_repeat=2'
          ]
          securityContext:
              privileged: true
              capabilities:
                  add: ["NET_ADMIN", "SYS_ADMIN", "SYS_PTRACE"]
              allowPrivilegeEscalation: true
          resources:
            requests:
              memory: "40Mi"
              cpu: "82m"
              ephemeral-storage: "128Mi"
          volumeMounts:
            - name: google-cloud-key
              mountPath: /var/secrets/google
            - name: proc
              readOnly: true
              mountPath: /host/proc
              mountPropagation: HostToContainer
          env:
            - name: POD_NAME
              valueFrom:
                fieldRef:
                  fieldPath: metadata.name
            - name: GOOGLE_APPLICATION_CREDENTIALS
              value: /var/secrets/google/key.json
_EOF_
