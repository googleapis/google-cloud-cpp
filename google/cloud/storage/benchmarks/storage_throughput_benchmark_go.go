// Copyright 2018 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package main

import (
	"context"
	"fmt"
	"io"
	"log"
	"math/rand"
	"os"
	"path"
	"runtime"
	"strconv"
	"strings"
	"time"

	"cloud.google.com/go/storage"
	"google.golang.org/api/iterator"
)

const (
	kDefaultDurationSeconds           = 60
	kDefaultObjectCount               = 1000
	kChunkSize                        = 1024 * 1024
	kDefaultObjectChunkCount          = 250
	kThroughputReportIntervalInChunks = 4
	kReadOp = 1
	kWriteOp = 2
	kCreateOp = 3
)

type IterationResult struct {
	op      int
	bytes   int
	elapsed time.Duration
}

type TestResult []IterationResult

func main() {
	duration := kDefaultDurationSeconds
	objectCount := kDefaultObjectCount
	objectChunkCount := kDefaultObjectChunkCount
	threadCount := 1

	if len(os.Args) < 2 {
		fmt.Fprintf(os.Stderr, "Usage: %s <location>"+
			" [duration-in-seconds (%d)] [object-count (%d)] [object-size-in-chunks (%d)] [thread-count (%d)]\n",
			path.Base(os.Args[0]), duration, objectCount, objectChunkCount, threadCount)
		os.Exit(1)
	}
	location := os.Args[1]

	if len(os.Args) > 2 {
		v, err := strconv.Atoi(os.Args[2])
		if err != nil {
			log.Fatal("%v while parsing duration argument (%s)", err, os.Args[2])
		}
		duration = v
	}
	if len(os.Args) > 3 {
		v, err := strconv.Atoi(os.Args[3])
		if err != nil {
			log.Fatal("%v while parsing object-count argument (%s)", err, os.Args[3])
		}
		objectCount = v
	}
	if len(os.Args) > 4 {
		v, err := strconv.Atoi(os.Args[4])
		if err != nil {
			log.Fatal("%v while parsing object-chunk-count argument (%s)", err, os.Args[4])
		}
		objectChunkCount = v
	}
	if len(os.Args) > 5 {
		v, err := strconv.Atoi(os.Args[5])
		if err != nil {
			log.Fatal("%v while parsing thread-count argument (%s)", err, os.Args[4])
		}
		threadCount = v
	}

	ctx := context.Background()

	projectID := os.Getenv("GOOGLE_CLOUD_PROJECT")
	if projectID == "" {
		fmt.Fprintf(os.Stderr, "GOOGLE_CLOUD_PROJECT environment variable must be set.\n")
		os.Exit(1)
	}

	client, err := storage.NewClient(ctx)
	if err != nil {
		log.Fatal(err)
	}

	// Initialize the default PRNG so that different runs do not generate the
	// same sequence.
	rand.Seed(time.Now().UnixNano())

	bucketName := MakeRandomBucketName()
	bucket := client.Bucket(bucketName)
	if err := bucket.Create(ctx, projectID, &storage.BucketAttrs{
		StorageClass:               "REGIONAL",
		Location:                   location,
		PredefinedACL:              "private",
		PredefinedDefaultObjectACL: "projectPrivate",
	}); err != nil {
		log.Fatal(err)
	}
	fmt.Printf("# Running test on bucket: %v\n", bucketName)
	fmt.Printf("# Start time: %s\n", time.Now().Format(time.RFC3339))
	fmt.Printf("# Location: %s\n", location)
	fmt.Printf("# Object Count: %d\n", objectCount)
	fmt.Printf("# Object Chunk Count: %d\n", objectChunkCount)
	fmt.Printf("# Thread Count: %d\n", threadCount)
	fmt.Printf("# Build info: %s\n", runtime.Version())

	objectNames := CreateAllObjects(bucket, ctx, objectCount, objectChunkCount, threadCount)
	RunTest(bucket, ctx, duration, objectNames, objectChunkCount, threadCount)
	DeleteAllObjects(bucket, ctx, objectCount)

	fmt.Printf("# Deleting %v\n", bucketName)
	if err := bucket.Delete(ctx); err != nil {
		log.Fatal(err)
	}
}

func sample(letters []rune, n int) string {
	b := make([]rune, n)
	for i := range b {
		b[i] = letters[rand.Intn(len(letters))]
	}
	return string(b)
}

func MakeRandomBucketName() string {
	letters := []rune("abcdefghijklmnopqrstuvwxyz0123456789")
	prefix := "gcs-go-throughput-test-"
	const maxBucketNameLength = 63

	return prefix + sample(letters, maxBucketNameLength-len(prefix))
}

func MakeRandomObjectName() string {
	letters := []rune("abcdefghijklmnopqrstuvwxyz" + "ABCDEFGHIJKLMNOPQRSTUVWXYZ" +
		"0123456789")
	return sample(letters, 128)
}

func PrintResult(result TestResult) {
	for _, r := range result {
	        op := "UNKNOWN"
		if r.op == kReadOp {
		    op = "READ"
		}
		if r.op == kWriteOp {
		    op = "WRITE"
		}
		if r.op == kCreateOp {
		    op = "CREATE"
		}
		fmt.Printf("%s,%d,%d\n", op, r.bytes, r.elapsed.Nanoseconds()/1000000)
	}
}

func MakeRandomData(desiredSize int) string {
	chars := []rune("abcdefghijklmnopqrstuvwxyz" + "ABCDEFGHIJKLMNOPQRSTUVWXYZ" +
		"0123456789" + " - _ : /")
	const (
		kLineSize = 128
	)
	result := ""
	for len(result)+kLineSize < desiredSize {
		result = result + sample(chars, kLineSize-1) + "\n"
	}
	if len(result) < desiredSize {
		result = result + sample(chars, desiredSize-len(result))
	}
	return result
}

func CreateOneObject(bucket *storage.BucketHandle, ctx context.Context,
	objectName string, data string, objectChunkCount int) []IterationResult {
	return WriteCommon(bucket, ctx, objectName, data, objectChunkCount, kCreateOp)
}

func CreateAllObjects(bucket *storage.BucketHandle, ctx context.Context,
	objectCount int, objectChunkCount int, threadCount int) []string {
	names := make([]string, 0, objectCount)
	for i := 0; i < objectCount; i++ {
		names = append(names, MakeRandomObjectName())
	}

	data := MakeRandomData(kChunkSize)
	fmt.Printf("# Creating test objects [N/A]\n")
	start := time.Now()
	for _, name := range names {
		r := CreateOneObject(bucket, ctx, name, data, objectChunkCount)
		PrintResult(r)
	}
	elapsed := time.Since(start)
	fmt.Printf("# Created in %dms\n", elapsed.Nanoseconds()/1000000)
	return names
}

func WriteOnce(bucket *storage.BucketHandle, ctx context.Context,
	objectName string, data string, objectChunkCount int) []IterationResult {
	return WriteCommon(bucket, ctx, objectName, data, objectChunkCount, kWriteOp)
}

func WriteCommon(bucket *storage.BucketHandle, ctx context.Context,
	objectName string, data string, objectChunkCount int, opName int) []IterationResult {
	start := time.Now()
	result := make([]IterationResult, 0, objectChunkCount)

	w := bucket.Object(objectName).NewWriter(ctx)
	for i := 0; i < objectChunkCount; i++ {
	    r := strings.NewReader(data)
	    n, err := io.Copy(w, r)
	    if err != nil {
			result = append(result, IterationResult{op: opName, bytes: -1, elapsed: time.Since(start)})
		}
	    if n != int64(len(data)) {
	       fmt.Printf("# Short write %d / %d\n", n, len(data))
	    }
		if i != 0 && i%kThroughputReportIntervalInChunks == 0 {
			result = append(result, IterationResult{op: opName, bytes: i * len(data), elapsed: time.Since(start)})
		}
	}
	if err := w.Close(); err != nil {
	   fmt.Printf("# Error %v\n", err);
	}
	result = append(result, IterationResult{op: opName, bytes: objectChunkCount * len(data), elapsed: time.Since(start)})
	return result
}

func ReadOnce(bucket *storage.BucketHandle, ctx context.Context, objectName string) []IterationResult {
	start := time.Now()
	result := make([]IterationResult, 0, kDefaultObjectChunkCount)

	rd, err := bucket.Object(objectName).NewReader(ctx)
	if err != nil {
		result = append(result, IterationResult{op: kReadOp, bytes: 0, elapsed: time.Since(start)})
		return result
	}
	buf := make([]byte, 4096)
	totalSize := 0
	const (
		report = kThroughputReportIntervalInChunks * kChunkSize
	)
	for {
		n, err := io.ReadFull(rd, buf)
		if err == io.EOF {
		   break
		}
		if err != nil {
			result = append(result, IterationResult{op: kReadOp, bytes: -1, elapsed: time.Since(start)})
			continue
		}
		totalSize += n
		if totalSize != 0 && totalSize%report == 0 {
			result = append(result, IterationResult{op: kReadOp, bytes: totalSize, elapsed: time.Since(start)})
		}
	}
	rd.Close()
	result = append(result, IterationResult{op: kReadOp, bytes: totalSize, elapsed: time.Since(start)})
	return result
}

func RunTestThread(bucket *storage.BucketHandle, ctx context.Context,
	duration int, objectNames []string, objectChunkCount int, ch chan TestResult) {
	data := MakeRandomData(kChunkSize)
	result := make([]IterationResult, 0, duration*objectChunkCount/10)
	deadline := time.Now().Add(time.Duration(duration) * time.Second)
	for time.Now().Before(deadline) {
		name := objectNames[rand.Intn(len(objectNames))]
		if rand.Intn(100) < 50 {
			result = append(result, WriteOnce(bucket, ctx, name, data, objectChunkCount)...)
		} else {
			result = append(result, ReadOnce(bucket, ctx, name)...)
		}
	}
	ch <- result
}

func RunTest(bucket *storage.BucketHandle, ctx context.Context,
	duration int, objectNames []string, objectChunkCount int, threadCount int) {
	ch := make(chan TestResult, threadCount)
	for i := 0; i < threadCount; i++ {
		go RunTestThread(bucket, ctx, duration, objectNames, objectChunkCount, ch)
	}
	for i := 0; i < threadCount; i++ {
		result := <-ch
		PrintResult(result)
	}
}

func DeleteObject(bucket *storage.BucketHandle, ctx context.Context, objectName string) {
	bucket.Object(objectName).Delete(ctx)
}

func DeleteAllObjects(bucket *storage.BucketHandle, ctx context.Context, objectCount int) {
	fmt.Printf("# Deleting test objects [N/A]\n")
	start := time.Now()
	names := make([]string, 0, objectCount)
	it := bucket.Objects(ctx, nil)
	for {
		objAttrs, err := it.Next()
		if err == iterator.Done {
			break
		}
		if err != nil {
			log.Fatal("%v while getting object list", err)
		}
		names = append(names, objAttrs.Name)
	}
	for _, name := range names {
		DeleteObject(bucket, ctx, name)
	}
	elapsed := time.Since(start)
	fmt.Printf("# Deleted in %dms\n", elapsed.Nanoseconds()/1000000)
}
