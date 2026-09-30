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

// Tests for 21.3--fgetc.sv (tags: 21.3)
//
//   module top();
//     int fd;
//     int c;
//     initial begin
//       fd = $fopen("tmp.txt", "w");
//       c = $fgetc(fd);
//     end
//     final
//       $fclose(fd);
//   endmodule
//
// What this file is about, and why each check below is the right one
// (every expectation was derived by reading the .sv source against the
// LRM, before any of this test code was written):
//
//   IEEE 1800-2023 Sec 21.3.4 "Reading data from a file", in the
//   character-at-a-time subclause that defines $fgetc: $fgetc is a
//   system FUNCTION -- "c = $fgetc(fd);" -- taking exactly ONE argument,
//   the file descriptor, and RETURNING the byte read (or EOF). So the
//   rhs of "c = $fgetc(fd)" must be a SysFuncCall, never a SysTaskCall,
//   with exactly one RefObj operand.
//
//   The corner unique to this file is that BOTH module-scope variables
//   are plain "int": "fd" holds the descriptor and "c" receives the
//   byte. Nothing in the node kinds or typespecs distinguishes them, so
//   a model that swapped them -- assigning to fd and reading from c --
//   would still look structurally plausible. Every reference below is
//   therefore checked for object identity against the right one of the
//   two Variables, and the two Variables are checked to be distinct
//   nodes. That identity pairing is the real content of this test.
//
//   IEEE 1800-2023 Sec 21.3.1 "Opening and closing files": $fopen is a
//   system FUNCTION whose return value is consumed by an assignment, so
//   its node kind is SysFuncCall too; the type argument ("w") selects
//   the two-argument form yielding a single-channel file descriptor.
//   $fclose, in the same clause, is a system TASK taking the descriptor.
//
//   IEEE 1800-2023 Sec 9.2.3 "final construct": a final_construct is
//   "final" followed by a single statement_or_null. This source has no
//   begin/end after "final", so the FinalStmt's body must be the
//   $fclose SysTaskCall directly, not a Begin wrapper -- the shape
//   established by 9.2.3--final.sv. This is also what makes "fd" the
//   only name in the file referenced from two different processes.
//
//   IEEE 1800-2023 Sec 6.11 "Integer data types", Table 6-8: "int" is
//   the 2-state 32-bit SIGNED type, so both fd and c resolve to an
//   IntTypespec with vpiSigned set. Neither declaration has an
//   "= expression", so neither carries a declaration-time value.
//
//   IEEE 1800-2023 Sec 6.7/6.8: "int" is a data_type keyword, never a
//   net_type keyword, and the module has no port list to trigger an
//   implicit-net rule, so neither fd nor c is a Net.
//
//   IEEE 1800-2023 Sec 6.21 "Scope and lifetime": unlike its siblings
//   21.3--feof.sv, 21.3--ferror.sv and 21.3--fflush.sv, this file
//   declares both variables at MODULE scope, outside the begin-end
//   block. So they must appear in the module's getVariables() and the
//   Begin must own no variables of its own -- the mirror image of those
//   files, and asserted in both directions here.
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
//   - module "top" exists with zero ports and zero nets
//   - the module declares exactly 2 variables, "fd" and "c", each with
//     an IntTypespec, each signed, neither with a declaration-time
//     initializer, and the two are distinct nodes
//   - the module has exactly 2 processes: exactly one Initial and
//     exactly one FinalStmt
//   - the Initial's body is a Begin that owns NO variables of its own
//     and holds exactly 2 statements
//   - stmt[0]: blocking Assignment; lhs RefObj "fd" resolving to
//     Variable fd; rhs a SysFuncCall (explicitly not a SysTaskCall)
//     named "$fopen" with exactly 2 arguments, Constant "tmp.txt"
//     (vpiStringConst, size 56) and Constant "w" (vpiStringConst, size 8)
//   - stmt[1]: blocking Assignment; lhs RefObj "c" resolving to Variable
//     c -- and specifically NOT to fd; rhs a SysFuncCall (explicitly not
//     a SysTaskCall) named "$fgetc" with exactly 1 argument, RefObj "fd"
//     resolving to Variable fd -- and specifically NOT to c
//   - the FinalStmt's body is directly a SysTaskCall named "$fclose"
//     (no Begin wrapper) with exactly 1 argument: RefObj "fd" resolving
//     to the same Variable fd the initial block wrote
//   - the compiler reports no fatal/syntax/error diagnostics, and
//     neither "fd" nor "c" fails to bind
//
// What is NOT checked, and why:
//   - The byte $fgetc returns, and whether it returns EOF. Sec 21.3.4
//     defines that as the result of reading the stream at the moment of
//     the call, which exists only while time advances; HLC is a static
//     compiler/elaborator and never reads the file. Permanently out of
//     scope. The static half of the same concern is
//     FgetcRhsIsSysFuncCallOnTheDescriptor, which pins the call's kind,
//     name, arity, and operand binding.
//   - That this source opens "tmp.txt" in write mode ("w") and then
//     reads from that same descriptor with $fgetc. Whether that read
//     fails, and what it yields if it does, is entirely a property of
//     the stream at run time, so it is permanently out of scope and is
//     not a compile-time diagnostic HLC could raise. The static fact
//     that both calls operate on one and the same descriptor object IS
//     asserted, by FopenAssignmentIsBlockingIntoFd and
//     FgetcRhsIsSysFuncCallOnTheDescriptor together.
//   - Whether "tmp.txt" is created on disk and what integer $fopen hands
//     back (Sec 21.3.1: nonzero on success, zero on failure).
//     Permanently out of scope; the nearest real assertion is
//     FopenRhsIsSysFuncCallWithFilenameAndMode.
//   - The runtime values of fd and c. Variable::getValue<T>() exposes
//     only a declaration-time initializer, and neither declaration has
//     one -- which is itself asserted by
//     ModuleHasTwoDistinctSignedIntVariables. Permanently out of scope.
//   - That $fclose runs at the end of simulation (Sec 9.2.3). Only the
//     static shape of the final construct is observable here, and that
//     is what FinalStmtIsDirectlyAnFcloseSysTaskCall asserts.
//   - The design-level typespec count, and whether fd and c share one
//     IntTypespec node or hold one each. That is a tool-internal
//     node-sharing convention the source text does not determine, so it
//     is deliberately left unasserted rather than guessed; what the
//     source does determine -- that the two VARIABLES are distinct
//     objects -- is asserted instead.

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

class FgetcTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.3--fgetc.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  // IEEE 1800-2023 Sec 6.21: both declarations sit at module scope here,
  // outside the begin-end block.
  static const hldb::Variable *getVariable(std::string_view name) {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getVariables() == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, top->getVariables());
  }

  // The module holds two processes; their relative order in the
  // collection is a tool-internal detail the source does not fix, so each
  // is located by node kind rather than by index.
  static const hldb::Initial *getInitial() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr) return nullptr;
    for (uint32_t i = 0, n = (uint32_t)top->getProcesses()->size(); i < n; ++i) {
      const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(i));
      if (init != nullptr) return init;
    }
    return nullptr;
  }

  static const hldb::FinalStmt *getFinal() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr) return nullptr;
    for (uint32_t i = 0, n = (uint32_t)top->getProcesses()->size(); i < n; ++i) {
      const hldb::FinalStmt *const fin = any_cast<hldb::FinalStmt>(top->getProcesses()->at(i));
      if (fin != nullptr) return fin;
    }
    return nullptr;
  }

  static const hldb::Begin *getInitialBody() {
    const hldb::Initial *const init = getInitial();
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }

  static const hldb::Assignment *getAssignment(uint32_t index) {
    const hldb::Begin *const blk = getInitialBody();
    if (blk == nullptr || blk->getStmts() == nullptr || index >= blk->getStmts()->size()) return nullptr;
    return any_cast<hldb::Assignment>(blk->getStmts()->at(index));
  }
};

// --- module shell -----------------------------------------------------------

TEST_F(FgetcTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// IEEE 1800-2023 Sec 23.2.2: "module top();" declares an empty
// list_of_ports.
TEST_F(FgetcTest, ModuleHasNoPorts) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top();' declares an empty port list";
}

// IEEE 1800-2023 Sec 6.7/6.8: both declarations use "int", a data_type
// keyword, and there is no port list, so nothing here is a Net.
TEST_F(FgetcTest, ModuleHasNoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  if (top->getNets() != nullptr) {
    EXPECT_TRUE(top->getNets()->empty()) << "'int' is a data type, not a net type";
    EXPECT_EQ(hldb::findByName<hldb::Net>("fd", top->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>("c", top->getNets()), nullptr);
  }
}

// --- the two module-scope descriptors ---------------------------------------

// IEEE 1800-2023 Table 6-8: "int" is 2-state, 32-bit, signed. Both
// variables share that type, which is exactly why they must still be two
// separate objects.
TEST_F(FgetcTest, ModuleHasTwoDistinctSignedIntVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 2u) << "the module declares exactly 'int fd' and 'int c'";

  const char *const names[2] = {"fd", "c"};
  for (uint32_t i = 0; i < 2u; ++i) {
    const hldb::Variable *const var = getVariable(names[i]);
    ASSERT_NE(var, nullptr) << "variable " << names[i] << " not found";
    ASSERT_NE(var->getTypespec(), nullptr);
    const hldb::IntTypespec *const it = var->getTypespec()->getActual<hldb::IntTypespec>();
    ASSERT_NE(it, nullptr) << names[i] << " must resolve to an IntTypespec";
    EXPECT_TRUE(it->getSigned()) << "Table 6-8: 'int' is a 2-state 32-bit signed type";
    EXPECT_EQ(var->getValue(), nullptr) << "'int " << names[i] << ";' has no '= expression' initializer";
  }

  EXPECT_NE(getVariable("fd"), getVariable("c"))
      << "'int fd;' and 'int c;' are two separate declarations and must be two separate Variable nodes";
}

// Unlike 21.3--feof.sv / 21.3--ferror.sv / 21.3--fflush.sv, this file
// declares nothing inside the begin-end block (IEEE 1800-2023 Sec 6.21).
TEST_F(FgetcTest, InitialBeginOwnsNoVariablesOfItsOwn) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr) << "'initial begin ... end' body should be a Begin block";
  EXPECT_TRUE(blk->getVariables() == nullptr || blk->getVariables()->empty())
      << "both 'int' declarations are at module scope, so the block declares nothing";
}

// --- process inventory ------------------------------------------------------

TEST_F(FgetcTest, ModuleHasOneInitialAndOneFinalProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 2u) << "one 'initial' and one 'final' construct";

  uint32_t nbInitial = 0;
  uint32_t nbFinal = 0;
  for (uint32_t i = 0, n = (uint32_t)top->getProcesses()->size(); i < n; ++i) {
    if (any_cast<hldb::Initial>(top->getProcesses()->at(i)) != nullptr) ++nbInitial;
    if (any_cast<hldb::FinalStmt>(top->getProcesses()->at(i)) != nullptr) ++nbFinal;
  }
  EXPECT_EQ(nbInitial, 1u);
  EXPECT_EQ(nbFinal, 1u);
}

TEST_F(FgetcTest, InitialBeginHasExactlyTwoStatements) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 2u) << "the block holds the $fopen assignment and the $fgetc assignment";
}

// --- stmt[0]: fd = $fopen("tmp.txt", "w") -----------------------------------

TEST_F(FgetcTest, FopenAssignmentIsBlockingIntoFd) {
  const hldb::Assignment *const assign = getAssignment(0);
  ASSERT_NE(assign, nullptr) << "stmt[0] should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "'fd = $fopen(...)' uses the blocking operator '='";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "assignment lhs should be a RefObj";
  EXPECT_EQ(lhs->getName(), "fd");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariable("fd"));
  EXPECT_NE(lhs->getActual<hldb::Variable>(), getVariable("c")) << "the descriptor is written into 'fd', not into 'c'";
}

// IEEE 1800-2023 Sec 21.3.1: $fopen is a system FUNCTION returning the
// descriptor, and the assignment consumes that return value.
TEST_F(FgetcTest, FopenRhsIsSysFuncCallWithFilenameAndMode) {
  const hldb::Assignment *const assign = getAssignment(0);
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

// --- stmt[1]: c = $fgetc(fd) -- the point of the file -----------------------

// The destination is "c", never "fd". Both are plain "int", so only the
// binding tells them apart.
TEST_F(FgetcTest, FgetcAssignmentIsBlockingIntoC) {
  const hldb::Assignment *const assign = getAssignment(1);
  ASSERT_NE(assign, nullptr) << "stmt[1] should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "'c = $fgetc(fd)' uses the blocking operator '='";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "assignment lhs should be a RefObj";
  EXPECT_EQ(lhs->getName(), "c");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariable("c"));
  EXPECT_NE(lhs->getActual<hldb::Variable>(), getVariable("fd"))
      << "the byte read is written into 'c', not back into the descriptor 'fd'";
}

// IEEE 1800-2023 Sec 21.3.4: $fgetc is a system FUNCTION of exactly one
// argument -- the descriptor -- returning the byte read. The operand is
// "fd", never "c".
TEST_F(FgetcTest, FgetcRhsIsSysFuncCallOnTheDescriptor) {
  const hldb::Assignment *const assign = getAssignment(1);
  ASSERT_NE(assign, nullptr);

  EXPECT_EQ(assign->getRhs<hldb::SysTaskCall>(), nullptr)
      << "IEEE 1800-2023 Sec 21.3.4: $fgetc returns the byte read, so it must be a SysFuncCall, not a SysTaskCall";
  const hldb::SysFuncCall *const call = assign->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(call, nullptr) << "assignment rhs should be a SysFuncCall";
  EXPECT_EQ(call->getName(), "$fgetc");

  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "'$fgetc(fd)' takes exactly one descriptor argument";
  EXPECT_EQ(any_cast<hldb::Constant>(call->getArguments()->at(0)), nullptr)
      << "the argument is the variable 'fd', not a literal";
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "the $fgetc argument should be a RefObj";
  EXPECT_EQ(arg->getName(), "fd");
  EXPECT_EQ(arg->getActual<hldb::Variable>(), getVariable("fd"))
      << "the descriptor read must be the same 'fd' the $fopen assignment wrote";
  EXPECT_NE(arg->getActual<hldb::Variable>(), getVariable("c")) << "$fgetc reads the descriptor, not the result slot";
}

// --- final $fclose(fd) ------------------------------------------------------

// IEEE 1800-2023 Sec 9.2.3: "final" followed by a single
// statement_or_null with no begin/end -- the body must be the task call
// itself, not a Begin wrapper.
TEST_F(FgetcTest, FinalStmtIsDirectlyAnFcloseSysTaskCall) {
  const hldb::FinalStmt *const fin = getFinal();
  ASSERT_NE(fin, nullptr) << "the module's 'final' construct should be a FinalStmt process";
  EXPECT_EQ(fin->getStmt<hldb::Begin>(), nullptr)
      << "IEEE 1800-2023 Sec 9.2.3: a single-statement 'final' body must not be wrapped in a Begin block";

  const hldb::SysTaskCall *const call = fin->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(call, nullptr) << "IEEE 1800-2023 Sec 21.3.1: $fclose is a task, so it must be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$fclose");

  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "'$fclose(fd)' takes exactly one descriptor argument";
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "the $fclose argument should be a RefObj";
  EXPECT_EQ(arg->getName(), "fd");
  EXPECT_EQ(arg->getActual<hldb::Variable>(), getVariable("fd"))
      << "the descriptor closed in 'final' must be the same 'fd' the initial block opened";
}

// --- compiler diagnostics ---------------------------------------------------

TEST_F(FgetcTest, CompilerReportsNoFatalOrSyntaxErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

// "fd" is referenced from three places, one of them in a different
// process -- the construct here with the most real binding risk.
TEST_F(FgetcTest, DescriptorAndResultReferencesBind) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "fd"), nullptr)
      << "'fd' must bind to the 'int fd' declaration in the assignment, in $fgetc, and in $fclose";
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "c"), nullptr)
      << "'c' must bind to the 'int c' declaration in the $fgetc assignment";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
