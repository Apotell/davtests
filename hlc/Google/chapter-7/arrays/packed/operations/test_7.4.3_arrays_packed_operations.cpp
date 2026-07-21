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

// Tests for operations.sv (tags: 7.4.3)
//   module top ();
//     bit [7:0] arr;
//     initial begin
//       arr = 8'h00;
//       $display(":assert: ('%h' == '00')", arr);
//       arr = 8'hde;
//       $display(":assert: ('%h' == 'de')", arr);
//       arr = 8'had;
//       $display(":assert: ('%h' == 'ad')", arr);
//     end
//   endmodule
//
// Checked:
//   - design has module work@top with exactly 1 net "arr"
//   - net "arr": RefTypespec -> BitTypespec, 1 range [7:0], vector=true
//   - module has exactly 1 typespec (the single BitTypespec)
//   - Initial process: 1 Begin with 6 stmts (3 Assignment + 3 SysFuncCall)
//   - each Assignment: RefObj lhs "arr", hexadecimal Constant rhs
//     ("8'h00" / "8'hde" / "8'had", vpiConstType=hexadecimal(5), size=8)
//   - each $display: 2 arguments (format string + RefObj "arr")
//   - design-level typespecs (4): ModuleTypespec, IntTypespec (signed),
//     IntTypespec (unsigned/default), StringTypespec
//   - compiler emits zero errors
//   - no continuous assignments
//
// Not checked:
//   - none -- every write/read in this file is a plain whole-variable
//     Assignment/RefObj with no runtime-only computation to defer

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class PackedOperationsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "operations.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module / net -------------------------------------------------------------

TEST_F(PackedOperationsTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(PackedOperationsTest, ModuleHasOneNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}
  
TEST_F(PackedOperationsTest, ModuleHasOneTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  EXPECT_EQ(top->getTypespecs()->size(), 1u);
}

TEST_F(PackedOperationsTest, NetArrIsBitTypespecRange7to0) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const arr = hldb::findByName<hldb::Net>("arr", top->getNets());
  ASSERT_NE(arr, nullptr);
  const hldb::BitTypespec *const bt = arr->getTypespec<hldb::RefTypespec>()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  EXPECT_TRUE(bt->getVector());
  ASSERT_NE(bt->getRanges(), nullptr);
  ASSERT_EQ(bt->getRanges()->size(), 1u);
  EXPECT_EQ(bt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(bt->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

// --- initial process ---------------------------------------------------------

TEST_F(PackedOperationsTest, InitialBeginHasSixStmts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 6u);
}

TEST_F(PackedOperationsTest, ThreeAssignmentsWriteZeroDeAndAd) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);

  const size_t stmtIndices[3] = {0, 2, 4};
  const char *const expectedValues[3] = {"00", "de", "ad"};
  for (size_t i = 0; i < 3; ++i) {
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(stmtIndices[i]));
    ASSERT_NE(assign, nullptr);
    EXPECT_TRUE(assign->getBlocking());
    const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), "arr");
    EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);
    const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->getConstType(), 5);  // hexadecimal = 5
    EXPECT_EQ(rhs->getSize(), 8);
    EXPECT_EQ(rhs->getValue(), expectedValues[i]);
  }
}

TEST_F(PackedOperationsTest, ThreeDisplaysAssertZeroDeAndAd) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);

  const size_t stmtIndices[3] = {1, 3, 5};
  const char *const expectedFmt[3] = {":assert: ('%h' == '00')", ":assert: ('%h' == 'de')",
                                       ":assert: ('%h' == 'ad')"};
  for (size_t i = 0; i < 3; ++i) {
    const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(stmtIndices[i]));
    ASSERT_NE(disp, nullptr);
    EXPECT_EQ(disp->getName(), "$display");
    ASSERT_NE(disp->getArguments(), nullptr);
    ASSERT_EQ(disp->getArguments()->size(), 2u);
    EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(), expectedFmt[i]);
    EXPECT_EQ(any_cast<hldb::RefObj>(disp->getArguments()->at(1))->getName(), "arr");
  }
}

// --- design-level typespecs / compiler diagnostics ---------------------------

TEST_F(PackedOperationsTest, DesignHasFourTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 4u);
}

TEST_F(PackedOperationsTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "work@top");
}

TEST_F(PackedOperationsTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(PackedOperationsTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
