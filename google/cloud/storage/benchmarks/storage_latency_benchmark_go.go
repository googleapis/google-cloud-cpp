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
	"io/ioutil"
	"log"
	"math/rand"
	"os"
	"runtime"
	"strconv"
	"time"

	"cloud.google.com/go/storage"
	"google.golang.org/api/iterator"
)

const (
	kDefaultDurationSeconds = 60
	kDefaultObjectCount     = 10000
)

type IterationResult struct {
	op      string
	success bool
	elapsed time.Duration
}

type TestResult []IterationResult

func main() {
	duration := kDefaultDurationSeconds
	objectCount := kDefaultObjectCount
	threadCount := runtime.NumCPU()

	if len(os.Args) < 2 {
		fmt.Fprintf(os.Stderr, "Usage: endurance_test_go <location>"+
			" [duration-in-seconds (%d)] [object-count (%d)] [thread-count (%d)]\n",
			duration, objectCount, threadCount)
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
		StorageClass:               "STANDARD",
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
	fmt.Printf("# Thread Count: %d\n", threadCount)
	fmt.Printf("# Build info: %s\n", runtime.Version())

	objectNames := CreateAllObjects(bucket, ctx, objectCount)

	ch := make(chan TestResult, threadCount)
	for i := 0; i < threadCount; i++ {
		go RunTest(bucket, ctx, objectNames, duration, ch)
	}
	for i := 0; i < threadCount; i++ {
		result := <-ch
		PrintResult(result)
	}

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
	prefix := "gcs-go-latency-"
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
		fmt.Printf("%s,%t,%d\n", r.op, r.success, r.elapsed.Nanoseconds()/1000000)
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

func CreateOneObject(bucket *storage.BucketHandle, ctx context.Context, objectName string, data string) {
	start := time.Now()
	w := bucket.Object(objectName).NewWriter(ctx)
	if _, err := w.Write([]byte(data)); err != nil {
		elapsed := time.Since(start)
		fmt.Printf("CREATE,false,%d\n", elapsed.Nanoseconds()/1000000)
	}
	if err := w.Close(); err != nil {
		elapsed := time.Since(start)
		fmt.Printf("CREATE,false,%d\n", elapsed.Nanoseconds()/1000000)
	}
	elapsed := time.Since(start)
	fmt.Printf("CREATE,true,%d\n", elapsed.Nanoseconds()/1000000)
}

func CreateAllObjects(bucket *storage.BucketHandle, ctx context.Context, objectCount int) []string {
	names := make([]string, 0, objectCount)
	for i := 0; i < objectCount; i++ {
		names = append(names, MakeRandomObjectName())
	}

	data := MakeRandomData(1024 * 1024)
	fmt.Printf("# Creating test objects [N/A]\n")
	start := time.Now()
	for _, name := range names {
		CreateOneObject(bucket, ctx, name, data)
	}
	elapsed := time.Since(start)
	fmt.Printf("# Created in %dms\n", elapsed.Nanoseconds()/1000000)
	return names
}

func WriteOnce(bucket *storage.BucketHandle, ctx context.Context, objectName string, data string) IterationResult {
	start := time.Now()
	w := bucket.Object(objectName).NewWriter(ctx)
	if _, err := w.Write([]byte(data)); err != nil {
		return IterationResult{op: "WRITE", success: false, elapsed: time.Since(start)}
	}
	if err := w.Close(); err != nil {
		return IterationResult{op: "WRITE", success: false, elapsed: time.Since(start)}
	}
	return IterationResult{op: "WRITE", success: true, elapsed: time.Since(start)}
}

func ReadOnce(bucket *storage.BucketHandle, ctx context.Context, objectName string) IterationResult {
	start := time.Now()
	rd, err := bucket.Object(objectName).NewReader(ctx)
	if err != nil {
		return IterationResult{op: "READ", success: false, elapsed: time.Since(start)}
	}
	_, err = ioutil.ReadAll(rd)
	rd.Close()
	if err != nil {
		return IterationResult{op: "READ", success: false, elapsed: time.Since(start)}
	}
	return IterationResult{op: "READ", success: true, elapsed: time.Since(start)}
}

func RunTest(bucket *storage.BucketHandle, ctx context.Context, objectNames []string,
	duration int,
	ch chan TestResult) {
	data := MakeRandomData(1024 * 1024)
	result := make([]IterationResult, 0, 5*duration)
	deadline := time.Now().Add(time.Duration(duration) * time.Second)
	for time.Now().Before(deadline) {
		name := objectNames[rand.Intn(len(objectNames))]
		if rand.Intn(100) < 50 {
			result = append(result, WriteOnce(bucket, ctx, name, data))
		} else {
			result = append(result, ReadOnce(bucket, ctx, name))
		}
	}
	ch <- result
}

func DeleteObject(bucket *storage.BucketHandle, ctx context.Context, objectName string) {
	start := time.Now()
	bucket.Object(objectName).Delete(ctx)
	elapsed := time.Since(start)
	fmt.Printf("DELETE,true,%d\n", elapsed.Nanoseconds()/1000000)
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
