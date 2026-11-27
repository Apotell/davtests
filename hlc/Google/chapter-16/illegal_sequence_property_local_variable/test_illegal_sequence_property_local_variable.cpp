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

// IEEE 1800-2023:
//   Sec 16.8: "It shall be an error if a cyclic dependency among named sequences results from their
//     instantiations": s_cyc1/s_cyc2 (lines 19-20) and s_self (21). s_use instantiates s_ok (legal).
//   Sec 16.12.11: an unbounded range is not permitted with s_always: p_salw_bad (25). A bounded
//     s_always and the weak always [2:$] are legal.
//   Sec 16.12.16: "Use of multiple default statements in one property case statement shall be
//     illegal": the second default of p_case (33).
//   Sec 16.10: "A local variable shall not be used in a clocking event expression": s_lv_clk (37).
//   Sec 16.12.1: the disable condition shall not reference local variables: p_lv_dis (38).
//   Sec 16.12.14: an abort condition shall not reference local variables: p_lv_abort (39).
//   p_lv_ok uses its local variable in the property body (legal).
// Arguments: illegal_sequence_property_local_variable.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/property_decl.h>
#include <hldb/property_spec.h>
#include <hldb/ref_obj.h>
#include <hldb/sequence_decl.h>
#include <hldb/sequence_inst.h>
#include <hldb/variable.h>

#include <gtest/gtest.h>

#include <string_view>

namespace hlc {

class IllegalSequencePropertyLocalVariableTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_sequence_property_local_variable.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *findModule() {
    return hldb::findByDefName<hldb::Module>("m", m_design->getAllModules());
  }
};

TEST_F(IllegalSequencePropertyLocalVariableTest, MutuallyDependentSequences) {
  EXPECT_NE(findEvent(EventType::CompIllegalSequence, "s_cyc1", 19, 26), nullptr);
  EXPECT_NE(findEvent(EventType::CompIllegalSequence, "s_cyc2", 20, 26), nullptr);
}

TEST_F(IllegalSequencePropertyLocalVariableTest, SequenceInstantiatingItself) {
  EXPECT_NE(findEvent(EventType::CompIllegalSequence, "s_self", 21, 26), nullptr);
}

TEST_F(IllegalSequencePropertyLocalVariableTest, StrongAlwaysWithUnboundedRange) {
  EXPECT_NE(findEvent(EventType::CompIllegalProperty, "s_always", 25, 24), nullptr);
}

TEST_F(IllegalSequencePropertyLocalVariableTest, SecondDefaultInPropertyCase) {
  EXPECT_NE(findEvent(EventType::CompIllegalProperty, "default", 33, 7), nullptr);
}

TEST_F(IllegalSequencePropertyLocalVariableTest, LocalVariableInClockingEvent) {
  EXPECT_NE(findEvent(EventType::CompIllegalLocalVariable, "v", 37, 57), nullptr);
}

TEST_F(IllegalSequencePropertyLocalVariableTest, LocalVariableInDisableCondition) {
  EXPECT_NE(findEvent(EventType::CompIllegalLocalVariable, "v", 38, 42), nullptr);
}

TEST_F(IllegalSequencePropertyLocalVariableTest, LocalVariableInAbortCondition) {
  EXPECT_NE(findEvent(EventType::CompIllegalLocalVariable, "v", 39, 60), nullptr);
}

TEST_F(IllegalSequencePropertyLocalVariableTest, InstanceOfAnotherSequence) {
  // sequence s_use; s_ok ##1 c; endsequence
  const hldb::Module *const m = findModule();
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *const sUse = hldb::findByName<hldb::SequenceDecl>("s_use", m->getSequenceDecls());
  const hldb::SequenceDecl *const sOk = hldb::findByName<hldb::SequenceDecl>("s_ok", m->getSequenceDecls());
  ASSERT_NE(sUse, nullptr);
  ASSERT_NE(sOk, nullptr);
  const hldb::Operation *const delay = sUse->getExpr<hldb::Operation>();
  ASSERT_NE(delay, nullptr);
  ASSERT_NE(delay->getOperands(), nullptr);
  ASSERT_FALSE(delay->getOperands()->empty());
  const hldb::SequenceInst *const inst = any_cast<hldb::SequenceInst>(delay->getOperands()->front());
  ASSERT_NE(inst, nullptr);
  EXPECT_EQ(inst->getSequenceDecl(), sOk);
}

TEST_F(IllegalSequencePropertyLocalVariableTest, LocalVariableBelongsToItsProperty) {
  const hldb::Module *const m = findModule();
  ASSERT_NE(m, nullptr);
  const hldb::PropertyDecl *const p = hldb::findByName<hldb::PropertyDecl>("p_lv_ok", m->getPropertyDecls());
  ASSERT_NE(p, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("v", p->getVariables());
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getParent(), p);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
