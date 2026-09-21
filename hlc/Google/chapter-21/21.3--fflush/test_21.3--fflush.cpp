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

// Tests for 21.3--fflush.sv (tags: 21.3)
//
//   module top();
//     initial begin
//       int fd;
//       fd = $fopen("tmp.txt", "w");
//       $fflush();
//       $fclose(fd);
//     end
//   endmodule
//
// What this file is about, and why each check below is the right one
// (every expectation was derived by reading the .sv source against the
// LRM, before any of this test code was written):
//
//   IEEE 1800-2023 Sec 21.3.6 "Flushing output": $fflush is a system
//   TASK with three forms -- "$fflush(mcd);", "$fflush(fd);" and
//   "$fflush();". The last one, with NO arguments, flushes every open
//   file, and it is the form this source uses. That makes arity zero the
//   whole point of the file: the SysTaskCall must carry an empty (or
//   absent) argument list. This is the only file in the 21.3 group that
//   calls a system task with no operand at all, which is exactly the
//   case a model is most likely to get wrong -- by inventing a
//   descriptor argument, by borrowing the neighbouring $fclose call's
//   operand, or by dropping the statement entirely. So the test asserts
//   arity zero directly, and then re-asserts it alongside $fclose's
//   arity of one in the same block, so that a leaked or shared argument
//   list cannot pass unnoticed.
//
//   Because $fflush is a task and nothing consumes a return value here,
//   the node must be a SysTaskCall and explicitly not a SysFuncCall --
//   the opposite of the $fopen call two lines above it.
//
//   IEEE 1800-2023 Sec 21.3.1 "Opening and closing files": $fopen is a
//   system FUNCTION whose return value is consumed by an assignment, so
//   the rhs of "fd = $fopen(...)" must be a SysFuncCall, never a
//   SysTaskCall. Supplying the type argument ("w") selects the
//   two-argument form yielding a single-channel file descriptor.
//   $fclose, in the same clause, is a system TASK taking the descriptor.
//
//   IEEE 1800-2023 Sec 6.11 "Integer data types", Table 6-8: "int" is
//   the 2-state 32-bit SIGNED type, so "int fd" resolves to an
//   IntTypespec with vpiSigned set. "int fd;" has no "= expression", so
//   it carries no declaration-time value; the descriptor arrives through
//   a procedural statement instead.
//
//   IEEE 1800-2023 Sec 6.21 "Scope and lifetime" with Sec 9.3.1
//   "Sequential blocks": "int fd;" is declared INSIDE the begin-end
//   block, so it is local to that block -- it must appear in the Begin's
//   own getVariables() and must not surface at module scope. Both
//   references to fd are checked for object identity against that one
//   block-local Variable, proving they bound to the local declaration
//   rather than to an implicitly created object.
//
//   IEEE 1800-2023 Sec 6.7/6.8: "int" is a data_type keyword, never a
//   net_type keyword, and the module has no port list to trigger an
//   implicit-net rule, so nothing in this file is a Net.
//
//   IEEE 1800-2023 Sec 5.9 "String literals": "tmp.txt" and "w" are
//   vpiStringConst sized at 8 bits per character -- 7 x 8 = 56 and
//   1 x 8 = 8. Neither contains an escape sequence, so the character
//   counts are unambiguous.
//
//   IEEE 1800-2023 Sec 23.2.2: "module top();" declares an empty
//   list_of_ports, so the module has zero ports.
//
// Checked:
//   - module "top" exists with zero ports, zero nets, and zero
//     module-scope variables
//   - the module has exactly 1 process, an Initial, and no final
//     construct
//   - the Initial's body is a Begin owning exactly 1 local Variable
//     "fd": IntTypespec, signed, no declaration-time initializer
//   - that Begin has exactly 3 statements
//   - stmt[0]: blocking Assignment; lhs RefObj "fd" resolving to the
//     block-local fd; rhs a SysFuncCall (explicitly not a SysTaskCall)
//     named "$fopen" with exactly 2 arguments, Constant "tmp.txt"
//     (vpiStringConst, size 56) and Constant "w" (vpiStringConst, size 8)
//   - stmt[1]: a SysTaskCall (explicitly not a SysFuncCall) named
//     "$fflush" with ZERO arguments
//   - stmt[2]: SysTaskCall "$fclose" with exactly 1 argument, RefObj
//     "fd" resolving to the block-local fd
//   - the two task calls in this block have different arities (0 for
//     $fflush, 1 for $fclose), asserted together so a shared or leaked
//     argument list cannot slip through
//   - the compiler reports no fatal/syntax/error diagnostics, and "fd"
//     never fails to bind
//
// What is NOT checked, and why:
//   - That $fflush actually writes buffered output, and which files it
//     reaches (Sec 21.3.6: the no-argument form flushes all open files).
//     Buffered output only exists while time advances; HLC is a static
//     compiler/elaborator and writes nothing. Permanently out of scope.
//     The static half of the same concern is
//     FflushCallIsAZeroArgumentSysTaskCall, which pins the call's kind,
//     name, and arity -- the arity being precisely what selects the
//     flush-everything form.
//   - Whether "tmp.txt" is created on disk and what integer $fopen hands
//     back (Sec 21.3.1: nonzero on success, zero on failure).
//     Permanently out of scope; the nearest real assertion is
//     FopenRhsIsSysFuncCallWithFilenameAndMode.
//   - The value held by "fd" between the $fopen assignment and the
//     $fclose call. Variable::getValue<T>() exposes only a
//     declaration-time initializer, and "int fd;" has none -- which is
//     itself asserted by InitialBeginDeclaresOneLocalSignedIntFd.
//     Permanently out of scope.
//   - Whether the source wrote "$fflush()" with empty parentheses or
//     "$fflush" without them. Both are legal ways to call a task with no
//     arguments and the LRM gives them identical meaning; the HLDB call
//     node records the argument list, not the punctuation, so there is
//     nothing to assert. Arity zero is the fact that matters and it is
//     asserted.
//   - That the three statements actually execute in the order written.
//     Their order in the Begin's statement list IS asserted below, one
//     index at a time; whether control reaches them in that order is a
//     simulation fact and permanently out of scope.
//   - The design-level typespec count, and whether the block-local
//     "int"'s IntTypespec is registered in the Begin's own
//     getTypespecs() or shared at design scope. That is a tool-internal
//     node-sharing convention the source text does not determine, so it
//     is deliberately left unasserted rather than guessed.

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
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FflushTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.3--fflush.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }

  // IEEE 1800-2023 Sec 6.21: "int fd;" sits inside the begin-end block,
  // so the Begin owns the one and only declaration of fd.
  static const hldb::Variable *getLocalFd() {
    const hldb::Begin *const blk = getInitialBody();
    if (blk == nullptr || blk->getVariables() == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("fd", blk->getVariables());
  }

  template <typename T>
  static const T *getStmt(uint32_t index) {
    const hldb::Begin *const blk = getInitialBody();
    if (blk == nullptr || blk->getStmts() == nullptr || index >= blk->getStmts()->size()) return nullptr;
    return any_cast<T>(blk->getStmts()->at(index));
  }

  // Arity of a system call, treating an absent collection as zero
  // arguments -- both spellings mean "no operands were written".
  static uint32_t argCount(const hldb::SysTaskCall *call) {
    if (call == nullptr || call->getArguments() == nullptr) return 0u;
    return (uint32_t)call->getArguments()->size();
  }
};

// --- module shell -----------------------------------------------------------

TEST_F(FflushTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// IEEE 1800-2023 Sec 23.2.2: "module top();" declares an empty
// list_of_ports.
TEST_F(FflushTest, ModuleHasNoPorts) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top();' declares an empty port list";
}

// IEEE 1800-2023 Sec 6.7/6.8: the file's only declaration uses "int", a
// data_type keyword, and there is no port list, so nothing here is a Net.
TEST_F(FflushTest, ModuleHasNoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  if (top->getNets() != nullptr) {
    EXPECT_TRUE(top->getNets()->empty()) << "'int' is a data type, not a net type";
    EXPECT_EQ(hldb::findByName<hldb::Net>("fd", top->getNets()), nullptr);
  }
}

// IEEE 1800-2023 Sec 6.21: "int fd;" is block-local, so it must not
// surface at module scope.
TEST_F(FflushTest, ModuleDeclaresNoVariablesOfItsOwn) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  if (top->getVariables() != nullptr) {
    EXPECT_TRUE(top->getVariables()->empty())
        << "IEEE 1800-2023 Sec 6.21: 'int fd;' is declared inside the begin-end block, not at module scope";
    EXPECT_EQ(hldb::findByName<hldb::Variable>("fd", top->getVariables()), nullptr)
        << "'fd' must be block-local, not hoisted to the module";
  }
}

// --- process inventory ------------------------------------------------------

TEST_F(FflushTest, ModuleHasExactlyOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u) << "the module holds one 'initial' construct and nothing else";
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr) << "the one process must be an Initial";
  EXPECT_EQ(any_cast<hldb::FinalStmt>(top->getProcesses()->at(0)), nullptr)
      << "this source contains no 'final' construct";
}

// --- the block-local descriptor ---------------------------------------------

TEST_F(FflushTest, InitialBeginDeclaresOneLocalSignedIntFd) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr) << "'initial begin ... end' body should be a Begin block";
  ASSERT_NE(blk->getVariables(), nullptr) << "the Begin owns the block-local 'int fd;' declaration";
  ASSERT_EQ(blk->getVariables()->size(), 1u) << "the block declares exactly one variable";

  const hldb::Variable *const fd = getLocalFd();
  ASSERT_NE(fd, nullptr) << "block-local variable 'fd' not found";
  ASSERT_NE(fd->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = fd->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "'int fd' must resolve to an IntTypespec";
  EXPECT_TRUE(it->getSigned()) << "IEEE 1800-2023 Table 6-8: 'int' is a 2-state 32-bit signed type";
  EXPECT_EQ(fd->getValue(), nullptr) << "'int fd;' has no '= expression' initializer";
}

TEST_F(FflushTest, InitialBeginHasExactlyThreeStatements) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 3u) << "the block holds the $fopen assignment, the $fflush call, and $fclose";
}

// --- stmt[0]: fd = $fopen("tmp.txt", "w") -----------------------------------

TEST_F(FflushTest, FopenAssignmentIsBlockingIntoLocalFd) {
  const hldb::Assignment *const assign = getStmt<hldb::Assignment>(0);
  ASSERT_NE(assign, nullptr) << "stmt[0] should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "'fd = $fopen(...)' uses the blocking operator '='";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "assignment lhs should be a RefObj";
  EXPECT_EQ(lhs->getName(), "fd");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getLocalFd())
      << "the lhs must bind to the block-local 'int fd', not to any other object";
}

// IEEE 1800-2023 Sec 21.3.1: $fopen is a system FUNCTION returning the
// descriptor, and the assignment consumes that return value -- the exact
// opposite of the $fflush statement below it.
TEST_F(FflushTest, FopenRhsIsSysFuncCallWithFilenameAndMode) {
  const hldb::Assignment *const assign = getStmt<hldb::Assignment>(0);
  ASSERT_NE(assign, nullptr);

  EXPECT_EQ(assign->getRhs<hldb::SysTaskCall>(), nullptr)
      << "IEEE 1800-2023 Sec 21.3.1: $fopen returns a descriptor, so it must be a SysFuncCall, not a SysTaskCall";
  const hldb::SysFuncCall *const call = assign->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(call, nullptr) << "assignment rhs should be a SysFuncCall";
  EXPECT_EQ(call->getName(), "$fopen");

  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u) << "the two-argument form '$fopen(filename, type)' is used here";

  const hldb::Constant *const filename = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(filename, nullptr) << "arg[0] should be the filename string literal";
  EXPECT_EQ(filename->getConstType(), vpiStringConst);
  EXPECT_EQ(filename->getValue(), "tmp.txt");
  EXPECT_EQ(filename->getSize(), 56) << "IEEE 1800-2023 Sec 5.9: \"tmp.txt\" = 7 characters x 8 bits = 56 bits";

  const hldb::Constant *const mode = any_cast<hldb::Constant>(call->getArguments()->at(1));
  ASSERT_NE(mode, nullptr) << "arg[1] should be the type string literal";
  EXPECT_EQ(mode->getConstType(), vpiStringConst);
  EXPECT_EQ(mode->getValue(), "w");
  EXPECT_EQ(mode->getSize(), 8) << "IEEE 1800-2023 Sec 5.9: \"w\" = 1 character x 8 bits = 8 bits";
}

// --- stmt[1]: $fflush() -- the point of the file ----------------------------

// IEEE 1800-2023 Sec 21.3.6: $fflush is a system TASK, and no value is
// consumed here, so the statement must be a SysTaskCall rather than a
// SysFuncCall.
TEST_F(FflushTest, FflushStatementIsATaskCallNamedFflush) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_GE(blk->getStmts()->size(), 2u);

  EXPECT_EQ(any_cast<hldb::SysFuncCall>(blk->getStmts()->at(1)), nullptr)
      << "IEEE 1800-2023 Sec 21.3.6: $fflush is a task; nothing consumes a return value here";
  const hldb::SysTaskCall *const call = getStmt<hldb::SysTaskCall>(1);
  ASSERT_NE(call, nullptr) << "stmt[1] should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$fflush");
}

// The whole reason this file exists: Sec 21.3.6 gives $fflush a
// no-argument form that flushes every open file, and that is the form
// written here. Arity zero is therefore the fact under test -- an
// invented descriptor operand would silently turn this into the
// single-file form.
TEST_F(FflushTest, FflushCallIsAZeroArgumentSysTaskCall) {
  const hldb::SysTaskCall *const call = getStmt<hldb::SysTaskCall>(1);
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$fflush");
  EXPECT_EQ(argCount(call), 0u)
      << "IEEE 1800-2023 Sec 21.3.6: '$fflush()' is the no-argument form -- it must carry no operands at all";
}

// --- stmt[2]: $fclose(fd) ---------------------------------------------------

TEST_F(FflushTest, FcloseCallClosesTheSameDescriptor) {
  const hldb::SysTaskCall *const call = getStmt<hldb::SysTaskCall>(2);
  ASSERT_NE(call, nullptr) << "IEEE 1800-2023 Sec 21.3.1: $fclose is a task, so stmt[2] must be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$fclose");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "'$fclose(fd)' takes exactly one descriptor argument";
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "the $fclose argument should be a RefObj";
  EXPECT_EQ(arg->getName(), "fd");
  EXPECT_EQ(arg->getActual<hldb::Variable>(), getLocalFd())
      << "the descriptor closed must be the same block-local 'fd' the $fopen assignment wrote";
}

// --- the two task calls side by side ----------------------------------------

// Asserting both arities in one place is what makes the zero meaningful:
// if an argument list were shared between the two calls, or if $fflush
// borrowed its neighbour's operand, exactly one of these two numbers
// would be wrong and the pair would catch it.
TEST_F(FflushTest, FflushTakesNoOperandWhileFcloseTakesOne) {
  const hldb::SysTaskCall *const fflush = getStmt<hldb::SysTaskCall>(1);
  const hldb::SysTaskCall *const fclose = getStmt<hldb::SysTaskCall>(2);
  ASSERT_NE(fflush, nullptr);
  ASSERT_NE(fclose, nullptr);
  ASSERT_EQ(fflush->getName(), "$fflush");
  ASSERT_EQ(fclose->getName(), "$fclose");
  EXPECT_NE(fflush, fclose) << "'$fflush();' and '$fclose(fd);' are two distinct statements";
  EXPECT_EQ(argCount(fflush), 0u) << "'$fflush()' is written with no operands";
  EXPECT_EQ(argCount(fclose), 1u) << "'$fclose(fd)' is written with exactly one operand";
}

// --- compiler diagnostics ---------------------------------------------------

TEST_F(FflushTest, CompilerReportsNoFatalOrSyntaxErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

// "fd" is block-scoped rather than module-scoped and is referenced from
// two statements -- the one construct here with real binding risk.
TEST_F(FflushTest, BlockLocalDescriptorReferenceBinds) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "fd"), nullptr)
      << "'fd' must bind to the block-local 'int fd' declaration in the assignment and in $fclose";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
