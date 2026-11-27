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

// IEEE 1800-2023 Sec 16.13.1: "Differently clocked or multiclocked sequence operands cannot be
// combined with any sequence operators other than ##1 and ##0." Illegal: s_delay2 (##2, line 21),
// s_intersect (22), s_within (23), s_range (##[1:3], 24) and the assertion A1 (27). Legal:
// s_concat1 and s_concat0 (the Sec 16.13.1 examples), s_same_clock (one clock), and p_and, whose and
// of differently clocked operands is a property conjunction (Sec 16.13.2).
// Arguments: clock_mismatch.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/clocked_seq.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/sequence_decl.h>
#include <hldb/sv_vpi_user.h>

#include <gtest/gtest.h>

#include <string_view>

namespace hlc {

class ClockMismatchTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "clock_mismatch.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(ClockMismatchTest, CycleDelayOtherThanOneOrZero) {
  EXPECT_NE(findEvent(EventType::CompClockMismatch, "s_delay2", 21, 38), nullptr);
}

TEST_F(ClockMismatchTest, Intersect) {
  EXPECT_NE(findEvent(EventType::CompClockMismatch, "s_intersect", 22, 41), nullptr);
}

TEST_F(ClockMismatchTest, Within) {
  EXPECT_NE(findEvent(EventType::CompClockMismatch, "s_within", 23, 22), nullptr);
}

TEST_F(ClockMismatchTest, CycleDelayRange) {
  EXPECT_NE(findEvent(EventType::CompClockMismatch, "s_range", 24, 37), nullptr);
}

TEST_F(ClockMismatchTest, InAssertion) {
  EXPECT_NE(findEvent(EventType::CompClockMismatch, "A1", 27, 40), nullptr);
}

TEST_F(ClockMismatchTest, SingleDelayJoinsTwoClocks) {
  // @(posedge clk0) sig0 ##1 @(posedge clk1) sig1
  const hldb::Module *const m = hldb::findByDefName<hldb::Module>("m", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *const s = hldb::findByName<hldb::SequenceDecl>("s_concat1", m->getSequenceDecls());
  ASSERT_NE(s, nullptr);
  ASSERT_NE(s->getExpr(), nullptr);
  ASSERT_EQ(s->getExpr()->getAnyType(), hldb::AnyType::ClockedSeq);
  const hldb::ClockedSeq *const outer = s->getExpr<hldb::ClockedSeq>();
  ASSERT_NE(outer->getClockingEvent(), nullptr);
  ASSERT_NE(outer->getSequenceExpr(), nullptr);
  ASSERT_EQ(outer->getSequenceExpr()->getAnyType(), hldb::AnyType::Operation);
  const hldb::Operation *const delay = outer->getSequenceExpr<hldb::Operation>();
  EXPECT_EQ(delay->getOpType(), vpiCycleDelayOp);
  ASSERT_NE(delay->getOperands(), nullptr);
  ASSERT_EQ(delay->getOperands()->size(), 3u);
  ASSERT_EQ(delay->getOperands()->at(1)->getAnyType(), hldb::AnyType::ClockedSeq);
  EXPECT_NE(any_cast<hldb::ClockedSeq>(delay->getOperands()->at(1))->getClockingEvent(), nullptr);
  const hldb::Constant *const one = any_cast<hldb::Constant>(delay->getOperands()->at(2));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), std::string_view("1"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
