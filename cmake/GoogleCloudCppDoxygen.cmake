# Copyright 2018 Google LLC
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

# Refactor the code to generate Doxygen documentation to a separate file.
if (${CMAKE_VERSION} VERSION_LESS "3.9")
    # Old versions of CMake have really poor support for Doxygen generation.
    message(STATUS "Doxygen generation only enabled for cmake 3.9 and higher")
else ()
    find_package(Doxygen)
    if (Doxygen_FOUND)
        set(GOOGLE_CLOUD_CPP_DOXYGEN_INPUTS
                "${PROJECT_SOURCE_DIR}/doc"
                "${PROJECT_SOURCE_DIR}/google/cloud"
                "${PROJECT_SOURCE_DIR}/google/cloud/bigtable"
                "${PROJECT_SOURCE_DIR}/google/cloud/bigtable/doc"
                "${PROJECT_SOURCE_DIR}/google/cloud/firestore"
                "${PROJECT_SOURCE_DIR}/storage/client")
        set(DOXYGEN_PREDEFINED
                "GOOGLE_CLOUD_CPP_NS=v${GOOGLE_CLOUD_CPP_VERSION_MAJOR}"
                "BIGTABLE_CLIENT_NS=v${BIGTABLE_CLIENT_VERSION_MAJOR}"
                "FIRESTORE_CLIENT_NS=v${FIRESTORE_CLIENT_VERSION_MAJOR}"
                "STORAGE_CLIENT_NS=v${STORAGE_CLIENT_VERSION_MAJOR}")
        set(DOXYGEN_EXAMPLE_PATH
            "${PROJECT_SOURCE_DIR}/google/cloud/bigtable/examples"
            "${PROJECT_SOURCE_DIR}/storage/examples")
        set(DOXYGEN_EXCLUDE_PATTERNS "*/README.md" "*_test.cc")

        # Do not recurse. Recursively going through the code picks up the
        # tests, examples, internal namespaces, and many other artifacts that
        # should not be included in the Doxygen documentation.
        set(DOXYGEN_RECURSIVE NO)
        # The Doxygen documentation is in *.dox files, which often contain
        # markdown, but also contain Doxygen directives. We reserve the *.md
        # files for GitHub markdown.
        set(DOXYGEN_FILE_PATTERNS *.h *.cc *.proto *.dox)
        set(DOXYGEN_EXAMPLE_RECURSIVE NO)
        set(DOXYGEN_EXCLUDE third_party)
        set(DOXYGEN_EXCLUDE_SYMLINKS YES)
        set(DOXYGEN_QUIET YES)
        set(DOXYGEN_WARN_AS_ERROR YES)
        set(DOXYGEN_INLINE_INHERITED_MEMB YES)
        set(DOXYGEN_JAVADOC_AUTOBRIEF YES)
        set(DOXYGEN_BUILTIN_STL_SUPPORT YES)
        set(DOXYGEN_IDL_PROPERTY_SUPPORT NO)
        set(DOXYGEN_EXTRACT_ALL YES)
        set(DOXYGEN_EXTRACT_STATIC YES)
        set(DOXYGEN_SORT_MEMBERS_CTORS_1ST YES)
        set(DOXYGEN_CLANG_ASSISTED_PARSING YES)
        set(DOXYGEN_CLANG_OPTIONS "\
-std=c++11 \
-I${PROJECT_SOURCE_DIR} \
-I${PROJECT_BINARY_DIR} \
-I${PROJECT_THIRD_PARTY_DIR}/googletest/googletest/include \
-I${PROJECT_THIRD_PARTY_DIR}/googletest/googlemock/include")
        set(DOXYGEN_GENERATE_LATEX NO)
        set(DOXYGEN_GRAPHICAL_HIERARCHY NO)
        set(DOXYGEN_DIRECTORY_GRAPH NO)
        set(DOXYGEN_CLASS_GRAPH NO)
        set(DOXYGEN_COLLABORATION_GRAPH NO)
        set(DOXYGEN_INCLUDE_GRAPH NO)
        set(DOXYGEN_INCLUDED_BY_GRAPH NO)
        set(DOXYGEN_DOT_TRANSPARENT YES)
        set(DOXYGEN_MACRO_EXPANSION YES)
        set(DOXYGEN_EXPAND_ONLY_PREDEF YES)
        set(DOXYGEN_HTML_TIMESTAMP )
        set(DOXYGEN_STRIP_FROM_INC_PATH "${PROJECT_SOURCE_DIR}")
        set(DOXYGEN_PROJECT_NAME "Google Cloud C++ Client Libraries")
        set(DOXYGEN_PROJECT_BRIEF "C++ Idiomatic Clients for Google Cloud Platform services.")
        set(DOXYGEN_PROJECT_NUMBER
                "${GOOGLE_CLOUD_CPP_VERSION_MAJOR}.${GOOGLE_CLOUD_CPP_VERSION_MINOR}.${GOOGLE_CLOUD_CPP_VERSION_PATCH}")

        # The external documentation does not include these lists.
        set(DOXYGEN_GENERATE_TODOLIST NO)
        set(DOXYGEN_GENERATE_BUGLIST NO)
        set(DOXYGEN_GENERATE_TESTLIST NO)
        doxygen_add_docs(doxygen-docs
                ${GOOGLE_CLOUD_CPP_DOXYGEN_INPUTS}
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                COMMENT "Generate External HTML documentation")

        # Enable bits that are interesting for developers and use a separate
        # target for them.
        set(DOXYGEN_GENERATE_TODOLIST YES)
        set(DOXYGEN_GENERATE_BUGLIST YES)
        set(DOXYGEN_GENERATE_TESTLIST YES)
        doxygen_add_docs(doxygen-developer-docs
                ${GOOGLE_CLOUD_CPP_DOXYGEN_INPUTS}
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                COMMENT "Generate Developer HTML documentation")
    endif ()
endif ()
