# Copyright 2017 Google Inc.
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


# Exit the script with an error status if any files had bad guards.
BEGIN {
    found_bad_include_guards = 0;
}
END {
    if (found_bad_include_guards) {
        exit 1;
    }
}

# Reset the state at the beginning of each file.
BEGINFILE {
    # The guard must begin with the name of the project.
    guard_prefix="GOOGLE_CLOUD_CPP_"
    # The guard must end with "_"
    guard_suffix="_"
    # The guard name is the filename (including path from the root of the
    # project), with "/" and "." characters replaced with "_", and all
    # characters converted to uppercase:
    guard_body=toupper(FILENAME)
    gsub("[/\\.]", "_", guard_body)
    guard=toupper(guard_prefix guard_body guard_suffix)
    matches=0
}

# By the end of the file we expect to find 3 matches for the include guards.
ENDFILE {
    if (matches != 3) {
        printf("%s has invalid include guards\n", FILENAME);
        found_bad_include_guards = 1;
    }
}

# Check only lines that start with #ifndef, #define, or #endif.
/^#ifndef / {
    # Ignore lines that do not look like guards at all.
    if ($0 !~ "_H_$") { next; }
    if (index($0, guard) == 9) {
        matches++;
    } else {
        printf("%s:\n", FILENAME)
        printf("expected: #ifndef %s\n", FILENAME, guard)
        printf("   found: %s\n", $0);
    }
}

/^#define / {
    # Ignore lines that do not look like guards at all.
    if ($0 !~ "_H_$") { next; }
    if (index($0, guard) == 9) {
      matches++;
    } else {
      printf("%s:\n", FILENAME)
      printf("expected: #define %s\n", guard)
      printf("   found: %s\n", $0);
    }
}

/^#endif / {
    # Ignore lines that do not look like guards at all.
    if ($0 !~ "_H_$") { next; }
    if (index($0, "// " guard) == 9) {
        matches++;
    } else {
      printf("%s:\n", FILENAME)
      printf("expected: #endif  // %s\n", guard)
      printf("   found: %s\n", $0);
    }
}
