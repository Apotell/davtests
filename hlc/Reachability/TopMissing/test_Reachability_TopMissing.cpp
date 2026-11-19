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

// Tests for tests/Reachability/dut.sv compiled with -top nosuch: the only top-level module named on
// the command line does not exist. The named top-levels replace the implicit ones, so the design --
// which has modules and programs -- has no top-level module at all (IEEE 1800-2023 Sec 23.3.1: "A
// design shall contain at least one top-level module"), and nothing is reachable.

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>

#include <gtest/gtest.h>

namespace hlc {

class ReachabilityTopMissingTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "TopMissing.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(ReachabilityTopMissingTest, MissingTopLevelIsReported) {
  EXPECT_NE(findEvent(EventType::ElabTopLevelDoesNotExist, "nosuch"), nullptr);
}

TEST_F(ReachabilityTopMissingTest, NoTopLevelModuleIsReported) {
  EXPECT_NE(findEvent(EventType::ElabNoTopLevelModule), nullptr);
}

TEST_F(ReachabilityTopMissingTest, NothingIsTopLevel) { EXPECT_EQ(getTopLevelCount(), 0u); }

TEST_F(ReachabilityTopMissingTest, ImplicitTopLevelIsNotUsed) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_FALSE(isTopLevel(top));
  EXPECT_FALSE(isReachable(top));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
