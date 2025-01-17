// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/inspection_results_cache.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

ModuleInspectionResult CreateTestModuleInspectionResult() {
  ModuleInspectionResult inspection_result;

  inspection_result.location = L"location";
  inspection_result.basename = L"basename";
  inspection_result.product_name = L"product_name";
  inspection_result.description = L"description";
  inspection_result.version = L"version";
  inspection_result.certificate_info.type =
      CertificateInfo::Type::CERTIFICATE_IN_FILE;
  inspection_result.certificate_info.path =
      base::FilePath(L"certificate_info_path");
  inspection_result.certificate_info.subject = L"certificate_info_subject";

  return inspection_result;
}

bool InspectionResultsEqual(const ModuleInspectionResult& lhs,
                            const ModuleInspectionResult& rhs) {
  return std::tie(lhs.location, lhs.basename, lhs.product_name, lhs.description,
                  lhs.version, lhs.certificate_info.type,
                  lhs.certificate_info.path, lhs.certificate_info.subject) ==
         std::tie(rhs.location, rhs.basename, rhs.product_name, rhs.description,
                  rhs.version, rhs.certificate_info.type,
                  rhs.certificate_info.path, rhs.certificate_info.subject);
}

}  // namespace

class InspectionResultsCacheTest : public testing::Test {
 public:
  InspectionResultsCacheTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    scoped_feature_list_.InitAndEnableFeature(kInspectionResultsCache);
  }

  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

  base::FilePath GetCacheFilePath() {
    return scoped_temp_dir_.GetPath().Append(L"cache.bin");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  base::ScopedTempDir scoped_temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(InspectionResultsCacheTest);
};

TEST_F(InspectionResultsCacheTest, ReadMissingCache) {
  InspectionResultsCache read_inspection_results_cache;
  EXPECT_EQ(ReadCacheResult::kFailReadFile,
            ReadInspectionResultsCache(GetCacheFilePath(), 0,
                                       &read_inspection_results_cache));
}

TEST_F(InspectionResultsCacheTest, WriteAndRead) {
  ModuleInfoKey module_key(base::FilePath(L"file_path.exe"), 0x1234, 0xABCD);
  ModuleInspectionResult inspection_result = CreateTestModuleInspectionResult();

  InspectionResultsCache inspection_results_cache;
  AddInspectionResultToCache(module_key, inspection_result,
                             &inspection_results_cache);

  EXPECT_TRUE(WriteInspectionResultsCache(GetCacheFilePath(),
                                          inspection_results_cache));

  // Now check that a cache read from the file is identical to the cache that
  // was written.
  InspectionResultsCache read_inspection_results_cache;
  EXPECT_EQ(ReadCacheResult::kSuccess,
            ReadInspectionResultsCache(GetCacheFilePath(), 0,
                                       &read_inspection_results_cache));

  auto read_inspection_result =
      GetInspectionResultFromCache(module_key, &read_inspection_results_cache);
  ASSERT_TRUE(read_inspection_result);
  EXPECT_TRUE(
      InspectionResultsEqual(inspection_result, *read_inspection_result));
}

TEST_F(InspectionResultsCacheTest, WriteAndReadExpired) {
  ModuleInfoKey module_key(base::FilePath(L"file_path.exe"), 0x1234, 0xABCD);
  ModuleInspectionResult inspection_result = CreateTestModuleInspectionResult();

  InspectionResultsCache inspection_results_cache;
  AddInspectionResultToCache(module_key, inspection_result,
                             &inspection_results_cache);

  EXPECT_TRUE(WriteInspectionResultsCache(GetCacheFilePath(),
                                          inspection_results_cache));

  // Now read the cache from disk with a minimum time stamp higher than
  // base::Time::Now() and it should be empty because the only element it
  // contains is expired.
  InspectionResultsCache read_inspection_results_cache;
  EXPECT_EQ(ReadCacheResult::kSuccess,
            ReadInspectionResultsCache(
                GetCacheFilePath(), CalculateTimeStamp(base::Time::Now()) + 1,
                &read_inspection_results_cache));

  EXPECT_TRUE(read_inspection_results_cache.empty());
  auto read_inspection_result =
      GetInspectionResultFromCache(module_key, &read_inspection_results_cache);
  EXPECT_FALSE(read_inspection_result);
}
