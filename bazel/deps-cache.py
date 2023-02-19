#!/usr/bin/env python3
#
# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Verify and (optionally) populate the GCS cache of Bazel dependencies."""

import argparse
import filecmp
import hashlib
import os.path
import subprocess
import sys
import tempfile
import urllib.parse
import urllib.request

# pylint: disable=broad-except
# pylint: disable=exec-used
# pylint: disable=unused-argument

default_deps_bzl = os.path.join(
    os.path.dirname(sys.argv[0]), "google_cloud_cpp_deps.bzl"
)

parser = argparse.ArgumentParser(description=(__doc__))
parser.add_argument(
    "-p",
    "--populate",
    action="store_true",
    help="cache write through on verification failure",
)
parser.add_argument(
    "google_cloud_cpp_deps.bzl",
    nargs="?",
    default=default_deps_bzl,
    help=(
        "path to the .bzl file that lists the cached HTTP archives"
        f' (default "{default_deps_bzl}")'
    ),
)
args = parser.parse_args()

# URL prefixes that are not a source-of-truth.
mirror_prefixes = [
    "https://mirror.bazel.build/",
    "https://storage.googleapis.com/",
]

# Cache bucket.
bucket = "cloud-cpp-community-archive"

# List of {name, source, sha256, cache, upload} dictionaries.
archives = []


def http_archive(**kwargs):
    """The repo_rule we expect to see in maybe()."""
    pass


def maybe(repo_rule, name, urls, sha256, **kwargs):
    """Accumulate maybe() arguments into global archives."""
    if repo_rule is not http_archive:
        sys.exit(f"{parser.prog}: Only supports http_archive rules")
    filtered_urls = [
        url
        for url in urls
        if not any(url.startswith(prefix) for prefix in mirror_prefixes)
    ]
    if len(filtered_urls) != 1:
        sys.exit(f"{name}: Ambiguous source URL {filtered_urls}")
    path = os.path.join(bucket, name, os.path.basename(filtered_urls[0]))
    archives.append(
        {
            "name": name,
            "source": filtered_urls[0],
            "sha256": sha256,
            "cache": f"https://storage.googleapis.com/{path}",
            "upload": f"gs://{path}",
        }
    )


def urlretrieve(url, filename, check=True):
    try:
        urllib.request.urlretrieve(url, filename)
    except Exception as e:
        if check:
            sys.exit(f"{url}: {e}")


def download(tmpdir, name, source, sha256, **kwargs):
    """Download archive and check signature."""
    source_file = os.path.join(tmpdir, urllib.parse.quote(source, safe=""))
    print(f"[ Downloading {source} ]")
    urlretrieve(source, source_file)
    try:
        with open(source_file, "rb") as f:
            chksum = hashlib.sha256(f.read()).hexdigest()
    except Exception as e:
        sys.exit(f"{name}: {e}")
    if chksum != sha256:
        sys.exit(f"{name}: Checksum mismatch")
    print("")


def cmp(f1, f2):
    try:
        return filecmp.cmp(f1, f2, shallow=False)
    except Exception:
        return False


def verify(tmpdir, name, source, cache, upload, **kwargs):
    """Verify and (optionally) populate the GCS cache."""
    source_file = os.path.join(tmpdir, urllib.parse.quote(source, safe=""))
    cache_file = os.path.join(tmpdir, urllib.parse.quote(cache, safe=""))
    print(f"[ Verifying {cache} ]")
    urlretrieve(cache, cache_file, check=False)
    same = cmp(source_file, cache_file)
    if not same and args.populate:
        print(f"[ Uploading {upload} ]")
        subprocess.run(["gsutil", "-q", "cp", source_file, upload], check=False)
        print(f"[ Reverifying {cache} ]")
        urlretrieve(cache, cache_file)
        same = cmp(source_file, cache_file)
    if not same:
        sys.exit(f"{name}: Source/cache mismatch")
    print("")


def main():
    bzl = vars(args)["google_cloud_cpp_deps.bzl"]
    exec_globals = {
        "__builtins__": None,
        "Label": lambda label: None,
        "load": lambda label, symbol: None,
        "native": type("native", (), {"bind": lambda name, actual: None}),
        "http_archive": http_archive,
        "maybe": maybe,
    }
    exec_locals = {}
    try:
        with open(bzl) as f:
            exec(compile(f.read(), bzl, "exec"), exec_globals, exec_locals)
    except Exception as e:
        sys.exit(f"{bzl}: {e}")
    google_cloud_cpp_deps = exec_locals.get("google_cloud_cpp_deps")
    try:
        google_cloud_cpp_deps(name="deps-cache")  # execute .bzl definitions
    except Exception as e:
        sys.exit(f"google_cloud_cpp_deps(): {e}")
    with tempfile.TemporaryDirectory() as tmpdir:
        for archive in archives:
            download(tmpdir, **archive)
        for archive in archives:
            verify(tmpdir, **archive)
    print("[ SUCCESS ]")


if __name__ == "__main__":
    main()
