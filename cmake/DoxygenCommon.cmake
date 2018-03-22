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

# Define common settings for all google-cloud-cpp subprojects.

set(DOXYGEN_RECURSIVE YES)
set(DOXYGEN_FILE_PATTERNS *.h *.cc *.proto *.md *.dox)
set(DOXYGEN_EXAMPLE_RECURSIVE YES)
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
set(DOXYGEN_GENERATE_TODOLIST NO)
set(DOXYGEN_GENERATE_BUGLIST NO)
set(DOXYGEN_GENERATE_TESTLIST NO)
set(DOXYGEN_CLANG_ASSISTED_PARSING YES)
set(DOXYGEN_CLANG_OPTIONS "-std=c++11 -I.. -I${PROJECT_SOURCE_DIR}/googletest/googletest/include -I${PROJECT_SOURCE_DIR}/googletest/googlemock/include")
set(DOXYGEN_GENERATE_LATEX NO)
set(DOXYGEN_GRAPHICAL_HIERARCHY NO)
set(DOXYGEN_DIRECTORY_GRAPH NO)
set(DOXYGEN_DOT_TRANSPARENT YES)
set(DOXYGEN_MACRO_EXPANSION YES)
set(DOXYGEN_EXPAND_ONLY_PREDEF YES)
set(DOXYGEN_HTML_TIMESTAMP )
set(DOXYGEN_STRIP_FROM_INC_PATH "${PROJECT_SOURCE_DIR}")
