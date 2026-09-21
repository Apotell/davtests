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

// ============================================================================
// SystemVerilog source under test:
// tests/Google/chapter-21/21.2--write-boh.sv
// ----------------------------------------------------------------------------
// // Copyright (C) 2019-2021  The SymbiFlow Authors.
// //
// // Use of this source code is governed by a ISC-style
// // license that can be found in the LICENSE file or at
// // https://opensource.org/licenses/ISC
// //
// // SPDX-License-Identifier: ISC
//
// /*
// :name: write_boh
// :description: $write test
// :tags: 21.2
// :type: simulation parsing
// */
// module top();
//
// initial begin
//     int val = 1234;
//     $writeb(val);
//     $writeo(val);
//     $writeh(val);
// end
//
// endmodule
// ============================================================================
//
// IEEE 1800-2017 construct under test: Sec 21.2 "Display system tasks", and
// specifically Sec 21.2.1's statement that "$write", "$writeb", "$writeo" and
// "$writeh" are the same display task differing only in the default radix
// applied to an argument that has no explicit format specification -- decimal,
// binary, octal and hexadecimal respectively. The "boh" in the file name is
// exactly that: binary / octal / hex, the three non-default radix forms, each
// applied to the very same operand.
//
// Structurally that gives the compiler three things to get right, and all
// three survive into the HLDB model with no simulation involved:
//
//   1. The three task names must be preserved verbatim and kept distinct.
//      The radix lives only in the task's *name* here -- there is no format
//      string, no extra argument, nothing else in the source that encodes it.
//      If the compiler normalized "$writeb"/"$writeo"/"$writeh" down to a
//      single "$write" node, or dropped the trailing radix letter, the entire
//      meaning of this file would be lost, and a static model is the only
//      place that loss is detectable.
//   2. Each call carries exactly one argument, and that argument is a plain
//      reference to a variable -- NOT a format string. Contrast the far more
//      common '$display("...%d", x)' shape seen throughout this corpus, where
//      argument 0 is a string Constant. Here argument 0 is the operand
//      itself, which is precisely why the radix has to come from the name.
//   3. "val" is declared *inside* the initial block, so it is a Variable owned
//      by the enclosing Begin scope, and all three task calls must bind to
//      that one object -- not to three separate lookups, and not to a
//      module-level object (the module declares nothing at all).
//
// Per IEEE 1800-2017 Sec 6.11.1 "int" is a 2-state signed 32-bit integer type,
// and per Sec 5.7.1 an unsized unbased literal such as "1234" is a plain
// decimal integer literal; both facts are asserted below rather than assumed.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - the design contains exactly one module, "top", and that module declares
//     nothing of its own: no ports (the header is "module top();"), no nets
//     and no variables -- everything in this file lives inside the initial
//     block.
//   - "top" has exactly one process, an Initial, whose statement is a Begin
//     (the source wrote "initial begin ... end").
//   - the Begin owns exactly one local Variable, "val", whose typespec
//     resolves to a signed IntTypespec (IEEE 1800-2017 Sec 6.11.1).
//   - "val"'s declaration-time initializer is a Constant of type
//     vpiUIntConst decompiling to "1234" with value "1234" -- an unsized,
//     unbased decimal literal (IEEE 1800-2017 Sec 5.7.1), not a sized or
//     based one.
//   - the Begin's getStmts() holds exactly 3 entries. The declaration of
//     "val" is NOT one of them: a declaration with an initializer lands in
//     the scope's getVariables() with its initializer reachable through
//     getValue(), leaving the statement list to hold only the three task
//     calls.
//   - those 3 statements are SysTaskCall nodes named, in source order,
//     "$writeb", "$writeo", "$writeh" -- three distinct names, in the binary
//     / octal / hex order the file name advertises.
//   - each of the three carries exactly one argument; that argument is a
//     RefObj named "val" and is not a Constant (there is no format string
//     anywhere in this file), and each RefObj's getActual<Variable>() is
//     pointer-identical to the single Begin-scoped "val" -- i.e. all three
//     calls bind to the same declaration.
//   - design-level typespecs: exactly 2, the ModuleTypespec for "top" and
//     the signed IntTypespec -- no StringTypespec, since the file contains
//     no string literal at all (this is the model-level counterpart of "the
//     $write calls have no format string").
//   - the compiler emits zero fatals, syntax errors, errors and warnings:
//     $writeb/$writeo/$writeh are standard IEEE 1800-2017 Sec 21.2.1 system
//     tasks, so a correct front end has nothing to complain about.
//
// ----------------------------------------------------------------------------
// WHAT IS NOT CHECKED, AND WHY (permanently out of scope -- HLC is a static
// compiler/elaborator, not a simulator, and never will be):
//   - The characters actually printed, and the radix actually used for each
//     one (1234 as "10011010010", "2322", "4d2"). Rendering a value in a
//     radix only happens while time advances. The nearest thing that does
//     exist statically, and is asserted, is
//     WriteTasksAreWritebWriteoWritehInSourceOrder: the radix is carried
//     entirely by the three distinct task names, and those names are checked
//     exactly.
//   - That $write-family tasks emit no trailing newline (unlike $display,
//     IEEE 1800-2017 Sec 21.2.1). That is a property of emitted output, so
//     there is no output stream here to inspect. The static stand-in that is
//     asserted is the same name check above: "$writeb" is recorded as
//     "$writeb", never rewritten to "$display".
//   - The order in which the three calls actually execute, and that they all
//     run in the same time step. The nearest real assertion is
//     BeginHasExactlyThreeStatements plus the source-order name check, which
//     pins the static statement sequence; anything beyond that is scheduling.
//   - The value "val" holds when each task reads it. A Variable exposes only
//     a declaration-time initializer, which is asserted in
//     ValInitializerIsUnsizedDecimal1234; there is no post-execution value
//     for any object in this model.
//
// No .log file was consulted at any point while writing this test.
// ============================================================================

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_task_call.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class WriteBohTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.2--write-boh.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }

  static const hldb::Variable *getVal() {
    const hldb::Begin *const blk = getInitialBody();
    return (blk == nullptr) ? nullptr : hldb::findByName<hldb::Variable>("val", blk->getVariables());
  }
};

// --- module shell: "module top();" declares nothing of its own --------------

TEST_F(WriteBohTest, DesignHasExactlyOneModule) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 1u) << "the source file declares exactly one module, 'top'";
}

TEST_F(WriteBohTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr) << "module 'top' not found"; }

TEST_F(WriteBohTest, ModuleTopDeclaresNoPortsNetsOrVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty())
      << "the header is 'module top();' -- an explicitly empty port list";
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "no net-type keyword appears anywhere in this file (IEEE 1800-2017 Sec 6.7)";
  EXPECT_TRUE(top->getVariables() == nullptr || top->getVariables()->empty())
      << "'val' is declared inside the initial block, so the module scope owns no variable";
}

// --- the single initial block ----------------------------------------------

TEST_F(WriteBohTest, ModuleHasOneInitialProcessWithBeginBody) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u) << "the file contains exactly one procedural block";

  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr) << "the process should specifically be an Initial block";
  EXPECT_NE(init->getStmt<hldb::Begin>(), nullptr) << "'initial begin ... end' must bind a Begin as its statement";
}

// --- the block-local declaration: "int val = 1234;" -------------------------

TEST_F(WriteBohTest, BeginOwnsOneLocalVariableValOfSignedIntType) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getVariables(), nullptr);
  ASSERT_EQ(blk->getVariables()->size(), 1u) << "the begin/end block declares exactly one object, 'val'";

  const hldb::Variable *const val = hldb::findByName<hldb::Variable>("val", blk->getVariables());
  ASSERT_NE(val, nullptr) << "'int val' should be a Begin-scoped Variable named 'val'";
  ASSERT_NE(val->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = val->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "'int' must resolve to an IntTypespec";
  EXPECT_TRUE(it->getSigned()) << "IEEE 1800-2017 Sec 6.11.1: 'int' is a 2-state SIGNED 32-bit integer type";
}

TEST_F(WriteBohTest, ValInitializerIsUnsizedDecimal1234) {
  const hldb::Variable *const val = getVal();
  ASSERT_NE(val, nullptr);
  const hldb::Constant *const init = val->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'int val = 1234;' carries a declaration-time initializer";
  EXPECT_EQ(init->getConstType(), vpiUIntConst)
      << "IEEE 1800-2017 Sec 5.7.1: '1234' is an unsized, unbased decimal literal";
  EXPECT_EQ(init->getDecompile(), "1234");
  EXPECT_EQ(init->getValue(), "1234");
}

// --- the point of the file: three radix variants of the same write task -----

TEST_F(WriteBohTest, BeginHasExactlyThreeStatements) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 3u) << "three task calls; the declaration of 'val' belongs to getVariables(), "
                                            "not to the statement list";
}

TEST_F(WriteBohTest, WriteTasksAreWritebWriteoWritehInSourceOrder) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 3u);

  // IEEE 1800-2017 Sec 21.2.1: these three differ from each other, and from
  // "$write", only in the default radix used for an argument with no format
  // specification. That radix is encoded nowhere but in the name, so the
  // names must survive verbatim and must stay distinct from one another.
  const char *const names[3] = {"$writeb", "$writeo", "$writeh"};
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(i));
    ASSERT_NE(call, nullptr) << "statement " << i << " should be a system task call";
    EXPECT_EQ(call->getName(), names[i]) << "statement " << i << " should keep its radix suffix verbatim";
  }
}

TEST_F(WriteBohTest, EachWriteTaskTakesValAsItsOnlyArgumentWithNoFormatString) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 3u);

  const hldb::Variable *const val = getVal();
  ASSERT_NE(val, nullptr);

  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(i));
    ASSERT_NE(call, nullptr) << "statement " << i;
    ASSERT_NE(call->getArguments(), nullptr) << "statement " << i;
    ASSERT_EQ(call->getArguments()->size(), 1u) << "statement " << i << " is written as '(val)' -- one argument";

    EXPECT_EQ(any_cast<hldb::Constant>(call->getArguments()->at(0)), nullptr)
        << "statement " << i << " has no leading format string; its sole argument is the operand itself";
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(call->getArguments()->at(0));
    ASSERT_NE(ref, nullptr) << "statement " << i << " should pass a plain variable reference";
    EXPECT_EQ(ref->getName(), "val");
    EXPECT_EQ(ref->getActual<hldb::Variable>(), val)
        << "statement " << i << " must bind to the one Begin-scoped 'val', not to a separate object";
  }
}

// --- design-level typespecs / compiler diagnostics --------------------------

TEST_F(WriteBohTest, DesignHasTwoTypespecsModuleAndSignedInt) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  ASSERT_EQ(m_design->getTypespecs()->size(), 2u)
      << "one ModuleTypespec for 'top' and one IntTypespec; no StringTypespec, because this file "
         "contains no string literal -- the $write calls pass 'val' directly, with no format string";
  EXPECT_NE(any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0)), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned()) << "IEEE 1800-2017 Sec 6.11.1: 'int' is signed";
}

TEST_F(WriteBohTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "$writeb/$writeo/$writeh are standard IEEE 1800-2017 Sec 21.2.1 system tasks";
  EXPECT_EQ(stats.nbWarning, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
