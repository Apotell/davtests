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
//   tests/Google/chapter-21/21.3--fgets.sv   (:name: fgets_function,
//   :description: $fgets test, :tags: 21.3, :type: simulation parsing)
// ----------------------------------------------------------------------------
//   module top();
//
//   int fd;
//   string tmp;
//
//   initial begin
//       fd = $fopen("tmp.txt", "w");
//       $fgets(tmp, fd);
//   end
//
//   final
//       $fclose(fd);
//
//   endmodule
// ============================================================================
//
// IEEE 1800-2023 constructs present in this file, and what each one pins down
// statically (all of this was derived by reading the .sv text above and the
// LRM; nothing here was taken from tool output):
//
//   Sec 6.8 "Variable declarations" / Sec 6.7 "Net declarations" -- "int" and
//       "string" are both data types, neither is a net_type keyword, and
//       "module top();" has an empty port list. Both "fd" and "tmp" are
//       therefore module-level Variables and neither is a Net.
//   Sec 6.11.1 "Integer data types" -- "int" is a 2-state 32-bit SIGNED
//       integer type, so "fd" carries a signed IntTypespec.
//   Sec 6.16 "String data type" -- "string" is its own data type, not a
//       packed vector, so "tmp" carries a StringTypespec. The two
//       declarations in this file therefore differ in TYPE as well as in
//       name, which is what makes the $fgets argument-order check below
//       decisive: the destination and the descriptor are not
//       interchangeable objects here.
//   Sec 9.2.2 "Initial procedures" / Sec 9.3.1 "Sequential blocks" -- the
//       "initial begin ... end" gives one Initial process whose body is a
//       Begin holding the two statements in source order. Nothing is declared
//       inside the block -- both variables are module-level -- so the Begin
//       owns no variables of its own and every name used inside it resolves
//       outward to the module scope.
//   Sec 9.2.3 "Final procedures" -- "final" is a distinct procedure kind from
//       "initial", and its body here is a single statement written WITHOUT
//       begin/end, so it must bind directly rather than through a Begin.
//   Sec 10.4.1 "Blocking procedural assignments" -- "fd = $fopen(...)" uses
//       the "=" operator, so the Assignment is blocking.
//   Sec 21.3.1 "Opening and closing files" -- $fopen is a system FUNCTION
//       returning an integer file descriptor, which is why it may sit on an
//       assignment's right-hand side; $fclose is a system TASK and yields
//       nothing. The object model mirrors that split: a system function is a
//       SysFuncCall, a system task is a SysTaskCall.
//   Sec 21.3.4.2 "Reading a line at a time - $fgets" -- this is the clause
//       the file is named for. The LRM's prototype is
//           integer code = $fgets(str, fd);
//       so $fgets is a system FUNCTION returning the number of characters
//       read. It is therefore a SysFuncCall even here, where the file writes
//       it in statement position and throws the count away -- discarding a
//       result does not turn a system function into a system task. The
//       argument ORDER is the substance of this fixture: $fgets takes the
//       DESTINATION STRING FIRST and the descriptor second, the same order
//       Sec 21.3.4.4's $fread uses and the exact opposite of Sec 21.3.4.1's
//       $fscanf, which leads with the descriptor. A model that normalized
//       every file-input call to one argument order would place "tmp" and
//       "fd" wrongly here, which is what the positional assertions below
//       catch.
//   Sec 5.9 "String literals" -- a string literal is an unsigned integer
//       constant 8 bits wide per character: "tmp.txt" is 7 * 8 = 56 bits and
//       "w" is 1 * 8 = 8 bits.
//
// ----------------------------------------------------------------------------
// WHAT IS CHECKED (every assertion below names a concrete value):
//   - module "top" exists, has no Nets, no ports and no continuous
//     assignments, and declares exactly 2 Variables, "fd" and "tmp".
//   - "fd" resolves to a signed IntTypespec and NOT to a StringTypespec;
//     "tmp" resolves to a StringTypespec and NOT to an IntTypespec; the two
//     are distinct objects; and neither carries a declaration initializer.
//     Checking each type both positively and negatively is what proves the
//     two declarations were not transposed.
//   - "top" has exactly 2 processes: the Initial at index 0 and the
//     FinalStmt at index 1, matching source order, each cross-checked as not
//     being the other kind.
//   - the Initial's body is a Begin that declares no variables of its own
//     (both names come from the module scope) and holds exactly 2
//     statements.
//   - stmt[0] is a blocking Assignment whose lhs is RefObj "fd" bound by
//     object identity to the declared Variable "fd", and whose rhs is a
//     SysFuncCall named "$fopen" and explicitly not a SysTaskCall, with
//     exactly 2 arguments: string Constants "tmp.txt" (56 bits) and "w"
//     (8 bits), in that order.
//   - stmt[1] is a SysFuncCall named "$fgets" and explicitly not a
//     SysTaskCall, with exactly 2 arguments: RefObj "tmp" bound to Variable
//     "tmp" first, then RefObj "fd" bound to Variable "fd".
//   - within that call, arg[0] is the destination and arg[1] the descriptor
//     and they are not swapped: arg[0] resolves to the StringTypespec
//     variable and is NOT "fd", arg[1] resolves to the IntTypespec variable
//     and is NOT "tmp", and the two actuals are different objects.
//   - the FinalStmt's body is directly a SysTaskCall named "$fclose" (not
//     wrapped in a Begin, not a SysFuncCall) with exactly 1 argument, RefObj
//     "fd" bound to the same Variable object that "$fopen" wrote.
//   - the compiler reports zero fatal / syntax / error diagnostics: the file
//     carries no ":should_fail_because:" tag and is legal per the clauses
//     above.
//
// ----------------------------------------------------------------------------
// WHAT IS NOT CHECKED, AND WHY (permanently out of scope -- HLC is a static
// compiler/elaborator and never a simulator):
//   - The line of text $fgets deposits into "tmp", and the character count it
//     returns. Both exist only once time advances; Variable::getValue()
//     exposes only a declaration-time initializer, and "string tmp;" has
//     none, so no field anywhere records a post-execution value. The nearest
//     real assertion is FgetsArgumentsAreDestinationThenDescriptor, which
//     pins which declared object the line would be read into.
//   - Whether the deposited line keeps or drops its trailing newline. That is
//     a property of text produced while time advances. The nearest real
//     assertion is the same FgetsArgumentsAreDestinationThenDescriptor.
//   - Whether $fopen actually opens "tmp.txt", and therefore whether "fd"
//     holds a real descriptor or 0, and whether $fclose then succeeds. The
//     nearest real assertion is FirstStatementAssignsFopenResultToFd, which
//     pins the static shape that would produce those values.
//   - That reading with $fgets from a descriptor opened in "w" (write-only)
//     mode yields nothing. That is a runtime I/O outcome, not a compile-time
//     error -- Sec 21.3.1 makes the mode a runtime property of the opened
//     stream, and no LRM clause makes this combination illegal to compile.
//     The nearest real assertions are FopenArgumentsAreFilenameAndMode
//     (which pins the literal mode string "w") and
//     FgetsArgumentsAreDestinationThenDescriptor.
//   - That the "final" procedure runs after the "initial" one, and that the
//     two statements inside the initial run in the order written. Execution
//     order is a time-domain fact. The nearest real assertions are
//     ModuleHasInitialThenFinalProcess and
//     InitialBodyIsBeginWithTwoStatements, which pin that the procedures and
//     statements exist in source order in the model.
//   - The Begin's own name. No ": label" is written, so the block is unnamed;
//     whether the model leaves the name empty or synthesizes an implicit
//     scope name is a tool-internal convention the source does not determine.
//     The scoping fact that matters is asserted instead, by
//     InitialBodyIsBeginWithTwoStatements, which requires the block to own no
//     variables of its own.
//   - The design-level typespec collection's size and which scope owns the
//     shared "int" and string typespec nodes. Typespec sharing is a
//     tool-internal convention that the source text does not determine, so it
//     is left unasserted; what the source DOES determine -- that each
//     variable resolves to its own kind of typespec and that the two
//     Variables are distinct -- is asserted by
//     VariableFdIsSignedIntAndNotAString and
//     VariableTmpIsAStringAndNotAnInt.
//   - The warning count. Whether a legal file also draws advisory warnings is
//     tool policy, not something the LRM fixes, so only the fatal/syntax/
//     error counts are asserted below.
//
// No getElaborated() gating is used anywhere in this file. That is the
// correct answer here rather than an omission: this fixture has no
// parameters, no constant expressions needing reduction, no generate
// constructs and no module instantiations, so nothing in it comes into
// existence only after elaboration. Every value asserted below is
// established by parsing and name resolution alone.
// ============================================================================

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/final_stmt.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FgetsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.3--fgets.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Variable *getVariable(const char *name) {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, top->getVariables());
  }

  // "initial begin ... end" -- written first, so it is the process at index 0.
  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }

  // "final $fclose(fd);" -- the second and last process in source order.
  static const hldb::FinalStmt *getFinalProcess() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->size() < 2u) return nullptr;
    return any_cast<hldb::FinalStmt>(top->getProcesses()->at(1));
  }

  // "$fgets(tmp, fd);" -- stmt[1] of the initial block.
  static const hldb::SysFuncCall *getFgetsCall() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() < 2u) return nullptr;
    return any_cast<hldb::SysFuncCall>(body->getStmts()->at(1));
  }

  // Both variables in this file are referenced by name from inside the
  // procedures. This checks that a given argument is a reference bound to the
  // one declared Variable of that name, not merely something spelled alike.
  static void checkArgumentRefersTo(const hldb::Any *argument, const char *name, const char *where) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(argument);
    ASSERT_NE(ref, nullptr) << where << " should be a RefObj";
    EXPECT_EQ(ref->getName(), name);
    EXPECT_EQ(ref->getActual<hldb::Variable>(), getVariable(name))
        << where << " must bind to the module-level Variable '" << name << "'";
  }
};

// --- module scope and its two variable declarations --------------------------

TEST_F(FgetsTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr); }

// Sec 6.7 lists the net_type keywords; neither "int" nor "string" is one of
// them, and "module top();" has an empty port list, so nothing here declares
// a net or drives one continuously.
TEST_F(FgetsTest, ModuleHasNoNetsNoPortsAndNoContinuousAssignments) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'int' and 'string' are not net-type keywords (IEEE 1800-2023 Sec 6.7)";
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top();' has an empty port list";
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "the file contains no 'assign' statement";
}

TEST_F(FgetsTest, ModuleHasExactlyTwoDistinctVariablesFdAndTmp) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr) << "'int fd;' and 'string tmp;' must both be Variables (Sec 6.8)";
  ASSERT_EQ(top->getVariables()->size(), 2u);

  const hldb::Variable *const fd = getVariable("fd");
  const hldb::Variable *const tmp = getVariable("tmp");
  ASSERT_NE(fd, nullptr);
  ASSERT_NE(tmp, nullptr);
  EXPECT_NE(fd, tmp) << "'int fd;' and 'string tmp;' are two separate declarations and must be two "
                        "separate Variable objects";
}

// Sec 6.11.1: "int" is a 2-state 32-bit signed integer type. The negative
// check against StringTypespec is what proves "fd" did not pick up "tmp"'s
// type.
TEST_F(FgetsTest, VariableFdIsSignedIntAndNotAString) {
  const hldb::Variable *const fd = getVariable("fd");
  ASSERT_NE(fd, nullptr);
  ASSERT_NE(fd->getTypespec<hldb::RefTypespec>(), nullptr);

  const hldb::IntTypespec *const ts = fd->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr) << "'int fd;' must resolve to an IntTypespec";
  EXPECT_TRUE(ts->getSigned()) << "IEEE 1800-2023 Sec 6.11.1: 'int' is a signed type";
  EXPECT_EQ(fd->getTypespec<hldb::RefTypespec>()->getActual<hldb::StringTypespec>(), nullptr)
      << "'int fd;' is not the string declaration -- the two declarations must not be transposed";
  EXPECT_EQ(fd->getValue(), nullptr) << "'int fd;' is declared with no '=' initializer";
}

// Sec 6.16: "string" is its own data type. The negative check against
// IntTypespec is the mirror image of the one above.
TEST_F(FgetsTest, VariableTmpIsAStringAndNotAnInt) {
  const hldb::Variable *const tmp = getVariable("tmp");
  ASSERT_NE(tmp, nullptr);
  ASSERT_NE(tmp->getTypespec<hldb::RefTypespec>(), nullptr);

  EXPECT_NE(tmp->getTypespec<hldb::RefTypespec>()->getActual<hldb::StringTypespec>(), nullptr)
      << "'string tmp;' must resolve to a StringTypespec (IEEE 1800-2023 Sec 6.16)";
  EXPECT_EQ(tmp->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr)
      << "'string tmp;' is not the int declaration -- the two declarations must not be transposed";
  EXPECT_EQ(tmp->getValue(), nullptr) << "'string tmp;' is declared with no '=' initializer";
}

// --- the two procedures ------------------------------------------------------

// Sec 9.2.2 and Sec 9.2.3 define "initial" and "final" as distinct procedure
// kinds, so the module owns two processes of two different node types, in the
// order they are written.
TEST_F(FgetsTest, ModuleHasInitialThenFinalProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 2u) << "one 'initial' and one 'final'";
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr) << "the 'initial' procedure is written first";
  EXPECT_EQ(any_cast<hldb::FinalStmt>(top->getProcesses()->at(0)), nullptr)
      << "the first process is an Initial, not a FinalStmt";
  EXPECT_NE(any_cast<hldb::FinalStmt>(top->getProcesses()->at(1)), nullptr)
      << "the 'final' procedure is written second and is its own node kind (Sec 9.2.3)";
  EXPECT_EQ(any_cast<hldb::Initial>(top->getProcesses()->at(1)), nullptr)
      << "'final' must not be modeled as an Initial";
}

// Sec 9.3.1: the "begin ... end" holds the two statements written. Nothing is
// declared inside it -- both "fd" and "tmp" are module-level here -- so the
// block owns no variables of its own.
TEST_F(FgetsTest, InitialBodyIsBeginWithTwoStatements) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' body should be a Begin";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "no block_item_declaration is written inside the begin-end (Sec 9.3.1); both names resolve "
         "outward to the module scope";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 2u) << "'fd = $fopen(...);' and '$fgets(tmp, fd);'";
}

// --- stmt[0]: fd = $fopen("tmp.txt", "w"); -----------------------------------

TEST_F(FgetsTest, FirstStatementAssignsFopenResultToFd) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "stmt[0] should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "Sec 10.4.1: 'fd = ...' uses the blocking '=' operator";

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "the lhs 'fd' should be a RefObj";
  EXPECT_EQ(lhs->getName(), "fd");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariable("fd")) << "'fd' must bind to the declared Variable 'fd'";

  // Sec 21.3.1: $fopen is a system FUNCTION returning a descriptor, which is
  // why it is legal as an rhs and why it is not a SysTaskCall.
  EXPECT_EQ(assign->getRhs<hldb::SysTaskCall>(), nullptr)
      << "IEEE 1800-2023 Sec 21.3.1: $fopen returns a file descriptor, so it is a system function, "
         "not a system task";
  const hldb::SysFuncCall *const fopenCall = assign->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(fopenCall, nullptr) << "the rhs should be a SysFuncCall";
  EXPECT_EQ(fopenCall->getName(), "$fopen");
}

// Sec 21.3.1: the two-argument form is $fopen(filename, mode). Sec 5.9 fixes
// each literal's width at 8 bits per character.
TEST_F(FgetsTest, FopenArgumentsAreFilenameAndMode) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::SysFuncCall *const fopenCall = assign->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(fopenCall, nullptr);
  ASSERT_NE(fopenCall->getArguments(), nullptr);
  ASSERT_EQ(fopenCall->getArguments()->size(), 2u) << "'$fopen(\"tmp.txt\", \"w\")' passes filename then mode";

  const hldb::Constant *const filename = any_cast<hldb::Constant>(fopenCall->getArguments()->at(0));
  ASSERT_NE(filename, nullptr) << "arg[0] is the string literal \"tmp.txt\"";
  EXPECT_EQ(filename->getConstType(), vpiStringConst);
  EXPECT_EQ(filename->getValue(), "tmp.txt");
  EXPECT_EQ(filename->getSize(), 56) << "Sec 5.9: \"tmp.txt\" = 7 chars x 8 bits";

  const hldb::Constant *const mode = any_cast<hldb::Constant>(fopenCall->getArguments()->at(1));
  ASSERT_NE(mode, nullptr) << "arg[1] is the string literal \"w\"";
  EXPECT_EQ(mode->getConstType(), vpiStringConst);
  EXPECT_EQ(mode->getValue(), "w") << "Sec 21.3.1: \"w\" opens the file for writing";
  EXPECT_EQ(mode->getSize(), 8) << "Sec 5.9: \"w\" = 1 char x 8 bits";
}

// --- stmt[1]: $fgets(tmp, fd); ------------------------------------------------

// Sec 21.3.4.2: "integer code = $fgets(str, fd);". The character count is
// discarded here by calling $fgets in statement position, which does not make
// it a system task.
TEST_F(FgetsTest, SecondStatementIsFgetsSysFuncCall) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  EXPECT_EQ(any_cast<hldb::SysTaskCall>(body->getStmts()->at(1)), nullptr)
      << "IEEE 1800-2023 Sec 21.3.4.2: $fgets returns the number of characters read, so it is a "
         "system function even when its result is discarded in statement position";
  const hldb::SysFuncCall *const fgetsCall = getFgetsCall();
  ASSERT_NE(fgetsCall, nullptr) << "stmt[1] should be a SysFuncCall";
  EXPECT_EQ(fgetsCall->getName(), "$fgets");
}

// Sec 21.3.4.2 fixes the order as (str, fd): the DESTINATION STRING FIRST,
// the descriptor second.
TEST_F(FgetsTest, FgetsArgumentsAreDestinationThenDescriptor) {
  const hldb::SysFuncCall *const fgetsCall = getFgetsCall();
  ASSERT_NE(fgetsCall, nullptr);
  ASSERT_NE(fgetsCall->getArguments(), nullptr);
  ASSERT_EQ(fgetsCall->getArguments()->size(), 2u) << "'$fgets(tmp, fd)' passes exactly two arguments";

  checkArgumentRefersTo(fgetsCall->getArguments()->at(0), "tmp", "the $fgets destination argument");
  checkArgumentRefersTo(fgetsCall->getArguments()->at(1), "fd", "the $fgets descriptor argument");
}

// The destination-first order of Sec 21.3.4.2 is the exact opposite of
// Sec 21.3.4.1's $fscanf, which leads with the descriptor. Because "tmp" and
// "fd" differ in TYPE as well as in name, a swap is detectable here not just
// by name but by the typespec each argument's actual resolves to -- so this
// pins the order from both directions.
TEST_F(FgetsTest, FgetsDestinationAndDescriptorAreNotSwapped) {
  const hldb::SysFuncCall *const fgetsCall = getFgetsCall();
  ASSERT_NE(fgetsCall, nullptr);
  ASSERT_NE(fgetsCall->getArguments(), nullptr);
  ASSERT_EQ(fgetsCall->getArguments()->size(), 2u);

  const hldb::RefObj *const first = any_cast<hldb::RefObj>(fgetsCall->getArguments()->at(0));
  const hldb::RefObj *const second = any_cast<hldb::RefObj>(fgetsCall->getArguments()->at(1));
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);

  const hldb::Variable *const destination = first->getActual<hldb::Variable>();
  const hldb::Variable *const descriptor = second->getActual<hldb::Variable>();
  ASSERT_NE(destination, nullptr);
  ASSERT_NE(descriptor, nullptr);

  EXPECT_NE(destination, descriptor) << "the destination and the descriptor are distinct declared variables";
  EXPECT_NE(destination, getVariable("fd"))
      << "IEEE 1800-2023 Sec 21.3.4.2: arg[0] is the destination string, not the descriptor -- the "
         "$fscanf argument order must not be applied here";
  EXPECT_NE(descriptor, getVariable("tmp")) << "arg[1] is the descriptor, not the destination string";

  ASSERT_NE(destination->getTypespec<hldb::RefTypespec>(), nullptr);
  EXPECT_NE(destination->getTypespec<hldb::RefTypespec>()->getActual<hldb::StringTypespec>(), nullptr)
      << "Sec 21.3.4.2: the first argument is the string the line is read into, so it must resolve "
         "to the StringTypespec declaration";
  ASSERT_NE(descriptor->getTypespec<hldb::RefTypespec>(), nullptr);
  EXPECT_NE(descriptor->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr)
      << "Sec 21.3.4.2: the second argument is the descriptor returned by $fopen, so it must "
         "resolve to the int declaration";
}

// --- final $fclose(fd); -------------------------------------------------------

// Sec 9.2.3: a "final" body written without begin/end is a single
// statement_or_null and must bind directly, not through a Begin wrapper.
// Sec 21.3.1: $fclose returns nothing, so it is a task -- the deliberate
// contrast with the $fopen and $fgets functions above.
TEST_F(FgetsTest, FinalBodyIsFcloseSysTaskCallOnTheSameFd) {
  const hldb::FinalStmt *const fin = getFinalProcess();
  ASSERT_NE(fin, nullptr);
  EXPECT_EQ(fin->getStmt<hldb::Begin>(), nullptr)
      << "Sec 9.2.3: no begin/end was written, so the body must not be wrapped in a Begin";
  EXPECT_EQ(fin->getStmt<hldb::SysFuncCall>(), nullptr)
      << "IEEE 1800-2023 Sec 21.3.1: $fclose returns nothing, so it is a system task, not a system "
         "function";

  const hldb::SysTaskCall *const fcloseCall = fin->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(fcloseCall, nullptr) << "the final body should be a SysTaskCall";
  EXPECT_EQ(fcloseCall->getName(), "$fclose");

  ASSERT_NE(fcloseCall->getArguments(), nullptr);
  ASSERT_EQ(fcloseCall->getArguments()->size(), 1u) << "'$fclose(fd)' passes exactly one argument";
  checkArgumentRefersTo(fcloseCall->getArguments()->at(0), "fd", "the $fclose descriptor argument");
}

// --- compiler diagnostics -----------------------------------------------------

// The source carries no ":should_fail_because:" tag and every construct in it
// is legal per the clauses cited above, so nothing may be reported as an
// error. (The warning count is deliberately not asserted -- see the header.)
TEST_F(FgetsTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
