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

// Tests for tests/Reachability/dut.sv compiled with
//   -top top -top leaf -top nested_implicit -top missing_mod
// Top-level modules named on the command line replace the implicit ones: exactly the modules and
// programs so named are the roots. leaf is instantiated by top and nested_implicit is nested in it
// (neither is top-level in the sense of IEEE 1800-2023 Sec 23.3.1): both are reported, and still
// used. missing_mod names nothing. prog is not named, so it is not a root.

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/program.h>

#include <gtest/gtest.h>

#include <string_view>

namespace hlc {

class ReachabilityTopNamedTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "TopNamed.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }
  static const hldb::Module *getNestedModule(std::string_view name) {
    const hldb::Module *const top = getModule("top");
    return (top == nullptr) ? nullptr : hldb::findByDefName<hldb::Module>(name, top->getModules());
  }
};

TEST_F(ReachabilityTopNamedTest, NamedModulesAreTheTopLevels) {
  ASSERT_NE(getModule("top"), nullptr);
  ASSERT_NE(getModule("leaf"), nullptr);
  ASSERT_NE(getNestedModule("nested_implicit"), nullptr);
  EXPECT_TRUE(isTopLevel(getModule("top")));
  EXPECT_TRUE(isTopLevel(getModule("leaf")));
  EXPECT_TRUE(isTopLevel(getNestedModule("nested_implicit")));
  EXPECT_EQ(getTopLevelCount(), 3u);
}

TEST_F(ReachabilityTopNamedTest, UnnamedProgramIsNotATopLevel) {
  const hldb::Program *const prog = hldb::findByDefName<hldb::Program>("prog", m_design->getAllPrograms());
  ASSERT_NE(prog, nullptr);
  EXPECT_FALSE(isTopLevel(prog));
  EXPECT_FALSE(isReachable(prog));
}

TEST_F(ReachabilityTopNamedTest, MissingTopLevelIsReported) {
  EXPECT_NE(findEvent(EventType::ElabTopLevelDoesNotExist, "missing_mod"), nullptr);
}

TEST_F(ReachabilityTopNamedTest, InstantiatedTopLevelIsReported) {
  EXPECT_NE(findEvent(EventType::ElabTopLevelIsNotATopLevel, "leaf"), nullptr);
}

TEST_F(ReachabilityTopNamedTest, NestedTopLevelIsReported) {
  EXPECT_NE(findEvent(EventType::ElabTopLevelIsNotATopLevel, "nested_implicit"), nullptr);
}

TEST_F(ReachabilityTopNamedTest, EveryNamedTopLevelIsReportedAsOne) {
  for (const std::string_view name : {"top", "leaf", "nested_implicit"}) {
    EXPECT_NE(findEvent(EventType::ElabTopLevelModule, name), nullptr) << name;
  }
  EXPECT_NE(findEvent(EventType::ElabMultipleTopLevelModules), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
