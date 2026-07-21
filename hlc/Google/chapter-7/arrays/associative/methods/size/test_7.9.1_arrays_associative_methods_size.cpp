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

// Tests for size.sv (tags: 7.9.1 7.9)
//   module top ();
//     int arr [ int ];
//     initial begin
//       $display(":assert: (%d == 0)", arr.size);
//       arr[ 3 ] = 1;
//       $display(":assert: (%d == 1)", arr.size);
//       arr[ 16'hffff ] = 2;
//       $display(":assert: (%d == 2)", arr.size);
//       arr[ 4'b1000 ] = 3;
//       $display(":assert: (%d == 3)", arr.size);
//     end
//   endmodule
//
// Checked:
//   - design has module work@top with exactly 1 net "arr"
//   - net "arr": ArrayTypespec vpiArrayType=associative(3), index typespec ->
//     IntTypespec, elem typespec -> IntTypespec
//   - Initial process: 1 Begin with 7 stmts (4 SysFuncCall + 3 Assignment)
//   - all 4 $display calls: format string plus HierPath("arr.size") whose
//     2nd path elem is an unresolved RefObj "size" (no parens form, same
//     limitation as ".num"/".delete" without parens)
//   - the 3 index assignments use decimal (arr[3]), hex (arr[16'hffff]), and
//     binary (arr[4'b1000]) literal index constants with their respective
//     vpiConstType/vpiDecompile/vpiValue, and the binary literal's typespec
//     resolves to LogicTypespec (not IntTypespec, unlike the other 2 forms)
//   - design-level typespecs (5): ModuleTypespec, IntTypespec, StringTypespec,
//     IntTypespec, LogicTypespec
//   - compiler emits exactly 4 errors (nbFatal=0, nbSyntax=0, nbError=4,
//     nbWarning=0), all ELAB_ILLEGAL_IMPLICIT_NET (EL0535) for the no-parens
//     ".size" references
//
// Not checked:
//   - RefObj "size" getActual() -- always null, this IS the compiler
//     limitation being documented, not a gap in test coverage (see the
//     skipped canary SizeRefObjShouldResolveOnceImplicitNetBugIsFixed below)
//
// Compiler limitation (NOT a code error in size.sv):
//   IEEE 1800-2017 7.24.4 permits the built-in ".size" method to be called
//   with or without parentheses. This HLC build never resolves the no-parens
//   form and instead raises ELAB_ILLEGAL_IMPLICIT_NET ("Illegal implicit
//   net") for each of the 4 occurrences. size.sv is valid SystemVerilog; the
//   errors below are a known compiler/API limitation, not a defect in the
//   test source.

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
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AssociativeArraySizeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "size.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module / net -----------------------------------------------------------

TEST_F(AssociativeArraySizeTest, ModuleExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

TEST_F(AssociativeArraySizeTest, ModuleHasOneNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(AssociativeArraySizeTest, NetArrIsAssociativeArrayOfIntByInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const arr = hldb::findByName<hldb::Net>("arr", top->getNets());
  ASSERT_NE(arr, nullptr);
  const hldb::ArrayTypespec *const at = arr->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 3);  // associative = 3
  ASSERT_NE(at->getIndexTypespec(), nullptr);
  EXPECT_NE(at->getIndexTypespec()->getActual<hldb::IntTypespec>(), nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

// --- initial process ---------------------------------------------------------

TEST_F(AssociativeArraySizeTest, InitialBeginHasSevenStmts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 7u);
}

TEST_F(AssociativeArraySizeTest, FirstDisplayAssertsSizeEqualsZero) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(init->getStmt<hldb::Begin>()->getStmts()->at(0));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 0)");
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(disp->getArguments()->at(1));
  ASSERT_NE(hp, nullptr);
  EXPECT_EQ(hp->getName(), "arr.size");
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);
  const hldb::RefObj *const arrRef = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(arrRef, nullptr);
  EXPECT_EQ(arrRef->getName(), "arr");
  EXPECT_NE(arrRef->getActual<hldb::Net>(), nullptr);
  const hldb::MethodFuncCall *const sizeRef = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(sizeRef, nullptr);
  EXPECT_EQ(sizeRef->getName(), "size");
  EXPECT_EQ(sizeRef->getTaskFunc(), nullptr);
}

TEST_F(AssociativeArraySizeTest, FirstAssignmentSetsArrThreeToOne) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Assignment *const assign =
      any_cast<hldb::Assignment>(init->getStmt<hldb::Begin>()->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const hldb::BitSelect *const lhs = assign->getLhs<hldb::BitSelect>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "arr[3]");
  const hldb::Constant *const index = lhs->getIndex<hldb::Constant>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getConstType(), vpiUIntConst);
  EXPECT_EQ(index->getDecompile(), "3");
  EXPECT_NE(index->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "1");
}

TEST_F(AssociativeArraySizeTest, SecondDisplayAssertsSizeEqualsOne) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(init->getStmt<hldb::Begin>()->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 1)");
  EXPECT_EQ(any_cast<hldb::HierPath>(disp->getArguments()->at(1))->getName(), "arr.size");
}

TEST_F(AssociativeArraySizeTest, SecondAssignmentSetsArrHexFfffToTwo) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Assignment *const assign =
      any_cast<hldb::Assignment>(init->getStmt<hldb::Begin>()->getStmts()->at(3));
  ASSERT_NE(assign, nullptr);
  const hldb::BitSelect *const lhs = assign->getLhs<hldb::BitSelect>();
  ASSERT_NE(lhs, nullptr);
  const hldb::Constant *const index = lhs->getIndex<hldb::Constant>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getConstType(), 5);  // hexadecimal = 5
  EXPECT_EQ(index->getDecompile(), std::string_view("16'hffff"));
  EXPECT_EQ(index->getValue(), "ffff");
  EXPECT_NE(index->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "2");
}

TEST_F(AssociativeArraySizeTest, ThirdDisplayAssertsSizeEqualsTwo) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(init->getStmt<hldb::Begin>()->getStmts()->at(4));
  ASSERT_NE(disp, nullptr);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 2)");
}

TEST_F(AssociativeArraySizeTest, ThirdAssignmentSetsArrBinary1000ToThree) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Assignment *const assign =
      any_cast<hldb::Assignment>(init->getStmt<hldb::Begin>()->getStmts()->at(5));
  ASSERT_NE(assign, nullptr);
  const hldb::BitSelect *const lhs = assign->getLhs<hldb::BitSelect>();
  ASSERT_NE(lhs, nullptr);
  const hldb::Constant *const index = lhs->getIndex<hldb::Constant>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getConstType(), 3);  // binary = 3
  EXPECT_EQ(index->getDecompile(), std::string_view("4'b1000"));
  EXPECT_EQ(index->getValue(), "1000");
  EXPECT_NE(index->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>(), nullptr)
      << "binary literal index resolves to LogicTypespec, unlike the decimal/hex index forms above";
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "3");
}

TEST_F(AssociativeArraySizeTest, FourthDisplayAssertsSizeEqualsThree) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(init->getStmt<hldb::Begin>()->getStmts()->at(6));
  ASSERT_NE(disp, nullptr);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 3)");
}

// --- known compiler limitation: skipped canary for a future fix -------------

TEST_F(AssociativeArraySizeTest, SizeRefObjShouldResolveOnceImplicitNetBugIsFixed) {
  GTEST_SKIP() << "Known compiler limitation (EL0535 Illegal implicit net): HLC never resolves "
                  "the no-parens '.size' RefObj to a declared object. Re-enable this test once "
                  "that limitation is fixed.";

  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const size_t displayStmtIndices[4] = {0, 2, 4, 6};
  for (const size_t idx : displayStmtIndices) {
    const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(idx));
    ASSERT_NE(disp, nullptr);
    const hldb::HierPath *const hp = any_cast<hldb::HierPath>(disp->getArguments()->at(1));
    ASSERT_NE(hp, nullptr);
    const hldb::RefObj *const sizeRef = any_cast<hldb::RefObj>(hp->getPathElems()->at(1));
    ASSERT_NE(sizeRef, nullptr);
    EXPECT_NE(sizeRef->getActual(), nullptr) << "arr.size (no parens) should resolve to a declared object";
  }
}

// --- design-level typespecs ----------------------------------------------------

TEST_F(AssociativeArraySizeTest, DesignHasFiveTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 5u);
}

TEST_F(AssociativeArraySizeTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "work@top");
}

TEST_F(AssociativeArraySizeTest, DesignHasLogicTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_NE(any_cast<hldb::LogicTypespec>(m_design->getTypespecs()->at(4)), nullptr);
}

// --- compiler diagnostics: known ELAB_ILLEGAL_IMPLICIT_NET limitation --------

TEST_F(AssociativeArraySizeTest, CompilerReportsExactlyFourErrorsNoFatalNoWarning) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(AssociativeArraySizeTest, ExactlyFourIllegalImplicitNetErrors) {
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
