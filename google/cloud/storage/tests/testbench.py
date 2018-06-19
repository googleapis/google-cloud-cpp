#!/usr/bin/env python
# Copyright 2018 Google Inc.
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

"""A test bench for the Google Cloud Storage C++ Client Library."""

import argparse
import json
import time
import flask
import httpbin
from werkzeug import serving
from werkzeug import wsgi


class ErrorResponse(Exception):
    """Simplify generation of error responses."""
    status_code = 400

    def __init__(self, message, status_code=None, payload=None):
        Exception.__init__(self)
        self.message = message
        if status_code is not None:
            self.status_code = status_code
        self.payload = payload

    def as_response(self):
        kv = dict(self.payload or ())
        kv['message'] = self.message
        response = flask.jsonify(kv)
        response.status_code = self.status_code
        return response


root = flask.Flask(__name__)
root.debug = True


@root.route('/')
def index():
    """Default handler for the test bench."""
    return 'OK'


@root.route('/shutdown', methods=['POST'])
def shutdown():
    """Gracefully shutdown the test bench."""
    func = flask.request.environ.get('werkzeug.server.shutdown')
    if func is None:
        raise RuntimeError('Not running with the Werkzeug Server')
    func()
    return 'Server shutting down...'


class GcsObjectVersion(object):
    """Represent a single revision of a GCS Object."""

    def __init__(self, bucket_name, name, generation, request):
        gcs_url = flask.url_for('gcs_index', _external=True)
        self.bucket_name = bucket_name
        self.name = name
        self.generation = generation
        self.object_id = bucket_name + '/o/' + name + '/' + str(generation)
        now = time.gmtime(time.time())
        timestamp = time.strftime('%Y-%m-%dT%H:%M:%SZ', now)
        self.media = request.data
        self.metadata = {
            'kind': 'storage#object',
            'id': self.object_id,
            'selfLink': gcs_url + self.object_id,
            'projectNumber': '123456789',
            'bucket': bucket_name,
            'name': name,
            'timeCreated': timestamp,
            'updated': timestamp,
            'metageneration': 1,
            'generation': generation,
            'location': 'US',
            'storageClass': 'STANDARD',
            'etag': 'XYZ='
        }


class GcsObject(object):
    """Represent a GCS Object, including all its revisions."""

    def __init__(self, bucket_name, name):
        self.bucket_name = bucket_name
        self.name = name
        # Define the current generation for the object, will use this as a
        # simple counter to increment on each object change.
        self.generation = 0
        self.revisions = {}

    def get_revision(self, revision):
        return self.revisions.get(revision, None)

    def get_latest(self):
        return self.revisions.get(self.generation, None)

    def check_preconditions(self, request):
        """Verify that the preconditions in request are met."""

        generation_match = request.args.get('ifGenerationMatch')
        if generation_match is not None \
                and int(generation_match) != self.generation:
            raise ErrorResponse('Precondition Failed', status_code=412)

        # This object does not exist (yet), testing in this case is special.
        generation_not_match = request.args.get('ifGenerationNotMatch')
        if generation_not_match is not None \
                and int(generation_not_match) == self.generation:
            raise ErrorResponse('Precondition Failed', status_code=412)

        metageneration_match = request.args.get('ifMetagenerationMatch')
        metageneration_not_match = request.args.get('ifMetagenerationNotMatch')
        if self.generation == 0:
            if metageneration_match is not None \
                    or metageneration_not_match is not None:
                raise ErrorResponse('Precondition Failed', status_code=412)
        else:
            current = self.revisions.get(self.generation)
            metageneration = current.metadata.get('metageneration')

            if metageneration_not_match is not None \
                    and int(metageneration_not_match) == metageneration:
                raise ErrorResponse('Precondition Failed', status_code=412)

            if metageneration_match is not None \
                    and int(metageneration_match) != metageneration:
                raise ErrorResponse('Precondition Failed', status_code=412)

    def insert(self, request):
        """Insert a new revision based on the give flask request."""
        self.generation += 1
        self.revisions[self.generation] = GcsObjectVersion(self.bucket_name,
                                                           self.name,
                                                           self.generation,
                                                           request)


# Define the collection of GcsObjects indexed by <bucket_name>/o/<object_name>
GCS_OBJECTS = dict()

# Define the WSGI application to handle bucket requests.
GCS_HANDLER_PATH = '/storage/v1'
gcs = flask.Flask(__name__)
gcs.debug = True


@gcs.route('/')
def gcs_index():
    """The default handler for GCS requests."""
    return 'OK'


@gcs.errorhandler(ErrorResponse)
def gcs_error(error):
    return error.as_response()


@gcs.route('/b/<bucket_name>')
def buckets_get(bucket_name):
    """Implement the 'Buckets: get' API: return the metadata for a bucket."""
    base_url = flask.url_for('gcs_index', _external=True)
    metageneration_match = flask.request.args.get('ifMetagenerationMatch', None)
    if metageneration_match is not None and metageneration_match != '4':
        raise ErrorResponse('Precondition Failed', status_code=412)
    metageneration_not_match = flask.request.args.get(
        'ifMetagenerationNotMatch', None)
    if metageneration_not_match is not None and metageneration_not_match == '4':
        raise ErrorResponse('Precondition Failed', status_code=412)
    return json.dumps({
        'kind': 'storage#bucket',
        'id': bucket_name,
        'selfLink': base_url + bucket_name,
        'projectNumber': '123456789',
        'name': bucket_name,
        'timeCreated': '2018-05-19T19:31:14Z',
        'updated': '2018-05-19T19:31:24Z',
        'metageneration': '4',
        'location': 'US',
        'storageClass': 'STANDARD',
        'etag': 'XYZ=',
        'labels': {
            'foo': 'bar',
            'baz': 'qux'
        }
    })


# Define the WSGI application to handle bucket requests.
UPLOAD_HANDLER_PATH = '/upload/storage/v1'
upload = flask.Flask(__name__)
upload.debug = True


@upload.route('/b/<bucket_name>/o', methods=['POST'])
def objects_insert(bucket_name):
    """Implement the 'Objects: insert' API.  Insert a new GCS Object."""
    object_name = flask.request.args.get('name', None)
    if object_name is None:
        raise ErrorResponse('Name not set in Objects: insert', status_code=412)

    object_path = bucket_name + '/o/' + object_name
    gcs_object = GCS_OBJECTS.get(object_path,
                                 GcsObject(bucket_name, object_name))
    gcs_object.check_preconditions(flask.request)
    GCS_OBJECTS[object_path] = gcs_object
    gcs_object.insert(flask.request)
    current_version = gcs_object.get_latest()

    return json.dumps(current_version.metadata)


def main():
    """Parse the arguments and run the test bench application."""
    parser = argparse.ArgumentParser(
        description='A testbench for the Google Cloud C++ Client Library')
    parser.add_argument('--host', default='localhost',
                        help='The listening port')
    parser.add_argument('--port', help='The listening port')
    # By default we do not turn on the debugging. This typically runs inside a
    # Docker image, with a uid that has not entry in /etc/passwd, and the
    # werkzeug debugger crashes in that environment (as it should probably).
    parser.add_argument('--debug', help='Use the WSGI debugger',
                        default=False, action='store_true')
    arguments = parser.parse_args()

    # Compose the different WSGI applications.
    application = wsgi.DispatcherMiddleware(root, {
        '/httpbin': httpbin.app,
        GCS_HANDLER_PATH: gcs,
        UPLOAD_HANDLER_PATH: upload,
    })
    serving.run_simple(arguments.host, int(arguments.port), application,
                       use_reloader=True, use_debugger=arguments.debug,
                       use_evalex=True)


if __name__ == '__main__':
    main()
