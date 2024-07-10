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
import base64
import filecmp
import glob
import hashlib
import json
import os.path
import subprocess
import sys
import tempfile
import urllib.parse
import urllib.request

# pylint: disable=broad-except
# pylint: disable=exec-used
# pylint: disable=unused-argument

DEFAULT_FILES = glob.glob(
    os.path.join(os.path.dirname(sys.argv[0]), "workspace*.bzl")
) + glob.glob(os.path.join(os.path.dirname(sys.argv[0]), "development*.bzl"))

parser = argparse.ArgumentParser(description=(__doc__))
parser.add_argument(
    "-p",
    "--populate",
    action="store_true",
    help="cache write through on verification failure",
)
parser.add_argument(
    "bzls",
    nargs="*",
    default=DEFAULT_FILES,
    help=(
        "paths to the .bzl files that lists the cached HTTP archives"
        f' (default "{DEFAULT_FILES}")'
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


def http_archive(name, urls, integrity, **kwargs):
    """The repo_rule we expect to see in maybe()."""
    filtered_urls = [
        url
        for url in urls
        if not any(url.startswith(prefix) for prefix in mirror_prefixes)
    ]
    if len(filtered_urls) != 1:
        sys.exit(f"{name}: Ambiguous source URL {filtered_urls}")
    if not integrity.startswith("sha256-"):
        sys.exit(f"unknown integrity format in integrity = {integrity}")
    url = filtered_urls[0]
    url = url.removeprefix("https://")
    url = url.removeprefix("http://")
    path = os.path.join(bucket, url)
    sha256 = base64.b64decode(integrity.removeprefix("sha256-")).hex()
    archives.append(
        {
            "name": name,
            "source": filtered_urls[0],
            "sha256": sha256,
            "cache": f"https://storage.googleapis.com/{path}",
            "upload": f"gs://{path}",
        }
    )


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
    url = filtered_urls[0]
    url = url.removeprefix("https://")
    url = url.removeprefix("http://")
    path = os.path.join(bucket, url)
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


def bzlmod_modules(bzlmod_deps) -> list[str]:
    """Convert a JSON description of recursive bzlmod deps into a list of dependencies."""
    result = []
    for k, v in bzlmod_deps.items():
        if k == "key" and "@" in v:
            result.append(v.split("@")[0])
        elif k == "dependencies":
            for d in v:
                result.extend(bzlmod_modules(d))
    return result


def main():
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
        p = subprocess.run(
            ["bazelisk", "mod", "deps", "--output=json", "google_cloud_cpp"],
            capture_output=True,
            text=True,
        )
        modules = bzlmod_modules(json.loads(p.stdout))
        p = subprocess.run(
            ["bazelisk", "mod", "show_repo"] + modules, capture_output=True, text=True
        )
        exec(compile(p.stdout, "<show repo output>", "exec"), exec_globals, exec_locals)

    except Exception as e:
        sys.exit(f"{e}")
    for bzl in args.bzls:
        try:
            with open(bzl) as f:
                exec(compile(f.read(), bzl, "exec"), exec_globals, exec_locals)
        except Exception as e:
            sys.exit(f"{bzl}: {e}")

    for key, func in exec_locals.items():
        # At the moment only the "gl_cpp_*0" functions load cacheable objects.
        if not callable(func) or not key.startswith("gl_cpp_") or not key.endswith("0"):
            continue
        try:
            func(name="deps-cache")  # execute .bzl definitions
        except Exception as e:
            sys.exit(f"{key}(): {e}")
    with tempfile.TemporaryDirectory() as tmpdir:
        for archive in archives:
            download(tmpdir, **archive)
        for archive in archives:
            verify(tmpdir, **archive)
    print("[ SUCCESS ]")


if __name__ == "__main__":
    main()
