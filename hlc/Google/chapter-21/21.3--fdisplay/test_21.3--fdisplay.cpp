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

// Tests for 21.3--fdisplay.sv (tags: 21.3)
//
//   module top();
//     int fd;
//     string str = "abc";
//     initial begin
//       fd = $fopen("tmp.txt", "w");
//       $fdisplay(fd, str);
//     end
//     final
//       $fclose(fd);
//   endmodule
//
// What this file is about, and why each check below is the right one
// (all expectations were derived by reading the .sv source against the
// LRM, before any of this test code was written):
//
//   IEEE 1800-2023 Sec 21.3.1 "Opening and closing files": $fopen is a
//   system FUNCTION -- "fd = $fopen(filename, type);". Because a type
//   argument ("w") is supplied, it returns a single-channel file
//   descriptor rather than a multichannel descriptor. The distinguishing
//   structural fact for HLDB is therefore that the right-hand side of
//   "fd = $fopen(...)" must be a SysFuncCall, never a SysTaskCall: the
//   return value is consumed by an assignment. $fclose, by contrast, is
//   a system TASK in the same clause, so it must be a SysTaskCall.
//
//   IEEE 1800-2023 Sec 21.3.2 "File output system tasks": $fdisplay is a
//   system TASK whose first argument is the file descriptor (or MCD) and
//   whose remaining arguments are exactly those of $display. That is the
//   whole point of this file, and it is what separates it from
//   21.2--display.sv: the descriptor argument comes FIRST, and here the
//   payload argument is a plain variable reference "str", not a format
//   string literal. So the SysTaskCall must carry exactly 2 arguments,
//   arg[0] being a RefObj that binds back to the "int fd" declaration
//   and arg[1] being a RefObj that binds back to the "string str"
//   declaration. If either were modeled as a Constant, or if the
//   argument order were swapped, $fdisplay would have been mis-modeled.
//
//   IEEE 1800-2023 Sec 6.11.1 "Integer data types": "int" is a 2-state
//   SIGNED 32-bit integer_atom_type -> IntTypespec with vpiSigned set,
//   and "int fd;" has no initializer, so no declaration-time value.
//
//   IEEE 1800-2023 Sec 6.16 "String data type" and Sec 5.9 "String
//   literals": "string str" is a StringTypespec variable; its
//   initializer "abc" is a vpiStringConst whose size is 8 x 3 = 24 bits
//   ("the number of bits required to hold a string is 8 times the number
//   of characters", and "abc" contains no escape sequences, so the count
//   is unambiguous).
//
//   IEEE 1800-2023 Sec 6.7/6.8: neither "int" nor "string" is a net_type
//   keyword, and there is no port list to trigger an implicit-net rule,
//   so both fd and str must be Variables and neither may also appear as
//   a Net.
//
//   IEEE 1800-2023 Sec 9.2.3 "final construct": a final_construct is
//   "final" followed by a single statement_or_null. This source has no
//   begin/end after "final", so the FinalStmt's body must be the
//   $fclose SysTaskCall directly, not a Begin wrapper -- the same shape
//   established by 9.2.3--final.sv.
//
//   IEEE 1800-2023 Sec 23.2.2: "module top();" declares an empty
//   list_of_ports, so the module has zero ports.
//
// Checked:
//   - module "top" exists, has zero ports and zero nets
//   - exactly 2 variables: "fd" (IntTypespec, signed, no initializer)
//     and "str" (StringTypespec, initializer Constant vpiStringConst
//     value "abc", decompile "\"abc\"", size 24)
//   - module has exactly 2 processes: exactly one Initial and exactly
//     one FinalStmt
//   - the Initial's body is a Begin with exactly 2 statements
//   - stmt[0] is a blocking Assignment; lhs is RefObj "fd" resolving to
//     Variable fd; rhs is a SysFuncCall (NOT a SysTaskCall) named
//     "$fopen" with exactly 2 Constant arguments, "tmp.txt" (size 56)
//     and "w" (size 8), both vpiStringConst
//   - stmt[1] is a SysTaskCall named "$fdisplay" with exactly 2
//     arguments: RefObj "fd" resolving Variable fd, then RefObj "str"
//     resolving Variable str -- in that order, and neither a Constant
//   - the FinalStmt's body is directly a SysTaskCall named "$fclose"
//     (no Begin wrapper) with exactly 1 argument: RefObj "fd" resolving
//     Variable fd
//   - the compiler reports no fatal/syntax errors and no failed bind for
//     "fd" or "str"
//
// What is NOT checked, and why:
//   - Whether "tmp.txt" is actually created on disk, and what integer
//     $fopen returns for it (Sec 21.3.1: nonzero on success, zero on
//     failure). This is permanently out of scope: HLC is a static
//     compiler/elaborator and never opens the file. The static half of
//     the same concern is covered by
//     AssignmentRhsIsFopenSysFuncCallWithFilenameAndMode, which pins the
//     call shape, name, and both literal arguments.
//   - The text $fdisplay writes, its radix, and the newline Sec 21.3.2
//     appends. Permanently out of scope -- nothing is printed without
//     time advancing. The nearest real assertion is
//     FdisplayCallPassesDescriptorThenStringVariable, which pins what is
//     handed to the task.
//   - The value held by "fd" or "str" at the moment $fdisplay executes.
//     Variable::getValue<T>() exposes only a declaration-time
//     initializer; there is no field anywhere that records a
//     post-assignment runtime value. Permanently out of scope. The
//     nearest real assertion is StrVariableInitialValueIsAbc, which
//     checks the one value the source itself fixes.
//   - That $fclose runs at the end of simulation (Sec 9.2.3). Only the
//     static shape of the final construct is observable here; that is
//     what FinalStmtIsDirectlyAnFcloseSysTaskCall asserts.
//   - The design-level typespec count and which scope owns the shared
//     StringTypespec / IntTypespec nodes. That is a tool-internal
//     sharing convention the source text does not determine, so it is
//     deliberately left unasserted rather than guessed.

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
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FdisplayTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.3--fdisplay.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Variable *getVariable(std::string_view name) {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getVariables() == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, top->getVariables());
  }

  // The module holds two processes (the initial and the final construct).
  // Their relative order in the collection is a tool-internal detail the
  // source does not fix, so both are located by node kind instead of by
  // index.
  static const hldb::Initial *getInitial() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr) return nullptr;
    const hldb::Initial *found = nullptr;
    for (uint32_t i = 0, n = (uint32_t)top->getProcesses()->size(); i < n; ++i) {
      const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(i));
      if (init != nullptr) found = init;
    }
    return found;
  }

  static const hldb::FinalStmt *getFinal() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr) return nullptr;
    const hldb::FinalStmt *found = nullptr;
    for (uint32_t i = 0, n = (uint32_t)top->getProcesses()->size(); i < n; ++i) {
      const hldb::FinalStmt *const fin = any_cast<hldb::FinalStmt>(top->getProcesses()->at(i));
      if (fin != nullptr) found = fin;
    }
    return found;
  }

  static const hldb::Begin *getInitialBody() {
    const hldb::Initial *const init = getInitial();
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }
};

// --- module shell -----------------------------------------------------------

TEST_F(FdisplayTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// IEEE 1800-2023 Sec 23.2.2: "module top();" declares an empty
// list_of_ports.
TEST_F(FdisplayTest, ModuleHasNoPorts) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top();' declares an empty port list";
}

// IEEE 1800-2023 Sec 6.7/6.8: neither "int" nor "string" is a net_type
// keyword, so neither fd nor str may appear in the net collection.
TEST_F(FdisplayTest, ModuleHasNoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  if (top->getNets() != nullptr) {
    EXPECT_TRUE(top->getNets()->empty()) << "'int' and 'string' are data types, not net types";
    EXPECT_EQ(hldb::findByName<hldb::Net>("fd", top->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>("str", top->getNets()), nullptr);
  }
}

// --- variable declarations --------------------------------------------------

TEST_F(FdisplayTest, ModuleHasExactlyTwoVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 2u) << "the module declares exactly 'int fd' and 'string str'";
}

TEST_F(FdisplayTest, FdVariableIsSignedInt) {
  const hldb::Variable *const fd = getVariable("fd");
  ASSERT_NE(fd, nullptr) << "variable 'fd' not found";
  ASSERT_NE(fd->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = fd->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "IEEE 1800-2023 Sec 6.11.1: 'int' must resolve to an IntTypespec";
  EXPECT_TRUE(it->getSigned()) << "IEEE 1800-2023 Sec 6.11.1: 'int' is a 2-state signed 32-bit type";
}

// "int fd;" carries no '= expression', so there is no declaration-time
// value. $fopen's result is assigned procedurally inside the initial
// block, which is a statement, not an initializer.
TEST_F(FdisplayTest, FdVariableHasNoDeclarationTimeInitializer) {
  const hldb::Variable *const fd = getVariable("fd");
  ASSERT_NE(fd, nullptr);
  EXPECT_EQ(fd->getValue(), nullptr) << "'int fd;' has no '= expression' initializer";
}

TEST_F(FdisplayTest, StrVariableIsString) {
  const hldb::Variable *const str = getVariable("str");
  ASSERT_NE(str, nullptr) << "variable 'str' not found";
  ASSERT_NE(str->getTypespec(), nullptr);
  EXPECT_NE(str->getTypespec()->getActual<hldb::StringTypespec>(), nullptr)
      << "IEEE 1800-2023 Sec 6.16: 'string' must resolve to a StringTypespec";
}

TEST_F(FdisplayTest, StrVariableInitialValueIsAbc) {
  const hldb::Variable *const str = getVariable("str");
  ASSERT_NE(str, nullptr);
  const hldb::Constant *const init = str->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'string str = \"abc\";' must carry a declaration-time initializer";
  EXPECT_EQ(init->getConstType(), vpiStringConst) << "IEEE 1800-2023 Sec 5.9: a string literal is vpiStringConst";
  EXPECT_EQ(init->getValue(), "abc");
  EXPECT_EQ(init->getDecompile(), "\"abc\"");
  EXPECT_EQ(init->getSize(), 24) << "IEEE 1800-2023 Sec 5.9: \"abc\" = 3 characters x 8 bits = 24 bits";
}

// --- process inventory ------------------------------------------------------

TEST_F(FdisplayTest, ModuleHasOneInitialAndOneFinalProcess) {
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

TEST_F(FdisplayTest, InitialBodyHasExactlyTwoStatements) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr) << "'initial begin ... end' body should be a Begin block";
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 2u) << "the initial block holds the $fopen assignment and the $fdisplay call";
}

// --- stmt[0]: fd = $fopen("tmp.txt", "w") -----------------------------------

TEST_F(FdisplayTest, FopenAssignmentIsBlockingIntoFd) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_GE(blk->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "stmt[0] should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "'fd = $fopen(...)' uses the blocking operator '='";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "assignment lhs should be a RefObj";
  EXPECT_EQ(lhs->getName(), "fd");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariable("fd"));
}

// IEEE 1800-2023 Sec 21.3.1: $fopen is a system FUNCTION whose return
// value is the file descriptor. Consuming that value in an assignment is
// what makes SysFuncCall the only correct node kind here; a SysTaskCall
// would mean the return value had been discarded.
TEST_F(FdisplayTest, AssignmentRhsIsFopenSysFuncCallWithFilenameAndMode) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_GE(blk->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
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

// --- stmt[1]: $fdisplay(fd, str) --------------------------------------------

// IEEE 1800-2023 Sec 21.3.2: $fdisplay is a system TASK; its first
// argument is the file descriptor and the rest follow $display's rules.
// This is the construct the file is named for.
TEST_F(FdisplayTest, FdisplayCallPassesDescriptorThenStringVariable) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_GE(blk->getStmts()->size(), 2u);

  const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(1));
  ASSERT_NE(call, nullptr) << "IEEE 1800-2023 Sec 21.3.2: $fdisplay is a task, so stmt[1] must be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$fdisplay");

  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u) << "'$fdisplay(fd, str)' passes a descriptor and one value";

  const hldb::RefObj *const descriptor = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(descriptor, nullptr) << "arg[0] must be the descriptor reference, not a literal";
  EXPECT_EQ(descriptor->getName(), "fd");
  EXPECT_EQ(descriptor->getActual<hldb::Variable>(), getVariable("fd"));

  // Unlike 21.2--display.sv, the payload here is a variable, not a
  // format string literal -- so it must be a RefObj, never a Constant.
  EXPECT_EQ(any_cast<hldb::Constant>(call->getArguments()->at(1)), nullptr)
      << "'str' is a variable reference, not a string literal";
  const hldb::RefObj *const payload = any_cast<hldb::RefObj>(call->getArguments()->at(1));
  ASSERT_NE(payload, nullptr) << "arg[1] should be a RefObj";
  EXPECT_EQ(payload->getName(), "str");
  EXPECT_EQ(payload->getActual<hldb::Variable>(), getVariable("str"));
}

// --- final $fclose(fd) ------------------------------------------------------

// IEEE 1800-2023 Sec 9.2.3: "final" followed by a single
// statement_or_null with no begin/end -- the body must be the task call
// itself, not a Begin wrapper.
TEST_F(FdisplayTest, FinalStmtIsDirectlyAnFcloseSysTaskCall) {
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

TEST_F(FdisplayTest, CompilerReportsNoFatalOrSyntaxErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

// The two references with real binding risk are "fd" (used in three
// separate places, one of them in a different process) and "str".
TEST_F(FdisplayTest, DescriptorAndStringReferencesBind) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "fd"), nullptr)
      << "'fd' must bind to the 'int fd' declaration in the assignment, in $fdisplay, and in $fclose";
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "str"), nullptr)
      << "'str' must bind to the 'string str' declaration in $fdisplay";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
