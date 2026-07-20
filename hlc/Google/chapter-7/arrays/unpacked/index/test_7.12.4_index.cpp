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

// Tests for index.sv (tags: 7.12.4 7.4.2 7.10 7.12.1)
//   module top ();
//     int arr[] = { 0, 1, 3, 3 };
//     int q[$];
//     initial begin
//       q = arr.find with ( item == item.index );
//       $display(":assert: ((%d == 3) and (%d == 0) and (%d == 1) and (%d == 3))",
//         q.size, q[0], q[1], q[2]);
//     end
//   endmodule
//
// Checked:
//   - design has module work@top with exactly 2 nets: "arr", "q"
//   - net "arr": RefTypespec -> ArrayTypespec vpiArrayType=dynamic(2), elem
//     -> IntTypespec (signed); initial value stored directly on the Net as
//     an Operation (vpiOpType=concatenation(33)) with 4 unsigned Constant
//     operands 0, 1, 3, 3 (the '{0,1,3,3}' literal)
//   - net "q": RefTypespec -> ArrayTypespec vpiArrayType=queue(4), elem ->
//     IntTypespec (signed); range left bound is an unbounded Constant "$"
//   - Initial process: 1 Begin with 2 stmts
//   - Stmt[0]: q = arr.find with (item == item.index) -- blocking
//     Assignment, lhs RefObj "q" resolving Net "q", rhs HierPath
//     "arr.find()" with 2 path elems: RefObj "arr" (resolving Net "arr")
//     and MethodFuncCall "find" whose vpiWith is an Operation
//     (vpiOpType=equal(14)) with 2 operands: RefObj "item" and HierPath
//     "item.index" (path elems RefObj "item" + RefObj "index")
//   - COMPILER BEHAVIOR: the iterator identifiers "item" (used twice) and
//     "index" are never resolved to any implicit iterator-argument
//     declaration -- all 3 RefObj occurrences have getActual()==nullptr,
//     and HLC raises ELAB_ILLEGAL_IMPLICIT_NET for each of the 3
//     occurrences (at 22:22, 22:30, 22:35)
//   - Stmt[1]: $display with 5 args -- format string + HierPath "q.size()"
//     (2 path elems: RefObj "q" resolving Net "q", MethodFuncCall "size"
//     with no arguments) + BitSelect q[0], q[1], q[2] (each prefix RefObj
//     "q" resolving Net "q")
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (signed),
//     StringTypespec
//   - compiler emits exactly 3 errors (nbFatal=0, nbSyntax=0, nbError=3,
//     nbWarning=0), all 3 ELAB_ILLEGAL_IMPLICIT_NET
//   - no continuous assignments
//
// Not checked:
//   - RefObj "item"/"index" getActual() -- always null, this IS the
//     compiler limitation being documented, not a gap in test coverage
//     (see the skipped canary
//     ItemAndIndexShouldResolveOnceImplicitNetBugIsFixed below)
//   - actual runtime contents of q after arr.find(...) -- simulation-only
//     (see the skipped canary RuntimeFindResultsRequireSimulation below)
//
// Compiler limitation (NOT a code error in index.sv):
//   IEEE 1800-2017 7.12.1/7.12.4 defines "item" (or the array method's
//   declared iterator name) and, for "with" expression clauses using
//   ".index", the implicit iterator-index variable as legal within an
//   array-locator method's "with" clause -- they are not meant to resolve
//   to module-level declarations. This HLC build's compile/elaborate-only
//   pass does not special-case these iterator identifiers and instead
//   treats them as ordinary implicit nets, rejecting each occurrence with
//   ELAB_ILLEGAL_IMPLICIT_NET.

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
#include <hldb/method_func_call.h>
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

class UnpackedIndexTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "index.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module / nets ------------------------------------------------------------

TEST_F(UnpackedIndexTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(UnpackedIndexTest, ModuleHasTwoNets) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u);
}

TEST_F(UnpackedIndexTest, NetArrIsDynamicArrayOfSignedInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const arr = hldb::findByName<hldb::Net>("arr", top->getNets());
  ASSERT_NE(arr, nullptr);
  const hldb::ArrayTypespec *const at = arr->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 2);  // dynamic = 2
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr);
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(UnpackedIndexTest, NetArrInitialValueIsConcatenationOfZeroOneThreeThree) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const arr = hldb::findByName<hldb::Net>("arr", top->getNets());
  ASSERT_NE(arr, nullptr);
  const hldb::Operation *const init = arr->getValue<hldb::Operation>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getOpType(), vpiConcatOp);
  ASSERT_NE(init->getOperands(), nullptr);
  ASSERT_EQ(init->getOperands()->size(), 4u);
  const std::string expected[4] = {"0", "1", "3", "3"};
  for (uint32_t i = 0; i < 4u; ++i) {
    EXPECT_EQ(any_cast<hldb::Constant>(init->getOperands()->at(i))->getDecompile(), expected[i]) << "operand " << i;
  }
}

TEST_F(UnpackedIndexTest, NetQIsQueueOfSignedIntWithUnboundedRange) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const q = hldb::findByName<hldb::Net>("q", top->getNets());
  ASSERT_NE(q, nullptr);
  const hldb::ArrayTypespec *const at = q->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 4);  // queue = 4
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const dollar = at->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(dollar, nullptr);
  EXPECT_EQ(dollar->getDecompile(), "$");
  EXPECT_EQ(dollar->getConstType(), vpiUnboundedConst);
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr);
  EXPECT_TRUE(elem->getSigned());
}

// --- initial process ---------------------------------------------------------

TEST_F(UnpackedIndexTest, InitialBeginHasTwoStmts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 2u);
}

TEST_F(UnpackedIndexTest, FirstStmtAssignsQFromArrFindWithClause) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "q");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);

  const hldb::HierPath *const hp = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);
  const hldb::RefObj *const arrRef = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(arrRef, nullptr);
  EXPECT_EQ(arrRef->getName(), "arr");
  EXPECT_NE(arrRef->getActual<hldb::Net>(), nullptr);

  const hldb::MethodFuncCall *const find = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(find, nullptr);
  EXPECT_EQ(find->getName(), "find");
  const hldb::Operation *const with = find->getWith<hldb::Operation>();
  ASSERT_NE(with, nullptr);
  EXPECT_EQ(with->getOpType(), vpiEqOp);
  ASSERT_NE(with->getOperands(), nullptr);
  ASSERT_EQ(with->getOperands()->size(), 2u);
}

TEST_F(UnpackedIndexTest, WithClauseFirstOperandIsUnresolvedItemRefObj) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::HierPath *const hp = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  const hldb::MethodFuncCall *const find = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(find, nullptr);
  const hldb::Operation *const with = find->getWith<hldb::Operation>();
  ASSERT_NE(with, nullptr);
  const hldb::RefObj *const item = any_cast<hldb::RefObj>(with->getOperands()->at(0));
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->getName(), "item");
}

TEST_F(UnpackedIndexTest, WithClauseSecondOperandIsItemDotIndexHierPath) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::HierPath *const outer = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(outer, nullptr);
  const hldb::MethodFuncCall *const find = any_cast<hldb::MethodFuncCall>(outer->getPathElems()->at(1));
  ASSERT_NE(find, nullptr);
  const hldb::Operation *const with = find->getWith<hldb::Operation>();
  ASSERT_NE(with, nullptr);
  const hldb::HierPath *const itemIndex = any_cast<hldb::HierPath>(with->getOperands()->at(1));
  ASSERT_NE(itemIndex, nullptr);
  EXPECT_EQ(itemIndex->getName(), "item.index");
  ASSERT_NE(itemIndex->getPathElems(), nullptr);
  ASSERT_EQ(itemIndex->getPathElems()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(itemIndex->getPathElems()->at(0))->getName(), "item");
  EXPECT_EQ(any_cast<hldb::RefObj>(itemIndex->getPathElems()->at(1))->getName(), "index");
}

TEST_F(UnpackedIndexTest, SecondStmtDisplaysSizeAndThreeElements) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(1));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 5u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: ((%d == 3) and (%d == 0) and (%d == 1) and (%d == 3))");

  const hldb::HierPath *const size = any_cast<hldb::HierPath>(disp->getArguments()->at(1));
  ASSERT_NE(size, nullptr);
  EXPECT_EQ(size->getName(), "q.size");
  ASSERT_NE(size->getPathElems(), nullptr);
  ASSERT_EQ(size->getPathElems()->size(), 2u);
  EXPECT_NE(any_cast<hldb::RefObj>(size->getPathElems()->at(0))->getActual<hldb::Net>(), nullptr);
  const hldb::MethodFuncCall *const sizeCall = any_cast<hldb::MethodFuncCall>(size->getPathElems()->at(1));
  ASSERT_NE(sizeCall, nullptr);
  EXPECT_EQ(sizeCall->getName(), "size");
  EXPECT_EQ(sizeCall->getArguments(), nullptr);

  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(disp->getArguments()->at(i + 2));
    ASSERT_NE(sel, nullptr) << "argument " << (i + 2);
    EXPECT_EQ(sel->getPrefix<hldb::RefObj>()->getName(), "q");
    EXPECT_EQ(sel->getIndex<hldb::Constant>()->getDecompile(), std::to_string(i));
  }
}

// --- design-level typespecs / compiler diagnostics ---------------------------

TEST_F(UnpackedIndexTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(UnpackedIndexTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "work@top");
}

TEST_F(UnpackedIndexTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(2)), nullptr);
}

TEST_F(UnpackedIndexTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

// --- compiler diagnostics: known ELAB_ILLEGAL_IMPLICIT_NET limitation --------

TEST_F(UnpackedIndexTest, CompilerReportsExactlyThreeErrorsNoFatalNoWarning) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 3);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(UnpackedIndexTest, ExactlyThreeIllegalImplicitNetErrorsAtExpectedLocations) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const std::vector<Error> &errors = m_session->getErrorContainer()->getErrors();
  std::vector<Error> implicitNetErrors;
  for (const Error &err : errors) {
    if (err.getType() == ErrorDefinition::ELAB_ILLEGAL_IMPLICIT_NET) {
      implicitNetErrors.push_back(err);
    }
  }
  ASSERT_EQ(implicitNetErrors.size(), 3u);
  const uint32_t expectedCols[3] = {22u, 30u, 35u};
  for (uint32_t i = 0; i < 3u; ++i) {
    ASSERT_FALSE(implicitNetErrors[i].getLocations().empty()) << "error " << i;
    EXPECT_EQ(implicitNetErrors[i].getLocations()[0].m_line, 22u) << "error " << i;
    EXPECT_EQ(implicitNetErrors[i].getLocations()[0].m_column, expectedCols[i]) << "error " << i;
  }
}

// --- known compiler limitation: expected to fail until EL0535 is fixed ------

// Not simulation-only: getActual() is a pure compile-time name-binding field
// (same kind as FuncCall::getTaskFunc<T>()), so no execution is required to
// populate it. It is expected to fail today -- see the "COMPILER BEHAVIOR"
// note above documenting that all 3 occurrences currently resolve to null.
TEST_F(UnpackedIndexTest, ItemAndIndexShouldResolveOnceImplicitNetBugIsFixed) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::HierPath *const hp = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  const hldb::MethodFuncCall *const find = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(find, nullptr);
  const hldb::Operation *const with = find->getWith<hldb::Operation>();
  ASSERT_NE(with, nullptr);
  EXPECT_NE(any_cast<hldb::RefObj>(with->getOperands()->at(0))->getActual(), nullptr)
      << "'item' should resolve to the implicit iterator argument";
  const hldb::HierPath *const itemIndex = any_cast<hldb::HierPath>(with->getOperands()->at(1));
  ASSERT_NE(itemIndex, nullptr);
  EXPECT_NE(any_cast<hldb::RefObj>(itemIndex->getPathElems()->at(0))->getActual(), nullptr)
      << "'item' (in item.index) should resolve";
  EXPECT_NE(any_cast<hldb::RefObj>(itemIndex->getPathElems()->at(1))->getActual(), nullptr)
      << "'index' should resolve to the implicit iterator-index method";
}

// --- known gap: runtime find() results require simulation --------------------

TEST_F(UnpackedIndexTest, RuntimeFindResultsRequireSimulation) {
  GTEST_SKIP() << "This harness only compiles/elaborates index.sv; it does not run a simulator, so "
                  "the actual runtime contents of q after 'arr.find with (...)' cannot be observed "
                  "here. index.sv's own $display format string documents the expected values.";

  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(1));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(),
            ":assert: ((%d == 3) and (%d == 0) and (%d == 1) and (%d == 3))")
      << "expected q.size==3, q[0]==0, q[1]==1, q[2]==3 -- the indices of arr where arr[i]==i";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
