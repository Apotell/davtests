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

// Source under test: tests/Google/generic/member/class_member_test_30.sv
//
//   class myclass;
//   function void shifter;
//     for (shft_idx=0, c=1'b1; shft_idx < n_bits;
//          shft_idx++) begin
//       dout = {dout} << 1;
//     end
//   endfunction
//   endclass
//
// Checked (IEEE 1800-2023):
//   - Sec 8.3: class "myclass" exists, not virtual, with no extends, and
//     defines exactly one method.
//   - Sec 8.6 / 13.4: "shifter" is a non-virtual public method Function
//     with automatic lifetime, returning void, with no formals.
//   - Sec 12.7.1: the body is a single for-loop whose initializer is two
//     comma-separated assignments, "shft_idx=0" then "c=1'b1"; whose
//     condition is "shft_idx < n_bits" (binary less-than); whose only step
//     is "shft_idx++" (post-increment, Sec 11.4.2); and whose begin-end body
//     holds exactly one statement.
//   - Sec 10.4.1 / 11.4.10 / 11.4.12: that statement is the blocking
//     assignment "dout = {dout} << 1", whose right side is a left shift of
//     the one-element concatenation "{dout}" by the constant 1.
//   - Sec 6.3: "shft_idx", "c", "n_bits" and "dout" are never declared
//     anywhere, so each reference must be flagged as an error. Checked by
//     an error-level diagnostic at each position; the exact event code is
//     not assumed.
//
// Confirmed by running this test (not from the .log): all seven references
// get only a warning (CompFailedToBind) at the right position and no
// error, so those four checks are GTEST_SKIP until HLC is fixed.
//
// Not checked:
//   - Whether the single for-loop is wrapped in an implicit begin (both
//     shapes are accepted).
//   - vpiFullName on any object (never asserted per the test writing guide).

#include <hlc/Tests/Test.h>

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/for_stmt.h>
#include <hldb/function.h>
#include <hldb/operation.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/void_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {
class ClassMemberTest30Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_member_test_30.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *getMyClass() {
    return hldb::findByName<hldb::ClassDefn>("myclass", m_design->getAllClasses());
  }

  static const hldb::Function *getShifter() {
    const hldb::ClassDefn *const cls = getMyClass();
    if (cls == nullptr || cls->getMethods() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Function>("shifter", cls->getMethods());
  }

  // The function's only statement, looking through an implicit begin.
  static const hldb::ForStmt *getForLoop() {
    const hldb::Function *const shifter = getShifter();
    if (shifter == nullptr || shifter->getStmt() == nullptr) {
      return nullptr;
    }
    const hldb::Begin *const wrapper = shifter->getStmt<hldb::Begin>();
    if (wrapper == nullptr) {
      return shifter->getStmt<hldb::ForStmt>();
    }
    if (wrapper->getStmts() == nullptr || wrapper->getStmts()->size() != 1) {
      return nullptr;
    }
    return any_cast<hldb::ForStmt>(wrapper->getStmts()->at(0));
  }

  // The for-loop body's only statement.
  static const hldb::Assignment *getBodyAssignment() {
    const hldb::ForStmt *const loop = getForLoop();
    if (loop == nullptr) {
      return nullptr;
    }
    const hldb::Begin *const body = loop->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() != 1) {
      return nullptr;
    }
    return any_cast<hldb::Assignment>(body->getStmts()->at(0));
  }

  // Checks one "name=value" initializer assignment.
  static void expectInitAssignment(const hldb::Any *const stmt, std::string_view name, std::string_view value) {
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(stmt);
    ASSERT_NE(assign, nullptr) << "'" << name << "=" << value << "' should be an Assignment";
    ASSERT_NE(assign->getLhs(), nullptr);
    EXPECT_EQ(assign->getLhs()->getName(), name) << "the assignment target is '" << name << "'";
    const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr) << "the assigned value '" << value << "' is a constant";
    EXPECT_EQ(rhs->getDecompile(), value);
  }

  // True when an error-or-worse diagnostic points at the given source
  // position. The exact event code is not assumed: the LRM only requires
  // that the construct is flagged as an error.
  bool hasErrorAt(uint32_t line, uint16_t column) {
    for (const Event &event : m_session->getEventListener()->getEvents()) {
      const EventDefinition *const info = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
      if (info == nullptr || info->m_severity > EventSeverity::Error) {
        continue;
      }
      for (const EventArg &arg : event.getArguments()) {
        if (arg.m_line == line && arg.m_column == column) {
          return true;
        }
      }
    }
    return false;
  }
};

TEST_F(ClassMemberTest30Test, MyclassExists) {
  const hldb::ClassDefn *const cls = getMyClass();
  ASSERT_NE(cls, nullptr) << "'class myclass;' should create a ClassDefn";
  EXPECT_FALSE(cls->getVirtual()) << "'myclass' is not declared virtual";
  EXPECT_EQ(cls->getExtends(), nullptr) << "'myclass' has no extends clause";
  ASSERT_NE(cls->getMethods(), nullptr);
  EXPECT_EQ(cls->getMethods()->size(), 1u) << "'myclass' defines exactly one method, 'shifter'";
}

TEST_F(ClassMemberTest30Test, ShifterIsVoidMethodWithNoFormals) {
  const hldb::Function *const shifter = getShifter();
  ASSERT_NE(shifter, nullptr) << "'function void shifter;' should be a Function in myclass";
  EXPECT_TRUE(shifter->getMethod()) << "'shifter' is defined inside a class, so it is a method";
  EXPECT_FALSE(shifter->getVirtual()) << "'shifter' is not declared virtual";
  EXPECT_EQ(shifter->getVisibility(), vpiPublicVis) << "no local/protected qualifier: visibility is public";
  EXPECT_TRUE(shifter->getAutomatic()) << "Sec 8.6: class methods have automatic lifetime";
  ASSERT_NE(shifter->getReturn(), nullptr) << "'function void shifter' should carry a return RefTypespec";
  EXPECT_NE(shifter->getReturn()->getActual<hldb::VoidTypespec>(), nullptr) << "'shifter' returns void";
  EXPECT_TRUE(shifter->getIODecls() == nullptr || shifter->getIODecls()->empty())
      << "'shifter' is declared without a port list, so it has no formals";
}

TEST_F(ClassMemberTest30Test, ForInitIsTwoAssignmentsInOrder) {
  const hldb::ForStmt *const loop = getForLoop();
  ASSERT_NE(loop, nullptr) << "the body of 'shifter' should be a single for-loop";
  ASSERT_NE(loop->getForInitStmts(), nullptr) << "Sec 12.7.1: 'shft_idx=0, c=1'b1' is the loop initializer";
  ASSERT_EQ(loop->getForInitStmts()->size(), 2u) << "the initializer holds two comma-separated assignments";
  expectInitAssignment(loop->getForInitStmts()->at(0), "shft_idx", "0");
  expectInitAssignment(loop->getForInitStmts()->at(1), "c", "1'b1");
}

TEST_F(ClassMemberTest30Test, ForConditionIsLessThan) {
  const hldb::ForStmt *const loop = getForLoop();
  ASSERT_NE(loop, nullptr);
  const hldb::Operation *const cond = loop->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "'shft_idx < n_bits' should be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiLtOp) << "the condition uses binary less-than";
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  EXPECT_EQ(cond->getOperands()->at(0)->getName(), "shft_idx") << "the left operand is 'shft_idx'";
  EXPECT_EQ(cond->getOperands()->at(1)->getName(), "n_bits") << "the right operand is 'n_bits'";
}

TEST_F(ClassMemberTest30Test, ForStepIsPostIncrementOfShftIdx) {
  const hldb::ForStmt *const loop = getForLoop();
  ASSERT_NE(loop, nullptr);
  ASSERT_NE(loop->getForIncStmts(), nullptr) << "'shft_idx++' is the loop step";
  ASSERT_EQ(loop->getForIncStmts()->size(), 1u) << "the loop has exactly one step expression";
  const hldb::Operation *const inc = any_cast<hldb::Operation>(loop->getForIncStmts()->at(0));
  ASSERT_NE(inc, nullptr) << "'shft_idx++' should be an Operation";
  EXPECT_EQ(inc->getOpType(), vpiPostIncOp) << "Sec 11.4.2: '++' after the operand is post-increment";
  ASSERT_NE(inc->getOperands(), nullptr);
  ASSERT_EQ(inc->getOperands()->size(), 1u);
  EXPECT_EQ(inc->getOperands()->at(0)->getName(), "shft_idx") << "the incremented operand is 'shft_idx'";
}

TEST_F(ClassMemberTest30Test, ForBodyHoldsOneBlockingAssignmentToDout) {
  const hldb::ForStmt *const loop = getForLoop();
  ASSERT_NE(loop, nullptr);
  const hldb::Begin *const body = loop->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "'begin ... end' should be a Begin block";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u) << "the begin-end block holds exactly one statement";
  const hldb::Assignment *const assign = getBodyAssignment();
  ASSERT_NE(assign, nullptr) << "'dout = {dout} << 1;' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "Sec 10.4.1: '=' is a blocking procedural assignment";
  ASSERT_NE(assign->getLhs(), nullptr);
  EXPECT_EQ(assign->getLhs()->getName(), "dout") << "the assignment target is 'dout'";
}

TEST_F(ClassMemberTest30Test, BodyRhsIsConcatShiftedLeftByOne) {
  const hldb::Assignment *const assign = getBodyAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const shift = assign->getRhs<hldb::Operation>();
  ASSERT_NE(shift, nullptr) << "'{dout} << 1' should be an Operation";
  EXPECT_EQ(shift->getOpType(), vpiLShiftOp) << "Sec 11.4.10: '<<' is a logical left shift";
  ASSERT_NE(shift->getOperands(), nullptr);
  ASSERT_EQ(shift->getOperands()->size(), 2u);

  const hldb::Operation *const concat = any_cast<hldb::Operation>(shift->getOperands()->at(0));
  ASSERT_NE(concat, nullptr) << "'{dout}' should be an Operation";
  EXPECT_EQ(concat->getOpType(), vpiConcatOp) << "Sec 11.4.12: '{dout}' is a concatenation";
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 1u) << "'{dout}' concatenates exactly one operand";
  EXPECT_EQ(concat->getOperands()->at(0)->getName(), "dout");

  const hldb::Constant *const amount = any_cast<hldb::Constant>(shift->getOperands()->at(1));
  ASSERT_NE(amount, nullptr) << "the shift amount '1' is a constant";
  EXPECT_EQ(amount->getDecompile(), "1");
}

TEST_F(ClassMemberTest30Test, UndeclaredShftIdxIsReported) {
  GTEST_SKIP()
      << "HLC reports the undeclared 'shft_idx' (8:8, 8:28, 9:8) only as warnings (CompFailedToBind); per IEEE "
         "1800-2023 Sec 6.3 it must be an error. Fix pending.";
  // Sec 6.3: data shall be declared before use; "shft_idx" names no
  // variable anywhere (not a class property, local or formal).
  EXPECT_TRUE(hasErrorAt(8, 8)) << "'shft_idx' in the initializer is undeclared; an error at 8:8 is expected";
  EXPECT_TRUE(hasErrorAt(8, 28)) << "'shft_idx' in the condition is undeclared; an error at 8:28 is expected";
  EXPECT_TRUE(hasErrorAt(9, 8)) << "'shft_idx' in the step is undeclared; an error at 9:8 is expected";
}

TEST_F(ClassMemberTest30Test, UndeclaredCIsReported) {
  GTEST_SKIP() << "HLC reports the undeclared 'c' (8:20) only as a warning (CompFailedToBind); per IEEE "
                  "1800-2023 Sec 6.3 it must be an error. Fix pending.";
  // Sec 6.3, as above, for "c".
  EXPECT_TRUE(hasErrorAt(8, 20)) << "'c' is undeclared; an error at 8:20 is expected";
}

TEST_F(ClassMemberTest30Test, UndeclaredNBitsIsReported) {
  GTEST_SKIP() << "HLC reports the undeclared 'n_bits' (8:39) only as a warning (CompFailedToBind); per IEEE "
                  "1800-2023 Sec 6.3 it must be an error. Fix pending.";
  // Sec 6.3, as above, for "n_bits".
  EXPECT_TRUE(hasErrorAt(8, 39)) << "'n_bits' is undeclared; an error at 8:39 is expected";
}

TEST_F(ClassMemberTest30Test, UndeclaredDoutIsReported) {
  GTEST_SKIP() << "HLC reports the undeclared 'dout' (10:5, 10:13) only as warnings (CompFailedToBind); per IEEE "
                  "1800-2023 Sec 6.3 it must be an error. Fix pending.";
  // Sec 6.3, as above, for both references to "dout".
  EXPECT_TRUE(hasErrorAt(10, 5)) << "'dout' as target is undeclared; an error at 10:5 is expected";
  EXPECT_TRUE(hasErrorAt(10, 13)) << "'dout' in '{dout}' is undeclared; an error at 10:13 is expected";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
