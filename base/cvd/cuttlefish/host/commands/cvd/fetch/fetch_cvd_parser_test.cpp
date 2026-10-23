//
// Copyright (C) 2024 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cuttlefish/host/commands/cvd/fetch/fetch_cvd_parser.h"

#include <optional>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "cuttlefish/host/commands/cvd/fetch/build_strings.h"
#include "cuttlefish/host/commands/cvd/fetch/vector_flags.h"
#include "cuttlefish/host/libs/web/android_build_string.h"
#include "cuttlefish/result/assert.h"
#include "cuttlefish/result/result.h"
#include "cuttlefish/result/result_matchers.h"

namespace cuttlefish {

using testing::ElementsAre;
using testing::Eq;

inline constexpr char kTargetDirectory[] = "--target_directory=/tmp/fetch_test";
inline constexpr char kDefaultBuild[] =
    "--default_build=aosp-android-latest-release/"
    "aosp_cf_x86_64_only_phone-userdebug";
inline constexpr char kCasDownloaderPath[] =
    "--cas_downloader_path=/tmp/casdownloader";
inline constexpr char kCasCacheDir[] = "--cas_cache_dir=/tmp/cas_cache";
inline constexpr char kCasCacheMaxSize[] = "--cas_cache_max_size=10000000000";

TEST(FetchCvdParserTests, CreatesCasDownloaderFlags) {
  std::string target_directory = std::string(kTargetDirectory);
  std::string default_build = std::string(kDefaultBuild);
  std::string cas_downloader_path = std::string(kCasDownloaderPath);
  std::string cas_cache_dir = std::string(kCasCacheDir);
  std::string cas_cache_max_size = std::string(kCasCacheMaxSize);
  std::vector<std::string> args = {target_directory, default_build,
                                   cas_downloader_path, cas_cache_dir,
                                   cas_cache_max_size};

  Result<FetchFlags> flagsRes = FetchFlags::Parse(args);

  EXPECT_THAT(flagsRes, IsOk());
  FetchFlags flags = flagsRes.value();
  EXPECT_THAT(
      flags.build_api_flags.cas_downloader_flags.downloader_path.value(),
      Eq("/tmp/casdownloader"));
  EXPECT_THAT(flags.build_api_flags.cas_downloader_flags.cache_dir.value(),
              Eq("/tmp/cas_cache"));
  EXPECT_THAT(flags.build_api_flags.cas_downloader_flags.cache_max_size.value(),
              Eq(10000000000));
}

TEST(FetchCvdParserTests, ParsesVendorBootImageBuildFlag) {
  std::vector<std::string> args = {
      std::string(kTargetDirectory),
      std::string(kDefaultBuild),
      "--vendor_boot_image_build=git_main/"
      "cf_x86_64_auto{vendor_boot-debug.img}",
  };

  FetchFlags flags = CF_ASSERT(FetchFlags::Parse(args));
  ASSERT_EQ(flags.vector_flags.vendor_boot_image_build.size(), 1);
  ASSERT_TRUE(flags.vector_flags.vendor_boot_image_build[0].has_value());
  EXPECT_THAT(GetFilepath(*flags.vector_flags.vendor_boot_image_build[0]),
              Eq(std::optional<std::string>("vendor_boot-debug.img")));
}

TEST(FetchCvdParserTests, ParsesSingleArtifactBuildDefaultTargetFilename) {
  std::vector<std::string> args = {
      std::string(kTargetDirectory),
      std::string(kDefaultBuild),
      "--artifact_build=git_main/cf_x86_64_phone-userdebug{opendice.aml}",
  };

  const FetchFlags flags = CF_ASSERT(FetchFlags::Parse(args));
  EXPECT_THAT(flags.vector_flags.NumberOfBuilds(), IsOkAndValue(1));
  ASSERT_EQ(flags.vector_flags.artifact_build.size(), 1);
  const ArtifactBuildString expected = {
      .build_string =
          DeviceBuildString{
              .branch_or_id = "git_main",
              .target = "cf_x86_64_phone-userdebug",
              .filepath = "opendice.aml",
          },
      .target_filepath = "opendice.aml",
  };
  EXPECT_THAT(flags.vector_flags.artifact_build[0], ElementsAre(expected));

  const BuildStrings build_strings =
      BuildStrings::Create(flags.vector_flags, 0);
  EXPECT_THAT(build_strings.artifact_builds, ElementsAre(expected));
}

TEST(FetchCvdParserTests, ParsesSingleArtifactBuildCustomTargetFilename) {
  std::vector<std::string> args = {
      std::string(kTargetDirectory),
      std::string(kDefaultBuild),
      "--artifact_build=git_main/"
      "cf_x86_64_phone-userdebug{file.bin}:subdir/custom.bin",
  };

  const FetchFlags flags = CF_ASSERT(FetchFlags::Parse(args));
  EXPECT_THAT(flags.vector_flags.NumberOfBuilds(), IsOkAndValue(1));
  ASSERT_EQ(flags.vector_flags.artifact_build.size(), 1);
  const ArtifactBuildString expected = {
      .build_string =
          DeviceBuildString{
              .branch_or_id = "git_main",
              .target = "cf_x86_64_phone-userdebug",
              .filepath = "file.bin",
          },
      .target_filepath = "subdir/custom.bin",
  };
  EXPECT_THAT(flags.vector_flags.artifact_build[0], ElementsAre(expected));
}

TEST(FetchCvdParserTests,
     ParsesSingleArtifactBuildCustomTargetFilenameInsideBraces) {
  std::vector<std::string> args = {
      std::string(kTargetDirectory),
      std::string(kDefaultBuild),
      "--artifact_build=git_main/"
      "cf_x86_64_phone-userdebug{file.bin:subdir/custom.bin}",
  };

  const FetchFlags flags = CF_ASSERT(FetchFlags::Parse(args));
  EXPECT_THAT(flags.vector_flags.NumberOfBuilds(), IsOkAndValue(1));
  ASSERT_EQ(flags.vector_flags.artifact_build.size(), 1);
  const ArtifactBuildString expected = {
      .build_string =
          DeviceBuildString{
              .branch_or_id = "git_main",
              .target = "cf_x86_64_phone-userdebug",
              .filepath = "file.bin",
          },
      .target_filepath = "subdir/custom.bin",
  };
  EXPECT_THAT(flags.vector_flags.artifact_build[0], ElementsAre(expected));
}

TEST(FetchCvdParserTests, ParsesMultipleArtifactsViaSemicolonAndRepeatedFlags) {
  std::vector<std::string> args = {
      std::string(kTargetDirectory),
      std::string(kDefaultBuild),
      "--artifact_build=b/t{f1.bin}:o1.bin;b/t{f2.bin:o2.bin}",
      "--artifact_build=b2/t2{f3.bin}",
  };

  const FetchFlags flags = CF_ASSERT(FetchFlags::Parse(args));
  EXPECT_THAT(flags.vector_flags.NumberOfBuilds(), IsOkAndValue(1));
  ASSERT_EQ(flags.vector_flags.artifact_build.size(), 1);
  const ArtifactBuildString expected1 = {
      .build_string =
          DeviceBuildString{
              .branch_or_id = "b", .target = "t", .filepath = "f1.bin"},
      .target_filepath = "o1.bin",
  };
  const ArtifactBuildString expected2 = {
      .build_string =
          DeviceBuildString{
              .branch_or_id = "b", .target = "t", .filepath = "f2.bin"},
      .target_filepath = "o2.bin",
  };
  const ArtifactBuildString expected3 = {
      .build_string =
          DeviceBuildString{
              .branch_or_id = "b2", .target = "t2", .filepath = "f3.bin"},
      .target_filepath = "f3.bin",
  };
  EXPECT_THAT(flags.vector_flags.artifact_build[0],
              ElementsAre(expected1, expected2, expected3));
}

TEST(FetchCvdParserTests, ParsesMultiInstanceArtifactBuilds) {
  std::vector<std::string> args = {
      std::string(kTargetDirectory),
      "--default_build=b0/t0,b1/t1",
      "--artifact_build=b0/t0{a1.bin}:out1.bin;b0/t0{a2.bin},b1/t1{b1.bin}",
      "--artifact_build=,b1/t1{b2.bin}:out2.bin",
  };

  const FetchFlags flags = CF_ASSERT(FetchFlags::Parse(args));
  EXPECT_THAT(flags.vector_flags.NumberOfBuilds(), IsOkAndValue(2));
  ASSERT_EQ(flags.vector_flags.artifact_build.size(), 2);

  const BuildStrings build_strings0 =
      BuildStrings::Create(flags.vector_flags, 0);
  const BuildStrings build_strings1 =
      BuildStrings::Create(flags.vector_flags, 1);

  EXPECT_THAT(build_strings0.artifact_builds,
              ElementsAre(
                  ArtifactBuildString{
                      .build_string = DeviceBuildString{.branch_or_id = "b0",
                                                        .target = "t0",
                                                        .filepath = "a1.bin"},
                      .target_filepath = "out1.bin",
                  },
                  ArtifactBuildString{
                      .build_string = DeviceBuildString{.branch_or_id = "b0",
                                                        .target = "t0",
                                                        .filepath = "a2.bin"},
                      .target_filepath = "a2.bin",
                  }));

  EXPECT_THAT(build_strings1.artifact_builds,
              ElementsAre(
                  ArtifactBuildString{
                      .build_string = DeviceBuildString{.branch_or_id = "b1",
                                                        .target = "t1",
                                                        .filepath = "b1.bin"},
                      .target_filepath = "b1.bin",
                  },
                  ArtifactBuildString{
                      .build_string = DeviceBuildString{.branch_or_id = "b1",
                                                        .target = "t1",
                                                        .filepath = "b2.bin"},
                      .target_filepath = "out2.bin",
                  }));
}

TEST(FetchCvdParserTests, RejectsArtifactBuildMissingFilepath) {
  std::vector<std::string> args = {
      std::string(kTargetDirectory),
      std::string(kDefaultBuild),
      "--artifact_build=git_main/cf_x86_64_phone-userdebug",
  };
  EXPECT_THAT(FetchFlags::Parse(args), IsError());
}

TEST(FetchCvdParserTests, RejectsArtifactBuildAbsoluteTargetFilepath) {
  std::vector<std::string> args = {
      std::string(kTargetDirectory),
      std::string(kDefaultBuild),
      "--artifact_build=git_main/cf_x86_64_phone-userdebug{file.bin}:/abs/"
      "path.bin",
  };
  EXPECT_THAT(FetchFlags::Parse(args), IsError());
}

TEST(FetchCvdParserTests, RejectsArtifactBuildParentTraversalTargetFilepath) {
  std::vector<std::string> args = {
      std::string(kTargetDirectory),
      std::string(kDefaultBuild),
      "--artifact_build=git_main/cf_x86_64_phone-userdebug{file.bin}:../"
      "escape.bin",
  };
  EXPECT_THAT(FetchFlags::Parse(args), IsError());
}

TEST(FetchCvdParserTests, RejectsMismatchedRepeatedArtifactBuildLengths) {
  std::vector<std::string> args = {
      std::string(kTargetDirectory),
      "--default_build=b0/t0,b1/t1",
      "--artifact_build=b0/t0{a1.bin},b1/t1{b1.bin}",
      "--artifact_build=b0/t0{a2.bin}",
  };
  EXPECT_THAT(FetchFlags::Parse(args), IsError());
}

}  // namespace cuttlefish
