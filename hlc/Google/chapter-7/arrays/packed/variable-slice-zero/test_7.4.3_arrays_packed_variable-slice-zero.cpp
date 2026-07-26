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

// Tests for variable-slice-zero.sv (tags: 7.4.3)
//   :should_fail_because: slicing array with zero part width
//   module top ();
//     bit [7:0] arr_a;
//     bit [7:0] arr_b;
//     parameter integer c = 0;
//     initial begin
//       arr_a = 8'hff;
//       arr_b = 8'h00;
//       $display(":assert: (('%h' == 'ff') and ('%h' == '00'))", arr_a, arr_b);
//       arr_b[4+:c] = arr_a[1+:c];
//       $display(":assert: ('%b' == '00000000')", arr_b);
//     end
//   endmodule
//
// Checked:
//   - design has module top with exactly 2 nets: "arr_a", "arr_b"
//   - both nets: RefTypespec -> BitTypespec, 1 range [7:0], vector=true
//   - module has exactly 1 Parameter "c" (RefTypespec -> IntegerTypespec,
//     signed) with 1 ParamAssign: lhs RefObj "c" resolving the Parameter,
//     rhs Constant "0" (zero-width part-select size)
//   - Initial process: 1 Begin with 5 stmts, same shape as variable-slice.sv:
//     Stmt[3] is arr_b[4+:c] = arr_a[1+:c] -- IndexedPartSelect lhs/rhs with
//     widthExpr RefObj "c" resolving to a Parameter whose value is 0
//   - design-level typespecs (4): ModuleTypespec, IntTypespec (signed),
//     IntTypespec (unsigned/default), StringTypespec
//   - HLC's compile/elaborate-only pass (-d ast -d db, no simulation) emits
//     zero errors for this file
//   - no continuous assignments
//
// Not checked (see Skipped below for canary coverage):
//   - the actual `:should_fail_because:` condition this file is designed to
//     test (IEEE 1800-2017 7.4.3: an indexed part-select width must be
//     constant and greater than zero) -- see the "Compiler limitation"
//     section below; this is a simulation/elaboration-time semantic check
//     this compile-only harness does not perform
//   - actual runtime bit pattern of arr_b -- simulation-only in general
//
// Skipped (documents a fix, not a gap):
//   - CompilerShouldRejectZeroWidthIndexedPartSelect: GTEST_SKIP canary
//     asserting the compiler SHOULD report an error for arr_b[4+:c] with
//     c==0, per the file's should_fail_because tag; re-enable once that
//     width check is implemented (at parse/elaborate time or via a
//     simulation-capable harness)
//   - RuntimeValueRequiresSimulation: GTEST_SKIP canary documenting the
//     runtime bit pattern of arr_b, which requires a simulator this harness
//     does not run
//
// Compiler limitation (NOT a defect in this test):
//   variable-slice-zero.sv is annotated ":should_fail_because: slicing array
//   with zero part width" -- a SystemVerilog simulator (e.g. Icarus, per the
//   TODO comments in the .sv source) is expected to reject `arr_b[4+:c]`
//   with `c == 0` at elaboration/simulation time ("Indexed part widths must
//   be constant and greater than zero"). This HLC build only parses and
//   builds the static AST/HLDB graph (-d ast -d db) without elaborating or
//   simulating the design, so it does not perform that width check and
//   compiles the file with zero errors. The tests below therefore document
//   the parse-time structure (which is valid) rather than the semantic
//   failure the .sv file's tag describes -- verifying that failure would
//   require a simulator, which is out of scope for this harness.

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
#include <hldb/indexed_part_select.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/integer_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class PackedVariableSliceZeroTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "variable-slice-zero.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module / nets ------------------------------------------------------------

TEST_F(PackedVariableSliceZeroTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(PackedVariableSliceZeroTest, ModuleHasTwoNets) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u);
}

// --- parameter c = 0 (the zero part-width under test) ------------------------

TEST_F(PackedVariableSliceZeroTest, ModuleHasOneParameterNamedCEqualsZero) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  ASSERT_EQ(top->getParameters()->size(), 1u);
  const hldb::Parameter *const c = any_cast<hldb::Parameter>(top->getParameters()->at(0));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getName(), "c");
  EXPECT_NE(c->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntegerTypespec>(), nullptr);

  ASSERT_NE(top->getParamAssigns(), nullptr);
  ASSERT_EQ(top->getParamAssigns()->size(), 1u);
  const hldb::ParamAssign *const pa = top->getParamAssigns()->at(0);
  ASSERT_NE(pa, nullptr);
  EXPECT_EQ(pa->getLhs<hldb::RefObj>()->getName(), "c");
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "0");
}

// --- initial process ---------------------------------------------------------

TEST_F(PackedVariableSliceZeroTest, InitialBeginHasFiveStmts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 5u);
}

TEST_F(PackedVariableSliceZeroTest, FirstAndSecondAssignmentsSetArrAAndArrB) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const a0 = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(a0, nullptr);
  EXPECT_EQ(a0->getLhs<hldb::RefObj>()->getName(), "arr_a");
  EXPECT_EQ(a0->getRhs<hldb::Constant>()->getValue(), "ff");
  const hldb::Assignment *const a1 = any_cast<hldb::Assignment>(begin->getStmts()->at(1));
  ASSERT_NE(a1, nullptr);
  EXPECT_EQ(a1->getLhs<hldb::RefObj>()->getName(), "arr_b");
  EXPECT_EQ(a1->getRhs<hldb::Constant>()->getValue(), "00");
}

TEST_F(PackedVariableSliceZeroTest, FirstDisplayHasThreeArguments) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 3u);
  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(),
            ":assert: (('%h' == 'ff') and ('%h' == '00'))");
}

TEST_F(PackedVariableSliceZeroTest, ThirdAssignmentIsIndexedPartSelectWithZeroWidthParam) {
  // This is the exact construct the file's ":should_fail_because" tag targets:
  // a variable-width part-select whose width parameter "c" is 0. HLC's
  // parse/elaborate-only pass still builds the same IndexedPartSelect shape
  // as variable-slice.sv; it does not reject the zero width (see file header).
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(3));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());

  const hldb::IndexedPartSelect *const lhs = assign->getLhs<hldb::IndexedPartSelect>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "arr_b[4+:c]");
  EXPECT_EQ(lhs->getBaseExpr<hldb::Constant>()->getDecompile(), "4");
  const hldb::RefObj *const lhsWidth = lhs->getWidthExpr<hldb::RefObj>();
  ASSERT_NE(lhsWidth, nullptr);
  EXPECT_EQ(lhsWidth->getName(), "c");
  const hldb::Parameter *const lhsWidthParam = lhsWidth->getActual<hldb::Parameter>();
  ASSERT_NE(lhsWidthParam, nullptr) << "widthExpr 'c' should resolve to the zero-valued Parameter";

  const hldb::IndexedPartSelect *const rhs = assign->getRhs<hldb::IndexedPartSelect>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "arr_a[1+:c]");
  EXPECT_EQ(rhs->getBaseExpr<hldb::Constant>()->getDecompile(), "1");
}

TEST_F(PackedVariableSliceZeroTest, SecondDisplayAssertsAllZeroBitPattern) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(4));
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(), ":assert: ('%b' == '00000000')");
  EXPECT_EQ(any_cast<hldb::RefObj>(disp->getArguments()->at(1))->getName(), "arr_b");
}

// --- design-level typespecs / compiler diagnostics ---------------------------

TEST_F(PackedVariableSliceZeroTest, DesignHasFourTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 4u);
}

TEST_F(PackedVariableSliceZeroTest, CompilerReportsZeroErrorsAtParseElaborateStage) {
  // Documents the compiler-limitation note above: this compile/elaborate-only
  // pass does not perform the "part width must be > 0" semantic check, so it
  // reports zero errors even though the file is annotated should_fail_because.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(PackedVariableSliceZeroTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

// --- known compiler limitation: skipped canary for a future fix -------------

TEST_F(PackedVariableSliceZeroTest, CompilerShouldRejectZeroWidthIndexedPartSelect) {
  GTEST_SKIP() << "IEEE 1800-2017 7.4.3 requires indexed part-select widths to be constant and "
                  "greater than zero (per this file's should_fail_because tag and the TODO "
                  "comments in variable-slice-zero.sv referencing Icarus's rejection: 'Indexed "
                  "part widths must be constant and greater than zero'). This HLC build's "
                  "compile/elaborate-only pass (-d ast -d db) does not perform that width check, "
                  "so it currently reports zero errors for arr_b[4+:c] with c==0. Re-enable this "
                  "test once the compiler enforces the >0 width rule.";

  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbError + stats.nbFatal, 0)
      << "expected the compiler to reject the zero-width indexed part-select arr_b[4+:c] (c==0) "
         "per IEEE 1800-2017 7.4.3, once that semantic check is implemented";
}

// --- known gap: the should_fail_because semantic check is simulation-only --

TEST_F(PackedVariableSliceZeroTest, RuntimeValueRequiresSimulation) {
  // GTEST_SKIP() << "This harness only compiles/elaborates variable-slice-zero.sv; it does not run a "
  //                 "simulator, so it cannot observe either (a) a simulator rejecting the zero-width "
  //                 "indexed part-select as the file's should_fail_because tag describes, or (b) the "
  //                 "actual runtime bit pattern of arr_b. variable-slice-zero.sv's own $display "
  //                 "format string documents the value a permissive simulator would produce.";

  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(4));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(), ":assert: ('%b' == '00000000')")
      << "expected arr_b unchanged (8'b00000000) since the zero-width slice copies nothing -- "
         "assuming a simulator tolerates the construct at all, per the file's should_fail_because tag";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
