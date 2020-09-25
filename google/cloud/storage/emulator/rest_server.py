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

import flask
import httpbin
from werkzeug import serving
from werkzeug.middleware.dispatcher import DispatcherMiddleware

db = None

# === DEFAULT ENTRY FOR REST SERVER === #
root = flask.Flask(__name__)
root.debug = True


@root.route("/")
def index():
    return "OK"


# === SERVER === #


server = DispatcherMiddleware(root, {"/httpbin": httpbin.app})


def run(port, database):
    global db
    db = database
    serving.run_simple(
        "localhost", int(port), server, use_reloader=False, threaded=True
    )
