//
// Copyright (C) 2023 The Android Open Source Project
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

#include "cuttlefish/host/commands/cvd/fetch/vector_flags.h"

#include <stddef.h>
#include <stdlib.h>

#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"

#include "cuttlefish/flag_parser/flag.h"
#include "cuttlefish/flag_parser/gflags_compat.h"
#include "cuttlefish/host/libs/web/android_build_string.h"
#include "cuttlefish/result/result.h"

namespace cuttlefish {
namespace {

Result<ArtifactBuildString> ParseArtifactBuildString(std::string_view input) {
  CF_EXPECT(!input.empty(), "Artifact build string cannot be empty");
  const size_t close_bracket = input.find('}');
  CF_EXPECT(close_bracket != std::string_view::npos,
            "Artifact build string must include '{filepath}': " << input);

  std::string_view build_string_part = input;
  std::optional<std::string> explicit_target;
  const size_t colon_after_close = input.find(':', close_bracket + 1);
  if (colon_after_close != std::string_view::npos) {
    CF_EXPECT(
        colon_after_close == close_bracket + 1 ||
            input[close_bracket + 1] == '#',
        "Unexpected characters after '}' in artifact build string: " << input);
    build_string_part = input.substr(0, colon_after_close);
    explicit_target = std::string(input.substr(colon_after_close + 1));
  } else {
    CF_EXPECT(
        close_bracket + 1 == input.size() || input[close_bracket + 1] == '#',
        "Unexpected characters after '}' in artifact build string: " << input);
  }

  BuildString build_string = CF_EXPECT(ParseBuildString(build_string_part));
  const std::optional<std::string> filepath = GetFilepath(build_string);
  CF_EXPECT(
      filepath.has_value() && !filepath->empty(),
      "Artifact build string must specify a non-empty filepath: " << input);

  std::string remote_filepath = *filepath;
  std::string target_filepath;
  const size_t colon_in_filepath = filepath->find(':');
  if (colon_in_filepath != std::string::npos) {
    CF_EXPECT(!explicit_target.has_value(),
              "Cannot specify target filepath both inside '{}' and after '}': "
                  << input);
    remote_filepath = filepath->substr(0, colon_in_filepath);
    target_filepath = filepath->substr(colon_in_filepath + 1);
    CF_EXPECT(!remote_filepath.empty(),
              "Remote filepath cannot be empty: " << input);
    SetFilepath(build_string, remote_filepath);
  } else if (explicit_target.has_value()) {
    target_filepath = *explicit_target;
  } else {
    target_filepath = remote_filepath;
  }

  CF_EXPECT(!target_filepath.empty(),
            "Target filepath cannot be empty: " << input);
  CF_EXPECT(!absl::StartsWith(target_filepath, "/"),
            "Target filepath must be relative: " << target_filepath);
  CF_EXPECT(!absl::StrContains(target_filepath, ".."),
            "Target filepath cannot contain '..': " << target_filepath);

  return ArtifactBuildString{
      .build_string = std::move(build_string),
      .target_filepath = std::move(target_filepath),
  };
}

Flag ArtifactBuildFlag(const std::string& name,
                       std::vector<std::vector<ArtifactBuildString>>& value) {
  return Flag::StringFlag(name)
      .Getter([&value]() {
        std::vector<std::string> slot_strs;
        slot_strs.reserve(value.size());
        for (const std::vector<ArtifactBuildString>& slot : value) {
          slot_strs.emplace_back(
              absl::StrJoin(slot, ";", absl::StreamFormatter()));
        }
        return absl::StrJoin(slot_strs, ",");
      })
      .Setter([&value](std::string_view arg) -> Result<void> {
        if (arg.empty()) {
          value.clear();
          return {};
        }
        const std::vector<std::string_view> slots = absl::StrSplit(arg, ',');
        if (value.empty()) {
          value.resize(slots.size());
        } else {
          CF_EXPECT_EQ(slots.size(), value.size(),
                       "Mismatched flag lengths in repeated --artifact_build");
        }
        for (size_t i = 0; i < slots.size(); ++i) {
          if (slots[i].empty()) {
            continue;
          }
          for (const std::string_view segment :
               absl::StrSplit(slots[i], ';', absl::SkipEmpty())) {
            value[i].emplace_back(CF_EXPECT(ParseArtifactBuildString(segment)));
          }
        }
        return {};
      });
}

}  // namespace

std::ostream& operator<<(std::ostream& out,
                         const ArtifactBuildString& artifact_build_string) {
  return out << artifact_build_string.build_string << ":"
             << artifact_build_string.target_filepath;
}

std::vector<Flag> VectorFlags::Flags() {
  std::vector<Flag> flags;

  flags.emplace_back(
      GflagsCompatFlag("target_subdirectory", this->target_subdirectory)
          .Help("Target subdirectory to fetch files into.  Specifically aimed "
                "at organizing builds when there are multiple fetches. "
                "**Note**: directory separator automatically prepended, only "
                "give the subdirectory name."));
  flags.emplace_back(
      GflagsCompatFlag("default_build", this->default_build)
          .Help("source for the cuttlefish build to use (vendor.img + host)"));
  flags.emplace_back(GflagsCompatFlag("system_build", this->system_build)
                         .Help("source for system.img and product.img"));
  flags.emplace_back(GflagsCompatFlag("kernel_build", this->kernel_build)
                         .Help("source for the kernel or gki target"));
  flags.emplace_back(GflagsCompatFlag("boot_build", this->boot_build)
                         .Help("source for the boot or gki target"));
  flags.emplace_back(
      GflagsCompatFlag("vendor_boot_image_build", this->vendor_boot_image_build)
          .Help("source for the vendor_boot image"));
  flags.emplace_back(
      GflagsCompatFlag("bootloader_build", this->bootloader_build)
          .Help("source for the bootloader target"));
  flags.emplace_back(GflagsCompatFlag("android_efi_loader_build",
                                      this->android_efi_loader_build)
                         .Help("source for the uefi app target"));
  flags.emplace_back(GflagsCompatFlag("otatools_build", this->otatools_build)
                         .Help("source for the host ota tools"));
  flags.emplace_back(
      GflagsCompatFlag("test_suites_build", this->test_suites_build)
          .Help("source for the test suites build"));
  flags.emplace_back(
      GflagsCompatFlag("chrome_os_build", this->chrome_os_build)
          .Help("source for a ChromeOS build. Formatted as as a numeric build "
                "id, or '<project>/<bucket>/<builder>'"));
  flags.emplace_back(
      ArtifactBuildFlag("artifact_build", this->artifact_build)
          .Help("source for arbitrary build artifacts, formatted as "
                "'<build_string>{<remote_filepath>}[:<target_filepath>]' or "
                "'<build_string>{<remote_filepath>:<target_filepath>}'. "
                "Multiple artifacts for a build may be separated by ';' or by "
                "repeating --artifact_build."));

  flags.emplace_back(GflagsCompatFlag("boot_artifact", this->boot_artifact)
                         .Help("name of the boot image in boot_build"));
  flags.emplace_back(GflagsCompatFlag("download_img_zip",
                                      this->download_img_zip,
                                      kDefaultDownloadImgZip)
                         .Help("Whether to fetch the -img-*.zip file."));
  flags.emplace_back(
      GflagsCompatFlag("download_target_files_zip",
                       this->download_target_files_zip,
                       kDefaultDownloadTargetFilesZip)
          .Help("Whether to fetch the -target_files-*.zip file."));
  flags.emplace_back(
      GflagsCompatFlag("dynamic_super_image", this->dynamic_super_image,
                       kDefaultDynamicSuperImageFragments)
          .Help("Fetch the super image members as independent files."));

  return flags;
}

Result<int> VectorFlags::NumberOfBuilds() const {
  std::optional<size_t> number_of_builds;
  for (const auto& flag_size :
       {this->default_build.size(), this->system_build.size(),
        this->kernel_build.size(), this->boot_build.size(),
        this->vendor_boot_image_build.size(), this->bootloader_build.size(),
        this->android_efi_loader_build.size(), this->otatools_build.size(),
        this->test_suites_build.size(), this->chrome_os_build.size(),
        this->artifact_build.size(), this->boot_artifact.size(),
        this->download_img_zip.size(), this->download_target_files_zip.size(),
        this->target_subdirectory.size()}) {
    if (flag_size == 0) {
      // a size zero flag vector means the flag was not given
      continue;
    }
    if (number_of_builds) {
      CF_EXPECT(
          flag_size == *number_of_builds,
          "Mismatched flag lengths: " << *number_of_builds << "," << flag_size);
    }
    number_of_builds = flag_size;
  }
  // if no flags had values there is 1 all-default build
  return number_of_builds.value_or(1);
}

}  // namespace cuttlefish
