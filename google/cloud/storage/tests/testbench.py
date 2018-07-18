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


@httpbin.app.errorhandler(ErrorResponse)
def httpbin_error(error):
    return error.as_response()


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

    def __init__(self, gcs_url, bucket_name, name, generation, request):
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
        self.insert_acl('project-owners-123456789', 'OWNER')
        self.insert_acl('project-editors-123456789', 'OWNER')
        self.insert_acl('project-viewers-123456789', 'READER')

    def insert_acl(self, entity, role):
        email = ''
        entity_id = ''
        if entity.startswith('user-'):
            email = entity
        # Replace or insert the entry
        indexed = {entry.get('entity'): entry for entry in self.metadata.get('acl', [])}
        indexed[entity] = {
            'bucket': self.bucket_name,
            'email': email,
            'entity': entity,
            'etag': self.metadata.get('etag', 'XYZ='),
            'generation': self.generation,
            'id': self.metadata.get('id', '') + '/' + entity,
            'kind': 'storage#objectAccessControl',
            'object': self.name,
            'role': role,
            'selfLink': self.metadata.get('selfLink') + '/acl/' + entity
        }
        self.metadata['acl'] = indexed.values()
        return indexed[entity]


class GcsObject(object):
    """Represent a GCS Object, including all its revisions."""

    def __init__(self, bucket_name, name):
        self.bucket_name = bucket_name
        self.name = name
        # Define the current generation for the object, will use this as a
        # simple counter to increment on each object change.
        self.generation = 0
        self.revisions = {}

    def get_revision(self, request):
        generation = request.args.get('generation')
        if generation is None:
            return self.get_latest()
        version = self.revisions.get(int(generation))
        if version is None:
            raise ErrorResponse('Precondition Failed: generation %s not found'
                                % generation)
        return version

    def del_revision(self, request):
        generation = request.args.get('generation')
        if generation is None:
            generation = self.generation
        self.revisions.pop(generation)
        if len(self.revisions) == 0:
            self.generation = None
            return True
        if generation == self.generation:
            self.generation = sorted(self.revisions.keys())[-1]
        return False

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

    def insert(self, gcs_url, request):
        """Insert a new revision based on the give flask request."""
        self.generation += 1
        self.revisions[self.generation] = GcsObjectVersion(gcs_url,
                                                           self.bucket_name,
                                                           self.name,
                                                           self.generation,
                                                           request)


class GcsBucket(object):
    """Represent a GCS Bucket."""

    def __init__(self, gcs_url, name):
        self.name = name
        self.metadata = {
            'id': name,
            'kind': 'storage#bucket',
            'metageneration': 1,
            'selfLink': gcs_url + name,
            'projectNumber': '123456789',
            'name': name,
            'timeCreated': '2018-05-19T19:31:14Z',
            'updated': '2018-05-19T19:31:24Z',
            'location': 'US',
            'storageClass': 'STANDARD',
            'etag': 'XYZ=',
            'labels': {
                'foo': 'bar',
                'baz': 'qux'
            }
        }

    def check_preconditions(self, request):
        """Verify that the preconditions in request are met."""

        metageneration_match = request.args.get('ifMetagenerationMatch')
        metageneration_not_match = request.args.get('ifMetagenerationNotMatch')
        metageneration = self.metadata.get('metageneration')

        if metageneration_not_match is not None \
                and int(metageneration_not_match) == metageneration:
            raise ErrorResponse('Precondition Failed', status_code=412)

        if metageneration_match is not None \
                and int(metageneration_match) != metageneration:
            raise ErrorResponse('Precondition Failed', status_code=412)


# Define the collection of GcsObjects indexed by <bucket_name>/o/<object_name>
GCS_OBJECTS = dict()

# Define the collection of Buckets indexed by <bucket_name>
GCS_BUCKETS = dict()

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


@gcs.route('/b')
def buckets_list():
    """Implement the 'Buckets: list' API: return the Buckets in a project."""
    base_url = flask.url_for('gcs_index', _external=True)
    result = {
        'next_page_token': '',
        'items': []
    }
    for name, b in GCS_BUCKETS.items():
        result['items'].append(b.metadata)
    return json.dumps(result)


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


@gcs.route('/b/<bucket_name>/o')
def objects_list(bucket_name):
    """Implement the 'Objects: list' API: return the objects in a bucket."""
    base_url = flask.url_for('gcs_index', _external=True)
    result = {
        'next_page_token': '',
        'items': []
    }
    for name, o in GCS_OBJECTS.items():
        if name.find(bucket_name + '/o') != 0:
            continue
        result['items'].append(o.get_latest().metadata)
    return json.dumps(result)


@gcs.route('/b/<bucket_name>/o/<object_name>')
def objects_get(bucket_name, object_name):
    """Implement the 'Objects: get' API.  Read objects or their metadata."""
    media = flask.request.args.get('alt', None)
    object_path = bucket_name + '/o/' + object_name
    gcs_object = GCS_OBJECTS.get(object_path,
                                 GcsObject(bucket_name, object_name))
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)

    if media is not None:
        if media != 'media':
            raise ErrorResponse('Invalid alt=%s parameter' % media)
        response = flask.make_response(revision.media)
        length = len(revision.media)
        response.headers['Content-Range'] = 'bytes 0-%d/%d' % (length - 1, length)
        return response

    return json.dumps(revision.metadata)


@gcs.route('/b/<bucket_name>/o/<object_name>', methods=['DELETE'])
def objects_delete(bucket_name, object_name):
    """Implement the 'Objects: delete' API.  Delete objects."""
    object_path = bucket_name + '/o/' + object_name
    gcs_object = GCS_OBJECTS.get(object_path,
                                 GcsObject(bucket_name, object_name))
    gcs_object.check_preconditions(flask.request)
    remove = gcs_object.del_revision(flask.request)
    if remove:
        GCS_OBJECTS.pop(object_path)

    return json.dumps({})


@gcs.route('/b/<bucket_name>/o/<object_name>/acl')
def objects_acl_list(bucket_name, object_name):
    """Implement the 'ObjectAccessControls: list' API.

     List Object Access Controls.
     """
    object_path = bucket_name + '/o/' + object_name
    gcs_object = GCS_OBJECTS.get(object_path,
                                 GcsObject(bucket_name, object_name))
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)
    result = {
        'items': revision.metadata.get('acl', []),
    }
    return json.dumps(result)


@gcs.route('/b/<bucket_name>/o/<object_name>/acl', methods=['POST'])
def objects_acl_create(bucket_name, object_name):
    """Implement the 'ObjectAccessControls: create' API.

     Create an Object Access Control.
     """
    object_path = bucket_name + '/o/' + object_name
    gcs_object = GCS_OBJECTS.get(object_path,
                                 GcsObject(bucket_name, object_name))
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)
    payload = json.loads(flask.request.data)
    return json.dumps(revision.insert_acl(payload.get('entity', ''), payload.get('role', '')))


# Define the WSGI application to handle bucket requests.
UPLOAD_HANDLER_PATH = '/upload/storage/v1'
upload = flask.Flask(__name__)
upload.debug = True


@upload.errorhandler(ErrorResponse)
def upload_error(error):
    return error.as_response()


@upload.route('/b/<bucket_name>/o', methods=['POST'])
def objects_insert(bucket_name):
    """Implement the 'Objects: insert' API.  Insert a new GCS Object."""
    gcs_url = flask.url_for('objects_insert', bucket_name=bucket_name,
                            _external=True).replace('/upload/', '/')
    object_name = flask.request.args.get('name', None)
    if object_name is None:
        raise ErrorResponse('Name not set in Objects: insert', status_code=412)

    object_path = bucket_name + '/o/' + object_name
    gcs_object = GCS_OBJECTS.get(object_path,
                                 GcsObject(bucket_name, object_name))
    gcs_object.check_preconditions(flask.request)
    GCS_OBJECTS[object_path] = gcs_object
    gcs_object.insert(gcs_url, flask.request)
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
