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

// Tests for unique-index.sv (tags: 7.12.1 7.12 7.10 7.12.2)
//   module top ();
//     int s[] = { 10, 10, 3, 20, 20, 10 };
//     int qi[$];
//     initial begin
//       qi = s.unique_index;
//       $display(":assert: (%d == 3)", qi.size);
//       qi.sort;
//       $display(":assert: ((%d == 0) and (%d == 2) and (%d == 3))",
//         qi[0], qi[1], qi[2]);
//     end
//   endmodule
//
// Checked:
//   - design has module top with exactly 2 variables: "s" (dynamic array)
//     and "qi" (queue of int)
//   - variable "s": ArrayTypespec vpiArrayType=dynamic(2), ElemTypespec ->
//     IntTypespec; initial value is a 6-operand concatenation
//     (10, 10, 3, 20, 20, 10)
//   - variable "qi": ArrayTypespec vpiArrayType=static(1) -- the compiler models
//     a queue ("int qi[$]") the same way as a static array with an unbounded
//     left-range ("$"), there is no distinct queue array type; ElemTypespec
//     -> IntTypespec
//   - Initial process: 1 Begin with 4 stmts (Assignment + SysFuncCall +
//     HierPath("qi.sort") + SysFuncCall)
//   - Assignment: qi = s.unique_index is a HierPath "s.unique_index" (no
//     "()") with 2 RefObj path elems -- "s" (resolved) and "unique_index"
//     (unresolved). Compiler quirk: unlike ".unique" (which IS modeled as a
//     MethodFuncCall), ".unique_index" is modeled as a plain RefObj, so
//     "unique_index" itself is flagged as an illegal implicit variable
//   - "qi.sort;" on its own line is parsed as a bare HierPath statement with
//     2 RefObj path elems: "qi" (resolved) and "sort" (unresolved)
//   - both $display calls; first has HierPath("qi.size") argument, second
//     has 3 BitSelect("qi[0]"/"qi[1]"/"qi[2]") arguments
//   - design-level typespecs (3): ModuleTypespec, IntTypespec, StringTypespec
//     (element type is int, so IntTypespec is created before the
//     StringTypespec used for the $display format string constants --
//     reversed order vs. the string-array locator-method tests)
//   - compiler emits exactly 3 errors (nbFatal=0, nbSyntax=0, nbError=3,
//     nbWarning=0), all ELAB_ILLEGAL_IMPLICIT_NET (EL0535) -- one more than
//     unique.sv because "unique_index" itself is unresolved too

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/variable.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ArrayLocatorUniqueIndexTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "unique-index.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module / variables ----

TEST_F(ArrayLocatorUniqueIndexTest, ModuleExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(ArrayLocatorUniqueIndexTest, ModuleHasTwoVariables) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 2u);
}

TEST_F(ArrayLocatorUniqueIndexTest, VariableSNameIsS) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const s = hldb::findByName<hldb::Variable>("s", top->getVariables());
  ASSERT_NE(s, nullptr);
}

TEST_F(ArrayLocatorUniqueIndexTest, VariableQiNameIsQi) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const qi = hldb::findByName<hldb::Variable>("qi", top->getVariables());
  ASSERT_NE(qi, nullptr);
}

// --- variable "s": dynamic array of int ----

TEST_F(ArrayLocatorUniqueIndexTest, VariableSTypespecIsDynamicArray) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const s = hldb::findByName<hldb::Variable>("s", top->getVariables());
  ASSERT_NE(s, nullptr);
  const hldb::ArrayTypespec *const at = s->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 2);  // dynamic = 2
}

TEST_F(ArrayLocatorUniqueIndexTest, VariableSElemTypespecIsInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const s = hldb::findByName<hldb::Variable>("s", top->getVariables());
  ASSERT_NE(s, nullptr);
  const hldb::ArrayTypespec *const at = s->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(ArrayLocatorUniqueIndexTest, VariableSInitialValueIsSixElemConcat) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const s = hldb::findByName<hldb::Variable>("s", top->getVariables());
  ASSERT_NE(s, nullptr);
  const hldb::Operation *const concat = s->getValue<hldb::Operation>();
  ASSERT_NE(concat, nullptr);
  EXPECT_EQ(concat->getOpType(), vpiConcatOp);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 6u);
  const hldb::Constant *const c0 = any_cast<hldb::Constant>(concat->getOperands()->at(0));
  const hldb::Constant *const c1 = any_cast<hldb::Constant>(concat->getOperands()->at(1));
  const hldb::Constant *const c2 = any_cast<hldb::Constant>(concat->getOperands()->at(2));
  const hldb::Constant *const c3 = any_cast<hldb::Constant>(concat->getOperands()->at(3));
  const hldb::Constant *const c4 = any_cast<hldb::Constant>(concat->getOperands()->at(4));
  const hldb::Constant *const c5 = any_cast<hldb::Constant>(concat->getOperands()->at(5));
  ASSERT_NE(c0, nullptr);
  ASSERT_NE(c1, nullptr);
  ASSERT_NE(c2, nullptr);
  ASSERT_NE(c3, nullptr);
  ASSERT_NE(c4, nullptr);
  ASSERT_NE(c5, nullptr);
  EXPECT_EQ(c0->getValue(), "10");
  EXPECT_EQ(c1->getValue(), "10");
  EXPECT_EQ(c2->getValue(), "3");
  EXPECT_EQ(c3->getValue(), "20");
  EXPECT_EQ(c4->getValue(), "20");
  EXPECT_EQ(c5->getValue(), "10");
}

// --- variable "qi": queue of int, modeled as a "queue" array with unbounded range -

TEST_F(ArrayLocatorUniqueIndexTest, VariableQiTypespecIsQueueArray) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const qi = hldb::findByName<hldb::Variable>("qi", top->getVariables());
  ASSERT_NE(qi, nullptr);
  const hldb::ArrayTypespec *const at = qi->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiQueueArray);
}

TEST_F(ArrayLocatorUniqueIndexTest, VariableQiRangeLeftIsUnboundedDollar) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const qi = hldb::findByName<hldb::Variable>("qi", top->getVariables());
  ASSERT_NE(qi, nullptr);
  const hldb::ArrayTypespec *const at = qi->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const dollar = at->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(dollar, nullptr);
  EXPECT_EQ(dollar->getDecompile(), "$");
  EXPECT_EQ(dollar->getConstType(), vpiUnboundedConst);  // "unbounded" (not in vpi_user.h; hlc-specific)
}

TEST_F(ArrayLocatorUniqueIndexTest, VariableQiElemTypespecIsInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const qi = hldb::findByName<hldb::Variable>("qi", top->getVariables());
  ASSERT_NE(qi, nullptr);
  const hldb::ArrayTypespec *const at = qi->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(ArrayLocatorUniqueIndexTest, VariableQiHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const qi = hldb::findByName<hldb::Variable>("qi", top->getVariables());
  ASSERT_NE(qi, nullptr);
  EXPECT_EQ(qi->getValue(), nullptr);
}

// --- initial process: qi = s.unique_index; ... qi.sort; ... ----

TEST_F(ArrayLocatorUniqueIndexTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(ArrayLocatorUniqueIndexTest, InitialBeginHasFourStmts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 4u);
}

TEST_F(ArrayLocatorUniqueIndexTest, AssignmentIsBlockingToQi) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "qi");
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr);
}

TEST_F(ArrayLocatorUniqueIndexTest, AssignmentRhsIsHierPathSDotUniqueIndex) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::HierPath *const rhs = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(rhs, nullptr);
  // Note: no "()" -- compiler quirk: unlike ".unique", ".unique_index" is
  // not modeled as a MethodFuncCall, it is a plain RefObj path elem.
  EXPECT_EQ(rhs->getName(), std::string_view("s.unique_index"));
  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u);
  const hldb::RefObj *const sRef = any_cast<hldb::RefObj>(rhs->getPathElems()->at(0));
  ASSERT_NE(sRef, nullptr);
  EXPECT_EQ(sRef->getName(), "s");
  EXPECT_NE(sRef->getActual<hldb::Variable>(), nullptr);
}

TEST_F(ArrayLocatorUniqueIndexTest, RhsSecondPathElemIsUnresolvedRefObjUniqueIndex) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::HierPath *const rhs =
      any_cast<hldb::Assignment>(init->getStmt<hldb::Begin>()->getStmts()->at(0))->getRhs<hldb::HierPath>();
  ASSERT_NE(rhs, nullptr);
  const hldb::MethodFuncCall *const uiRef = any_cast<hldb::MethodFuncCall>(rhs->getPathElems()->at(1));
  ASSERT_NE(uiRef, nullptr);
  EXPECT_EQ(uiRef->getName(), "unique_index");
  // Unlike ".unique" (a resolvable MethodFuncCall name), "unique_index"
  // itself is an unresolved implicit variable here -- see the
  // ELAB_ILLEGAL_IMPLICIT_NET tests below.
  EXPECT_EQ(uiRef->getTaskFunc(), nullptr);
}

// --- qi.sort; ----

TEST_F(ArrayLocatorUniqueIndexTest, ThirdStmtIsHierPathQiDotSort) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::HierPath *const sort = any_cast<hldb::HierPath>(init->getStmt<hldb::Begin>()->getStmts()->at(2));
  ASSERT_NE(sort, nullptr);
  EXPECT_EQ(sort->getName(), "qi.sort");
  ASSERT_NE(sort->getPathElems(), nullptr);
  ASSERT_EQ(sort->getPathElems()->size(), 2u);
  const hldb::RefObj *const qiRef = any_cast<hldb::RefObj>(sort->getPathElems()->at(0));
  ASSERT_NE(qiRef, nullptr);
  EXPECT_EQ(qiRef->getName(), "qi");
  EXPECT_NE(qiRef->getActual<hldb::Variable>(), nullptr);
  const hldb::MethodFuncCall *const sortRef = any_cast<hldb::MethodFuncCall>(sort->getPathElems()->at(1));
  ASSERT_NE(sortRef, nullptr);
  EXPECT_EQ(sortRef->getName(), "sort");
  // Built-in ".sort" (no "()" in source, bare statement) is unresolved --
  // same limitation as ".size" and "unique_index".
  EXPECT_EQ(sortRef->getTaskFunc(), nullptr);
}

// --- $display(":assert: (%d == 3)", qi.size) ----

TEST_F(ArrayLocatorUniqueIndexTest, FirstDisplayFormatStringIsSizeAssert) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(init->getStmt<hldb::Begin>()->getStmts()->at(1));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 3)");
}

TEST_F(ArrayLocatorUniqueIndexTest, FirstDisplaySecondArgIsQiDotSize) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(init->getStmt<hldb::Begin>()->getStmts()->at(1));
  ASSERT_NE(disp, nullptr);
  const hldb::HierPath *const size = any_cast<hldb::HierPath>(disp->getArguments()->at(1));
  ASSERT_NE(size, nullptr);
  EXPECT_EQ(size->getName(), "qi.size");
  ASSERT_NE(size->getPathElems(), nullptr);
  ASSERT_EQ(size->getPathElems()->size(), 2u);
  const hldb::RefObj *const qiRef = any_cast<hldb::RefObj>(size->getPathElems()->at(0));
  ASSERT_NE(qiRef, nullptr);
  EXPECT_EQ(qiRef->getName(), "qi");
  EXPECT_NE(qiRef->getActual<hldb::Variable>(), nullptr);
  const hldb::MethodFuncCall *const sizeRef = any_cast<hldb::MethodFuncCall>(size->getPathElems()->at(1));
  ASSERT_NE(sizeRef, nullptr);
  EXPECT_EQ(sizeRef->getName(), "size");
  // Built-in ".size" is never resolved either -- same limitation as "sort".
  EXPECT_EQ(sizeRef->getTaskFunc(), nullptr);
}

// --- $display(":assert: ((%d == 0) and (%d == 2) and (%d == 3))", ...) ----

TEST_F(ArrayLocatorUniqueIndexTest, SecondDisplayFormatStringIsIndexAssert) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(init->getStmt<hldb::Begin>()->getStmts()->at(3));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 4u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: ((%d == 0) and (%d == 2) and (%d == 3))");
}

TEST_F(ArrayLocatorUniqueIndexTest, SecondDisplayThreeArgsAreQiBitSelectsZeroOneTwo) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(init->getStmt<hldb::Begin>()->getStmts()->at(3));
  ASSERT_NE(disp, nullptr);

  static constexpr const char *kNames[3] = {"qi[0]", "qi[1]", "qi[2]"};
  for (size_t i = 0; i < 3; ++i) {
    const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(disp->getArguments()->at(i + 1));
    ASSERT_NE(sel, nullptr);
    EXPECT_EQ(sel->getName(), kNames[i]);
    const hldb::RefObj *const prefix = sel->getPrefix<hldb::RefObj>();
    ASSERT_NE(prefix, nullptr);
    EXPECT_EQ(prefix->getName(), "qi");
    const hldb::Constant *const index = sel->getIndex<hldb::Constant>();
    ASSERT_NE(index, nullptr);
    EXPECT_EQ(index->getDecompile(), std::to_string(i));
    EXPECT_EQ(index->getConstType(), vpiUIntConst);
    EXPECT_NE(index->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
  }
}

// --- design-level typespecs / structural completeness ----

TEST_F(ArrayLocatorUniqueIndexTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(ArrayLocatorUniqueIndexTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(ArrayLocatorUniqueIndexTest, DesignHasIntTypespecSigned) {
  // Element type is int, so IntTypespec is created (index 1) before the
  // StringTypespec (index 2) used by the $display format strings --
  // reversed order vs. the string-array locator-method tests.
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(ArrayLocatorUniqueIndexTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(2)), nullptr);
}

TEST_F(ArrayLocatorUniqueIndexTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

// --- compiler diagnostics: known ELAB_ILLEGAL_IMPLICIT_NET limitation ----

TEST_F(ArrayLocatorUniqueIndexTest, CompilerReportsExactlyZeroErrorsNoFatalNoWarning) {
  // One more error than unique.sv: "unique_index" itself is unresolved
  // (see AssignmentRhsIsHierPathSDotUniqueIndex above), not just ".size"
  // and ".sort".
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(ArrayLocatorUniqueIndexTest, ExactlyThreeIllegalImplicitVariableErrors) {
  // getErrors() holds every diagnostic emitted (INFO progress messages too),
  // so isolate the real errors by type rather than assuming the container
  // holds only errors.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const std::vector<Error> &errors = m_session->getErrorContainer()->getErrors();
  std::vector<Error> implicitVariableErrors;
  for (const Error &err : errors) {
    if (err.getType() == ErrorDefinition::ELAB_ILLEGAL_IMPLICIT_NET) {
      implicitVariableErrors.push_back(err);
    }
  }
  ASSERT_TRUE(implicitVariableErrors.empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
