#!/bin/bash

# --- Configuration ---

# MODIFIED: This now uses your bazel run command.
# The -- separates bazel args from the program's args.
BENCHMARK_CMD="bazel run //google/cloud/storage/benchmarks:storage_throughput_vs_cpu_benchmark --"

# Set this to the object prefix you are testing against
# IMPORTANT: You must have 1M+ objects at this prefix
PREFIX="gcs-cpp-benchmark-prefix-2-million-right/"

# --- FIX ---
# Set the bucket name
BUCKET_NAME="vaibhav-test-001"
# --- END FIX ---

# Page size is constant for all runs
PAGE_SIZE=5000

# Set the number of samples (e.g., 10) for each experiment
SAMPLES_PER_RUN=3

# --- MODIFICATION FOR PARALLEL RUN ---
# Directory to store temporary results, one file per job
TMP_DIR="benchmark_tmp_results"
# The final combined results file
RESULTS_FILE="benchmark_results.csv"
# --- END MODIFICATION ---


# Define the parameters for your 18 runs
# Using "Rest" for "json" as that is what the --enabled-transports flag expects
TRANSPORTS=("Grpc" "DirectPath" "Json") 
STRATEGIES=("start-offset")
OBJECT_COUNTS=(5000000)


# --- MODIFICATION FOR PARALLEL RUN ---
# Clean up old results and create the temp directory
rm -f $RESULTS_FILE
rm -rf $TMP_DIR
mkdir -p $TMP_DIR
echo "Temporary results will be stored in $TMP_DIR/"
# --- END MODIFICATION ---

# This line is moved down, *after* the wait
# echo "Transport,Strategy,TotalObjects,PageSize,LatencySeconds" > $RESULTS_FILE

echo "Starting all 6 benchmark experiments in PARALLEL..."
echo "(Based on your config: 3 transports * 2 strategies * 1 object count = 6 jobs)"

job_num=0 # Job counter for unique filenames

# Loop through all combinations
for transport in "${TRANSPORTS[@]}"; do
  for strategy in "${STRATEGIES[@]}"; do
    for objects in "${OBJECT_COUNTS[@]}"; do
      
      # Using your exact logic
      let max_pages=100000
      
      echo "---"
      echo "STARTING Job $job_num: Transport=$transport, Strategy=$strategy, Objects=$objects, Pages=$max_pages"
      
      # Construct the command
      # We use --thread-count=1 for a clean measurement
      CMD="$BENCHMARK_CMD \
        --bucket=$BUCKET_NAME \
        --prefix=$PREFIX \
        --strategy=$strategy \
        --enabled-transports=$transport \
        --minimum-object-size=$PAGE_SIZE \
        --maximum-object-size=$max_pages \
        --minimum-sample-count=$SAMPLES_PER_RUN \
        --maximum-sample-count=$SAMPLES_PER_RUN \
        --thread-count=1"
      
      # --- MODIFICATION FOR PARALLEL RUN ---
      # Define a unique temp file for this job's output
      TMP_FILE="$TMP_DIR/results-job-$job_num.csv"
      
      # Run the command in a subshell in the background (&)
      # All output is piped and redirected to the unique temp file
      (
        echo "Job $job_num ($transport/$strategy/$objects) processing..."
        # We pipe stderr (2) to stdout (1) using 2>&1
        $CMD 2>&1 | grep "DATA_ROW," | cut -d',' -f2- > $TMP_FILE
        echo "Job $job_num ($transport/$strategy/$objects) FINISHED."
      ) &
      # --- END MODIFICATION ---

      ((job_num++)) # Increment job counter
      
    done
  done
done

# --- MODIFICATION FOR PARALLEL RUN ---
echo "---"
echo "All $job_num jobs launched. Waiting for them to complete..."

# This pauses the script and waits for ALL background jobs to finish
wait

echo "All jobs finished."
echo "Combining results into $RESULTS_FILE..."

# Create the final results file and print the header
echo "Transport,Strategy,TotalObjects,PageSize,LatencySeconds" > $RESULTS_FILE

# Concatenate all temporary files into the final one
cat $TMP_DIR/results-job-*.csv >> $RESULTS_FILE
# --- END MODIFICATION ---

echo "---"
echo "All benchmarks complete. Data is ready in $RESULTS_FILE"
echo "Cleaning up temporary directory $TMP_DIR..."
rm -r $TMP_DIR