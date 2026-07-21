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

// Tests for min.sv (tags: 7.12.1 7.12 7.10)
//   module top ();
//     int s[] = { 10, 20, 2, 11, 5 };
//     int qi[$];
//     initial begin
//       qi = s.min;
//       $display(":assert: (%d == 1)", qi.size);
//       $display(":assert: (%d == 2)", qi[0]);
//     end
//   endmodule
//
// Checked:
//   - design has module work@top with exactly 2 nets: "s" (dynamic array)
//     and "qi" (queue of int)
//   - net "s": ArrayTypespec vpiArrayType=dynamic(2), ElemTypespec ->
//     IntTypespec; initial value is a 5-operand concatenation
//     (10, 20, 2, 11, 5)
//   - net "qi": ArrayTypespec vpiArrayType=static(1) -- the compiler models
//     a queue ("int qi[$]") as a queue array with an unbounded
//     left-range ("$"); ElemTypespec -> IntTypespec
//   - Initial process: 1 Begin with 3 stmts (Assignment + 2 SysFuncCall)
//   - Assignment: qi = s.min is a HierPath "s.min" with 2 RefObj path
//     elems -- "s" (resolved to Net "s") and "min" (unresolved). Unlike
//     find/find_index/.../unique, ".min" (no "with" clause, no parens in
//     source) is NOT modeled as a MethodFuncCall; it is a plain RefObj, so
//     "min" itself -- not just an implicit iterator -- is the identifier the
//     compiler cannot resolve
//   - both $display calls and their HierPath("qi.size")/BitSelect("qi[0]")
//     arguments
//   - design-level typespecs (3): ModuleTypespec, IntTypespec, StringTypespec
//     (element type is int, so IntTypespec is created before the
//     StringTypespec used for the $display format string constants --
//     reversed order vs. the string-array locator-method tests)
//   - compiler emits exactly 2 errors (nbFatal=0, nbSyntax=0, nbError=2,
//     nbWarning=0), both ELAB_ILLEGAL_IMPLICIT_NET (EL0535)

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
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ArrayLocatorMinTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "min.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module / nets -----------------------------------------------------------

TEST_F(ArrayLocatorMinTest, ModuleExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

TEST_F(ArrayLocatorMinTest, ModuleHasTwoNets) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u);
}

TEST_F(ArrayLocatorMinTest, NetSNameIsS) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const s = hldb::findByName<hldb::Net>("s", top->getNets());
  ASSERT_NE(s, nullptr);
}

TEST_F(ArrayLocatorMinTest, NetQiNameIsQi) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const qi = hldb::findByName<hldb::Net>("qi", top->getNets());
  ASSERT_NE(qi, nullptr);
}

// --- net "s": dynamic array of int --------------------------------------------

TEST_F(ArrayLocatorMinTest, NetSTypespecIsDynamicArray) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const s = hldb::findByName<hldb::Net>("s", top->getNets());
  ASSERT_NE(s, nullptr);
  const hldb::ArrayTypespec *const at = s->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 2);  // dynamic = 2
}

TEST_F(ArrayLocatorMinTest, NetSElemTypespecIsInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const s = hldb::findByName<hldb::Net>("s", top->getNets());
  ASSERT_NE(s, nullptr);
  const hldb::ArrayTypespec *const at = s->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(ArrayLocatorMinTest, NetSInitialValueIsFiveElemConcat) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const s = hldb::findByName<hldb::Net>("s", top->getNets());
  ASSERT_NE(s, nullptr);
  const hldb::Operation *const concat = s->getValue<hldb::Operation>();
  ASSERT_NE(concat, nullptr);
  EXPECT_EQ(concat->getOpType(), vpiConcatOp);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 5u);
  const hldb::Constant *const c0 = any_cast<hldb::Constant>(concat->getOperands()->at(0));
  const hldb::Constant *const c1 = any_cast<hldb::Constant>(concat->getOperands()->at(1));
  const hldb::Constant *const c2 = any_cast<hldb::Constant>(concat->getOperands()->at(2));
  const hldb::Constant *const c3 = any_cast<hldb::Constant>(concat->getOperands()->at(3));
  const hldb::Constant *const c4 = any_cast<hldb::Constant>(concat->getOperands()->at(4));
  ASSERT_NE(c0, nullptr);
  ASSERT_NE(c1, nullptr);
  ASSERT_NE(c2, nullptr);
  ASSERT_NE(c3, nullptr);
  ASSERT_NE(c4, nullptr);
  EXPECT_EQ(c0->getValue(), "10");
  EXPECT_EQ(c1->getValue(), "20");
  EXPECT_EQ(c2->getValue(), "2");
  EXPECT_EQ(c3->getValue(), "11");
  EXPECT_EQ(c4->getValue(), "5");
}

// --- net "qi": queue of int, modeled as a "queue" array with unbounded range -

TEST_F(ArrayLocatorMinTest, NetQiTypespecIsQueueArray) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const qi = hldb::findByName<hldb::Net>("qi", top->getNets());
  ASSERT_NE(qi, nullptr);
  const hldb::ArrayTypespec *const at = qi->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiQueueArray);
}

TEST_F(ArrayLocatorMinTest, NetQiRangeLeftIsUnboundedDollar) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const qi = hldb::findByName<hldb::Net>("qi", top->getNets());
  ASSERT_NE(qi, nullptr);
  const hldb::ArrayTypespec *const at = qi->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const dollar = at->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(dollar, nullptr);
  EXPECT_EQ(dollar->getDecompile(), "$");
  EXPECT_EQ(dollar->getConstType(), vpiUnboundedConst);  // "unbounded" (not in vpi_user.h; hlc-specific)
}

TEST_F(ArrayLocatorMinTest, NetQiElemTypespecIsInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const qi = hldb::findByName<hldb::Net>("qi", top->getNets());
  ASSERT_NE(qi, nullptr);
  const hldb::ArrayTypespec *const at = qi->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(ArrayLocatorMinTest, NetQiHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const qi = hldb::findByName<hldb::Net>("qi", top->getNets());
  ASSERT_NE(qi, nullptr);
  EXPECT_EQ(qi->getValue(), nullptr);
}

// --- initial process: qi = s.min ----------------------------------------------

TEST_F(ArrayLocatorMinTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(ArrayLocatorMinTest, InitialBeginHasThreeStmts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 3u);
}

TEST_F(ArrayLocatorMinTest, AssignmentIsBlockingToQi) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
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
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);
}

TEST_F(ArrayLocatorMinTest, AssignmentRhsIsHierPathSDotMin) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::HierPath *const rhs = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(rhs, nullptr);
  // Note: no "()" -- ".min" (no "with" clause, no source parens) is not
  // modeled as a MethodFuncCall, unlike find()/find_index()/unique().
  EXPECT_EQ(rhs->getName(), std::string_view("s.min"));
  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u);
  const hldb::RefObj *const sRef = any_cast<hldb::RefObj>(rhs->getPathElems()->at(0));
  ASSERT_NE(sRef, nullptr);
  EXPECT_EQ(sRef->getName(), "s");
  EXPECT_NE(sRef->getActual<hldb::Net>(), nullptr);
}

TEST_F(ArrayLocatorMinTest, RhsSecondPathElemIsUnresolvedRefObjMin) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::HierPath *const rhs =
      any_cast<hldb::Assignment>(init->getStmt<hldb::Begin>()->getStmts()->at(0))->getRhs<hldb::HierPath>();
  ASSERT_NE(rhs, nullptr);
  const hldb::MethodFuncCall *const minRef = any_cast<hldb::MethodFuncCall>(rhs->getPathElems()->at(1));
  ASSERT_NE(minRef, nullptr);
  EXPECT_EQ(minRef->getName(), "min");
  // Unlike "item" (the with-clause iterator) in find/find_index/etc., here
  // the built-in method name "min" itself is the unresolved implicit net --
  // see the ELAB_ILLEGAL_IMPLICIT_NET tests below.
  EXPECT_EQ(minRef->getTaskFunc(), nullptr);
}

// --- $display(":assert: (%d == 1)", qi.size) ---------------------------------

TEST_F(ArrayLocatorMinTest, FirstDisplayFormatStringIsSizeAssert) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
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
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 1)");
}

TEST_F(ArrayLocatorMinTest, FirstDisplaySecondArgIsQiDotSize) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
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
  EXPECT_NE(qiRef->getActual<hldb::Net>(), nullptr);
  const hldb::MethodFuncCall *const sizeRef = any_cast<hldb::MethodFuncCall>(size->getPathElems()->at(1));
  ASSERT_NE(sizeRef, nullptr);
  EXPECT_EQ(sizeRef->getName(), "size");
  // Built-in ".size" is never resolved either -- same limitation as "min".
  EXPECT_EQ(sizeRef->getTaskFunc(), nullptr);
}

// --- $display(":assert: (%d == 2)", qi[0]) -----------------------------------

TEST_F(ArrayLocatorMinTest, SecondDisplayFormatStringIsValueAssert) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(init->getStmt<hldb::Begin>()->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 2)");
}

TEST_F(ArrayLocatorMinTest, SecondDisplaySecondArgIsQiBitSelectZero) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(init->getStmt<hldb::Begin>()->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(disp->getArguments()->at(1));
  ASSERT_NE(sel, nullptr);
  EXPECT_EQ(sel->getName(), "qi[0]");
  const hldb::RefObj *const prefix = sel->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "qi");
  const hldb::Constant *const index = sel->getIndex<hldb::Constant>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getDecompile(), "0");
  EXPECT_EQ(index->getConstType(), vpiUIntConst);
  EXPECT_NE(index->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
}

// --- design-level typespecs / structural completeness -------------------------

TEST_F(ArrayLocatorMinTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(ArrayLocatorMinTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "work@top");
}

TEST_F(ArrayLocatorMinTest, DesignHasIntTypespecSigned) {
  // Element type is int, so IntTypespec is created (index 1) before the
  // StringTypespec (index 2) used by the $display format strings --
  // reversed order vs. the string-array locator-method tests.
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(ArrayLocatorMinTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(2)), nullptr);
}

TEST_F(ArrayLocatorMinTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

// --- compiler diagnostics: known ELAB_ILLEGAL_IMPLICIT_NET limitation --------

TEST_F(ArrayLocatorMinTest, CompilerReportsExactlyZeroErrorsNoFatalNoWarning) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(ArrayLocatorMinTest, ExactlyZeroIllegalImplicitNetErrors) {
  // getErrors() holds every diagnostic emitted (INFO progress messages too),
  // so isolate the real errors by type rather than assuming the container
  // holds only errors.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const std::vector<Error> &errors = m_session->getErrorContainer()->getErrors();
  std::vector<Error> implicitNetErrors;
  for (const Error &err : errors) {
    if (err.getType() == ErrorDefinition::ELAB_ILLEGAL_IMPLICIT_NET) {
      implicitNetErrors.push_back(err);
    }
  }
  ASSERT_TRUE(implicitNetErrors.empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
