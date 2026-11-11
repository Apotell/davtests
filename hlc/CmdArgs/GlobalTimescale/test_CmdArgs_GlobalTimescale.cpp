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

// Command-line test: the -timescale default is used for a module without a timescale.
// Arguments (after -wd tests/CmdArgs top.sv): -timescale=1ns/1ns
// The design is compiled: the event is raised after the command line is parsed.

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace hlc {

namespace {
std::string testsDir() {
  return (std::filesystem::path(__FILE__).parent_path() / "../../../tests/CmdArgs").lexically_normal().string();
}
}  // namespace

class CmdArgsGlobalTimescaleTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-wd", testsDir().c_str(), "top.sv", "-timescale=1ns/1ns"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsGlobalTimescaleTest, ReportsCmdUsingGlobalTimescale) {
  EXPECT_NE(findEvent(EventType::CmdUsingGlobalTimescale), nullptr) << "CmdUsingGlobalTimescale expected for: the -timescale default is used for a module without a timescale";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
