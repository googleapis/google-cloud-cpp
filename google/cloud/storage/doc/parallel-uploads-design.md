# Parallel uploads design
## About this doc
This doc summarizes how parallel uploads are implemented, why and what
trade-offs there are.

## Purpose
Provide the GCS client libraries with a way to upload data much faster than a
simple, single stream upload.

## Feature overview
When uploading an object to GCS over a single connection the bandwidth does not
exceed `50MiB/s` (as measured in
[#2657](https://github.com/googleapis/google-cloud-cpp/issues/2657)). CPU,
memory or network are not the limiting factors - individual HTTP requests’
latency is. When the data is known upfront, one can overcome this issue by
opening multiple connections and uploading shards of the data concurrently and
eventually creating the destination object by composing the shards. This is
almost exactly how parallel uploads work from 10000ft.

## Atomicity considerations
The implementation of parallel uploads involves multiple operations. GCS doesn’t
provide any transaction support, so a decision needs to be made on how to handle
this situation. One has to think about the following situations:
* a failure in the middle of the parallel upload
* a failure in cleaning up the temporary files
* a long-lasting connectivity problem
* client library crash

Below are several approaches considered to tackle these.

### 1. Not do anything special
Just pretend that the problem doesn’t exist.
#### Pros
* simple
#### Cons
* if the bucket doesn’t have a lifecycle management, stray objects might pollute
it indefinitely
* the extra stray objects might be a surprise to the users

### 2. Return the objects to clean up to the user
The API to perform a parallel upload might return the list of files (empty on
success) to clean up if it fails to clean up itself.
#### Pros
* still pretty simple
* the user has to be aware of the need to clean up the files
#### Cons
* if the client library crashes, the user will not know what stray files to
  delete or, worse, there would have to be a database of those files somewhere
* in the event of a network problem, the user can’t do much with the information
  \- they can’t store it in GCS so it has to be stored either in a different
  storage system or become even more transient

### 3. Use retention policies or lifecycle management
A retention policy of lifecycle management could be set on the bucket to ensure
eventual deletion of the temporary files.
#### Pros
* stray files will eventually be deleted
* API is as simple as if the operation was atomic
#### Cons
* retention policies and lifecycle management are per-bucket and likely the
  users’ needs regarding them are going to be different than the need to remove
  the stray files from parallel uploads

### 4. Use a separate bucket for temporary objects
[option4-ref]: #4-use-a-separate-bucket-for-temporary-objects
We might let the user have a separate bucket for the temporary objects with an
aggressive retention policy. The destination object would be rewritten to the
desired bucket at the end of the whole operation
#### Pros
* stray files will be deleted quickly and will not pollute the namespace of the
  users’ buckets
#### Cons
* the user/account performing the upload might not have permissions to create
  new buckets, so setup would have to be separate from the upload itself
* if the temporary bucket is not in the same region or not the same storage
  class, rewriting the destination object to the desired bucket would make the
  customer pay extra (might be unexpected), so they would have to either have a
  complicated logic of having multiple temporary buckets in sync with their
  "primary" buckets or pay more

### 5. Let the user control the naming of the temporary files
[option5-ref]: #5-let-the-user-control-the-naming-of-the-temporary-files
We might ask the user to provide a prefix, which will be prepended to all
temporary files created by the operation.
#### Pros
* the user will know which files are stray
* the user might prefix operations with `year-month-day-<random_bit>` and run a
  cron job to delete all objects with a `prefix year-month-<day - 2>`
* the prefix is a mandatory argument, so stray files won’t surprise the user
#### Cons
* the stray files have to be manually cleaned up by the user

### 6. Introduce atomicity by creating a write-ahead-log
We might implement a complex logic to keep track of all temporary files in a
separate object(s) in GCS (log) and clean them up whenever the log indicates so.
#### Pros
* the API would be as simple as possible
* the cleanup would be transparent to the user
#### Cons
* the temporary files might surprise the user
* there would be a performance penalty to the implementation
* the implementation would be pretty complex, considering concurrency and GCS
  API

### 7. Hybrid of [4.][option4-ref] and [5.][option5-ref]
[option7-ref]: #7-hybrid-of-4-and-5
We might ask the user to provide both a bucket and a prefix for the temporary
files, giving them the choice of using a bucket with lifecycle policies for the
temporaries at the cost of a (single; final) rewrite if the bucket names do not
match.
#### Pros
* the user will know which files are stray
* the user might prefix operations with `year-month-day-<random_bit>` and run a
  cron job to delete all objects with a prefix `year-month-<day - 2>`
* the user might delegate removing the stray files to a separate’s bucket policy
* the prefix is a mandatory argument, so stray files won’t surprise the user
#### Cons
* the user might be surprised by the extra cost of rewriting the data

### Decision
We went with the prefix-approach ([5.][option5-ref]), but we should probably
extend it to the hybrid approach ([7.][option7-ref])

## Detailed design
Depending on the user’s needs, they can choose one of the two options:
* non-resumable upload (if the client library crashes, they have to start from
  scratch)
* resumable upload (a tiny bit more costly, but allows for continuing the upload
  after crash or, perhaps in the future, for multiple processes uploading
  different shards)

### Computing the optimal number of shards
The formula for the optimal number of shards was obtained by experimenting with
a simple benchmark. The raw results and discussion is in the [github
issue](https://github.com/googleapis/google-cloud-cpp/issues/2951#issuecomment-565760791).
We split the file into shards which are no less than 32MiB but cap their number
to 64. The short summary for this decision is:
* total bandwidth drops if shards are smaller than 32MiB
* with 64 streams we can do around 10Gib/s
* in Jan 2020 all the machine types (except `n1-standard-1`) have at least 10Gib
  network
* in Jan 2020 the only storage option which can go significantly faster than
  this is local SSD

### Non-resumable upload
There are three APIs building on each other. They are:
#### `ParallelUploadFile()`
This is just one function used for uploading files with known size. It spins up
additional threads under the hood. Its use is straight-forward.
#### `CreateUploadShards()`
`CreateUploadShards()` gives the user more control over which threads to execute
the upload on. It returns a vector of `ParallelUploadFileShard`. The user is
expected to call `Upload()` member function on each of them in whichever threads
they like. Once all of them complete, the shards are automatically composed and
temporary objects are cleaned up. The metadata of the resulting object can be
obtained via futures returned by `ParallelUploadFileShard::WaitForCompletion()`
member function.
#### `PrepareParallelUpload()`
`PrepareParallelUpload()` gives the user the most control because it abstracts
the file away from `CreateUploadShards()`. Instead of specifying the file, the
user gets `ObjectWriteStreams`, which they can write to. Similarly to
`CreateUploadShards()`, once all of them complete, the shards are automatically
composed and temporary objects are cleaned up. The metadata of the resulting
object can be obtained via futures returned by
`NonResumableParallelUploadState::WaitForCompletion()` member function.

#### Control flow
`ParallelUploadFile()`, works the following way:
* check the file size to compute the boundaries of each shard to upload
* create a prefix lock (an object whose name is the supplied `prefix` for
  temporary objects) to avoid prefix name collisions
* create a `NonResumableParallelUploadState` object via
  `PrepareParallelUpload()`, which will collect the status of uploading each
  shard and hooks into ObjectWriteStreams associated with shards so that is
  happens automatically when the streams close. When the last stream closes,
  `NonResumableParallelUploadState` will also invoke `ComposeMany` to create the
  final object and then cleanup the shard objects and the prefix lock.
* create `ParallelObjectWriteStreambuf` for every shard
* create `ObjectWriteStreams` with previously mentioned
  `ParallelObjectWriteStreambufs`
* bind the `ObjectWriteStream` with relevant parts of the file by creating
  `ParallelUploadFileShards`
* create one thread per every `ParallelUploadFileShard` and upload the relevant
  shards in those threads (as mentioned before - composing all shards into the
  destination object and cleanup happen in the thread finishing shard upload
  last)
* join all threads

### Resumable upload
When one needs to upload a large file, it would be wasteful to start over in
case of a crash. The API is identical and consists of the same three layers as
the non-resumable version. The parallel upload state needs to be persistent to
allow for that, so we keep it in GCS as one of the temporary files and we call
it the state object.

#### State object
State object is a simple GCS object. It is supposed to reflect
`ResumableParallelUploadState` - a stateful counterpart of
`NonResumableParallelUploadState`. It holds a JSON with the following keys:
* `state`: a string with either `InProgress` or `Finished`
* `status_code`: an integer with the status code (only meaningful after the
  whole upload is finished)
* `prefix`: the prefix this operation was initiated with
* `dest_name`: a string with the destination object name
* `status_message`: the message accompanying the above code
* `streams`: an array of objects, which contain:
  * `name`: the name of the object underlying this stream
  * `resumable_id`: the resumable session ID of the object if it has not been
    not fully uploaded yet
  * `generation_id`: the generation ID of the object if it has been fully
    uploaded

This object is updated every time it changes using optimistic locking (via
`IfGenerationMatch`).

#### Faking resumable session ID
Parallel uploads are a concept built on top of the regular GCS API, hence GCS
does not have any concept of resuming them. In order to allow for that we create
our own resumable session IDs, which are essentially the name of the state
object.

#### Control flow
The resumable version of `ParallelUploadFile()` works in a very similar way,
except:
* state changes yield writing to GCS
* `ObjectWriteStreams` are created with the resumable session IDs obtained from
  the state file
* the offsets in the file are accordingly adjusted to account for the already
  uploaded data
