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

// IEEE 1800-2023 Sec 10.7: "If the width of the right-hand expression is larger than the width of the
// left-hand side, the MSBs of the right-hand expression will always be discarded to match the size of
// the left-hand side." HLC warns when the discarded bits of a sized literal are not all zero: the
// initializer of z (line 22), the continuous assignment to w (23), x = 8'hFF (25) and y = 12'o7777
// (29). y = 8'h0F (26), x = 4'd9 (27) and the unsized x = 255 (28) are not reported.
// Arguments: assignment_truncation.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/range.h>
#include <hldb/variable.h>

#include <gtest/gtest.h>

#include <string_view>

namespace hlc {

class AssignmentTruncationTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "assignment_truncation.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(AssignmentTruncationTest, DeclarationInitializer) {
  EXPECT_NE(findEvent(EventType::CompAssignmentTruncation, "z", 22, 19), nullptr);
}

TEST_F(AssignmentTruncationTest, ContinuousAssignment) {
  EXPECT_NE(findEvent(EventType::CompAssignmentTruncation, "w", 23, 14), nullptr);
}

TEST_F(AssignmentTruncationTest, ProceduralAssignments) {
  EXPECT_NE(findEvent(EventType::CompAssignmentTruncation, "x", 25, 9), nullptr);
  EXPECT_NE(findEvent(EventType::CompAssignmentTruncation, "y", 29, 9), nullptr);
}

TEST_F(AssignmentTruncationTest, TargetIsFourBitsWide) {
  const hldb::Module *const m = hldb::findByDefName<hldb::Module>("m", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  const hldb::Variable *const x = hldb::findByName<hldb::Variable>("x", m->getVariables());
  ASSERT_NE(x, nullptr);
  const hldb::LogicTypespec *const ts = hldb::getTypespec<hldb::LogicTypespec>(x);
  ASSERT_NE(ts, nullptr);
  ASSERT_NE(ts->getRanges(), nullptr);
  ASSERT_EQ(ts->getRanges()->size(), 1u);
  const hldb::Range *const range = ts->getRanges()->front();
  ASSERT_NE(range->getLeftExpr(), nullptr);
  ASSERT_NE(range->getRightExpr(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
