// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"
#include <chrono>
#include <cmath>
#include <fstream>
#include <future>
#include <iomanip>
#include <numeric>
#include <sstream>

/**
 * @file
 *
 * Shows how to compute estimated split points for sharding the computations.
 *
 * Many computations over timeseries can be sharded based on the key of the
 * timeseries.  Splitting the key space into more or less equal sized shards is
 * advantageous because that way each shard has similar amount of work to do.
 * This program shows how to sample the input data, in this case the
 * `taq-quotes-YYYYMMDD` table created by `upload_taq_nbbo.cc`, to estimate good
 * split points for the data.
 *
 * This computation itself is sharded, but the split points are simply rough
 * guesses.
 */

/// Helper functions for this demo.
namespace {
using SymbolWeights = std::map<std::string, long>;
namespace cbt = google::cloud::bigtable;

/// Sample the data and get approximate weights for each symbol.
SymbolWeights ApproximateWeights(cbt::Table& input);

/// Compute good split points based on symbol weights.
std::vector<std::string> Splits(int nsplits, SymbolWeights const& weights);
}  // anonymous namespace

int main(int argc, char* argv[]) try {
  // Make sure we have the right number of arguments.
  if (argc != 5) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project> <instance> <input_table_id> <nsplits>"
              << std::endl;
    return 1;
  }
  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];
  std::string const input_table_id = argv[3];
  int const nsplits = std::stoi(argv[4]);

  cbt::ClientOptions options;
  options.SetLoadBalancingPolicyName("round_robin");
  cbt::Table input(
      cbt::CreateDefaultDataClient(project_id, instance_id, options),
      input_table_id);

  auto symbol_weights = ApproximateWeights(input);
  auto splits = Splits(nsplits, symbol_weights);

  std::cout << "The following symbols are good split points: \n";
  char const* sep = "{";
  for (auto const& s : splits) {
    std::cout << sep << s;
    sep = ", ";
  }
  std::cout << "}" << std::endl;

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << std::endl;
  return 1;
}

namespace {

std::vector<std::string> Splits(int nsplits, SymbolWeights const& weights) {
  // First find the total count of samples across all symbols.
  long tcount =
      std::accumulate(weights.begin(), weights.end(), 0,
                      [](long acc, SymbolWeights::value_type const& kv) {
                        return acc + kv.second;
                      });

  std::vector<std::string> splits{""};
  long sum = 0;
  for (auto const& s : weights) {
    sum += s.second;
    if (sum >= tcount / nsplits) {
      splits.push_back(s.first);
      sum = 0;
    }
  }
  return splits;
}

/// Get the approximate weights for the symbols in the range [@p begin, @p end)
SymbolWeights ApproximateWeights(cbt::Table& input, std::string begin,
                                 std::string end) {
  using F = cbt::Filter;
  SymbolWeights weights;

  // Make a scan over the prescribed range, strip the results of any values,
  // because we only care about the row keys, and sample 0.1% of the rows.
  // TODO(#209) - use SampleRowKeys directly when it is implemented.
  auto sampler = input.ReadRows(
      cbt::RowSet(cbt::RowRange::Range(std::move(begin), std::move(end))),
      F::Chain(F::StripValueTransformer(), F::RowSample(0.001)));
  long count = 0;
  for (auto& row : sampler) {
    std::istringstream tokens(row.row_key());
    tokens.exceptions(std::ios::failbit);

    std::string symbol;
    std::getline(tokens, symbol, '#');

    auto ins = weights.emplace(symbol, 0);
    ins.first->second = ins.first->second + 1;
    // Report progress as the iteration proceeds.
    if (ins.second) {
      if (++count % 100 == 0) {
        std::cout << "." << std::flush;
      }
    }
  }
  return weights;
}

std::map<std::string, long> ApproximateWeights(cbt::Table& input) {
  std::cout << "Sampling input data " << std::flush;
  using namespace std::chrono;
  auto start = steady_clock::now();
  // Start with a guess on where to split the data, this does not have to
  // be a good guess.
  std::vector<std::string> split_guess{
      "A", "B", "C", "D", "E", "F", "G", "H", "I", "M", "P", "S", "T",
  };
  // Start and end the list with the magical "" values, which mean "infinity" in
  // the Cloud Bigtable ranges.
  split_guess.emplace_back("");
  split_guess.insert(split_guess.begin(), "");

  // Create a thread for each point in split_guess, and call
  // ApproximateWeightsRange() in each thread.
  std::vector<std::future<SymbolWeights>> tasks;
  for (std::size_t i = 0; i != split_guess.size() - 1; ++i) {
    auto worker = [&input](std::string begin, std::string end) {
      return ApproximateWeights(input, std::move(begin), std::move(end));
    };
    tasks.emplace_back(std::async(std::launch::async, worker, split_guess[i],
                                  split_guess[i + 1]));
  }

  // Collect the results from running each thread.
  SymbolWeights weights;
  for (auto& t : tasks) {
    auto partial_weights = t.get();
    for (auto& w : partial_weights) {
      weights.emplace(w.first, w.second);
    }
  }

  // Report the elapsed time.
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::steady_clock::now() - start);
  std::cout << " DONE in " << elapsed.count() << "s" << std::endl;
  return weights;
}

}  // anonymous namespace
