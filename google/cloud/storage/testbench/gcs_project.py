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

import error_response
import flask
import testbench_utils


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

    def service_account_email(self):
        """Return the GCS service account email for this project."""
        username = 'service-%d' % self.project_number
        domain = 'gs-project-accounts.iam.gserviceaccount.com'
        return '%s@%s' % (username, domain)


PROJECTS_HANDLER_PATH = '/storage/v1/projects'
projects = flask.Flask(__name__)
projects.debug = True


VALID_PROJECTS = {}


@projects.route('/<project_id>/serviceAccount')
def projects_get(project_id):
    """Implement the `Projects.serviceAccount: get` API."""
    # Dynamically create the projects. The GCS testbench does not have functions
    # to create projects, nor do we want to create such functions. The point is
    # to test the GCS client library, not the IAM client library.
    project = VALID_PROJECTS.setdefault(project_id, GcsProject(project_id))
    email = project.service_account_email()
    return testbench_utils.filtered_response(
        flask.request, {
            'kind': 'storage#serviceAccount',
            'email_address': email
        })


def get_projects_app():
    return PROJECTS_HANDLER_PATH, projects
