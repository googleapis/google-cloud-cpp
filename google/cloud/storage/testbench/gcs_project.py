#!/usr/bin/env python
# Copyright 2019 Google Inc.
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
"""Implement a class to simulate projects (service accounts and HMAC keys)."""

import base64
import error_response
import flask
import random
import testbench_utils
import time


class ServiceAccount(object):
    """Represent a service account and its HMAC keys."""
    key_id_generator = 20000

    @classmethod
    def next_key_id(cls):
        cls.key_id_generator += 1
        return 'key-id-%d' % cls.key_id_generator

    def __init__(self, email):
        self.email = email
        self.keys = {}

    def insert_key(self, project_id):
        """Insert a new HMAC key to the service account."""
        key_id = ServiceAccount.next_key_id()
        secret_key = ''.join([
            random.choice("abcdefghijklmnopqrstuvwxyz0123456789") for _ in range(40)])
        now = time.gmtime(time.time())
        timestamp = time.strftime('%Y-%m-%dT%H:%M:%SZ', now)
        return self.keys.setdefault(key_id, {
            "kind": "storage#hmacKeyCreate",
            "secretKey": base64.b64encode(secret_key),
            "resource": {
                "accessId": key_id,
                "id": key_id,
                "kind": "storage#hmacKey",
                "projectId": project_id,
                "serviceAccountEmail": self.email,
                "state": "ACTIVE",
                "timeCreated": timestamp,
            }
        })

    def key_items(self):
        """Return the keys in this service account as a list of JSON objects."""
        return [k.get('resource') for k in self.keys.values()]


class GcsProject(object):
    """Represent a GCS project."""
    project_number_generator = 100000

    @classmethod
    def next_project_number(cls):
        cls.project_number_generator += 1
        return cls.project_number_generator

    def __init__(self, project_id):
        self.project_id = project_id
        self.project_number = GcsProject.next_project_number()
        self.service_accounts = {}

    def service_account_email(self):
        """Return the GCS service account email for this project."""
        username = 'service-%d' % self.project_number
        domain = 'gs-project-accounts.iam.gserviceaccount.com'
        return '%s@%s' % (username, domain)

    def insert_hmac_key(self, service_account):
        """Insert a new HMAC key (or an error)."""
        sa = self.service_accounts.setdefault(service_account, ServiceAccount(service_account))
        return sa.insert_key(self.project_id)

    def service_account(self, service_account_email):
        """Return a ServiceAccount object given its email."""
        return self.service_accounts.get(service_account_email)


PROJECTS_HANDLER_PATH = '/storage/v1/projects'
projects = flask.Flask(__name__)
projects.debug = True


VALID_PROJECTS = {}


def get_project(project_id):
    """Find a project and return the GcsProject object."""
    # Dynamically create the projects. The GCS testbench does not have functions
    # to create projects, nor do we want to create such functions. The point is
    # to test the GCS client library, not the IAM client library.
    return VALID_PROJECTS.setdefault(project_id, GcsProject(project_id))


@projects.route('/<project_id>/serviceAccount')
def projects_get(project_id):
    """Implement the `Projects.serviceAccount: get` API."""
    project = get_project(project_id)
    email = project.service_account_email()
    return testbench_utils.filtered_response(
        flask.request, {
            'kind': 'storage#serviceAccount',
            'email_address': email
        })


@projects.route('/<project_id>/hmacKeys', methods=['POST'])
def hmac_keys_insert(project_id):
    """Implement the `HmacKeys: insert` API."""
    project = get_project(project_id)
    service_account = flask.request.args.get('serviceAccount')
    if service_account is None:
        raise error_response.ErrorResponse(
            'serviceAccount is a required parameter', status_code=400)
    return testbench_utils.filtered_response(
        flask.request,
        project.insert_hmac_key(service_account))


@projects.route('/<project_id>/hmacKeys')
def hmac_keys_list(project_id):
    """Implement the 'HmacKeys: list' API: return the HMAC keys in a project."""
    # Lookup the bucket, if this fails the bucket does not exist, and this
    # function should return an error.
    project = get_project(project_id)
    result = {
        'kind': 'storage#hmacKeys',
        'next_page_token': '',
        'items': []
    }

    state_filter = lambda x: x.get('state') != 'DELETED'
    if flask.request.args.get('deleted') == 'true':
        state_filter = lambda x: True

    items = []
    if flask.request.args.get('serviceAccount'):
        sa = flask.request.args.get('serviceAccount')
        service_account = project.service_account(sa)
        if service_account:
            items = service_account.key_items()
    else:
        for sa in project.service_accounts.values():
            items.extend(sa.key_items())

    result['items'] = [i for i in items if state_filter(i)]
    return testbench_utils.filtered_response(flask.request, result)


def get_projects_app():
    return PROJECTS_HANDLER_PATH, projects
