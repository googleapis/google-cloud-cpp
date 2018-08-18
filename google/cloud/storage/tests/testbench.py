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
import os
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


def canonical_entity_name(entity):
    """
    Convert entity names to their canonical form.

    Some entities (notably project-<team>-) have more than one name, for
    example the project-owners-<project_id> entities are called
    project-owners-<project_number> internally.  This function
    :param entity:str convert this entity to its canonical name.
    :return:str the name in canonical form.
    """
    if entity.startswith('project-owners-'):
        entity = 'project-owners-123456789'
    if entity.startswith('project-editors-'):
        entity = 'project-editors-123456789'
    if entity.startswith('project-viewers-'):
        entity = 'project-viewers-123456789'
    return entity.lower()


def index_acl(acl):
    """Return a ACL as a dictionary indexed by the 'entity' values of the ACL.

    We represent ACLs as lists of dictionaries, that makes it easy to convert
    them to JSON objects. When changing them though, we need to make sure there
    is a single element in the list for each `entity` value, so it is convenient
    to convert the list to a dictionary (indexed by `entity`) of dictionaries.
    This function performs that conversion.

    :param acl:list of dict
    :return:dict the ACL indexed by the entity of each entry.
    """
    # This can be expressed by a comprehension but turns out to be less
    # readable in that form.
    indexed = dict()
    for e in acl:
        indexed[e['entity']] = e
    return indexed


class GcsObjectVersion(object):
    """Represent a single revision of a GCS Object."""

    def __init__(self, gcs_url, bucket_name, name, generation, request):
        """
        Initialize a new object revision.

        :param gcs_url:str the base URL for the GCS service.
        :param bucket_name:str the name of the bucket that contains the object.
        :param name:str the name of the object.
        :param generation:int the generation number for this object.
        :param request:flask.Request the contents of the HTTP request.
        """
        self.bucket_name = bucket_name
        self.name = name
        self.generation = generation
        self.object_id = bucket_name + '/o/' + name + '/' + str(generation)
        now = time.gmtime(time.time())
        timestamp = time.strftime('%Y-%m-%dT%H:%M:%SZ', now)
        if request.environ.get('HTTP_TRANSFER_ENCODING', '') == 'chunked':
            self.media = request.environ.get('wsgi.input').read()
        else:
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
            'size': len(self.media),
            'etag': 'XYZ='
        }
        self.insert_acl(
            canonical_entity_name('project-owners-123456789'), 'OWNER')
        self.insert_acl(
            canonical_entity_name('project-editors-123456789'), 'OWNER')
        self.insert_acl(
            canonical_entity_name('project-viewers-123456789'), 'READER')

    def insert_acl(self, entity, role):
        """
        Insert (or update) a new AccessControl entry for this object.

        :param entity:str the name of the entity to insert.
        :param role:str the new role
        :return:dict the dictionary representing the new AccessControl metadata.
        """
        entity = canonical_entity_name(entity)
        email = ''
        if entity.startswith('user-'):
            email = entity
        # Replace or insert the entry.
        indexed = index_acl(self.metadata.get('acl', []))
        indexed[entity] = {
            'bucket': self.bucket_name,
            'email': email,
            'entity': entity,
            'entity_id': '',
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

    def delete_acl(self, entity):
        """
        Delete a single AccessControl entry from the Object revision.

        :param entity:str the name of the entity.
        :return:None
        """
        entity = canonical_entity_name(entity)
        indexed = index_acl(self.metadata.get('acl', []))
        indexed.pop(entity)
        self.metadata['acl'] = indexed.values()

    def get_acl(self, entity):
        """
        Get a single AccessControl entry from the Object revision.

        :param entity:str the name of the entity.
        :return:dict with the contents of the ObjectAccessControl.
        """
        entity = canonical_entity_name(entity)
        for acl in self.metadata.get('acl', []):
            if acl.get('entity', '').lower() == entity:
                return acl
        raise ErrorResponse(
            'Entity %s not found in object %s' % (entity, self.name))

    def update_acl(self, entity, role):
        """
        Update a single AccessControl entry in this Object revision.

        :param entity:str the name of the entity.
        :param role:str the new role for the entity.
        :return:dict with the contents of the ObjectAccessControl.
        """
        return self.insert_acl(entity, role)

    def patch_acl(self, entity, request):
        """
        Patch a single AccessControl entry in this Object revision.

        :param entity:str the name of the entity.
        :param request:flask.Request the parameters for this request.
        :return:dict with the contents of the ObjectAccessControl.
        """
        acl = self.get_acl(entity)
        payload = json.loads(request.data)
        request_entity = payload.get('entity')
        if request_entity is not None and request_entity != entity:
            raise ErrorResponse(
                'Entity mismatch in ObjectAccessControls: patch, expected=%s, got=%s'
                % (entity, request_entity))
        etag_match = request.headers.get('if-match')
        if etag_match is not None and etag_match != acl.get('etag'):
            raise ErrorResponse('Precondition Failed', status_code=412)
        etag_none_match = request.headers.get('if-none-match')
        if (etag_none_match is not None
                and etag_none_match != acl.get('etag')):
            raise ErrorResponse('Precondition Failed', status_code=412)
        role = payload.get('role')
        if role is None:
            raise ErrorResponse('Missing role value')
        return self.insert_acl(entity, role)


class GcsObject(object):
    """Represent a GCS Object, including all its revisions."""

    def __init__(self, bucket_name, name):
        """
        Initialize a fake GCS Blob.

        :param bucket_name:str the bucket that will contain the new object.
        :param name:str the name of the new object.
        """
        self.bucket_name = bucket_name
        self.name = name
        # Define the current generation for the object, will use this as a
        # simple counter to increment on each object change.
        self.generation = 0
        self.revisions = {}

    def get_revision(self, request):
        """
        Get the information about a particular object revision or raise.

        :param request:flask.Request
        :return:GcsObjectRevision the object revision.
        :raises:ErrorResponse if the request contains an invalid generation
            number.
        """
        generation = request.args.get('generation')
        if generation is None:
            return self.get_latest()
        version = self.revisions.get(int(generation))
        if version is None:
            raise ErrorResponse(
                'Precondition Failed: generation %s not found' % generation)
        return version

    def del_revision(self, request):
        """
        Delete a version of a fake GCS Blob.

        :param request:flask.Request the contents of the HTTP request.
        :return: None
        """
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
        """
        Insert a new revision based on the give flask request.

        :param gcs_url:str the root URL for the fake GCS service.
        :param request:flask.Request the contents of the HTTP request.
        :return:None
        """
        self.generation += 1
        self.revisions[self.generation] = GcsObjectVersion(
            gcs_url, self.bucket_name, self.name, self.generation, request)


class GcsBucket(object):
    """Represent a GCS Bucket."""

    def __init__(self, gcs_url, name):
        self.name = name
        self.gcs_url = gcs_url
        self.metadata = {
            'metageneration': 0,
            'name': self.name,
            'location': 'US',
            'storageClass': 'STANDARD',
            'etag': 'XYZ=',
            'labels': {
                 'foo': 'bar',
                 'baz': 'qux'
             }
        }
        # Update the derived metadata attributes (e.g.: id, kind, selfLink)
        self.update_from_metadata({})
        self.insert_acl(
            canonical_entity_name('project-owners-123456789'), 'OWNER')
        self.insert_acl(
            canonical_entity_name('project-editors-123456789'), 'OWNER')
        self.insert_acl(
            canonical_entity_name('project-viewers-123456789'), 'READER')
        self.insert_default_object_acl(
            canonical_entity_name('project-owners-123456789'), 'OWNER')
        self.insert_default_object_acl(
            canonical_entity_name('project-editors-123456789'), 'OWNER')
        self.insert_default_object_acl(
            canonical_entity_name('project-viewers-123456789'), 'READER')

    def update_from_metadata(self, metadata):
        """Update from a metadata dictionary.

        :param metadata:dict a dictionary with new metadata values.
        """
        tmp = self.metadata.copy()
        tmp.update(metadata)
        tmp['name'] = tmp.get('name', self.name)
        tmp.update({
            'id': self.name,
            'kind': 'storage#bucket',
            'selfLink': self.gcs_url + self.name,
            'projectNumber': '123456789',
            'timeCreated': '2018-05-19T19:31:14Z',
            'updated': '2018-05-19T19:31:24Z',
        })
        tmp['metageneration'] = tmp.get('metageneration', 0) + 1
        self.metadata = tmp

    def apply_patch(self, patch):
        """Update from a metadata dictionary.

        :param patch:dict a dictionary with metadata changes.
        """
        tmp = self.metadata.copy()
        writeable_keys = {'acl', 'billing', 'cors', 'defaultObjectAcl',
                          'encryption', 'labels', 'lifecycle', 'location',
                          'logging', 'storageClass', 'versioning', 'website'}
        for key, value in patch.iteritems():
            if key not in writeable_keys:
                raise ErrorResponse('Invalid metadata change. %s is not writeable' % key, status_code=503)
            if value is None:
                if key in tmp:
                    del(tmp[key])
            else:
                tmp[key] = value
        tmp['name'] = tmp.get('name', self.name)
        tmp['metageneration'] = tmp.get('metageneration', 0) + 1
        self.metadata = tmp

    def check_preconditions(self, request):
        """
        Verify that the preconditions in request are met.

        :param request:flask.Request the contents of the HTTP request.
        :return:None
        :raises:ErrorResponse if the request does not pass the preconditions,
            for example, the request has a `ifMetagenerationMatch` restriction
            that is not met.
        """

        metageneration_match = request.args.get('ifMetagenerationMatch')
        metageneration_not_match = request.args.get('ifMetagenerationNotMatch')
        metageneration = self.metadata.get('metageneration')

        if metageneration_not_match is not None \
                and int(metageneration_not_match) == metageneration:
            raise ErrorResponse('Precondition Failed (metageneration = %s)' % metageneration, status_code=412)

        if metageneration_match is not None \
                and int(metageneration_match) != metageneration:
            raise ErrorResponse('Precondition Failed (metageneration = %s)' % metageneration, status_code=412)

    def insert_acl(self, entity, role):
        """
        Insert (or update) a new BucketAccessControl entry for this bucket.

        :param entity:str the name of the entity to insert.
        :param role:str the new role
        :return:dict the dictionary representing the new AccessControl metadata.
        """
        entity = canonical_entity_name(entity)
        email = ''
        if entity.startswith('user-'):
            email = entity.replace('user-', '', 1)
        # Replace or insert the entry.
        indexed = index_acl(self.metadata.get('acl', []))
        indexed[entity] = {
            'bucket': self.name,
            'email': email,
            'entity': entity,
            'etag': self.metadata.get('etag', 'XYZ='),
            'id': self.metadata.get('id', '') + '/' + entity,
            'kind': 'storage#bucketAccessControl',
            'role': role,
            'selfLink': self.metadata.get('selfLink') + '/acl/' + entity
        }
        self.metadata['acl'] = indexed.values()
        return indexed[entity]

    def delete_acl(self, entity):
        """
        Delete a single BucketAccessControl entry from this bucket.

        :param entity:str the name of the entity.
        :return:None
        """
        entity = canonical_entity_name(entity)
        indexed = index_acl(self.metadata.get('acl', []))
        indexed.pop(entity)
        self.metadata['acl'] = indexed.values()

    def get_acl(self, entity):
        """
        Get a single BucketAccessControl entry from this bucket.

        :param entity:str the name of the entity.
        :return:dict with the contents of the BucketAccessControl.
        """
        entity = canonical_entity_name(entity)
        for acl in self.metadata.get('acl', []):
            if acl.get('entity', '').lower() == entity:
                return acl
        raise ErrorResponse(
            'Entity %s not found in object %s' % (entity, self.name))

    def update_acl(self, entity, role):
        """
        Update a single BucketAccessControl entry in this bucket.

        :param entity:str the name of the entity.
        :param role:str the new role for the entity.
        :return:dict with the contents of the BucketAccessControl.
        """
        return self.insert_acl(entity, role)

    def insert_default_object_acl(self, entity, role):
        """
        Insert (or update) a new default ObjectAccessControl entry for this bucket.

        :param entity:str the name of the entity to insert.
        :param role:str the new role
        :return:dict the dictionary representing the new ObjectAccessControl.
        """
        entity = canonical_entity_name(entity)
        email = ''
        if entity.startswith('user-'):
            email = email.replace('user-', '', 1)
        # Replace or insert the entry.
        indexed = index_acl(self.metadata.get('defaultObjectAcl', []))
        indexed[entity] = {
            'bucket': self.name,
            'email': email,
            'entity': entity,
            'etag': self.metadata.get('etag', 'XYZ='),
            'id': self.metadata.get('id', '') + '/' + entity,
            'kind': 'storage#objectAccessControl',
            'role': role,
            'selfLink': self.metadata.get('selfLink') + '/acl/' + entity
        }
        self.metadata['defaultObjectAcl'] = indexed.values()
        return indexed[entity]

    def delete_default_object_acl(self, entity):
        """
        Delete a single default ObjectAccessControl entry from this bucket.

        :param entity:str the name of the entity.
        :return:None
        """
        entity = canonical_entity_name(entity)
        indexed = index_acl(self.metadata.get('defaultObjectAcl', []))
        indexed.pop(entity)
        self.metadata['defaultObjectAcl'] = indexed.values()

    def get_default_object_acl(self, entity):
        """
        Get a single default ObjectAccessControl entry from this Bucket.

        :param entity:str the name of the entity.
        :return:dict with the contents of the BucketAccessControl.
        """
        entity = canonical_entity_name(entity)
        for acl in self.metadata.get('defaultObjectAcl', []):
            if acl.get('entity', '').lower() == entity:
                return acl
        raise ErrorResponse(
            'Entity %s not found in object %s' % (entity, self.name))

    def update_default_object_acl(self, entity, role):
        """
        Update a single default ObjectAccessControl entry in this Bucket.

        :param entity:str the name of the entity.
        :param role:str the new role for the entity.
        :return:dict with the contents of the ObjectAccessControl.
        """
        return self.insert_acl(entity, role)


# Define the collection of GcsObjects indexed by <bucket_name>/o/<object_name>
GCS_OBJECTS = dict()

# Define the collection of Buckets indexed by <bucket_name>
GCS_BUCKETS = dict()

# Define the WSGI application to handle bucket requests.
GCS_HANDLER_PATH = '/storage/v1'
gcs = flask.Flask(__name__)
gcs.debug = True


# TODO(#821) TODO(#820) - until we implement CreateBucket + DeleteBucket insert
# a well-known bucket based on the BUCKET_NAME environment (this is set by our
# CI builds), to make the integration tests easy to write.
def insert_magic_bucket(base_url):
    if len(GCS_BUCKETS) == 0:
        bucket_name = os.environ.get('BUCKET_NAME', 'test-bucket')
        bucket = GcsBucket(base_url, bucket_name)
        # Enable versioning in the Bucket, the integration tests expect this to
        # be the case, this brings the metageneration number to 2.
        bucket.update_from_metadata({'versioning': {'enabled': True}})
        # Perform trivial updates that bring the metageneration to 4, the value
        # expected by the integration tests.
        bucket.update_from_metadata({})
        bucket.update_from_metadata({})
        GCS_BUCKETS[bucket_name] = bucket


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
    insert_magic_bucket(base_url)
    result = {'next_page_token': '', 'items': []}
    for name, b in GCS_BUCKETS.items():
        result['items'].append(b.metadata)
    return json.dumps(result)


@gcs.route('/b', methods=['POST'])
def buckets_insert():
    """Implement the 'Buckets: insert' API: create a new Bucket."""
    base_url = flask.url_for('gcs_index', _external=True)
    insert_magic_bucket(base_url)
    payload = json.loads(flask.request.data)
    bucket_name = payload.get('name')
    if bucket_name is None:
        raise ErrorResponse('Missing bucket name in `Buckets: insert`',
                            status_code=412)
    bucket = GCS_BUCKETS.get(bucket_name)
    if bucket is not None:
        raise ErrorResponse('Bucket %s already exists' % bucket_name, status_code=503)
    bucket = GcsBucket(base_url, bucket_name)
    bucket.update_from_metadata(payload)
    GCS_BUCKETS[bucket_name] = bucket
    return json.dumps(bucket.metadata)


@gcs.route('/b/<bucket_name>', methods=['PUT'])
def buckets_update(bucket_name):
    """Implement the 'Buckets: update' API: update an existing Bucket."""
    base_url = flask.url_for('gcs_index', _external=True)
    insert_magic_bucket(base_url)
    payload = json.loads(flask.request.data)
    bucket_name = payload.get('name')
    if bucket_name is None:
        raise ErrorResponse('Missing bucket name in `Buckets: update`',
                            status_code=412)
    bucket = GCS_BUCKETS.get(bucket_name)
    if bucket is None:
        raise ErrorResponse('Bucket %s does not exist' % bucket_name, status_code=404)
    bucket.check_preconditions(flask.request)
    bucket.update_from_metadata(payload)
    return json.dumps(bucket.metadata)


@gcs.route('/b/<bucket_name>')
def buckets_get(bucket_name):
    """Implement the 'Buckets: get' API: return the metadata for a bucket."""
    base_url = flask.url_for('gcs_index', _external=True)
    insert_magic_bucket(base_url)
    # TODO(#821) - until we implement Client::CreateBucket, simply insert every
    # bucket that the application queries.
    bucket = GcsBucket(base_url, bucket_name)
    bucket.update_from_metadata({})
    bucket.update_from_metadata({})
    bucket.update_from_metadata({})
    bucket = GCS_BUCKETS.setdefault(bucket_name, bucket)
    # end of TODO(#821)
    if bucket is None:
        raise ErrorResponse('Bucket %s not found' % bucket_name, status_code=404)
    bucket.check_preconditions(flask.request)
    return json.dumps(bucket.metadata)


@gcs.route('/b/<bucket_name>', methods=['DELETE'])
def buckets_delete(bucket_name):
    """Implement the 'Buckets: delete' API.

      Delete a Bucket.
      """
    bucket = GCS_BUCKETS.get(bucket_name, None)
    if bucket is None:
        raise ErrorResponse(
            'Bucket %s not found' % bucket_name, status_code=404)
    bucket.check_preconditions(flask.request)
    del (GCS_BUCKETS[bucket_name])
    return json.dumps({})


@gcs.route('/b/<bucket_name>', methods=['PATCH'])
def buckets_patch(bucket_name):
    """Implement the 'Buckets: patch' API.

      Patch the metadata in a bucket.
      """
    bucket = GCS_BUCKETS.get(bucket_name, None)
    if bucket is None:
        raise ErrorResponse(
            'Bucket %s not found' % bucket_name, status_code=404)
    bucket.check_preconditions(flask.request)
    patch = json.loads(flask.request.data)
    bucket.apply_patch(patch)
    return json.dumps(bucket.metadata)


@gcs.route('/b/<bucket_name>/acl')
def bucket_acl_list(bucket_name):
    """Implement the 'BucketAccessControls: list' API.

     List Bucket Access Controls.
     """
    gcs_bucket = GCS_BUCKETS.get(bucket_name)
    if gcs_bucket is None:
        raise ErrorResponse(
            'Bucket %s not found' % bucket_name, status_code=404)
    gcs_bucket.check_preconditions(flask.request)
    result = {
        'items': gcs_bucket.metadata.get('acl', []),
    }
    return json.dumps(result)


@gcs.route('/b/<bucket_name>/acl', methods=['POST'])
def bucket_acl_create(bucket_name):
    """Implement the 'BucketAccessControls: create' API.

     Create a Bucket Access Control.
     """
    gcs_bucket = GCS_BUCKETS.get(bucket_name)
    payload = json.loads(flask.request.data)
    return json.dumps(
        gcs_bucket.insert_acl(
            payload.get('entity', ''), payload.get('role', '')))


@gcs.route('/b/<bucket_name>/acl/<entity>', methods=['DELETE'])
def bucket_acl_delete(bucket_name, entity):
    """Implement the 'BucketAccessControls: delete' API.

      Delete a Bucket Access Control.
      """
    gcs_bucket = GCS_BUCKETS.get(bucket_name)
    gcs_bucket.delete_acl(entity)
    return json.dumps({})


@gcs.route('/b/<bucket_name>/acl/<entity>')
def bucket_acl_get(bucket_name, entity):
    """Implement the 'BucketAccessControls: get' API.

      Get the access control configuration for a particular entity.
      """
    gcs_bucket = GCS_BUCKETS.get(bucket_name)
    acl = gcs_bucket.get_acl(entity)
    return json.dumps(acl)


@gcs.route('/b/<bucket_name>/acl/<entity>', methods=['PUT'])
def bucket_acl_update(bucket_name, entity):
    """Implement the 'BucketAccessControls: update' API.

      Update the access control configuration for a particular entity.
      """
    gcs_bucket = GCS_BUCKETS.get(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    acl = gcs_bucket.update_acl(entity, payload.get('role', ''))
    return json.dumps(acl)


@gcs.route('/b/<bucket_name>/defaultObjectAcl')
def bucket_default_object_acl_list(bucket_name):
    """Implement the 'BucketAccessControls: list' API.

     List Bucket Access Controls.
     """
    gcs_bucket = GCS_BUCKETS.get(bucket_name)
    if gcs_bucket is None:
        raise ErrorResponse(
            'Bucket %s not found' % bucket_name, status_code=404)
    gcs_bucket.check_preconditions(flask.request)
    result = {
        'items': gcs_bucket.metadata.get('defaultObjectAcl', []),
    }
    return json.dumps(result)


@gcs.route('/b/<bucket_name>/defaultObjectAcl', methods=['POST'])
def bucket_default_object_acl_create(bucket_name):
    """Implement the 'BucketAccessControls: create' API.

     Create a Bucket Access Control.
     """
    gcs_bucket = GCS_BUCKETS.get(bucket_name)
    payload = json.loads(flask.request.data)
    return json.dumps(
        gcs_bucket.insert_default_object_acl(
            payload.get('entity', ''), payload.get('role', '')))


@gcs.route('/b/<bucket_name>/defaultObjectAcl/<entity>', methods=['DELETE'])
def bucket_default_object_acl_delete(bucket_name, entity):
    """Implement the 'BucketAccessControls: delete' API.

      Delete a Bucket Access Control.
      """
    gcs_bucket = GCS_BUCKETS.get(bucket_name)
    gcs_bucket.delete_default_object_acl(entity)
    return json.dumps({})


@gcs.route('/b/<bucket_name>/defaultObjectAcl/<entity>')
def bucket_default_object_acl_get(bucket_name, entity):
    """Implement the 'BucketAccessControls: get' API.

      Get the access control configuration for a particular entity.
      """
    gcs_bucket = GCS_BUCKETS.get(bucket_name)
    acl = gcs_bucket.get_default_object_acl(entity)
    return json.dumps(acl)


@gcs.route('/b/<bucket_name>/defaultObjectAcl/<entity>', methods=['PUT'])
def bucket_default_object_acl_update(bucket_name, entity):
    """Implement the 'DefaultObjectAccessControls: update' API.

      Update the access control configuration for a particular entity.
      """
    gcs_bucket = GCS_BUCKETS.get(bucket_name)
    gcs_bucket.check_preconditions(flask.request)
    payload = json.loads(flask.request.data)
    acl = gcs_bucket.update_default_object_acl(entity, payload.get('role', ''))
    return json.dumps(acl)


@gcs.route('/b/<bucket_name>/o')
def objects_list(bucket_name):
    """Implement the 'Objects: list' API: return the objects in a bucket."""
    result = {'next_page_token': '', 'items': []}
    versions_parameter = flask.request.args.get('versions')
    all_versions = (versions_parameter is not None and bool(versions_parameter))
    for name, o in GCS_OBJECTS.items():
        if name.find(bucket_name + '/o') != 0:
            continue
        if o.get_latest() is None:
            continue
        if all_versions:
            for object_version in o.revisions.itervalues():
                result['items'].append(object_version.metadata)
        else:
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
        response.headers['Content-Range'] = 'bytes 0-%d/%d' % (length - 1,
                                                               length)
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
    return json.dumps(
        revision.insert_acl(
            payload.get('entity', ''), payload.get('role', '')))


@gcs.route('/b/<bucket_name>/o/<object_name>/acl/<entity>', methods=['DELETE'])
def objects_acl_delete(bucket_name, object_name, entity):
    """Implement the 'ObjectAccessControls: delete' API.

      Delete an Object Access Control.
      """
    object_path = bucket_name + '/o/' + object_name
    gcs_object = GCS_OBJECTS.get(object_path,
                                 GcsObject(bucket_name, object_name))
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)
    revision.delete_acl(entity)
    return json.dumps({})


@gcs.route('/b/<bucket_name>/o/<object_name>/acl/<entity>')
def objects_acl_get(bucket_name, object_name, entity):
    """Implement the 'ObjectAccessControls: get' API.

      Get the access control configuration for a particular entity.
      """
    object_path = bucket_name + '/o/' + object_name
    gcs_object = GCS_OBJECTS.get(object_path,
                                 GcsObject(bucket_name, object_name))
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)
    acl = revision.get_acl(entity)
    return json.dumps(acl)


@gcs.route('/b/<bucket_name>/o/<object_name>/acl/<entity>', methods=['PUT'])
def objects_acl_update(bucket_name, object_name, entity):
    """Implement the 'ObjectAccessControls: update' API.

      Update the access control configuration for a particular entity.
      """
    object_path = bucket_name + '/o/' + object_name
    gcs_object = GCS_OBJECTS.get(object_path,
                                 GcsObject(bucket_name, object_name))
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)
    payload = json.loads(flask.request.data)
    acl = revision.update_acl(entity, payload.get('role', ''))
    return json.dumps(acl)


@gcs.route('/b/<bucket_name>/o/<object_name>/acl/<entity>', methods=['PATCH'])
def objects_acl_patch(bucket_name, object_name, entity):
    """Implement the 'ObjectAccessControls: patch' API.

      Patch the access control configuration for a particular entity.
      """
    object_path = bucket_name + '/o/' + object_name
    gcs_object = GCS_OBJECTS.get(object_path,
                                 GcsObject(bucket_name, object_name))
    gcs_object.check_preconditions(flask.request)
    revision = gcs_object.get_revision(flask.request)
    acl = revision.patch_acl(entity, flask.request)
    return json.dumps(acl)


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
    gcs_url = flask.url_for(
        'objects_insert', bucket_name=bucket_name, _external=True).replace(
            '/upload/', '/')
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


application = wsgi.DispatcherMiddleware(root, {
    '/httpbin': httpbin.app,
    GCS_HANDLER_PATH: gcs,
    UPLOAD_HANDLER_PATH: upload,
})



def main():
    """Parse the arguments and run the test bench application."""
    parser = argparse.ArgumentParser(
        description='A testbench for the Google Cloud C++ Client Library')
    parser.add_argument(
        '--host', default='localhost', help='The listening port')
    parser.add_argument('--port', help='The listening port')
    # By default we do not turn on the debugging. This typically runs inside a
    # Docker image, with a uid that has not entry in /etc/passwd, and the
    # werkzeug debugger crashes in that environment (as it should probably).
    parser.add_argument(
        '--debug',
        help='Use the WSGI debugger',
        default=False,
        action='store_true')
    arguments = parser.parse_args()

    # Compose the different WSGI applications.
    serving.run_simple(
        arguments.host,
        int(arguments.port),
        application,
        use_reloader=True,
        use_debugger=arguments.debug,
        use_evalex=True)


if __name__ == '__main__':
    main()
