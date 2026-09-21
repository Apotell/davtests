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
// tests/Google/chapter-21/21.3--fdisplay-boh.sv
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
// :name: fdisplay_boh
// :description: $fdisplay test
// :tags: 21.3
// :type: simulation parsing
// */
// module top();
//
// int fd;
// string str = "abc";
//
// initial begin
//     fd = $fopen("tmp.txt", "w");
//     $fdisplayb(fd, str);
//     $fdisplayo(fd, str);
//     $fdisplayh(fd, str);
// end
//
// final
//     $fclose(fd);
//
// endmodule
// ============================================================================
//
// IEEE 1800-2017 construct under test: Sec 21.3 "File input-output system
// tasks and functions". Three separate pieces of that clause meet in this one
// file, and every one of them leaves a distinct, checkable shape in the model:
//
//   1. Sec 21.3.1 "Opening and closing files". "$fopen" is a system FUNCTION
//      -- it returns the file descriptor -- so it can only appear on the
//      right-hand side of an assignment, never as a statement. "$fclose" is a
//      system TASK and is a statement in its own right. That asymmetry is
//      visible statically: one is a SysFuncCall reached through an
//      Assignment's rhs, the other is a SysTaskCall bound directly as a
//      process body. A front end that modeled both as the same node kind
//      would be wrong in a way no amount of running could hide.
//   2. Sec 21.3.2 "File output system tasks". "$fdisplay" and its "b"/"o"/"h"
//      variants take the multichannel/file descriptor as their FIRST
//      argument, with the remaining arguments behaving exactly as in the
//      corresponding Sec 21.2 display task. So argument 0 here is a reference
//      to "fd" -- not a format string, and not the value being printed. This
//      is the structural difference between this file and its Sec 21.2
//      sibling 21.2--write-boh.sv, where the sole argument was the operand.
//      As in Sec 21.2.1, the radix ("b" binary, "o" octal, "h" hexadecimal --
//      the "boh" of the file name) is encoded nowhere but in the task name,
//      so the three names must survive verbatim and stay distinct.
//   3. Sec 9.2.3 "final construct". The "final" here has no begin/end, so its
//      body must be the "$fclose" call itself rather than a Begin wrapper,
//      and it is a FinalStmt process distinct from the Initial one.
//
// Supporting declarations, each also asserted rather than assumed:
// "int fd;" is a 2-state signed 32-bit integer with NO initializer
// (Sec 6.11.1), which matters because its entire value comes from the $fopen
// assignment; "string str = "abc";" is a string-typed Variable (Sec 6.16)
// whose initializer is a string literal, and per Sec 5.9 a string literal is
// a vpiStringConst whose size is 8 bits times its character count.
//
// Cross-process name binding is the one thing here with real risk of going
// wrong: "fd" is written in the initial block and read again from a
// completely separate final block, and "str" is read three times from a
// third scope. All of those must resolve back to the two module-level
// declarations, which is checked by object identity, not by name alone.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - the design has exactly one module, "top", with no ports (the header is
//     "module top();") and no nets -- neither "int" nor "string" carries a
//     net-type keyword (IEEE 1800-2017 Sec 6.7/6.8).
//   - "top" declares exactly 2 variables:
//       * "fd": typespec resolves to a signed IntTypespec (Sec 6.11.1), and
//         getValue() is null -- "int fd;" is written with no decl-assignment.
//       * "str": typespec resolves to StringTypespec, and its initializer is
//         a Constant with getConstType() == vpiStringConst, getValue()
//         == "abc", getSize() == 24 (Sec 5.9: 3 characters x 8 bits) and its
//         own StringTypespec.
//   - "top" owns no anonymous module-level typespecs: neither "int" nor
//     "string" declares a packed range, so there is no per-declaration
//     typespec for the module scope to hold.
//   - "top" has exactly 2 processes, in source order: an Initial, then a
//     FinalStmt.
//   - the Initial's body is a Begin holding exactly 4 statements and
//     declaring no local variables of its own (everything is module-level).
//   - statement 0 is a blocking Assignment whose lhs is a RefObj "fd"
//     resolving to the module-level "fd", and whose rhs is a SysFuncCall
//     named "$fopen" with exactly 2 arguments -- proving $fopen is modeled
//     as a value-producing function, not a task.
//   - those 2 $fopen arguments are string Constants "tmp.txt" (vpiStringConst,
//     size 56 = 7 x 8) and "w" (vpiStringConst, size 8 = 1 x 8), in that
//     order: file name first, then open mode.
//   - statements 1..3 are SysTaskCall nodes named, in source order,
//     "$fdisplayb", "$fdisplayo", "$fdisplayh".
//   - each of those three takes exactly 2 arguments: argument 0 is a RefObj
//     "fd" pointer-identical to the module-level "fd" (the descriptor comes
//     first per Sec 21.3.2, and it is specifically NOT a Constant, so there
//     is no format string anywhere in this file), and argument 1 is a RefObj
//     "str" pointer-identical to the module-level "str".
//   - the FinalStmt's body is NOT a Begin (no begin/end was written); it is a
//     SysTaskCall named "$fclose" with exactly 1 argument, a RefObj "fd"
//     pointer-identical to the same module-level "fd" the initial block
//     assigned -- i.e. the descriptor binds across two separate processes.
//   - design-level typespecs: exactly 3, containing exactly one
//     ModuleTypespec (for "top"), one IntTypespec (signed, for "int") and one
//     StringTypespec (for "string" and the string literals). These are
//     counted by scanning the collection rather than by index, because the
//     order in which the design registers them is a tool-internal convention
//     that the source text does not determine.
//   - no COMP_FAILED_TO_BIND diagnostic is emitted, and the compiler reports
//     zero fatals, syntax errors, errors and warnings: $fopen, $fclose and
//     the $fdisplay radix variants are all standard Sec 21.3 system
//     tasks/functions.
//
// ----------------------------------------------------------------------------
// WHAT IS NOT CHECKED, AND WHY (permanently out of scope -- HLC is a static
// compiler/elaborator, not a simulator, and never will be):
//   - Anything written into "tmp.txt": the characters emitted, the radix each
//     variant renders "abc" in, and the newline $fdisplay appends (Sec
//     21.3.2 / Sec 21.2.1). Rendering a value in a radix and emitting bytes
//     only happen while time advances, and there is no file here to inspect.
//     The nearest thing that does exist statically, and is asserted, is
//     FdisplayTasksAreBOHInSourceOrder: the radix is carried entirely by the
//     three distinct task names, and those names are checked exactly.
//   - The numeric descriptor value $fopen returns at runtime, and whether the
//     open succeeded. "fd" has no initializer at all, which is itself
//     asserted in VariableFdIsSignedIntWithNoInitializer; no object in this
//     model carries a post-execution value.
//   - That the final block runs after the initial block, at the end of
//     simulation (Sec 9.2.3), so that $fclose sees an open descriptor. That
//     is scheduling. The static half -- that both processes exist, in source
//     order, and that both refer to the same "fd" object -- is asserted in
//     ModuleHasTwoProcessesInitialThenFinal and
//     FinalProcessClosesFdDirectlyWithNoBeginWrapper.
//
// No .log file was consulted at any point while writing this test.
// ============================================================================

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/final_stmt.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FdisplayBohTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.3--fdisplay-boh.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Variable *getVariable(const char *const name) {
    const hldb::Module *const top = getTop();
    return (top == nullptr) ? nullptr : hldb::findByName<hldb::Variable>(name, top->getVariables());
  }

  static const hldb::Variable *getFd() { return getVariable("fd"); }
  static const hldb::Variable *getStr() { return getVariable("str"); }

  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }

  static const hldb::FinalStmt *getFinalProcess() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->size() < 2u) return nullptr;
    return any_cast<hldb::FinalStmt>(top->getProcesses()->at(1));
  }
};

// --- module shell -----------------------------------------------------------

TEST_F(FdisplayBohTest, DesignHasExactlyOneModule) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 1u) << "the source file declares exactly one module, 'top'";
}

TEST_F(FdisplayBohTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr) << "module 'top' not found"; }

TEST_F(FdisplayBohTest, ModuleTopHasNoPortsAndNoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty())
      << "the header is 'module top();' -- an explicitly empty port list";
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'int' and 'string' carry no net-type keyword, so per IEEE 1800-2017 Sec 6.7/6.8 both "
         "declarations are Variables and the module owns no Net";
}

// --- the two module-level declarations --------------------------------------

TEST_F(FdisplayBohTest, ModuleHasExactlyTwoVariablesFdAndStr) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 2u) << "'int fd;' and 'string str = \"abc\";'";
  EXPECT_NE(hldb::findByName<hldb::Variable>("fd", top->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("str", top->getVariables()), nullptr);
}

TEST_F(FdisplayBohTest, VariableFdIsSignedIntWithNoInitializer) {
  const hldb::Variable *const fd = getFd();
  ASSERT_NE(fd, nullptr);
  ASSERT_NE(fd->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = fd->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "'int' must resolve to an IntTypespec";
  EXPECT_TRUE(it->getSigned()) << "IEEE 1800-2017 Sec 6.11.1: 'int' is a 2-state SIGNED 32-bit integer type";
  EXPECT_EQ(fd->getValue(), nullptr) << "'int fd;' is written with no decl-assignment -- its whole value "
                                        "comes from the '$fopen' assignment in the initial block";
}

TEST_F(FdisplayBohTest, VariableStrIsStringTypedInitializedToAbc) {
  const hldb::Variable *const str = getStr();
  ASSERT_NE(str, nullptr);
  ASSERT_NE(str->getTypespec(), nullptr);
  EXPECT_NE(str->getTypespec()->getActual<hldb::StringTypespec>(), nullptr)
      << "IEEE 1800-2017 Sec 6.16: 'string' is its own data type, not a packed vector";

  const hldb::Constant *const init = str->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'string str = \"abc\";' carries a declaration-time initializer";
  EXPECT_EQ(init->getConstType(), vpiStringConst) << "IEEE 1800-2017 Sec 5.9: a double-quoted literal is a "
                                                     "string constant";
  EXPECT_EQ(init->getValue(), "abc") << "the stored value is the raw characters, without the quotes";
  EXPECT_EQ(init->getSize(), 24) << "IEEE 1800-2017 Sec 5.9: 3 characters x 8 bits = 24 bits";
  ASSERT_NE(init->getTypespec(), nullptr);
  EXPECT_NE(init->getTypespec()->getActual<hldb::StringTypespec>(), nullptr);
}

TEST_F(FdisplayBohTest, ModuleOwnsNoAnonymousTypespecs) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getTypespecs() == nullptr || top->getTypespecs()->empty())
      << "neither 'int' nor 'string' declares a packed range, so no per-declaration typespec "
         "belongs to the module scope";
}

// --- process structure: an initial block and a final block ------------------

TEST_F(FdisplayBohTest, ModuleHasTwoProcessesInitialThenFinal) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 2u) << "the file contains one 'initial' and one 'final' block";
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr) << "the 'initial' block comes first";
  EXPECT_NE(any_cast<hldb::FinalStmt>(top->getProcesses()->at(1)), nullptr)
      << "IEEE 1800-2017 Sec 9.2.3: 'final' is its own process kind, distinct from Initial";
}

TEST_F(FdisplayBohTest, InitialBodyIsBeginWithFourStatementsAndNoLocalDecls) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr) << "'initial begin ... end' must bind a Begin as the Initial's statement";
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 4u) << "one '$fopen' assignment plus three '$fdisplay' calls";
  EXPECT_TRUE(blk->getVariables() == nullptr || blk->getVariables()->empty())
      << "nothing is declared inside the begin/end block; 'fd' and 'str' are module-level";
}

// --- Sec 21.3.1: $fopen is a function, reached through an assignment --------

TEST_F(FdisplayBohTest, FirstStatementAssignsFopenResultToFd) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 4u);

  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "'fd = $fopen(...)' is an assignment, because Sec 21.3.1 makes $fopen a "
                                "value-returning system function rather than a task";
  EXPECT_TRUE(assign->getBlocking()) << "'fd = ...' uses the blocking assignment operator '='";

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "fd");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getFd()) << "the lhs must bind to the module-level 'int fd'";

  const hldb::SysFuncCall *const fopen = assign->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(fopen, nullptr) << "the rhs must be a system FUNCTION call, not a task";
  EXPECT_EQ(fopen->getName(), "$fopen");
  ASSERT_NE(fopen->getArguments(), nullptr);
  EXPECT_EQ(fopen->getArguments()->size(), 2u) << "'$fopen(\"tmp.txt\", \"w\")' passes a file name and a mode";
}

TEST_F(FdisplayBohTest, FopenArgumentsAreFileNameThenModeStringLiterals) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::SysFuncCall *const fopen = assign->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(fopen, nullptr);
  ASSERT_NE(fopen->getArguments(), nullptr);
  ASSERT_EQ(fopen->getArguments()->size(), 2u);

  // IEEE 1800-2017 Sec 5.9: a string literal's size is 8 bits per character.
  const char *const values[2] = {"tmp.txt", "w"};
  const int sizes[2] = {56, 8};
  for (uint32_t i = 0; i < 2u; ++i) {
    const hldb::Constant *const arg = any_cast<hldb::Constant>(fopen->getArguments()->at(i));
    ASSERT_NE(arg, nullptr) << "argument " << i << " should be a string literal Constant";
    EXPECT_EQ(arg->getConstType(), vpiStringConst) << "argument " << i;
    EXPECT_EQ(arg->getValue(), values[i]) << "argument " << i << " (file name first, then open mode)";
    EXPECT_EQ(arg->getSize(), sizes[i]) << "argument " << i << ": Sec 5.9 size is 8 bits per character";
    ASSERT_NE(arg->getTypespec(), nullptr) << "argument " << i;
    EXPECT_NE(arg->getTypespec()->getActual<hldb::StringTypespec>(), nullptr) << "argument " << i;
  }
}

// --- Sec 21.3.2: the three radix variants of $fdisplay ----------------------

TEST_F(FdisplayBohTest, FdisplayTasksAreBOHInSourceOrder) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 4u);

  // Sec 21.3.2 defers to Sec 21.2.1 for argument handling: these three differ
  // from "$fdisplay" and from each other only in the default radix used for
  // an argument with no format specification. That radix is encoded nowhere
  // but in the name, so the names must survive verbatim and stay distinct.
  const char *const names[3] = {"$fdisplayb", "$fdisplayo", "$fdisplayh"};
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(i + 1u));
    ASSERT_NE(call, nullptr) << "statement " << (i + 1u) << " should be a system task call";
    EXPECT_EQ(call->getName(), names[i]) << "statement " << (i + 1u) << " should keep its radix suffix verbatim";
  }
}

TEST_F(FdisplayBohTest, EachFdisplayTakesDescriptorThenValueAndNoFormatString) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 4u);

  const hldb::Variable *const fd = getFd();
  const hldb::Variable *const str = getStr();
  ASSERT_NE(fd, nullptr);
  ASSERT_NE(str, nullptr);

  for (uint32_t i = 1u; i < 4u; ++i) {
    const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(i));
    ASSERT_NE(call, nullptr) << "statement " << i;
    ASSERT_NE(call->getArguments(), nullptr) << "statement " << i;
    ASSERT_EQ(call->getArguments()->size(), 2u) << "statement " << i << " is written as '(fd, str)'";

    // Sec 21.3.2: the file descriptor is the FIRST argument -- unlike the
    // Sec 21.2 display tasks, whose first argument is typically a format
    // string. There is no format string anywhere in this file.
    EXPECT_EQ(any_cast<hldb::Constant>(call->getArguments()->at(0)), nullptr)
        << "statement " << i << ": argument 0 is the descriptor, not a format string";
    const hldb::RefObj *const descriptor = any_cast<hldb::RefObj>(call->getArguments()->at(0));
    ASSERT_NE(descriptor, nullptr) << "statement " << i;
    EXPECT_EQ(descriptor->getName(), "fd");
    EXPECT_EQ(descriptor->getActual<hldb::Variable>(), fd)
        << "statement " << i << ": the descriptor must bind to the module-level 'fd'";

    const hldb::RefObj *const value = any_cast<hldb::RefObj>(call->getArguments()->at(1));
    ASSERT_NE(value, nullptr) << "statement " << i << ": argument 1 should be a plain variable reference";
    EXPECT_EQ(value->getName(), "str");
    EXPECT_EQ(value->getActual<hldb::Variable>(), str)
        << "statement " << i << ": the printed operand must bind to the module-level 'str'";
  }
}

// --- Sec 9.2.3 / Sec 21.3.1: the final block closes the descriptor ----------

TEST_F(FdisplayBohTest, FinalProcessClosesFdDirectlyWithNoBeginWrapper) {
  const hldb::FinalStmt *const fin = getFinalProcess();
  ASSERT_NE(fin, nullptr) << "the second process should be a FinalStmt";
  EXPECT_EQ(fin->getStmt<hldb::Begin>(), nullptr)
      << "IEEE 1800-2017 Sec 9.2.3: 'final' wraps a single statement_or_null, and no begin/end was "
         "written, so the body must not be a Begin";

  const hldb::SysTaskCall *const fclose = fin->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(fclose, nullptr) << "'$fclose' is a system TASK (Sec 21.3.1) and stands alone as a statement";
  EXPECT_EQ(fclose->getName(), "$fclose");
  ASSERT_NE(fclose->getArguments(), nullptr);
  ASSERT_EQ(fclose->getArguments()->size(), 1u) << "'$fclose(fd)' passes only the descriptor";

  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(fclose->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "fd");
  EXPECT_EQ(arg->getActual<hldb::Variable>(), getFd())
      << "the descriptor read in the final block must be the same object the initial block assigned";
}

// --- design-level typespecs / compiler diagnostics --------------------------

TEST_F(FdisplayBohTest, DesignHasThreeTypespecsModuleSignedIntAndString) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  ASSERT_EQ(m_design->getTypespecs()->size(), 3u)
      << "one ModuleTypespec for 'top', one IntTypespec for 'int fd', one StringTypespec for "
         "'string str' and the string literals";

  // Counted by scanning rather than by index: which slot each one lands in is
  // a tool-internal registration order that the source text does not fix.
  uint32_t nbModule = 0;
  uint32_t nbInt = 0;
  uint32_t nbString = 0;
  for (uint32_t i = 0; i < 3u; ++i) {
    if (any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(i)) != nullptr) ++nbModule;
    const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(i));
    if (it != nullptr) {
      ++nbInt;
      EXPECT_TRUE(it->getSigned()) << "IEEE 1800-2017 Sec 6.11.1: 'int' is signed";
    }
    if (any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(i)) != nullptr) ++nbString;
  }
  EXPECT_EQ(nbModule, 1u);
  EXPECT_EQ(nbInt, 1u);
  EXPECT_EQ(nbString, 1u);
}

// "fd" is assigned in the initial block and read again from a separate final
// block; "str" is read from a third scope. Those cross-process lookups are the
// likeliest place for binding to fail.
TEST_F(FdisplayBohTest, AllReferencesBindToTheirDeclarations) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr)
      << "'fd' and 'str' must bind to the module-level declarations from both processes";
}

TEST_F(FdisplayBohTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "$fopen, $fclose and the $fdisplay radix variants are all standard "
                                 "IEEE 1800-2017 Sec 21.3 system tasks and functions";
  EXPECT_EQ(stats.nbWarning, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
