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

// IEEE 1800-2023 Sec 14.12: "default clocking <name>;" shall name a clocking block of the scope.
// nocb (line 19), other (line 31, declared in another module), inone (line 37) and cnone (line 43)
// name no such block. later (line 20, declared below its use), icb (line 36) and ccb (line 42) are
// legal and become the scope's default clocking.
// Arguments: undefined_clocking_block.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/clocking_block.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/module.h>

#include <gtest/gtest.h>

namespace hlc {

class UndefinedClockingBlockTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "undefined_clocking_block.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(UndefinedClockingBlockTest, ReportsUndefinedNameInModule) {
  EXPECT_NE(findEvent(EventType::CompUndefinedClockingBlock, "nocb", 19, 20), nullptr) << "no clocking block nocb";
}

TEST_F(UndefinedClockingBlockTest, ReportsBlockOfAnotherModule) {
  EXPECT_NE(findEvent(EventType::CompUndefinedClockingBlock, "other", 31, 20), nullptr) << "other is declared in m2";
}

TEST_F(UndefinedClockingBlockTest, ReportsUndefinedNameInInterface) {
  EXPECT_NE(findEvent(EventType::CompUndefinedClockingBlock, "inone", 37, 20), nullptr) << "no clocking block inone";
}

TEST_F(UndefinedClockingBlockTest, ReportsUndefinedNameInChecker) {
  EXPECT_NE(findEvent(EventType::CompUndefinedClockingBlock, "cnone", 43, 20), nullptr) << "no clocking block cnone";
}

TEST_F(UndefinedClockingBlockTest, ForwardReferenceBecomesDefaultClocking) {
  const hldb::Module *const m1 = hldb::findByDefName<hldb::Module>("m1", m_design->getAllModules());
  ASSERT_NE(m1, nullptr);
  ASSERT_NE(m1->getDefaultClocking(), nullptr) << "default clocking later; sets getDefaultClocking()";
  EXPECT_EQ(m1->getDefaultClocking()->getName(), std::string_view("later"));
  EXPECT_EQ(m1->getDefaultClocking()->getStartLine(), 21);
}

TEST_F(UndefinedClockingBlockTest, InterfaceReferenceBecomesDefaultClocking) {
  const hldb::Interface *const i1 = hldb::findByDefName<hldb::Interface>("i1", m_design->getAllInterfaces());
  ASSERT_NE(i1, nullptr);
  ASSERT_NE(i1->getDefaultClocking(), nullptr) << "default clocking icb; sets getDefaultClocking()";
  EXPECT_EQ(i1->getDefaultClocking()->getName(), std::string_view("icb"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
