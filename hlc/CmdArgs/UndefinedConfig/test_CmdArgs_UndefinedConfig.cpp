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

// Command-line test: -cfg names no config of the design.
// Arguments (UndefinedConfig.hlc): -wd ../../../tests/CmdArgs top.sv -cfg nosuchcfg
// The design is compiled: the event is raised after the command line is parsed.

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class CmdArgsUndefinedConfigTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "UndefinedConfig.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsUndefinedConfigTest, ReportsCmdUndefinedConfig) {
  EXPECT_NE(findEvent(EventType::CmdUndefinedConfig, "nosuchcfg"), nullptr)
      << "CmdUndefinedConfig expected for: -cfg names no config of the design";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
