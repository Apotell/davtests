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

// Tests for 21.2--write.sv (tags: 21.2)
//   module top();
//     initial begin
//       int val = 1234;
//       $write(val);
//     end
//   endmodule
//
// This is the plain-$write counterpart of 21.2--display.sv, which has the
// exact same body except for the task name. The corner this file exercises
// is IEEE 1800-2023 Sec 21.2.1: "$write" is one of the "display" system
// tasks and, called here with a single bare expression argument (no format
// string at all), formats that argument using the default radix (decimal)
// for its type -- the same rule that governs "$display(val)". What sets
// "$write" apart from "$display" is purely a runtime detail (no trailing
// newline is appended); it is not a structural difference the compiler's
// static model can show, since HLC records the call as a SysTaskCall node
// keyed by name string regardless of which of the display-family tasks was
// used.
//
// Behaviour observed while writing this file (hlc.exe -d ast -d db over the
// fixture, run directly and independently of any previously recorded
// golden log): the file compiles with zero errors, zero warnings, zero
// syntax errors. "val" is declared inside the initial block's begin-end,
// so it belongs to that Begin scope, not to the module: "top" itself has
// no variables and no nets at all.
//
// What is checked:
//   - module top exists, has no nets and no module-level variables (the
//     only declaration in the file is scoped to the initial block's Begin)
//   - the initial block is a Begin that declares exactly one variable,
//     "val": RefTypespec -> IntTypespec (signed, per IEEE 1800-2023 Sec
//     6.11.2 "int" is a signed 32-bit integer), with a declaration-time
//     getValue<Constant>() decompiling to "1234"
//   - that Begin has exactly 1 statement: SysTaskCall "$write" with
//     exactly 1 argument, RefObj "val" resolving (getActual<Variable>())
//     to the "val" declared above -- not a re-typed literal
//   - design-level typespecs (2): ModuleTypespec "top" and the shared,
//     signed IntTypespec used by "val"'s declaration -- there is no
//     string literal anywhere in this file, so no StringTypespec, unlike
//     the sibling tests that $display/$write a format string
//   - compiler emits zero errors
//
// What is NOT checked and why:
//   - that "$write" omits the trailing newline "$display" would add. That
//     is a runtime console-output fact; HLC is a static compiler/
//     elaborator with no simulated output stream to inspect here, and the
//     SysTaskCall node itself carries only the name string "$write", so
//     this genuine simulation-only gap is called out explicitly below
//     instead of being silently skipped.

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
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>

namespace hlc {

class WriteTaskTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.2--write.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }
};

// --- module ----

TEST_F(WriteTaskTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(WriteTaskTest, ModuleHasNoNetsAndNoModuleLevelVariables) {
  // "val" is declared inside the initial block's begin-end, so it belongs
  // to that Begin's scope, not to the module (IEEE 1800-2023 Sec 6.3
  // "Variable declarations", block-scoped data).
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getNets(), nullptr);
  EXPECT_EQ(top->getVariables(), nullptr);
}

// --- initial block: int val = 1234; $write(val); ----

TEST_F(WriteTaskTest, InitialBlockDeclaresIntValInitializedTo1234) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getVariables(), nullptr);
  ASSERT_EQ(blk->getVariables()->size(), 1u);
  const hldb::Variable *const val = hldb::findByName<hldb::Variable>("val", blk->getVariables());
  ASSERT_NE(val, nullptr);
  const hldb::IntTypespec *const intTypespec = val->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(intTypespec, nullptr);
  EXPECT_TRUE(intTypespec->getSigned()) << "IEEE 1800-2023 Sec 6.11.2: 'int' is a signed 32-bit integer type";
  ASSERT_NE(val->getValue<hldb::Constant>(), nullptr);
  EXPECT_EQ(val->getValue<hldb::Constant>()->getDecompile(), "1234");
}

TEST_F(WriteTaskTest, InitialBlockHasOneStatementWritingVal) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 1u);
  const hldb::SysTaskCall *const write = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(0));
  ASSERT_NE(write, nullptr);
  EXPECT_EQ(write->getName(), "$write");
  ASSERT_NE(write->getArguments(), nullptr);
  ASSERT_EQ(write->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(write->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "val");
  EXPECT_NE(arg->getActual<hldb::Variable>(), nullptr) << "'$write(val)' should reference the 'val' declared above";
}

// --- design-level typespecs / compiler diagnostics ----

TEST_F(WriteTaskTest, DesignHasModuleAndIntTypespecsOnly) {
  // No string literal appears anywhere in this file (unlike the sibling
  // tests that $display/$write a format string), so there is no
  // StringTypespec: just the module itself and the "int" data type used
  // by "val"'s declaration.
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  ASSERT_EQ(m_design->getTypespecs()->size(), 2u);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(WriteTaskTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

// --- known gap: $write's missing trailing newline is runtime-only ----

TEST_F(WriteTaskTest, WriteOmitsTheTrailingNewlineDisplayWouldAdd) {
  GTEST_SKIP() << "IEEE 1800-2023 Sec 21.2.1: '$write' is identical to '$display' except that it "
                  "does not automatically append a new line to its output. HLC is a static "
                  "compiler/elaborator with no simulated console/output stream to inspect here; "
                  "the SysTaskCall node for '$write' carries only its name string and argument "
                  "list, which looks structurally identical to the one '$display(val)' would "
                  "produce. This behavioral difference is a genuine simulation-only fact, not "
                  "something observable anywhere in the static HLDB model.";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
