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

// Validates the HLDB model built for tests/PatternAssignment/dut.sv:
//
//   module dut();
//
//   typedef struct packed {
//     logic a;
//     logic b;
//   } typedef_struct;
//
//   typedef_struct c;
//   logic test = 1'b1;
//   assign c = '{
//     a : test == 1'b1 ? 1'b0 : 1'b1,
//     b : 1'b1 ? 1'b1 : 1'b0
//   };
//
//   endmodule
//
// The fixture assigns a structure assignment pattern with member keys
// (10.9.2) whose values are conditional operators (11.4.11). The .hlc also
// passes -synth.
//
// What is checked, and why:
//   - c is a variable of the packed structure typedef typedef_struct
//     (6.18, 7.2.1); test is a logic variable initialized to 1'b1 (6.8)
//   - the continuous assignment (10.3) targets c
//   - its RHS is an assignment pattern of 2 items; the keys a and b bind to
//     the structure members a and b (10.9.2)
//   - a's value is 'test == 1'b1 ? 1'b0 : 1'b1': a conditional operator
//     (11.4.11) whose condition is an equality (11.4.5) of the variable test
//     and 1'b1
//   - b's value is '1'b1 ? 1'b1 : 1'b0'
//   - dut's '()' declares one null port (37.14 detail 10)
//   - no fatal, syntax or error diagnostic: the file is legal
//
// Model convention: each 'member: value' item is a TaggedPattern whose tag
// is a RefObj bound to the TypespecMember and whose pattern is the value.
//
// What is NOT checked, and why:
//   - The value of c: reducing the pattern is elaboration, and no .hlc in
//     this suite elaborates.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/tagged_pattern.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PatternAssignmentTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PatternAssignment.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getDut() { return hldb::findByDefName<hldb::Module>("dut", m_design->getAllModules()); }

  static const hldb::Variable *getVar(std::string_view name) {
    return hldb::findByName<hldb::Variable>(name, getDut()->getVariables());
  }

  static const hldb::Operation *getPattern() {
    const hldb::Module *const m = getDut();
    if ((m == nullptr) || (m->getContAssigns() == nullptr) || (m->getContAssigns()->size() != 1)) return nullptr;
    return m->getContAssigns()->at(0)->getRhs<hldb::Operation>();
  }

  static const hldb::TaggedPattern *getItem(size_t i) {
    const hldb::Operation *const pat = getPattern();
    if ((pat == nullptr) || (pat->getOperands() == nullptr) || (pat->getOperands()->size() != 2)) return nullptr;
    return any_cast<hldb::TaggedPattern>(pat->getOperands()->at(i));
  }

  static void ExpectConstant(const hldb::Any *e, std::string_view text, std::string_view why) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(e);
    ASSERT_NE(c, nullptr) << why << ": a literal";
    EXPECT_EQ(c->getDecompile(), text) << why;
  }

  static void ExpectKey(const hldb::TaggedPattern *tp, std::string_view member) {
    ASSERT_NE(tp, nullptr) << "10.9.2: a member:value item";
    const hldb::RefObj *const key = tp->getTag<hldb::RefObj>();
    ASSERT_NE(key, nullptr);
    EXPECT_EQ(key->getName(), member);
    const hldb::TypespecMember *const tm = key->getActual<hldb::TypespecMember>();
    ASSERT_NE(tm, nullptr) << "10.9.2: \"A member:value specifies an explicit value for a named member of the "
                              "structure.\"";
    EXPECT_EQ(tm->getName(), member);
  }

  // Verifies 'e' is a conditional operator and returns its operands.
  static const hldb::Operation *checkConditional(const hldb::Any *e) {
    const hldb::Operation *const op = any_cast<hldb::Operation>(e);
    EXPECT_NE(op, nullptr) << "11.4.11: a conditional operator";
    if ((op == nullptr) || (op->getOperands() == nullptr)) return nullptr;
    EXPECT_EQ(op->getOpType(), vpiConditionOp) << "11.4.11: '?:'";
    EXPECT_EQ(op->getOperands()->size(), 3u) << "condition, then and else";
    return (op->getOperands()->size() == 3) ? op : nullptr;
  }
};

TEST_F(PatternAssignmentTest, CIsTypedefStructAndTestIsLogic) {
  ASSERT_NE(getDut(), nullptr);
  const hldb::Variable *const c = getVar("c");
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getTypespec(), nullptr);
  const hldb::TypedefTypespec *const td = c->getTypespec()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(td, nullptr) << "6.18: c is of the typedef type";
  EXPECT_EQ(td->getName(), "typedef_struct");
  const hldb::Variable *const t = getVar("test");
  ASSERT_NE(t, nullptr);
  ASSERT_NE(t->getTypespec(), nullptr);
  EXPECT_NE(t->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr) << "6.11: logic";
  ExpectConstant(t->getValue(), "1'b1", "6.8: test's initializer");
}

TEST_F(PatternAssignmentTest, ContAssignTargetsC) {
  const hldb::Module *const m = getDut();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getContAssigns(), nullptr);
  ASSERT_EQ(m->getContAssigns()->size(), 1u) << "10.3: one continuous assignment";
  const hldb::RefObj *const lhs = m->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual(), getVar("c"));
}

TEST_F(PatternAssignmentTest, PatternKeysBindToMembersAAndB) {
  const hldb::Operation *const pat = getPattern();
  ASSERT_NE(pat, nullptr);
  EXPECT_EQ(pat->getOpType(), vpiAssignmentPatternOp) << "10.9.2: an assignment pattern";
  ASSERT_NE(pat->getOperands(), nullptr);
  ASSERT_EQ(pat->getOperands()->size(), 2u);
  ExpectKey(getItem(0), "a");
  ExpectKey(getItem(1), "b");
}

TEST_F(PatternAssignmentTest, ValueOfAIsConditionalOnTest) {
  const hldb::TaggedPattern *const tp = getItem(0);
  ASSERT_NE(tp, nullptr);
  const hldb::Operation *const cond = checkConditional(tp->getPattern());
  ASSERT_NE(cond, nullptr);
  const hldb::Operation *const eq = any_cast<hldb::Operation>(cond->getOperands()->at(0));
  ASSERT_NE(eq, nullptr);
  EXPECT_EQ(eq->getOpType(), vpiEqOp) << "11.4.5: '=='";
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);
  const hldb::RefObj *const t = any_cast<hldb::RefObj>(eq->getOperands()->at(0));
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->getActual(), getVar("test"));
  ExpectConstant(eq->getOperands()->at(1), "1'b1", "the compared literal");
  ExpectConstant(cond->getOperands()->at(1), "1'b0", "11.4.11: the then value");
  ExpectConstant(cond->getOperands()->at(2), "1'b1", "11.4.11: the else value");
}

TEST_F(PatternAssignmentTest, ValueOfBIsConstantConditional) {
  const hldb::TaggedPattern *const tp = getItem(1);
  ASSERT_NE(tp, nullptr);
  const hldb::Operation *const cond = checkConditional(tp->getPattern());
  ASSERT_NE(cond, nullptr);
  ExpectConstant(cond->getOperands()->at(0), "1'b1", "11.4.11: the condition");
  ExpectConstant(cond->getOperands()->at(1), "1'b1", "11.4.11: the then value");
  ExpectConstant(cond->getOperands()->at(2), "1'b0", "11.4.11: the else value");
}

TEST_F(PatternAssignmentTest, DutHasOneNullPort) {
  const hldb::Module *const m = getDut();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: '()' declares a null port";
  ASSERT_EQ(m->getPorts()->size(), 1u);
  EXPECT_EQ(m->getPorts()->at(0)->getName(), "");
}

TEST_F(PatternAssignmentTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
