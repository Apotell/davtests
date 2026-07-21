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

// Tests for equality.sv (tags: 7.4.3)
//   module top ();
//     bit arr_a [7:0];
//     bit arr_b [7:0];
//     initial begin
//       arr_a = '{1, 1, 1, 0, 0, 1, 1, 1};
//       arr_b = '{1, 1, 1, 0, 0, 1, 1, 1};
//       $display(":assert: ('%b%b%b%b_%b%b%b%b' == '1110_0111')",
//         arr_a[7], arr_a[6], arr_a[5], arr_a[4], arr_a[3], arr_a[2], arr_a[1], arr_a[0]);
//       $display(":assert: ('%b%b%b%b_%b%b%b%b' == '1110_0111')",
//         arr_b[7], arr_b[6], arr_b[5], arr_b[4], arr_b[3], arr_b[2], arr_b[1], arr_b[0]);
//       $display(":assert: (%d == 1)", (arr_a == arr_b));
//       $display(":assert: (%d == 0)", (arr_a != arr_b));
//     end
//   endmodule
//
// Checked:
//   - design has module work@top with exactly 2 nets: "arr_a", "arr_b"
//   - both nets: RefTypespec -> ArrayTypespec static(1) range [7:0], elem
//     -> BitTypespec -- the SAME (shared) BitTypespec instance for both
//     nets, unlike the ArrayTypespec instances which are distinct per net
//   - Initial process: 1 Begin with 6 stmts (2 Assignment + 4 SysTaskCall)
//   - Stmt[0]/Stmt[1]: blocking Assignment, RefObj lhs, rhs Operation
//     (vpiOpType=assign pattern(87)) with 8 unsigned-int Constant operands
//     1,1,1,0,0,1,1,1
//   - Stmt[2]/Stmt[3]: $display with 9 args (format + 8 BitSelect
//     arr_a[7..0] / arr_b[7..0])
//   - Stmt[4]: $display(":assert: (%d == 1)", (arr_a == arr_b)) -- 2nd arg
//     is an Operation (vpiOpType=equal(14)) with 2 RefObj operands
//     resolving Net "arr_a" / Net "arr_b" (unlike packed arrays' PartSelect
//     comparisons, unpacked whole-array equality compares plain RefObjs)
//   - Stmt[5]: $display(":assert: (%d == 0)", (arr_a != arr_b)) -- 2nd arg
//     is an Operation (vpiOpType=not equal(15)) with 2 RefObj operands
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (signed),
//     StringTypespec -- NOTE: unlike chapter-7/arrays/packed/equality,
//     there is no extra unsigned IntTypespec here (only 3 typespecs total,
//     not 4)
//   - compiler emits zero errors
//   - no continuous assignments
//
// Not checked:
//   - actual runtime result of the == / != comparisons -- simulation-only
//     (see the skipped canary RuntimeComparisonResultsRequireSimulation
//     below)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
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

class UnpackedEqualityTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "equality.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module / nets ------------------------------------------------------------

TEST_F(UnpackedEqualityTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(UnpackedEqualityTest, ModuleHasTwoNets) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u);
}

TEST_F(UnpackedEqualityTest, NetArrAAndArrBAreArraysOfSharedBitTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const arrA = hldb::findByName<hldb::Net>("arr_a", top->getNets());
  const hldb::Net *const arrB = hldb::findByName<hldb::Net>("arr_b", top->getNets());
  ASSERT_NE(arrA, nullptr);
  ASSERT_NE(arrB, nullptr);
  const hldb::ArrayTypespec *const atA = arrA->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  const hldb::ArrayTypespec *const atB = arrB->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(atA, nullptr);
  ASSERT_NE(atB, nullptr);
  EXPECT_NE(atA, atB) << "each net should get its own distinct ArrayTypespec instance";
  EXPECT_EQ(atA->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(atA->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "0");
  const hldb::BitTypespec *const elemA = atA->getElemTypespec()->getActual<hldb::BitTypespec>();
  const hldb::BitTypespec *const elemB = atB->getElemTypespec()->getActual<hldb::BitTypespec>();
  ASSERT_NE(elemA, nullptr);
  ASSERT_NE(elemB, nullptr);
  EXPECT_EQ(elemA, elemB) << "the element BitTypespec should be shared between arr_a and arr_b";
}

// --- initial process ---------------------------------------------------------

TEST_F(UnpackedEqualityTest, InitialBeginHasSixStmts) {
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

TEST_F(UnpackedEqualityTest, FirstTwoStmtsAssignPatternLiteralsToArrAAndArrB) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const char *const names[2] = {"arr_a", "arr_b"};
  const std::string expected[8] = {"1", "1", "1", "0", "0", "1", "1", "1"};
  for (uint32_t i = 0; i < 2u; ++i) {
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(i));
    ASSERT_NE(assign, nullptr) << "stmt[" << i << "]";
    EXPECT_TRUE(assign->getBlocking());
    EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), names[i]);
    const hldb::Operation *const op = assign->getRhs<hldb::Operation>();
    ASSERT_NE(op, nullptr);
    EXPECT_EQ(op->getOpType(), vpiAssignmentPatternOp);
    ASSERT_NE(op->getOperands(), nullptr);
    ASSERT_EQ(op->getOperands()->size(), 8u);
    for (uint32_t j = 0; j < 8u; ++j) {
      EXPECT_EQ(any_cast<hldb::Constant>(op->getOperands()->at(j))->getDecompile(), expected[j]) << "operand " << j;
    }
  }
}

TEST_F(UnpackedEqualityTest, ThirdAndFourthStmtsDisplayEightBitSelectsEach) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const char *const names[2] = {"arr_a", "arr_b"};
  for (uint32_t s = 0; s < 2u; ++s) {
    const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(s + 2));
    ASSERT_NE(disp, nullptr) << "stmt[" << (s + 2) << "]";
    ASSERT_NE(disp->getArguments(), nullptr);
    ASSERT_EQ(disp->getArguments()->size(), 9u);
    for (uint32_t i = 0; i < 8u; ++i) {
      const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(disp->getArguments()->at(i + 1));
      ASSERT_NE(sel, nullptr) << "argument " << (i + 1);
      EXPECT_EQ(sel->getPrefix<hldb::RefObj>()->getName(), names[s]);
      EXPECT_EQ(sel->getIndex<hldb::Constant>()->getDecompile(), std::to_string(7 - i));
    }
  }
}

TEST_F(UnpackedEqualityTest, FifthStmtArgIsEqualOperationOnRefObjs) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(4));
  ASSERT_NE(disp, nullptr);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 1)");
  const hldb::Operation *const op = any_cast<hldb::Operation>(disp->getArguments()->at(1));
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiEqOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "arr_a");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);
  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(op->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "arr_b");
  EXPECT_NE(rhs->getActual<hldb::Net>(), nullptr);
}

TEST_F(UnpackedEqualityTest, SixthStmtArgIsNotEqualOperationOnRefObjs) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(5));
  ASSERT_NE(disp, nullptr);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 0)");
  const hldb::Operation *const op = any_cast<hldb::Operation>(disp->getArguments()->at(1));
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiNeqOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(op->getOperands()->at(0))->getName(), "arr_a");
  EXPECT_EQ(any_cast<hldb::RefObj>(op->getOperands()->at(1))->getName(), "arr_b");
}

// --- design-level typespecs / compiler diagnostics ---------------------------

TEST_F(UnpackedEqualityTest, DesignHasThreeTypespecs) {
  // NOTE: unlike packed/equality, no extra unsigned IntTypespec is added
  // here for the comparison result type.
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(UnpackedEqualityTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "work@top");
}

TEST_F(UnpackedEqualityTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(2)), nullptr);
}

TEST_F(UnpackedEqualityTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(UnpackedEqualityTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

// --- known gap: runtime comparison results require simulation ---------------

TEST_F(UnpackedEqualityTest, RuntimeComparisonResultsRequireSimulation) {
  GTEST_SKIP() << "This harness only compiles/elaborates equality.sv; it does not run a simulator, "
                  "so the actual runtime results of (arr_a == arr_b) / (arr_a != arr_b) cannot be "
                  "observed here. equality.sv's own $display format strings document the expected "
                  "values instead.";

  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const eqDisplay = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(4));
  ASSERT_NE(eqDisplay, nullptr);
  EXPECT_EQ(any_cast<hldb::Constant>(eqDisplay->getArguments()->at(0))->getValue(), ":assert: (%d == 1)")
      << "expected (arr_a == arr_b) == 1 since both hold '{1,1,1,0,0,1,1,1}";
  const hldb::SysTaskCall *const neqDisplay = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(5));
  ASSERT_NE(neqDisplay, nullptr);
  EXPECT_EQ(any_cast<hldb::Constant>(neqDisplay->getArguments()->at(0))->getValue(), ":assert: (%d == 0)")
      << "expected (arr_a != arr_b) == 0 since both hold '{1,1,1,0,0,1,1,1}";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
