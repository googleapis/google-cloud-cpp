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
import flask
import httpbin
from werkzeug import serving
from werkzeug import wsgi

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


# Define the WSGI application to handle bucket requests.
buckets = flask.Flask(__name__)
buckets.debug = True

BUCKETS_HANDLER_PATH = '/storage/v1/b'

@buckets.route('/')
def buckets_index():
    """The default for the buckets."""
    return 'OK'


@buckets.route('/<bucket_name>')
def get_bucket(bucket_name):
    """Implement the 'Buckets: get' API: return the metadata for a bucket."""
    base_url = flask.url_for('buckets_index', _external=True)
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
        BUCKETS_HANDLER_PATH: buckets,
    })
    serving.run_simple(arguments.host, int(arguments.port), application,
                       use_reloader=True, use_debugger=arguments.debug,
                       use_evalex=True)


if __name__ == '__main__':
    main()
