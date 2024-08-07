// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Generated by the Codegen C++ plugin.
// If you make any local changes, they will be lost.
// source: google/cloud/automl/v1/service.proto

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_AUTOML_V1_MOCKS_MOCK_AUTO_ML_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_AUTOML_V1_MOCKS_MOCK_AUTO_ML_CONNECTION_H

#include "google/cloud/automl/v1/auto_ml_connection.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace automl_v1_mocks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A class to mock `AutoMlConnection`.
 *
 * Application developers may want to test their code with simulated responses,
 * including errors, from an object of type `AutoMlClient`. To do so,
 * construct an object of type `AutoMlClient` with an instance of this
 * class. Then use the Google Test framework functions to program the behavior
 * of this mock.
 *
 * @see [This example][bq-mock] for how to test your application with GoogleTest.
 * While the example showcases types from the BigQuery library, the underlying
 * principles apply for any pair of `*Client` and `*Connection`.
 *
 * [bq-mock]: @cloud_cpp_docs_link{bigquery,bigquery-read-mock}
 */
class MockAutoMlConnection : public automl_v1::AutoMlConnection {
 public:
  MOCK_METHOD(Options, options, (), (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// CreateDataset(Matcher<google::cloud::automl::v1::CreateDatasetRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::automl::v1::Dataset>>,
              CreateDataset,
              (google::cloud::automl::v1::CreateDatasetRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, CreateDataset(_, _))
  /// @endcode
  MOCK_METHOD(StatusOr<google::longrunning::Operation>, CreateDataset,
              (NoAwaitTag,
               google::cloud::automl::v1::CreateDatasetRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock, CreateDataset(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::automl::v1::Dataset>>,
              CreateDataset, (google::longrunning::Operation const& operation),
              (override));

  MOCK_METHOD(StatusOr<google::cloud::automl::v1::Dataset>, GetDataset,
              (google::cloud::automl::v1::GetDatasetRequest const& request),
              (override));

  MOCK_METHOD((StreamRange<google::cloud::automl::v1::Dataset>), ListDatasets,
              (google::cloud::automl::v1::ListDatasetsRequest request),
              (override));

  MOCK_METHOD(StatusOr<google::cloud::automl::v1::Dataset>, UpdateDataset,
              (google::cloud::automl::v1::UpdateDatasetRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// DeleteDataset(Matcher<google::cloud::automl::v1::DeleteDatasetRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::automl::v1::OperationMetadata>>,
              DeleteDataset,
              (google::cloud::automl::v1::DeleteDatasetRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, DeleteDataset(_, _))
  /// @endcode
  MOCK_METHOD(StatusOr<google::longrunning::Operation>, DeleteDataset,
              (NoAwaitTag,
               google::cloud::automl::v1::DeleteDatasetRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock, DeleteDataset(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::automl::v1::OperationMetadata>>,
              DeleteDataset, (google::longrunning::Operation const& operation),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// ImportData(Matcher<google::cloud::automl::v1::ImportDataRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::automl::v1::OperationMetadata>>,
              ImportData,
              (google::cloud::automl::v1::ImportDataRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, ImportData(_, _))
  /// @endcode
  MOCK_METHOD(StatusOr<google::longrunning::Operation>, ImportData,
              (NoAwaitTag,
               google::cloud::automl::v1::ImportDataRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock, ImportData(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::automl::v1::OperationMetadata>>,
              ImportData, (google::longrunning::Operation const& operation),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// ExportData(Matcher<google::cloud::automl::v1::ExportDataRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::automl::v1::OperationMetadata>>,
              ExportData,
              (google::cloud::automl::v1::ExportDataRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, ExportData(_, _))
  /// @endcode
  MOCK_METHOD(StatusOr<google::longrunning::Operation>, ExportData,
              (NoAwaitTag,
               google::cloud::automl::v1::ExportDataRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock, ExportData(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::automl::v1::OperationMetadata>>,
              ExportData, (google::longrunning::Operation const& operation),
              (override));

  MOCK_METHOD(
      StatusOr<google::cloud::automl::v1::AnnotationSpec>, GetAnnotationSpec,
      (google::cloud::automl::v1::GetAnnotationSpecRequest const& request),
      (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// CreateModel(Matcher<google::cloud::automl::v1::CreateModelRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::automl::v1::Model>>, CreateModel,
              (google::cloud::automl::v1::CreateModelRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, CreateModel(_, _))
  /// @endcode
  MOCK_METHOD(StatusOr<google::longrunning::Operation>, CreateModel,
              (NoAwaitTag,
               google::cloud::automl::v1::CreateModelRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock, CreateModel(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::automl::v1::Model>>, CreateModel,
              (google::longrunning::Operation const& operation), (override));

  MOCK_METHOD(StatusOr<google::cloud::automl::v1::Model>, GetModel,
              (google::cloud::automl::v1::GetModelRequest const& request),
              (override));

  MOCK_METHOD((StreamRange<google::cloud::automl::v1::Model>), ListModels,
              (google::cloud::automl::v1::ListModelsRequest request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// DeleteModel(Matcher<google::cloud::automl::v1::DeleteModelRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::automl::v1::OperationMetadata>>,
              DeleteModel,
              (google::cloud::automl::v1::DeleteModelRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, DeleteModel(_, _))
  /// @endcode
  MOCK_METHOD(StatusOr<google::longrunning::Operation>, DeleteModel,
              (NoAwaitTag,
               google::cloud::automl::v1::DeleteModelRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock, DeleteModel(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::automl::v1::OperationMetadata>>,
              DeleteModel, (google::longrunning::Operation const& operation),
              (override));

  MOCK_METHOD(StatusOr<google::cloud::automl::v1::Model>, UpdateModel,
              (google::cloud::automl::v1::UpdateModelRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// DeployModel(Matcher<google::cloud::automl::v1::DeployModelRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::automl::v1::OperationMetadata>>,
              DeployModel,
              (google::cloud::automl::v1::DeployModelRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, DeployModel(_, _))
  /// @endcode
  MOCK_METHOD(StatusOr<google::longrunning::Operation>, DeployModel,
              (NoAwaitTag,
               google::cloud::automl::v1::DeployModelRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock, DeployModel(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::automl::v1::OperationMetadata>>,
              DeployModel, (google::longrunning::Operation const& operation),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// UndeployModel(Matcher<google::cloud::automl::v1::UndeployModelRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::automl::v1::OperationMetadata>>,
              UndeployModel,
              (google::cloud::automl::v1::UndeployModelRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, UndeployModel(_, _))
  /// @endcode
  MOCK_METHOD(StatusOr<google::longrunning::Operation>, UndeployModel,
              (NoAwaitTag,
               google::cloud::automl::v1::UndeployModelRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock, UndeployModel(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::automl::v1::OperationMetadata>>,
              UndeployModel, (google::longrunning::Operation const& operation),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock,
  /// ExportModel(Matcher<google::cloud::automl::v1::ExportModelRequest
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::automl::v1::OperationMetadata>>,
              ExportModel,
              (google::cloud::automl::v1::ExportModelRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// EXPECT_CALL(*mock, ExportModel(_, _))
  /// @endcode
  MOCK_METHOD(StatusOr<google::longrunning::Operation>, ExportModel,
              (NoAwaitTag,
               google::cloud::automl::v1::ExportModelRequest const& request),
              (override));

  /// To disambiguate calls, use:
  ///
  /// @code
  /// using ::testing::_;
  /// using ::testing::Matcher;
  /// EXPECT_CALL(*mock, ExportModel(Matcher<google::longrunning::Operation
  /// const&>(_)))
  /// @endcode
  MOCK_METHOD(future<StatusOr<google::cloud::automl::v1::OperationMetadata>>,
              ExportModel, (google::longrunning::Operation const& operation),
              (override));

  MOCK_METHOD(
      StatusOr<google::cloud::automl::v1::ModelEvaluation>, GetModelEvaluation,
      (google::cloud::automl::v1::GetModelEvaluationRequest const& request),
      (override));

  MOCK_METHOD((StreamRange<google::cloud::automl::v1::ModelEvaluation>),
              ListModelEvaluations,
              (google::cloud::automl::v1::ListModelEvaluationsRequest request),
              (override));
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace automl_v1_mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_AUTOML_V1_MOCKS_MOCK_AUTO_ML_CONNECTION_H
