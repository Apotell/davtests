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

// Source under test: tests/Google/generic/member/class_member_test_31.sv
//
//   class myclass;
//   int dout;
//   int n_bits;
//
//   function void shifter;
//     for (int shft_idx=0, bit c=1'b1; shft_idx < n_bits;
//          shft_idx++) begin
//       dout = {dout} << 1;
//     end
//   endfunction
//   endclass
//
//   module test;
//   endmodule
//
// Checked (IEEE 1800-2023):
//   - Sec 8.3: class "myclass" exists, not virtual, with no extends, holding
//     exactly two int properties "dout" and "n_bits" (signed, Sec 6.11) and
//     exactly one method.
//   - Sec 8.6 / 13.4: "shifter" is a non-virtual public method Function
//     with automatic lifetime, returning void, with no formals.
//   - Sec 12.7.1: the body is a single for-loop that declares its own loop
//     variables: "int shft_idx" initialized to 0 and "bit c" initialized to
//     1'b1, in that order.
//   - Sec 12.7.1: the condition is "shft_idx < n_bits", where "shft_idx"
//     binds to the loop variable and "n_bits" to the class property; the
//     only step is "shft_idx++" (post-increment) on the loop variable.
//   - Sec 10.4.1 / 11.4.10 / 11.4.12: the begin-end body holds exactly the
//     blocking assignment "dout = {dout} << 1", and both "dout" references
//     bind to the class property.
//   - Sec 23.2: module "test" exists.
//   - The file is legal: no fatal, syntax or error diagnostics.
//
// Not checked:
//   - Whether loop-variable declarations are modelled as Assignments or as
//     initialized Variables (both shapes are accepted, as in other suite
//     tests).
//   - Whether the single for-loop is wrapped in an implicit begin (both
//     shapes are accepted).
//   - vpiFullName on any object (never asserted per the test writing guide).

#include <hlc/Tests/Test.h>

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_typespec.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/for_stmt.h>
#include <hldb/function.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/void_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {
class ClassMemberTest31Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_member_test_31.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *getMyClass() {
    return hldb::findByName<hldb::ClassDefn>("myclass", m_design->getAllClasses());
  }

  static const hldb::Variable *getProperty(std::string_view name) {
    const hldb::ClassDefn *const cls = getMyClass();
    if (cls == nullptr || cls->getVariables() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>(name, cls->getVariables());
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

  // The loop variable declared by the i-th initializer, whether modelled as
  // an initialized Variable or as an Assignment whose left side is (or
  // binds to) that Variable.
  static const hldb::Variable *getLoopVar(size_t index) {
    const hldb::ForStmt *const loop = getForLoop();
    if (loop == nullptr || loop->getForInitStmts() == nullptr || loop->getForInitStmts()->size() <= index) {
      return nullptr;
    }
    const hldb::Any *const init = loop->getForInitStmts()->at(index);
    if (const hldb::Variable *const var = any_cast<hldb::Variable>(init)) {
      return var;
    }
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(init);
    if (assign == nullptr) {
      return nullptr;
    }
    if (const hldb::Variable *const var = assign->getLhs<hldb::Variable>()) {
      return var;
    }
    const hldb::RefObj *const ref = assign->getLhs<hldb::RefObj>();
    if (ref == nullptr) {
      return nullptr;
    }
    return ref->getActual<hldb::Variable>();
  }

  // The value given by the i-th initializer, in either modelled shape.
  static const hldb::Constant *getLoopInitValue(size_t index) {
    const hldb::ForStmt *const loop = getForLoop();
    if (loop == nullptr || loop->getForInitStmts() == nullptr || loop->getForInitStmts()->size() <= index) {
      return nullptr;
    }
    const hldb::Any *const init = loop->getForInitStmts()->at(index);
    if (const hldb::Assignment *const assign = any_cast<hldb::Assignment>(init)) {
      return assign->getRhs<hldb::Constant>();
    }
    const hldb::Variable *const var = any_cast<hldb::Variable>(init);
    if (var == nullptr) {
      return nullptr;
    }
    if (var->getExpr() != nullptr) {
      return any_cast<hldb::Constant>(var->getExpr());
    }
    return var->getValue<hldb::Constant>();
  }

  // The declaration a reference expression binds to.
  static const hldb::Any *bindingOf(const hldb::Any *const expr) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
    if (ref == nullptr) {
      return nullptr;
    }
    return ref->getActual();
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
};

TEST_F(ClassMemberTest31Test, MyclassExists) {
  const hldb::ClassDefn *const cls = getMyClass();
  ASSERT_NE(cls, nullptr) << "'class myclass;' should create a ClassDefn";
  EXPECT_FALSE(cls->getVirtual()) << "'myclass' is not declared virtual";
  EXPECT_EQ(cls->getExtends(), nullptr) << "'myclass' has no extends clause";
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 2u) << "'myclass' declares exactly two properties";
  ASSERT_NE(cls->getMethods(), nullptr);
  EXPECT_EQ(cls->getMethods()->size(), 1u) << "'myclass' defines exactly one method, 'shifter'";
}

TEST_F(ClassMemberTest31Test, PropertiesDoutAndNBitsAreInt) {
  for (std::string_view name : {"dout", "n_bits"}) {
    const hldb::Variable *const prop = getProperty(name);
    ASSERT_NE(prop, nullptr) << "property '" << name << "' should exist";
    ASSERT_NE(prop->getTypespec(), nullptr);
    const hldb::IntTypespec *const intTs = prop->getTypespec()->getActual<hldb::IntTypespec>();
    ASSERT_NE(intTs, nullptr) << "'int " << name << ";' should resolve to IntTypespec";
    EXPECT_TRUE(intTs->getSigned()) << "Sec 6.11: int is signed by default";
  }
}

TEST_F(ClassMemberTest31Test, ShifterIsVoidMethodWithNoFormals) {
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

TEST_F(ClassMemberTest31Test, ForDeclaresTwoLoopVariables) {
  const hldb::ForStmt *const loop = getForLoop();
  ASSERT_NE(loop, nullptr) << "the body of 'shifter' should be a single for-loop";
  EXPECT_TRUE(loop->getLocalVarDecls()) << "Sec 12.7.1: the loop header declares its own variables";
  ASSERT_NE(loop->getForInitStmts(), nullptr);
  EXPECT_EQ(loop->getForInitStmts()->size(), 2u) << "the header declares two loop variables";
}

TEST_F(ClassMemberTest31Test, LoopVarShftIdxIsIntInitializedTo0) {
  const hldb::Variable *const shftIdx = getLoopVar(0);
  ASSERT_NE(shftIdx, nullptr) << "the first initializer declares a loop variable";
  EXPECT_EQ(shftIdx->getName(), "shft_idx");
  ASSERT_NE(shftIdx->getTypespec(), nullptr);
  EXPECT_NE(shftIdx->getTypespec()->getActual<hldb::IntTypespec>(), nullptr) << "'int shft_idx' is an int";
  const hldb::Constant *const value = getLoopInitValue(0);
  ASSERT_NE(value, nullptr) << "'shft_idx' is initialized by a constant";
  EXPECT_EQ(value->getDecompile(), "0");
}

TEST_F(ClassMemberTest31Test, LoopVarCIsBitInitializedTo1b1) {
  const hldb::Variable *const c = getLoopVar(1);
  ASSERT_NE(c, nullptr) << "the second initializer declares a loop variable";
  EXPECT_EQ(c->getName(), "c");
  ASSERT_NE(c->getTypespec(), nullptr);
  EXPECT_NE(c->getTypespec()->getActual<hldb::BitTypespec>(), nullptr) << "'bit c' is a bit";
  const hldb::Constant *const value = getLoopInitValue(1);
  ASSERT_NE(value, nullptr) << "'c' is initialized by a constant";
  EXPECT_EQ(value->getDecompile(), "1'b1");
}

TEST_F(ClassMemberTest31Test, ForConditionBindsLoopVarAndProperty) {
  const hldb::ForStmt *const loop = getForLoop();
  ASSERT_NE(loop, nullptr);
  const hldb::Operation *const cond = loop->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "'shft_idx < n_bits' should be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiLtOp) << "the condition uses binary less-than";
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  EXPECT_EQ(cond->getOperands()->at(0)->getName(), "shft_idx");
  EXPECT_EQ(cond->getOperands()->at(1)->getName(), "n_bits");
  const hldb::Variable *const shftIdx = getLoopVar(0);
  ASSERT_NE(shftIdx, nullptr);
  ASSERT_NE(getProperty("n_bits"), nullptr);
  EXPECT_EQ(bindingOf(cond->getOperands()->at(0)), shftIdx)
      << "Sec 12.7.1: 'shft_idx' binds to the loop variable declared in the header";
  EXPECT_EQ(bindingOf(cond->getOperands()->at(1)), getProperty("n_bits"))
      << "Sec 8.3: 'n_bits' binds to the class property";
}

TEST_F(ClassMemberTest31Test, ForStepIsPostIncrementOfLoopVar) {
  const hldb::ForStmt *const loop = getForLoop();
  ASSERT_NE(loop, nullptr);
  ASSERT_NE(loop->getForIncStmts(), nullptr) << "'shft_idx++' is the loop step";
  ASSERT_EQ(loop->getForIncStmts()->size(), 1u) << "the loop has exactly one step expression";
  const hldb::Operation *const inc = any_cast<hldb::Operation>(loop->getForIncStmts()->at(0));
  ASSERT_NE(inc, nullptr) << "'shft_idx++' should be an Operation";
  EXPECT_EQ(inc->getOpType(), vpiPostIncOp) << "Sec 11.4.2: '++' after the operand is post-increment";
  ASSERT_NE(inc->getOperands(), nullptr);
  ASSERT_EQ(inc->getOperands()->size(), 1u);
  EXPECT_EQ(inc->getOperands()->at(0)->getName(), "shft_idx");
  ASSERT_NE(getLoopVar(0), nullptr);
  EXPECT_EQ(bindingOf(inc->getOperands()->at(0)), getLoopVar(0)) << "Sec 12.7.1: the step increments the loop variable";
}

TEST_F(ClassMemberTest31Test, ForBodyAssignsDoutShiftedLeft) {
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
  EXPECT_EQ(assign->getLhs()->getName(), "dout");

  const hldb::Operation *const shift = assign->getRhs<hldb::Operation>();
  ASSERT_NE(shift, nullptr) << "'{dout} << 1' should be an Operation";
  EXPECT_EQ(shift->getOpType(), vpiLShiftOp) << "Sec 11.4.10: '<<' is a logical left shift";
  ASSERT_NE(shift->getOperands(), nullptr);
  ASSERT_EQ(shift->getOperands()->size(), 2u);
  const hldb::Operation *const concat = any_cast<hldb::Operation>(shift->getOperands()->at(0));
  ASSERT_NE(concat, nullptr) << "'{dout}' should be an Operation";
  EXPECT_EQ(concat->getOpType(), vpiConcatOp) << "Sec 11.4.12: '{dout}' is a concatenation";
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 1u);
  EXPECT_EQ(concat->getOperands()->at(0)->getName(), "dout");
  const hldb::Constant *const amount = any_cast<hldb::Constant>(shift->getOperands()->at(1));
  ASSERT_NE(amount, nullptr) << "the shift amount '1' is a constant";
  EXPECT_EQ(amount->getDecompile(), "1");
}

TEST_F(ClassMemberTest31Test, BodyDoutReferencesBindToProperty) {
  const hldb::Assignment *const assign = getBodyAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::Variable *const dout = getProperty("dout");
  ASSERT_NE(dout, nullptr);
  EXPECT_EQ(bindingOf(assign->getLhs()), dout) << "Sec 8.3: the target 'dout' binds to the class property";
  const hldb::Operation *const shift = assign->getRhs<hldb::Operation>();
  ASSERT_NE(shift, nullptr);
  ASSERT_NE(shift->getOperands(), nullptr);
  ASSERT_FALSE(shift->getOperands()->empty());
  const hldb::Operation *const concat = any_cast<hldb::Operation>(shift->getOperands()->at(0));
  ASSERT_NE(concat, nullptr);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_FALSE(concat->getOperands()->empty());
  EXPECT_EQ(bindingOf(concat->getOperands()->at(0)), dout) << "Sec 8.3: '{dout}' binds to the class property";
}

TEST_F(ClassMemberTest31Test, ModuleTestExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("test", m_design->getAllModules()), nullptr)
      << "'module test; endmodule' should create a Module";
}

TEST_F(ClassMemberTest31Test, CompilesWithNoErrors) {
  const hlc::EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0) << "the source is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
