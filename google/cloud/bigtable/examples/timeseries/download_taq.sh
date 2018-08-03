#!/usr/bin/env bash
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

set -e

readonly SRC="ftp://ftp.nyxdata.com/Historical Data Samples/Daily TAQ Sample/"
readonly NBBO_SRC="${SRC}/EQY_US_ALL_NBBO_20161024.gz"
readonly TRADE_SRC="${SRC}/EQY_US_ALL_TRADE_20161024.gz"

if [ ! -r NBBO.unsorted.txt ]; then
  curl "${NBBO_SRC}" | gunzip - >NBBO.unsorted.txt
fi

if [ ! -r TRADE.unsorted.txt ]; then
  curl "${TRADE_SRC}" | gunzip - >TRADE.unsorted.txt
fi

# The data is sorted by symbol, which is not very realistic, resort by
# timestamp, but preserve the header and footer lines.
if [ ! -r NBBO.sorted.txt ]; then
  awk 'NR<1{print $0; next}
       /^END/ {print $0; next}
       {print $0 | "sort -n"}' NBBO.unsorted.txt >NBBO.sorted.txt
fi

if [ ! -r TRADE.sorted.txt ]; then
  awk 'NR<1{print $0; next}
       /^END/ {print $0; next}
       {print $0 | "sort -n"}' TRADE.unsorted.txt >TRADE.sorted.txt
fi
