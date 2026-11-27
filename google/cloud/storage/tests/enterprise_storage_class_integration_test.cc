// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/lifecycle_rule.h"
#include "google/cloud/storage/storage_class.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
#include "google/cloud/storage/async/bucket_name.h"
#include "google/cloud/storage/async/client.h"
#include "google/cloud/storage/async/options.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/grpc_options.h"
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/match.h"
#include <gmock/gmock.h>
#include <chrono>
#include <iterator>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Optional;
using ::testing::SizeIs;

constexpr char kEnableEnvVar[] = "GOOGLE_CLOUD_CPP_STORAGE_TEST_ENTERPRISE";
constexpr char kRegionalLocation[] = "us-central1";
constexpr char kMultiRegionalLocation[] = "us";
constexpr std::string_view kPayload =
    "enterprise storage class end-to-end verification payload";

bool IsEnterpriseTestEnabled() {
  std::string const enabled = GetEnv(kEnableEnvVar).value_or("");
  return enabled == "1" || absl::EqualsIgnoreCase(enabled, "true");
}

#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
Options MakeAsyncTestOptions() {
  Options opts = Options{}
                     .set<storage_experimental::EnableGrpcMetricsOption>(false)
                     .set<GrpcNumChannelsOption>(1);
  if (std::optional<std::string> v =
          GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_GRPC_ENDPOINT")) {
    opts.set<EndpointOption>(*std::move(v));
  }
  if (std::optional<std::string> v =
          GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_AUTHORITY")) {
    opts.set<AuthorityOption>(*std::move(v));
  }
  return opts;
}

std::string CollectPayload(ReadPayload const& payload) {
  std::string result;
  for (std::string_view chunk : payload.contents()) {
    result.append(chunk.data(), chunk.size());
  }
  return result;
}
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC

/**
 * End-to-end integration tests for GCS Enterprise storage classes
 * (`STANDARD_ENTERPRISE`, `LR_STANDARD_ENTERPRISE`, and
 * `STANDARD_FLEXBACKUP_ENTERPRISE`) across REST (JSON), gRPC, and AsyncClient.
 *
 * Requires a project allowlisted for Enterprise storage classes in
 * `GOOGLE_CLOUD_PROJECT` and opt-in activation via
 * `GOOGLE_CLOUD_CPP_STORAGE_TEST_ENTERPRISE=true` (or `1`).
 */
class EnterpriseStorageClassIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    if (UsingEmulator()) {
      GTEST_SKIP() << "Enterprise storage class tests require production GCS.";
    }
    if (!IsEnterpriseTestEnabled()) {
      GTEST_SKIP() << "Skipping Enterprise storage class integration tests; "
                      "set "
                   << kEnableEnvVar
                   << "=true on an allowlisted project to enable.";
    }
    project_id_ = GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_THAT(project_id_, Not(IsEmpty()))
        << "GOOGLE_CLOUD_PROJECT must be set to an Enterprise-allowlisted "
           "project.";
  }

  std::string const& project_id() const { return project_id_; }

 private:
  std::string project_id_;
};

TEST_F(EnterpriseStorageClassIntegrationTest,
       StandardEnterpriseBucketAndObjectOperations) {
  Client client = MakeBucketIntegrationTestClient();
  std::string const bucket_name = MakeRandomBucketName();

  BucketMetadata const request_metadata =
      BucketMetadata{}
          .set_location(kRegionalLocation)
          .set_storage_class(storage_class::StandardEnterprise())
          .set_soft_delete_policy(std::chrono::hours(7 * 24));
  StatusOr<BucketMetadata> created = client.CreateBucketForProject(
      bucket_name, project_id(), request_metadata, Projection::Full());
  ASSERT_THAT(created, IsOk());
  ScheduleForDelete(*created);
  EXPECT_THAT(created->storage_class(),
              Eq(storage_class::StandardEnterprise()));

  StatusOr<BucketMetadata> fetched =
      client.GetBucketMetadata(bucket_name, Projection::Full());
  ASSERT_THAT(fetched, IsOk());
  EXPECT_THAT(fetched->storage_class(),
              Eq(storage_class::StandardEnterprise()));

  // Patch bucket default storage class:
  // STANDARD_ENTERPRISE -> LR_STANDARD_ENTERPRISE -> STANDARD_ENTERPRISE.
  StatusOr<BucketMetadata> patched_lr =
      client.PatchBucket(bucket_name,
                         BucketMetadataPatchBuilder{}.SetStorageClass(
                             storage_class::LrStandardEnterprise()),
                         IfMetagenerationMatch(fetched->metageneration()));
  ASSERT_THAT(patched_lr, IsOk());
  EXPECT_THAT(patched_lr->storage_class(),
              Eq(storage_class::LrStandardEnterprise()));

  StatusOr<BucketMetadata> patched_se =
      client.PatchBucket(bucket_name,
                         BucketMetadataPatchBuilder{}.SetStorageClass(
                             storage_class::StandardEnterprise()),
                         IfMetagenerationMatch(patched_lr->metageneration()));
  ASSERT_THAT(patched_se, IsOk());
  EXPECT_THAT(patched_se->storage_class(),
              Eq(storage_class::StandardEnterprise()));

  // Simple upload inheriting bucket default storage class.
  std::string const inherited_name = MakeRandomObjectName();
  StatusOr<ObjectMetadata> inherited = client.InsertObject(
      bucket_name, inherited_name, std::string(kPayload), IfGenerationMatch(0),
      WithObjectMetadata(ObjectMetadata{}.set_content_type("text/plain")));
  ASSERT_THAT(inherited, IsOk());
  EXPECT_THAT(inherited->storage_class(),
              Eq(storage_class::StandardEnterprise()));

  {
    ObjectReadStream reader = client.ReadObject(
        bucket_name, inherited_name, Generation(inherited->generation()));
    std::string const contents(std::istreambuf_iterator<char>(reader), {});
    EXPECT_THAT(reader.status(), IsOk());
    EXPECT_THAT(contents, Eq(kPayload));
  }

  // Upload with explicit per-object storage class override.
  std::string const explicit_lr_name = MakeRandomObjectName();
  StatusOr<ObjectMetadata> explicit_lr = client.InsertObject(
      bucket_name, explicit_lr_name, std::string(kPayload),
      IfGenerationMatch(0),
      WithObjectMetadata(
          ObjectMetadata{}
              .set_content_type("text/plain")
              .set_storage_class(storage_class::LrStandardEnterprise())));
  ASSERT_THAT(explicit_lr, IsOk());
  EXPECT_THAT(explicit_lr->storage_class(),
              Eq(storage_class::LrStandardEnterprise()));

  StatusOr<ObjectMetadata> explicit_lr_meta = client.GetObjectMetadata(
      bucket_name, explicit_lr_name, Generation(explicit_lr->generation()));
  ASSERT_THAT(explicit_lr_meta, IsOk());
  EXPECT_THAT(explicit_lr_meta->storage_class(),
              Eq(storage_class::LrStandardEnterprise()));

  // Resumable upload with explicit STANDARD_ENTERPRISE storage class.
  std::string const resumable_name = MakeRandomObjectName();
  ObjectWriteStream writer = client.WriteObject(
      bucket_name, resumable_name, IfGenerationMatch(0),
      WithObjectMetadata(
          ObjectMetadata{}
              .set_content_type("text/plain")
              .set_storage_class(storage_class::StandardEnterprise())));
  writer.write(kPayload.data(), static_cast<std::streamsize>(kPayload.size()));
  writer.Close();
  StatusOr<ObjectMetadata> resumable_meta = writer.metadata();
  ASSERT_THAT(resumable_meta, IsOk());
  EXPECT_THAT(resumable_meta->storage_class(),
              Eq(storage_class::StandardEnterprise()));

  {
    ObjectReadStream reader = client.ReadObject(
        bucket_name, resumable_name, Generation(resumable_meta->generation()));
    std::string const contents(std::istreambuf_iterator<char>(reader), {});
    EXPECT_THAT(reader.status(), IsOk());
    EXPECT_THAT(contents, Eq(kPayload));
  }

  // In-place rewrite: STANDARD_ENTERPRISE -> LR_STANDARD_ENTERPRISE ->
  // STANDARD -> STANDARD_ENTERPRISE.
  StatusOr<ObjectMetadata> rewritten_lr = client.RewriteObjectBlocking(
      bucket_name, inherited_name, bucket_name, inherited_name,
      IfGenerationMatch(inherited->generation()),
      WithObjectMetadata(ObjectMetadata{}.set_storage_class(
          storage_class::LrStandardEnterprise())));
  ASSERT_THAT(rewritten_lr, IsOk());
  EXPECT_THAT(rewritten_lr->storage_class(),
              Eq(storage_class::LrStandardEnterprise()));

  StatusOr<ObjectMetadata> rewritten_standard = client.RewriteObjectBlocking(
      bucket_name, inherited_name, bucket_name, inherited_name,
      IfGenerationMatch(rewritten_lr->generation()),
      WithObjectMetadata(
          ObjectMetadata{}.set_storage_class(storage_class::Standard())));
  ASSERT_THAT(rewritten_standard, IsOk());
  EXPECT_THAT(rewritten_standard->storage_class(),
              Eq(storage_class::Standard()));

  StatusOr<ObjectMetadata> rewritten_se = client.RewriteObjectBlocking(
      bucket_name, inherited_name, bucket_name, inherited_name,
      IfGenerationMatch(rewritten_standard->generation()),
      WithObjectMetadata(ObjectMetadata{}.set_storage_class(
          storage_class::StandardEnterprise())));
  ASSERT_THAT(rewritten_se, IsOk());
  EXPECT_THAT(rewritten_se->storage_class(),
              Eq(storage_class::StandardEnterprise()));

  // Compose two objects into a STANDARD_ENTERPRISE destination object.
  std::string const composed_name = MakeRandomObjectName();
  std::vector<ComposeSourceObject> const sources = {
      {inherited_name, rewritten_se->generation(), {}},
      {resumable_name, resumable_meta->generation(), {}},
  };
  StatusOr<ObjectMetadata> composed = client.ComposeObject(
      bucket_name, sources, composed_name, IfGenerationMatch(0),
      WithObjectMetadata(
          ObjectMetadata{}
              .set_content_type("text/plain")
              .set_storage_class(storage_class::StandardEnterprise())));
  ASSERT_THAT(composed, IsOk());
  EXPECT_THAT(composed->storage_class(),
              Eq(storage_class::StandardEnterprise()));
  EXPECT_THAT(composed->size(), Eq(kPayload.size() * 2));

  // Soft-delete and restore object, verifying Enterprise storage class is
  // preserved.
  Status const deleted = client.DeleteObject(
      bucket_name, explicit_lr_name, Generation(explicit_lr->generation()));
  ASSERT_THAT(deleted, IsOk());

  StatusOr<ObjectMetadata> restored = client.RestoreObject(
      bucket_name, explicit_lr_name, explicit_lr->generation());
  ASSERT_THAT(restored, IsOk());
  EXPECT_THAT(restored->storage_class(),
              Eq(storage_class::LrStandardEnterprise()));

  {
    ObjectReadStream reader = client.ReadObject(
        bucket_name, explicit_lr_name, Generation(restored->generation()));
    std::string const contents(std::istreambuf_iterator<char>(reader), {});
    EXPECT_THAT(reader.status(), IsOk());
    EXPECT_THAT(contents, Eq(kPayload));
  }
}

TEST_F(EnterpriseStorageClassIntegrationTest,
       LrStandardEnterpriseBucketAndObjectOperations) {
  Client client = MakeBucketIntegrationTestClient();
  std::string const bucket_name = MakeRandomBucketName();

  BucketMetadata const request_metadata =
      BucketMetadata{}
          .set_location(kRegionalLocation)
          .set_storage_class(storage_class::LrStandardEnterprise());
  StatusOr<BucketMetadata> created = client.CreateBucketForProject(
      bucket_name, project_id(), request_metadata, Projection::Full());
  ASSERT_THAT(created, IsOk());
  ScheduleForDelete(*created);
  EXPECT_THAT(created->storage_class(),
              Eq(storage_class::LrStandardEnterprise()));

  std::string const object_name = MakeRandomObjectName();
  StatusOr<ObjectMetadata> uploaded = client.InsertObject(
      bucket_name, object_name, std::string(kPayload), IfGenerationMatch(0));
  ASSERT_THAT(uploaded, IsOk());
  EXPECT_THAT(uploaded->storage_class(),
              Eq(storage_class::LrStandardEnterprise()));

  StatusOr<ObjectMetadata> fetched = client.GetObjectMetadata(
      bucket_name, object_name, Generation(uploaded->generation()));
  ASSERT_THAT(fetched, IsOk());
  EXPECT_THAT(fetched->storage_class(),
              Eq(storage_class::LrStandardEnterprise()));

  ObjectReadStream reader = client.ReadObject(
      bucket_name, object_name, Generation(uploaded->generation()));
  std::string const contents(std::istreambuf_iterator<char>(reader), {});
  EXPECT_THAT(reader.status(), IsOk());
  EXPECT_THAT(contents, Eq(kPayload));
}

TEST_F(EnterpriseStorageClassIntegrationTest,
       StandardFlexbackupEnterpriseBucketAndObjectOperations) {
  Client client = MakeBucketIntegrationTestClient();
  std::string const bucket_name = MakeRandomBucketName();

  BucketMetadata const request_metadata =
      BucketMetadata{}
          .set_location(kMultiRegionalLocation)
          .set_storage_class(storage_class::StandardFlexbackupEnterprise());
  StatusOr<BucketMetadata> created = client.CreateBucketForProject(
      bucket_name, project_id(), request_metadata, Projection::Full());
  ASSERT_THAT(created, IsOk());
  ScheduleForDelete(*created);
  EXPECT_THAT(created->storage_class(),
              Eq(storage_class::StandardFlexbackupEnterprise()));

  std::string const object_name = MakeRandomObjectName();
  StatusOr<ObjectMetadata> uploaded = client.InsertObject(
      bucket_name, object_name, std::string(kPayload), IfGenerationMatch(0));
  ASSERT_THAT(uploaded, IsOk());
  EXPECT_THAT(uploaded->storage_class(),
              Eq(storage_class::StandardFlexbackupEnterprise()));

  StatusOr<ObjectMetadata> fetched = client.GetObjectMetadata(
      bucket_name, object_name, Generation(uploaded->generation()));
  ASSERT_THAT(fetched, IsOk());
  EXPECT_THAT(fetched->storage_class(),
              Eq(storage_class::StandardFlexbackupEnterprise()));

  ObjectReadStream reader = client.ReadObject(
      bucket_name, object_name, Generation(uploaded->generation()));
  std::string const contents(std::istreambuf_iterator<char>(reader), {});
  EXPECT_THAT(reader.status(), IsOk());
  EXPECT_THAT(contents, Eq(kPayload));
}

TEST_F(EnterpriseStorageClassIntegrationTest, HnsBucketWithStandardEnterprise) {
  Client client = MakeBucketIntegrationTestClient();
  std::string const bucket_name = MakeRandomBucketName();

  BucketIamConfiguration iam_config;
  iam_config.uniform_bucket_level_access = UniformBucketLevelAccess{true, {}};
  BucketMetadata const request_metadata =
      BucketMetadata{}
          .set_location(kRegionalLocation)
          .set_storage_class(storage_class::StandardEnterprise())
          .set_hierarchical_namespace(BucketHierarchicalNamespace{true})
          .set_iam_configuration(std::move(iam_config));

  StatusOr<BucketMetadata> created = client.CreateBucketForProject(
      bucket_name, project_id(), request_metadata, Projection::Full());
  ASSERT_THAT(created, IsOk());
  ScheduleForDelete(*created);
  EXPECT_THAT(created->storage_class(),
              Eq(storage_class::StandardEnterprise()));
  ASSERT_TRUE(created->has_hierarchical_namespace());
  EXPECT_TRUE(created->hierarchical_namespace().enabled);

  std::string const src_name = MakeRandomObjectName();
  StatusOr<ObjectMetadata> uploaded = client.InsertObject(
      bucket_name, src_name, std::string(kPayload), IfGenerationMatch(0));
  ASSERT_THAT(uploaded, IsOk());
  EXPECT_THAT(uploaded->storage_class(),
              Eq(storage_class::StandardEnterprise()));

  // Move object within the HNS bucket and verify storage class is preserved.
  std::string const dst_name = MakeRandomObjectName();
  StatusOr<ObjectMetadata> moved = client.MoveObject(
      bucket_name, src_name, dst_name,
      IfSourceGenerationMatch(uploaded->generation()), IfGenerationMatch(0));
  ASSERT_THAT(moved, IsOk());
  EXPECT_THAT(moved->storage_class(), Eq(storage_class::StandardEnterprise()));

  {
    ObjectReadStream reader = client.ReadObject(
        bucket_name, dst_name, Generation(moved->generation()));
    std::string const contents(std::istreambuf_iterator<char>(reader), {});
    EXPECT_THAT(reader.status(), IsOk());
    EXPECT_THAT(contents, Eq(kPayload));
  }

#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
  if (UsingGrpc()) {
    AsyncClient async(MakeAsyncTestOptions());
    google::storage::v2::BidiReadObjectSpec spec;
    spec.set_bucket(BucketName(bucket_name).FullName());
    spec.set_object(dst_name);
    spec.set_generation(moved->generation());

    StatusOr<ObjectDescriptor> descriptor = async.Open(std::move(spec)).get();
    ASSERT_THAT(descriptor, IsOk());
    ASSERT_TRUE(descriptor->metadata().has_value());
    EXPECT_THAT(descriptor->metadata()->storage_class(),
                Eq(storage_class::StandardEnterprise()));

    AsyncReader reader;
    AsyncToken token;
    std::tie(reader, token) =
        descriptor->Read(0, static_cast<std::int64_t>(kPayload.size()));
    std::string bidi_contents;
    while (token.valid()) {
      StatusOr<std::pair<ReadPayload, AsyncToken>> read =
          reader.Read(std::move(token)).get();
      ASSERT_THAT(read, IsOk());
      ReadPayload chunk;
      std::tie(chunk, token) = *std::move(read);
      bidi_contents += CollectPayload(chunk);
    }
    EXPECT_THAT(bidi_contents, Eq(kPayload));
  }
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
}

TEST_F(EnterpriseStorageClassIntegrationTest,
       LifecycleRulesWithEnterpriseStorageClasses) {
  Client client = MakeBucketIntegrationTestClient();
  std::string const bucket_name = MakeRandomBucketName();

  std::vector<LifecycleRule> const rules = {
      LifecycleRule(LifecycleRule::ConditionConjunction(
                        LifecycleRule::MaxAge(10),
                        LifecycleRule::MatchesStorageClassStandard()),
                    LifecycleRule::SetStorageClassStandardEnterprise()),
      LifecycleRule(LifecycleRule::ConditionConjunction(
                        LifecycleRule::MaxAge(20),
                        LifecycleRule::MatchesStorageClasses(
                            {storage_class::Standard(), "LR_STANDARD",
                             storage_class::StandardEnterprise()})),
                    LifecycleRule::SetStorageClassLrStandardEnterprise()),
      LifecycleRule(LifecycleRule::ConditionConjunction(
                        LifecycleRule::MaxAge(30),
                        LifecycleRule::MatchesStorageClasses(
                            {storage_class::StandardEnterprise(),
                             storage_class::LrStandardEnterprise()})),
                    LifecycleRule::Delete()),
  };

  BucketMetadata const request_metadata =
      BucketMetadata{}
          .set_location(kRegionalLocation)
          .set_storage_class(storage_class::StandardEnterprise())
          .set_lifecycle(BucketLifecycle{rules});
  StatusOr<BucketMetadata> created = client.CreateBucketForProject(
      bucket_name, project_id(), request_metadata, Projection::Full());
  ASSERT_THAT(created, IsOk());
  ScheduleForDelete(*created);

  ASSERT_TRUE(created->has_lifecycle());
  std::vector<LifecycleRule> const& remote_rules = created->lifecycle().rule;
  ASSERT_THAT(remote_rules, SizeIs(3));

  EXPECT_THAT(remote_rules[0].action(),
              Eq(LifecycleRule::SetStorageClassStandardEnterprise()));
  EXPECT_THAT(remote_rules[0].condition().matches_storage_class,
              Optional(ElementsAre(storage_class::Standard())));

  EXPECT_THAT(remote_rules[1].action(),
              Eq(LifecycleRule::SetStorageClassLrStandardEnterprise()));
  EXPECT_THAT(remote_rules[1].condition().matches_storage_class,
              Optional(ElementsAre(storage_class::Standard(), "LR_STANDARD",
                                   storage_class::StandardEnterprise())));

  EXPECT_THAT(remote_rules[2].action(), Eq(LifecycleRule::Delete()));
  EXPECT_THAT(remote_rules[2].condition().matches_storage_class,
              Optional(ElementsAre(storage_class::StandardEnterprise(),
                                   storage_class::LrStandardEnterprise())));
}

TEST_F(EnterpriseStorageClassIntegrationTest,
       AutoclassWithEnterpriseStorageClassRejected) {
  Client client = MakeBucketIntegrationTestClient();
  std::string const bucket_name = MakeRandomBucketName();

  BucketMetadata const request_metadata =
      BucketMetadata{}
          .set_location(kRegionalLocation)
          .set_storage_class(storage_class::StandardEnterprise())
          .set_autoclass(BucketAutoclass{true});
  StatusOr<BucketMetadata> created = client.CreateBucketForProject(
      bucket_name, project_id(), request_metadata, Projection::Full());
  if (created) ScheduleForDelete(*created);
  EXPECT_THAT(created, StatusIs(StatusCode::kInvalidArgument));
}

#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
TEST_F(EnterpriseStorageClassIntegrationTest,
       AsyncClientEnterpriseStorageClasses) {
  if (!UsingGrpc()) GTEST_SKIP();

  Client control_client = MakeBucketIntegrationTestClient();
  std::string const bucket_name = MakeRandomBucketName();

  BucketMetadata const request_metadata =
      BucketMetadata{}
          .set_location(kRegionalLocation)
          .set_storage_class(storage_class::StandardEnterprise());
  StatusOr<BucketMetadata> created = control_client.CreateBucketForProject(
      bucket_name, project_id(), request_metadata, Projection::Full());
  ASSERT_THAT(created, IsOk());
  ScheduleForDelete(*created);

  AsyncClient async(MakeAsyncTestOptions());
  BucketName const bucket_resource(bucket_name);

  StatusOr<google::storage::v2::Bucket> fetched_bucket =
      async.GetBucket(bucket_resource).get();
  ASSERT_THAT(fetched_bucket, IsOk());
  EXPECT_THAT(fetched_bucket->storage_class(),
              Eq(storage_class::StandardEnterprise()));

  // Inherited default storage class upload via AsyncClient::InsertObject.
  std::string const inherited_name = MakeRandomObjectName();
  StatusOr<google::storage::v2::Object> inherited =
      async.InsertObject(bucket_resource, inherited_name, std::string(kPayload))
          .get();
  ASSERT_THAT(inherited, IsOk());
  EXPECT_THAT(inherited->storage_class(),
              Eq(storage_class::StandardEnterprise()));

  StatusOr<ReadPayload> read_inherited =
      async
          .ReadObjectRange(bucket_resource, inherited_name, 0,
                           static_cast<std::int64_t>(kPayload.size()))
          .get();
  ASSERT_THAT(read_inherited, IsOk());
  EXPECT_THAT(CollectPayload(*read_inherited), Eq(kPayload));
  ASSERT_TRUE(read_inherited->metadata().has_value());
  EXPECT_THAT(read_inherited->metadata()->storage_class(),
              Eq(storage_class::StandardEnterprise()));

  // Explicit per-object storage class override via StartBufferedUpload.
  std::string const buffered_name = MakeRandomObjectName();
  google::storage::v2::StartResumableWriteRequest start_req;
  google::storage::v2::Object& resource =
      *start_req.mutable_write_object_spec()->mutable_resource();
  resource.set_bucket(bucket_resource.FullName());
  resource.set_name(buffered_name);
  resource.set_storage_class(storage_class::LrStandardEnterprise());
  start_req.mutable_write_object_spec()->set_if_generation_match(0);

  StatusOr<std::pair<AsyncWriter, AsyncToken>> started =
      async.StartBufferedUpload(std::move(start_req)).get();
  ASSERT_THAT(started, IsOk());
  AsyncWriter writer;
  AsyncToken token;
  std::tie(writer, token) = *std::move(started);

  StatusOr<AsyncToken> written =
      writer.Write(std::move(token), WritePayload(std::string(kPayload))).get();
  ASSERT_THAT(written, IsOk());
  StatusOr<google::storage::v2::Object> finalized =
      writer.Finalize(*std::move(written)).get();
  ASSERT_THAT(finalized, IsOk());
  EXPECT_THAT(finalized->storage_class(),
              Eq(storage_class::LrStandardEnterprise()));

  // In-place rewrite via AsyncClient::StartRewrite.
  google::storage::v2::RewriteObjectRequest rewrite_req;
  rewrite_req.set_source_bucket(bucket_resource.FullName());
  rewrite_req.set_source_object(inherited_name);
  rewrite_req.set_destination_bucket(bucket_resource.FullName());
  rewrite_req.set_destination_name(inherited_name);
  rewrite_req.set_if_generation_match(inherited->generation());
  rewrite_req.mutable_destination()->set_storage_class(
      storage_class::LrStandardEnterprise());

  AsyncRewriter rewriter;
  AsyncToken rewrite_token;
  std::tie(rewriter, rewrite_token) =
      async.StartRewrite(std::move(rewrite_req));
  google::storage::v2::Object rewritten;
  while (rewrite_token.valid()) {
    StatusOr<std::pair<google::storage::v2::RewriteResponse, AsyncToken>> step =
        rewriter.Iterate(std::move(rewrite_token)).get();
    ASSERT_THAT(step, IsOk());
    google::storage::v2::RewriteResponse response;
    std::tie(response, rewrite_token) = *std::move(step);
    if (!response.has_resource()) continue;
    rewritten = std::move(*response.mutable_resource());
  }
  EXPECT_THAT(rewritten.storage_class(),
              Eq(storage_class::LrStandardEnterprise()));
}
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
