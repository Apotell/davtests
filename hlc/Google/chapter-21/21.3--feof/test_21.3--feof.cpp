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

// Tests for 21.3--feof.sv (tags: 21.3)
//
//   module top();
//     initial begin
//       int fd;
//       fd = $fopen("tmp.txt", "w");
//       $display($feof(fd));
//       $fclose(fd);
//     end
//   endmodule
//
// What this file is about, and why each check below is the right one
// (every expectation was derived by reading the .sv source against the
// LRM, before any of this test code was written):
//
//   IEEE 1800-2023 Sec 21.3.8 "Detecting EOF": $feof is a system
//   FUNCTION -- it takes a file descriptor and RETURNS a value (nonzero
//   once EOF has been detected on that descriptor, zero otherwise). Here
//   that return value is not assigned anywhere; it is handed straight to
//   $display as its only argument. So the structural fact this file
//   exists to pin down is a nesting: a SysTaskCall ($display) whose
//   single argument is itself a SysFuncCall ($feof), whose own single
//   argument is a RefObj bound to fd. If HLC flattened that -- modeling
//   the argument as a bare RefObj "fd", or as a Constant, or as a second
//   SysTaskCall -- the call would have been mis-modeled, so those shapes
//   are ruled out explicitly and not merely left unmentioned.
//
//   IEEE 1800-2023 Sec 21.3.1 "Opening and closing files": $fopen is a
//   system FUNCTION whose return value is consumed by an assignment, so
//   the rhs of "fd = $fopen(...)" must be a SysFuncCall, never a
//   SysTaskCall. Because a type argument ("w") is supplied, this is the
//   two-argument form that yields a single-channel file descriptor
//   rather than a multichannel descriptor. $fclose, in the same clause,
//   is a system TASK, so it must be a SysTaskCall.
//
//   IEEE 1800-2023 Sec 21.2 "Display system tasks": $display is a system
//   TASK, hence a SysTaskCall and not an expression.
//
//   IEEE 1800-2023 Sec 6.21 "Scope and lifetime" with Sec 9.3.1
//   "Sequential blocks": this is the corner that separates this file
//   from its sibling 21.3--fdisplay.sv. There, "int fd" is declared at
//   module scope; here it is declared INSIDE the begin-end block, so it
//   is local to that block. It must therefore appear in the Begin's own
//   getVariables() and must NOT appear at module scope at all. Both
//   halves are asserted, and all three references to fd are checked for
//   object identity against that one block-local Variable -- proving
//   they bound to the local declaration rather than to some implicitly
//   created object.
//
//   IEEE 1800-2023 Sec 6.11.1 "Integer data types": "int" is a 2-state
//   SIGNED 32-bit integer_atom_type -> IntTypespec with vpiSigned set.
//   "int fd;" has no "= expression", so there is no declaration-time
//   value; $fopen's result arrives via a procedural statement instead.
//
//   IEEE 1800-2023 Sec 6.7/6.8: "int" is a data_type keyword, never a
//   net_type keyword, and the module has no port list to trigger any
//   implicit-net rule, so nothing in this file is a Net.
//
//   IEEE 1800-2023 Sec 5.9 "String literals": "tmp.txt" and "w" are
//   vpiStringConst, sized at 8 bits per character -- 7 x 8 = 56 and
//   1 x 8 = 8. Neither contains an escape sequence, so the character
//   counts are unambiguous.
//
//   IEEE 1800-2023 Sec 23.2.2: "module top();" declares an empty
//   list_of_ports, so the module has zero ports.
//
// Checked:
//   - module "top" exists with zero ports, zero nets, and -- unlike
//     21.3--fdisplay.sv -- zero module-scope variables
//   - the module has exactly 1 process, an Initial (there is no final
//     construct in this file)
//   - the Initial's body is a Begin owning exactly 1 local Variable
//     "fd": IntTypespec, signed, no declaration-time initializer
//   - that Begin has exactly 3 statements
//   - stmt[0] is a blocking Assignment; lhs is RefObj "fd" resolving to
//     the block-local Variable; rhs is a SysFuncCall (explicitly NOT a
//     SysTaskCall) named "$fopen" with exactly 2 arguments: Constant
//     "tmp.txt" (vpiStringConst, size 56) and Constant "w"
//     (vpiStringConst, size 8)
//   - stmt[1] is a SysTaskCall named "$display" with exactly 1 argument,
//     and that argument is neither a Constant nor a bare RefObj
//   - that single argument is a SysFuncCall named "$feof" with exactly
//     1 argument: RefObj "fd" resolving to the same block-local Variable
//   - stmt[2] is a SysTaskCall named "$fclose" with exactly 1 argument:
//     RefObj "fd" resolving to the same block-local Variable
//   - the compiler reports no fatal/syntax/error diagnostics, and "fd"
//     never fails to bind
//
// What is NOT checked, and why:
//   - What $feof actually returns. Sec 21.3.8 defines that as a property
//     of the stream's state at the moment of the call, which exists only
//     while time advances; HLC is a static compiler/elaborator and never
//     reads the file. Permanently out of scope. The static half of the
//     same concern is DisplayArgumentIsFeofSysFuncCallOnFd, which pins
//     the call's kind, name, arity, and operand binding.
//   - Whether "tmp.txt" is created on disk and what integer $fopen
//     hands back (Sec 21.3.1: nonzero on success, zero on failure).
//     Permanently out of scope for the same reason; the nearest real
//     assertion is AssignmentRhsIsFopenSysFuncCallWithFilenameAndMode.
//   - What $display prints, the decimal radix it uses for the integer
//     $feof result, and the newline Sec 21.2 appends. Nothing is printed
//     without simulation, so this is permanently out of scope. The
//     nearest real assertion is DisplayCallTakesExactlyOneComputedValue,
//     which pins what is handed to the task.
//   - The value held by "fd" between the $fopen assignment and the
//     $fclose call. Variable::getValue<T>() exposes only a
//     declaration-time initializer, and "int fd;" has none -- which is
//     itself asserted by InitialBeginDeclaresOneLocalSignedIntFd.
//     Permanently out of scope.
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

class FeofTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.3--feof.hlc"}); }
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
  // so the one and only declaration of fd is owned by the Begin.
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
};

// --- module shell -----------------------------------------------------------

TEST_F(FeofTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// IEEE 1800-2023 Sec 23.2.2: "module top();" declares an empty
// list_of_ports.
TEST_F(FeofTest, ModuleHasNoPorts) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top();' declares an empty port list";
}

// IEEE 1800-2023 Sec 6.7/6.8: the file's only declaration uses "int", a
// data_type keyword, and there is no port list, so nothing here is a Net.
TEST_F(FeofTest, ModuleHasNoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  if (top->getNets() != nullptr) {
    EXPECT_TRUE(top->getNets()->empty()) << "'int' is a data type, not a net type";
    EXPECT_EQ(hldb::findByName<hldb::Net>("fd", top->getNets()), nullptr);
  }
}

// The corner that distinguishes this file from 21.3--fdisplay.sv:
// IEEE 1800-2023 Sec 6.21 makes "int fd;" local to the begin-end block,
// so it must not surface at module scope.
TEST_F(FeofTest, ModuleDeclaresNoVariablesOfItsOwn) {
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

// Unlike 21.3--fdisplay.sv, this file has no final construct: the single
// initial block does the open, the query, and the close.
TEST_F(FeofTest, ModuleHasExactlyOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u) << "the module holds one 'initial' construct and nothing else";
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr) << "the one process must be an Initial";
  EXPECT_EQ(any_cast<hldb::FinalStmt>(top->getProcesses()->at(0)), nullptr)
      << "this source contains no 'final' construct";
}

// --- the block-local descriptor ---------------------------------------------

TEST_F(FeofTest, InitialBeginDeclaresOneLocalSignedIntFd) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr) << "'initial begin ... end' body should be a Begin block";
  ASSERT_NE(blk->getVariables(), nullptr) << "the Begin owns the block-local 'int fd;' declaration";
  ASSERT_EQ(blk->getVariables()->size(), 1u) << "the block declares exactly one variable";

  const hldb::Variable *const fd = getLocalFd();
  ASSERT_NE(fd, nullptr) << "block-local variable 'fd' not found";
  ASSERT_NE(fd->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = fd->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "IEEE 1800-2023 Sec 6.11.1: 'int' must resolve to an IntTypespec";
  EXPECT_TRUE(it->getSigned()) << "IEEE 1800-2023 Sec 6.11.1: 'int' is a 2-state signed 32-bit type";
  EXPECT_EQ(fd->getValue(), nullptr) << "'int fd;' has no '= expression' initializer";
}

TEST_F(FeofTest, InitialBeginHasExactlyThreeStatements) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 3u) << "the block holds the $fopen assignment, the $display call, and $fclose";
}

// --- stmt[0]: fd = $fopen("tmp.txt", "w") -----------------------------------

TEST_F(FeofTest, FopenAssignmentIsBlockingIntoLocalFd) {
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
// descriptor. The assignment consumes that return value, so SysFuncCall
// is the only correct node kind; a SysTaskCall would mean the result had
// been thrown away.
TEST_F(FeofTest, AssignmentRhsIsFopenSysFuncCallWithFilenameAndMode) {
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

// --- stmt[1]: $display($feof(fd)) -------------------------------------------

// IEEE 1800-2023 Sec 21.2: $display is a system TASK. Its whole argument
// list here is the single computed value $feof(fd) -- there is no format
// string, so exactly one argument, and it is not a literal.
TEST_F(FeofTest, DisplayCallTakesExactlyOneComputedValue) {
  const hldb::SysTaskCall *const disp = getStmt<hldb::SysTaskCall>(1);
  ASSERT_NE(disp, nullptr) << "IEEE 1800-2023 Sec 21.2: $display is a task, so stmt[1] must be a SysTaskCall";
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u) << "'$display($feof(fd))' passes exactly one argument";

  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0)), nullptr)
      << "the argument is a call to be evaluated, not a literal";
  EXPECT_EQ(any_cast<hldb::RefObj>(disp->getArguments()->at(0)), nullptr)
      << "the argument is '$feof(fd)', not the bare name 'fd' -- the call must not be flattened away";
}

// The point of the file: IEEE 1800-2023 Sec 21.3.8 makes $feof a system
// FUNCTION, so the $display argument must be a SysFuncCall nested one
// level down, carrying the descriptor as its own single operand.
TEST_F(FeofTest, DisplayArgumentIsFeofSysFuncCallOnFd) {
  const hldb::SysTaskCall *const disp = getStmt<hldb::SysTaskCall>(1);
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);

  const hldb::SysFuncCall *const feof = any_cast<hldb::SysFuncCall>(disp->getArguments()->at(0));
  ASSERT_NE(feof, nullptr)
      << "IEEE 1800-2023 Sec 21.3.8: $feof returns a value, so it must be a SysFuncCall, not a SysTaskCall";
  EXPECT_EQ(feof->getName(), "$feof");

  ASSERT_NE(feof->getArguments(), nullptr);
  ASSERT_EQ(feof->getArguments()->size(), 1u) << "'$feof(fd)' takes exactly one descriptor argument";
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(feof->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "the $feof argument should be a RefObj";
  EXPECT_EQ(arg->getName(), "fd");
  EXPECT_EQ(arg->getActual<hldb::Variable>(), getLocalFd())
      << "the descriptor queried must be the same block-local 'fd' the $fopen assignment wrote";
}

// --- stmt[2]: $fclose(fd) ---------------------------------------------------

TEST_F(FeofTest, FcloseCallClosesTheSameDescriptor) {
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

// --- compiler diagnostics ---------------------------------------------------

TEST_F(FeofTest, CompilerReportsNoFatalOrSyntaxErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

// "fd" is referenced three times, and its declaration is block-scoped
// rather than module-scoped -- the one construct here with real binding
// risk.
TEST_F(FeofTest, BlockLocalDescriptorReferenceBinds) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "fd"), nullptr)
      << "'fd' must bind to the block-local 'int fd' declaration in the assignment, in $feof, and in $fclose";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
