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

// Tests for 21.3--ferror.sv (tags: 21.3)
//
//   module top();
//     initial begin
//       int fd;
//       string str;
//       integer errno;
//       fd = $fopen("tmp.txt", "w");
//       errno = $ferror(fd, str);
//       $display(errno);
//       $display(str);
//       $fclose(fd);
//     end
//   endmodule
//
// What this file is about, and why each check below is the right one
// (every expectation was derived by reading the .sv source against the
// LRM, before any of this test code was written):
//
//   IEEE 1800-2023 Sec 21.3.7 "I/O error status": $ferror is a system
//   FUNCTION taking TWO arguments -- the file descriptor and a string
//   variable that the call fills in with a description of the error --
//   and RETURNING the error number. That two-argument, value-returning
//   shape is exactly what this file exists to pin down, and it is what
//   separates it from its siblings: 21.3--feof.sv calls a one-argument
//   query and drops the result straight into $display, whereas here the
//   result is assigned, and a second operand is carried along. So the
//   rhs of "errno = $ferror(fd, str)" must be a SysFuncCall (never a
//   SysTaskCall, since the return value is consumed) with exactly 2
//   arguments, arg[0] a RefObj bound to fd and arg[1] a RefObj bound to
//   str -- emphatically not a string literal, which is the shape a
//   careless model of "a system task that takes a message" would produce.
//
//   IEEE 1800-2023 Sec 21.3.1 "Opening and closing files": $fopen is a
//   system FUNCTION whose result is consumed by an assignment, so its
//   node kind is SysFuncCall as well; supplying the type argument ("w")
//   selects the two-argument form yielding a single-channel file
//   descriptor. $fclose, in the same clause, is a system TASK.
//
//   IEEE 1800-2023 Sec 21.2 "Display system tasks": $display is a system
//   TASK. This file calls it twice, with no format string either time --
//   just one bare value argument each -- so each call carries exactly 1
//   argument and neither argument is a Constant.
//
//   IEEE 1800-2023 Sec 6.11 "Integer data types", Table 6-8: "int" is a
//   2-state 32-bit SIGNED type and "integer" is a 4-state 32-bit SIGNED
//   type. They are distinct types, so they must resolve to distinct
//   typespec kinds: fd -> IntTypespec, errno -> IntegerTypespec. This is
//   the only file in the 21.3 group that declares both side by side, so
//   the negative half is asserted too (fd is not an IntegerTypespec and
//   errno is not an IntTypespec); collapsing the two would be a real
//   modeling bug that a positive-only check would miss.
//
//   IEEE 1800-2023 Sec 6.16 "String data type": "string str;" resolves
//   to a StringTypespec. It has no "= expression", so -- like the other
//   two locals -- it carries no declaration-time value.
//
//   IEEE 1800-2023 Sec 6.21 "Scope and lifetime" with Sec 9.3.1
//   "Sequential blocks": all three declarations sit INSIDE the
//   begin-end block, so all three are local to it. They must appear in
//   the Begin's own getVariables() and must not surface at module scope.
//   Every reference below is checked for object identity against the
//   matching block-local Variable, which proves the names bound to these
//   declarations rather than to implicitly created objects.
//
//   IEEE 1800-2023 Sec 6.7/6.8: "int", "string" and "integer" are all
//   data_type keywords, never net_type keywords, and the module has no
//   port list to trigger an implicit-net rule, so nothing here is a Net.
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
//   - the Initial's body is a Begin owning exactly 3 local Variables:
//     "fd" (IntTypespec, signed, and NOT an IntegerTypespec), "str"
//     (StringTypespec), "errno" (IntegerTypespec, signed, and NOT an
//     IntTypespec); none of the three has a declaration-time initializer
//   - that Begin has exactly 5 statements
//   - stmt[0]: blocking Assignment; lhs RefObj "fd" resolving to the
//     block-local fd; rhs a SysFuncCall (explicitly not a SysTaskCall)
//     named "$fopen" with 2 arguments, Constant "tmp.txt"
//     (vpiStringConst, size 56) and Constant "w" (vpiStringConst, size 8)
//   - stmt[1]: blocking Assignment; lhs RefObj "errno" resolving to the
//     block-local errno; rhs a SysFuncCall (explicitly not a
//     SysTaskCall) named "$ferror" with exactly 2 arguments, RefObj "fd"
//     and RefObj "str", each resolving to its block-local Variable, and
//     arg[1] explicitly not a Constant
//   - stmt[2]: SysTaskCall "$display" with exactly 1 argument, RefObj
//     "errno" resolving to the block-local errno, not a Constant
//   - stmt[3]: SysTaskCall "$display" with exactly 1 argument, RefObj
//     "str" resolving to the block-local str, not a Constant; and it is
//     a distinct node from stmt[2]
//   - stmt[4]: SysTaskCall "$fclose" with exactly 1 argument, RefObj
//     "fd" resolving to the block-local fd
//   - the compiler reports no fatal/syntax/error diagnostics, and none
//     of "fd", "str", "errno" fails to bind
//
// What is NOT checked, and why:
//   - The error number $ferror returns and the message text it writes
//     into "str" (Sec 21.3.7). Both describe the state of a stream at
//     the moment of the call, which exists only while time advances;
//     HLC is a static compiler/elaborator and never touches the file.
//     Permanently out of scope. The static half of the same concern is
//     FerrorRhsIsSysFuncCallOnDescriptorAndStringVariable, which pins
//     the call's kind, name, arity, and both operand bindings.
//   - That "str" is an OUTPUT argument of $ferror rather than an input.
//     Sec 21.3.7 defines that direction, but a SysFuncCall's actual
//     argument list carries no per-formal direction to read it back
//     from, so it is not observable in the model and is permanently out
//     of scope. What IS observable, and is asserted, is that arg[1] is a
//     RefObj bound to the block-local "str" rather than a literal --
//     which is the structural precondition for it to be writable at all.
//   - Whether "tmp.txt" is created on disk and what integer $fopen hands
//     back (Sec 21.3.1: nonzero on success, zero on failure).
//     Permanently out of scope; the nearest real assertion is
//     FopenRhsIsSysFuncCallWithFilenameAndMode.
//   - What the two $display calls print, the radix used for the integer
//     errno, and the newlines Sec 21.2 appends. Nothing is printed
//     without simulation, so this is permanently out of scope. The
//     nearest real assertions are DisplayErrnoPassesTheIntegerVariable
//     and DisplayStrPassesTheStringVariable, which pin what is handed to
//     each task.
//   - The runtime values of fd, str and errno. Variable::getValue<T>()
//     exposes only a declaration-time initializer, and none of the three
//     declarations has one -- which is itself asserted by
//     InitialBeginDeclaresThreeLocalsWithNoInitializers. Permanently out
//     of scope.
//   - That the five statements actually execute in the order written.
//     Their order in the Begin's statement list IS asserted below, one
//     index at a time; whether control reaches them in that order is a
//     simulation fact and permanently out of scope.
//   - The design-level typespec count, and whether the block-local
//     typespecs are registered in the Begin's own getTypespecs() or
//     shared at design scope. That is a tool-internal node-sharing
//     convention the source text does not determine, so it is
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
#include <hldb/integer_typespec.h>
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

class FerrorTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.3--ferror.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }

  // IEEE 1800-2023 Sec 6.21: all three declarations sit inside the
  // begin-end block, so the Begin owns them.
  static const hldb::Variable *getLocal(std::string_view name) {
    const hldb::Begin *const blk = getInitialBody();
    if (blk == nullptr || blk->getVariables() == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, blk->getVariables());
  }

  template <typename T>
  static const T *getStmt(uint32_t index) {
    const hldb::Begin *const blk = getInitialBody();
    if (blk == nullptr || blk->getStmts() == nullptr || index >= blk->getStmts()->size()) return nullptr;
    return any_cast<T>(blk->getStmts()->at(index));
  }
};

// --- module shell -----------------------------------------------------------

TEST_F(FerrorTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// IEEE 1800-2023 Sec 23.2.2: "module top();" declares an empty
// list_of_ports.
TEST_F(FerrorTest, ModuleHasNoPorts) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top();' declares an empty port list";
}

// IEEE 1800-2023 Sec 6.7/6.8: int/string/integer are data_type keywords,
// and there is no port list, so nothing here is a Net.
TEST_F(FerrorTest, ModuleHasNoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  if (top->getNets() != nullptr) {
    EXPECT_TRUE(top->getNets()->empty()) << "'int', 'string' and 'integer' are data types, not net types";
    EXPECT_EQ(hldb::findByName<hldb::Net>("fd", top->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>("str", top->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>("errno", top->getNets()), nullptr);
  }
}

// IEEE 1800-2023 Sec 6.21: every declaration in this file is block-local,
// so none of them may surface at module scope.
TEST_F(FerrorTest, ModuleDeclaresNoVariablesOfItsOwn) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  if (top->getVariables() != nullptr) {
    EXPECT_TRUE(top->getVariables()->empty())
        << "IEEE 1800-2023 Sec 6.21: fd/str/errno are declared inside the begin-end block, not at module scope";
    EXPECT_EQ(hldb::findByName<hldb::Variable>("fd", top->getVariables()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Variable>("str", top->getVariables()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Variable>("errno", top->getVariables()), nullptr);
  }
}

// --- process inventory ------------------------------------------------------

TEST_F(FerrorTest, ModuleHasExactlyOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u) << "the module holds one 'initial' construct and nothing else";
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr) << "the one process must be an Initial";
  EXPECT_EQ(any_cast<hldb::FinalStmt>(top->getProcesses()->at(0)), nullptr)
      << "this source contains no 'final' construct";
}

// --- the three block-local declarations -------------------------------------

TEST_F(FerrorTest, InitialBeginDeclaresThreeLocalsWithNoInitializers) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr) << "'initial begin ... end' body should be a Begin block";
  ASSERT_NE(blk->getVariables(), nullptr) << "the Begin owns the block-local declarations";
  ASSERT_EQ(blk->getVariables()->size(), 3u) << "the block declares exactly fd, str and errno";

  const char *const names[3] = {"fd", "str", "errno"};
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::Variable *const var = getLocal(names[i]);
    ASSERT_NE(var, nullptr) << "block-local variable " << names[i] << " not found";
    EXPECT_EQ(var->getValue(), nullptr) << names[i] << " is declared without an '= expression' initializer";
  }
}

// IEEE 1800-2023 Sec 6.11 Table 6-8: "int" is the 2-state 32-bit signed
// type. The negative half matters here because this file also declares an
// "integer", and the two must not be modeled as the same typespec kind.
TEST_F(FerrorTest, FdIsASignedIntAndNotAnInteger) {
  const hldb::Variable *const fd = getLocal("fd");
  ASSERT_NE(fd, nullptr);
  ASSERT_NE(fd->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = fd->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "'int fd' must resolve to an IntTypespec";
  EXPECT_TRUE(it->getSigned()) << "Table 6-8: 'int' is a 2-state 32-bit signed type";
  EXPECT_EQ(fd->getTypespec()->getActual<hldb::IntegerTypespec>(), nullptr)
      << "'int' and 'integer' are distinct types; 'int fd' must not resolve to an IntegerTypespec";
}

// IEEE 1800-2023 Sec 6.11 Table 6-8: "integer" is the 4-state 32-bit
// signed type -- a different type from "int", checked both ways.
TEST_F(FerrorTest, ErrnoIsASignedIntegerAndNotAnInt) {
  const hldb::Variable *const errnoVar = getLocal("errno");
  ASSERT_NE(errnoVar, nullptr);
  ASSERT_NE(errnoVar->getTypespec(), nullptr);
  const hldb::IntegerTypespec *const it = errnoVar->getTypespec()->getActual<hldb::IntegerTypespec>();
  ASSERT_NE(it, nullptr) << "'integer errno' must resolve to an IntegerTypespec";
  EXPECT_TRUE(it->getSigned()) << "Table 6-8: 'integer' is a 4-state 32-bit signed type";
  EXPECT_EQ(errnoVar->getTypespec()->getActual<hldb::IntTypespec>(), nullptr)
      << "'integer' and 'int' are distinct types; 'integer errno' must not resolve to an IntTypespec";
}

// IEEE 1800-2023 Sec 6.16: "string" is the built-in dynamic string type.
TEST_F(FerrorTest, StrIsAString) {
  const hldb::Variable *const str = getLocal("str");
  ASSERT_NE(str, nullptr);
  ASSERT_NE(str->getTypespec(), nullptr);
  EXPECT_NE(str->getTypespec()->getActual<hldb::StringTypespec>(), nullptr)
      << "IEEE 1800-2023 Sec 6.16: 'string str' must resolve to a StringTypespec";
}

TEST_F(FerrorTest, InitialBeginHasExactlyFiveStatements) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 5u) << "the block holds $fopen, $ferror, two $display calls, and $fclose";
}

// --- stmt[0]: fd = $fopen("tmp.txt", "w") -----------------------------------

TEST_F(FerrorTest, FopenAssignmentIsBlockingIntoLocalFd) {
  const hldb::Assignment *const assign = getStmt<hldb::Assignment>(0);
  ASSERT_NE(assign, nullptr) << "stmt[0] should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "'fd = $fopen(...)' uses the blocking operator '='";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "assignment lhs should be a RefObj";
  EXPECT_EQ(lhs->getName(), "fd");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getLocal("fd"))
      << "the lhs must bind to the block-local 'int fd', not to any other object";
}

// IEEE 1800-2023 Sec 21.3.1: $fopen is a system FUNCTION returning the
// descriptor, and the assignment consumes that return value.
TEST_F(FerrorTest, FopenRhsIsSysFuncCallWithFilenameAndMode) {
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

// --- stmt[1]: errno = $ferror(fd, str) --------------------------------------

TEST_F(FerrorTest, FerrorAssignmentIsBlockingIntoLocalErrno) {
  const hldb::Assignment *const assign = getStmt<hldb::Assignment>(1);
  ASSERT_NE(assign, nullptr) << "stmt[1] should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "'errno = $ferror(...)' uses the blocking operator '='";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "assignment lhs should be a RefObj";
  EXPECT_EQ(lhs->getName(), "errno");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getLocal("errno"))
      << "the lhs must bind to the block-local 'integer errno'";
}

// The point of the file: IEEE 1800-2023 Sec 21.3.7 makes $ferror a system
// FUNCTION of two arguments -- a descriptor and a string variable it
// fills in -- returning the error number. Both operands must therefore be
// RefObjs bound to the block-local declarations; a literal in arg[1]
// would mean the string-output operand had been mis-modeled.
TEST_F(FerrorTest, FerrorRhsIsSysFuncCallOnDescriptorAndStringVariable) {
  const hldb::Assignment *const assign = getStmt<hldb::Assignment>(1);
  ASSERT_NE(assign, nullptr);

  EXPECT_EQ(assign->getRhs<hldb::SysTaskCall>(), nullptr)
      << "IEEE 1800-2023 Sec 21.3.7: $ferror returns the error number, so it must be a SysFuncCall, not a SysTaskCall";
  const hldb::SysFuncCall *const call = assign->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(call, nullptr) << "assignment rhs should be a SysFuncCall";
  EXPECT_EQ(call->getName(), "$ferror");

  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u) << "'$ferror(fd, str)' takes a descriptor and a string variable";

  const hldb::RefObj *const descriptor = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(descriptor, nullptr) << "arg[0] should be a RefObj";
  EXPECT_EQ(descriptor->getName(), "fd");
  EXPECT_EQ(descriptor->getActual<hldb::Variable>(), getLocal("fd"))
      << "the descriptor queried must be the same block-local 'fd' the $fopen assignment wrote";

  EXPECT_EQ(any_cast<hldb::Constant>(call->getArguments()->at(1)), nullptr)
      << "IEEE 1800-2023 Sec 21.3.7: arg[1] is the string variable $ferror fills in, not a literal message";
  const hldb::RefObj *const message = any_cast<hldb::RefObj>(call->getArguments()->at(1));
  ASSERT_NE(message, nullptr) << "arg[1] should be a RefObj";
  EXPECT_EQ(message->getName(), "str");
  EXPECT_EQ(message->getActual<hldb::Variable>(), getLocal("str"))
      << "arg[1] must bind to the block-local 'string str'";
}

// --- stmt[2] and stmt[3]: $display(errno) then $display(str) ----------------

// IEEE 1800-2023 Sec 21.2: $display is a system TASK. Neither call here
// supplies a format string, so each carries exactly one bare value.
TEST_F(FerrorTest, DisplayErrnoPassesTheIntegerVariable) {
  const hldb::SysTaskCall *const disp = getStmt<hldb::SysTaskCall>(2);
  ASSERT_NE(disp, nullptr) << "IEEE 1800-2023 Sec 21.2: $display is a task, so stmt[2] must be a SysTaskCall";
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u) << "'$display(errno)' passes exactly one argument";

  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0)), nullptr)
      << "the argument is the variable 'errno', not a format string literal";
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(disp->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "the argument should be a RefObj";
  EXPECT_EQ(arg->getName(), "errno");
  EXPECT_EQ(arg->getActual<hldb::Variable>(), getLocal("errno"))
      << "the displayed value must be the same block-local 'errno' that $ferror wrote";
}

TEST_F(FerrorTest, DisplayStrPassesTheStringVariable) {
  const hldb::SysTaskCall *const disp = getStmt<hldb::SysTaskCall>(3);
  ASSERT_NE(disp, nullptr) << "IEEE 1800-2023 Sec 21.2: $display is a task, so stmt[3] must be a SysTaskCall";
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u) << "'$display(str)' passes exactly one argument";

  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0)), nullptr)
      << "the argument is the variable 'str', not a string literal";
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(disp->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "the argument should be a RefObj";
  EXPECT_EQ(arg->getName(), "str");
  EXPECT_EQ(arg->getActual<hldb::Variable>(), getLocal("str"))
      << "the displayed value must be the same block-local 'str' that $ferror filled in";
}

// The source writes two separate $display statements, so they must be two
// separate nodes -- not one call reused or one statement dropped.
TEST_F(FerrorTest, TheTwoDisplayCallsAreDistinctNodes) {
  const hldb::SysTaskCall *const first = getStmt<hldb::SysTaskCall>(2);
  const hldb::SysTaskCall *const second = getStmt<hldb::SysTaskCall>(3);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_NE(first, second) << "'$display(errno);' and '$display(str);' are two distinct statements";
}

// --- stmt[4]: $fclose(fd) ---------------------------------------------------

TEST_F(FerrorTest, FcloseCallClosesTheSameDescriptor) {
  const hldb::SysTaskCall *const call = getStmt<hldb::SysTaskCall>(4);
  ASSERT_NE(call, nullptr) << "IEEE 1800-2023 Sec 21.3.1: $fclose is a task, so stmt[4] must be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$fclose");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "'$fclose(fd)' takes exactly one descriptor argument";
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "the $fclose argument should be a RefObj";
  EXPECT_EQ(arg->getName(), "fd");
  EXPECT_EQ(arg->getActual<hldb::Variable>(), getLocal("fd"))
      << "the descriptor closed must be the same block-local 'fd' the $fopen assignment wrote";
}

// --- compiler diagnostics ---------------------------------------------------

TEST_F(FerrorTest, CompilerReportsNoFatalOrSyntaxErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

// All three names are block-scoped rather than module-scoped, and each is
// referenced from more than one statement -- the constructs here with real
// binding risk.
TEST_F(FerrorTest, BlockLocalReferencesBind) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "fd"), nullptr)
      << "'fd' must bind to the block-local 'int fd' in the assignment, in $ferror, and in $fclose";
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "str"), nullptr)
      << "'str' must bind to the block-local 'string str' in $ferror and in $display";
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "errno"), nullptr)
      << "'errno' must bind to the block-local 'integer errno' in the assignment and in $display";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
