/*
 Copyright 2020 Apotell

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

// IEEE 1800-2023 Sec 33.3.1.1: a file that still matches file specs of two libraries is an error.
// Arguments: file_in_two_libraries.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class FileInTwoLibrariesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "file_in_two_libraries.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(FileInTwoLibrariesTest, ReportsFileMappedToTwoLibraries) {
  EXPECT_NE(findEvent(EventType::LibFileMapsToMultipleLibs), nullptr) << "lib_top.sv is in lib1 and lib2";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
