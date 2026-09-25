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
//   tests/Google/chapter-21/21.3--fpos.sv   (:name: file_pos_tasks,
//   :description: $fseek, $ftell and $rewind test, :tags: 21.3,
//   :type: simulation parsing)
// ----------------------------------------------------------------------------
//   module top();
//
//   initial begin
//       int fd;
//       fd = $fopen("tmp.txt", "w");
//       $display(":assert: (%d == 0)", $ftell(fd));
//       $fseek(fd, 12, 0);
//       $display(":assert: (%d == 12)", $ftell(fd));
//       $rewind(fd);
//       $display(":assert: (%d == 0)", $ftell(fd));
//       $fclose(fd);
//   end
//
//   endmodule
// ============================================================================
//
// IEEE 1800-2023 constructs present in this file, and what each one pins down
// statically (all of this was derived by reading the .sv text above and the
// LRM; nothing here was taken from tool output):
//
//   Sec 9.3.1 "Sequential blocks" -- the grammar is
//       "seq_block ::= begin [ : block_identifier ] { block_item_declaration }
//       { statement_or_null } end [ : block_identifier ]".
//       A block_item_declaration is NOT a statement_or_null, so "int fd;" is a
//       declaration of the block and the block holds exactly SEVEN statements,
//       not eight.
//   Sec 6.21 "Scope and lifetime" -- a begin-end block is a scope, so "fd" is
//       local to it and the enclosing module "top" declares nothing at all.
//   Sec 6.11.1 "Integer data types" -- "int" is a 2-state 32-bit SIGNED
//       integer type, and an integer_atom_type in the Sec 6.8 variable
//       declaration grammar, never a net_type (Sec 6.7). "fd" is a Variable.
//   Sec 9.2.2 "Initial procedures" -- the one "initial" gives exactly one
//       Initial process, the module's only process. There is no "final" here,
//       unlike the sibling 21.3--fgets.sv / 21.3--fmonitor.sv.
//   Sec 10.4.1 "Blocking procedural assignments" -- "fd = $fopen(...)" uses
//       the "=" operator, so the Assignment is blocking.
//   Sec 21.3.1 "Opening and closing files" -- $fopen is a system FUNCTION
//       returning an integer file descriptor, which is why it may sit on an
//       assignment's right-hand side; $fclose is a system TASK and yields
//       nothing. The object model mirrors that split: a system function is a
//       SysFuncCall, a system task is a SysTaskCall.
//   Sec 21.3.5 "File positioning" -- this is the clause the file is named
//       for, and all three of its subjects appear here. The LRM's prototypes
//       are "integer code = $fseek(fd, offset, operation);",
//       "integer pos = $ftell(fd);" and "integer code = $rewind(fd);": ALL
//       THREE ARE SYSTEM FUNCTIONS, each returning an integer. That makes
//       every one of them a SysFuncCall, including "$fseek(fd, 12, 0);" and
//       "$rewind(fd);", which this file writes in statement position and
//       whose return codes it discards -- discarding a result does not turn a
//       system function into a system task.
//       Sec 21.3.5 also fixes the meaning of $fseek's third argument: 0 sets
//       the position to "offset" bytes from the start of the file, 1 is
//       relative to the current position and 2 to the end. This file passes a
//       literal 0, so the seek is absolute, and the literal 12 is the target
//       byte offset. $rewind(fd) is defined there as equivalent to
//       $fseek(fd, 0, 0), but it is written as its own named call and must be
//       modeled as one.
//   Sec 21.2.1 "$display and $write" -- $display is a system TASK, hence a
//       SysTaskCall, and takes a format string followed by the values to
//       print. Each of the three calls here passes a $ftell(fd) call as that
//       value, which is the expression context that makes $ftell's
//       SysFuncCall nature directly observable.
//   Sec 5.9 "String literals" -- a string literal is an unsigned integer
//       constant 8 bits wide per character: "tmp.txt" is 7 * 8 = 56 bits,
//       "w" is 1 * 8 = 8 bits, ":assert: (%d == 0)" is 18 * 8 = 144 bits and
//       ":assert: (%d == 12)" is 19 * 8 = 152 bits.
//   Sec 5.7.1 "Integer literal constants" -- a bare decimal such as 12 or 0
//       is an unsized, unbased, unsigned integer literal.
//
// ----------------------------------------------------------------------------
// WHAT IS CHECKED (every assertion below names a concrete value):
//   - module "top" exists and declares nothing of its own: no Nets, no
//     Variables, no ports, no continuous assignments. "fd" must not leak out
//     of the begin-end scope into the module (Sec 6.21).
//   - "top" has exactly one process and it is an Initial.
//   - the Initial's body is a Begin owning exactly one Variable, "fd",
//     resolving to a signed IntTypespec with no initializer, and holding
//     exactly SEVEN statements (the declaration is not one of them).
//   - stmt[0] is a blocking Assignment: lhs RefObj "fd" bound by object
//     identity to the block-scope Variable, rhs a SysFuncCall "$fopen" and
//     explicitly not a SysTaskCall, with 2 arguments -- string Constants
//     "tmp.txt" (56 bits) and "w" (8 bits) in that order.
//   - stmt[1], stmt[3] and stmt[5] are each a SysTaskCall "$display" with
//     exactly 2 arguments: a vpiStringConst format string whose exact text
//     and bit width are checked per call (":assert: (%d == 0)" / 144 bits,
//     ":assert: (%d == 12)" / 152 bits, ":assert: (%d == 0)" / 144 bits),
//     followed by a SysFuncCall "$ftell" -- explicitly not a SysTaskCall --
//     carrying exactly 1 argument, RefObj "fd" bound to the block-scope
//     Variable.
//   - stmt[2] is a SysFuncCall "$fseek" (not a SysTaskCall) with exactly 3
//     arguments: RefObj "fd" bound to the block-scope Variable, Constant 12
//     and Constant 0, both vpiUIntConst -- the Sec 21.3.5 (fd, offset,
//     operation) order, with operation 0 meaning "from the start of file".
//   - stmt[4] is a SysFuncCall "$rewind" (not a SysTaskCall) with exactly 1
//     argument, RefObj "fd" bound to the block-scope Variable, and its name
//     is "$rewind" rather than "$fseek" -- the equivalence Sec 21.3.5 states
//     is semantic, so the written call must survive as its own node.
//   - stmt[6] is a SysTaskCall "$fclose" (not a SysFuncCall) with exactly 1
//     argument, RefObj "fd" bound to the same block-scope Variable object
//     that "$fopen" wrote.
//   - the compiler reports zero fatal / syntax / error diagnostics: the file
//     carries no ":should_fail_because:" tag and is legal per the clauses
//     above.
//
// ----------------------------------------------------------------------------
// WHAT IS NOT CHECKED, AND WHY (permanently out of scope -- HLC is a static
// compiler/elaborator and never a simulator):
//   - The three positions the ":assert:" format strings advertise (0 after
//     the open, 12 after the seek, 0 after the rewind). Those are the values
//     $ftell would return once time advances; nothing in the model records a
//     call's result. What the source DOES determine is the exact text of
//     each format string, and that text is asserted verbatim, together with
//     the literal 12 passed to $fseek, by DisplayAfterOpenExpectsPositionZero,
//     FseekArgumentsAreFdOffsetTwelveAndWhenceZero and
//     DisplayAfterSeekExpectsPositionTwelve.
//   - Whether $fseek actually moves the file position, whether $rewind
//     actually resets it, and what integer code either returns. All three are
//     runtime I/O outcomes. The nearest real assertions are
//     FseekArgumentsAreFdOffsetTwelveAndWhenceZero and
//     RewindIsItsOwnCallOnFd, which pin the calls and their operands.
//   - The printed output itself, the "%d" radix and any trailing newline.
//     Formatting happens while time advances. The nearest real assertions are
//     the three Display* tests, which pin the format strings that would drive
//     it.
//   - Whether $fopen actually opens "tmp.txt", and therefore whether "fd"
//     holds a real descriptor or 0, and whether $fclose then succeeds.
//     Variable::getValue() exposes only a declaration-time initializer, and
//     "int fd;" has none, so no field records a runtime value. The nearest
//     real assertion is FirstStatementAssignsFopenResultToFd.
//   - That the seven statements execute in the order written. Execution order
//     is a time-domain fact. The nearest real assertion is
//     InitialBodyIsBeginWithSevenStatements plus the per-statement tests,
//     which pin the source order in the model.
//   - The bit width of the unsized decimal literals 12 and 0. Sec 5.7.1
//     requires only "at least 32 bits" for an unsized literal, so the exact
//     width is implementation-determined rather than fixed by the source
//     text; the constants' type and value are asserted instead. (The string
//     literals' widths ARE asserted, because Sec 5.9 fixes them exactly at 8
//     bits per character.)
//   - The Begin's own name. No ": label" is written, so the block is unnamed;
//     whether the model leaves the name empty or synthesizes an implicit
//     scope name is a tool-internal convention the source does not determine.
//     The scoping fact that matters is asserted instead, by
//     ModuleTopDeclaresNothingOfItsOwn together with
//     BeginScopeOwnsTheIntVariableFd.
//   - The design-level typespec collection's size and which scope owns the
//     shared "int" / string typespec nodes. Typespec sharing is likewise a
//     tool-internal convention.
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
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FposTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.3--fpos.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  // The "begin ... end" body of the module's single "initial" procedure.
  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }

  // "int fd;" lives in the Begin's scope, not the module's (Sec 6.21).
  static const hldb::Variable *getBlockScopeFd() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("fd", body->getVariables());
  }

  // Every call in this file takes "fd" somewhere in its argument list. This
  // checks that a given argument really is a reference bound to the one
  // block-scope Variable, not merely something spelled "fd".
  static void checkArgumentIsFd(const hldb::Any *argument, const char *where) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(argument);
    ASSERT_NE(ref, nullptr) << where << " should be a RefObj";
    EXPECT_EQ(ref->getName(), "fd");
    EXPECT_EQ(ref->getActual<hldb::Variable>(), getBlockScopeFd())
        << where << " must bind to the Variable declared in this block (Sec 6.21)";
  }

  // Shared body for the three "$display(<fmt>, $ftell(fd));" statements,
  // which differ only in the format string. Sec 21.2.1 makes $display a
  // task; Sec 21.3.5 makes the nested $ftell a function.
  static void checkDisplayOfFtell(uint32_t stmtIndex, const char *expectedFormat, int32_t expectedFormatSize) {
    const hldb::Begin *const body = getInitialBody();
    ASSERT_NE(body, nullptr);
    ASSERT_NE(body->getStmts(), nullptr);
    ASSERT_GT(body->getStmts()->size(), stmtIndex);

    const hldb::SysTaskCall *const display = any_cast<hldb::SysTaskCall>(body->getStmts()->at(stmtIndex));
    ASSERT_NE(display, nullptr) << "stmt[" << stmtIndex << "] should be a $display SysTaskCall (Sec 21.2.1)";
    EXPECT_EQ(display->getName(), "$display");
    ASSERT_NE(display->getArguments(), nullptr);
    ASSERT_EQ(display->getArguments()->size(), 2u) << "$display here passes a format string and one value";

    const hldb::Constant *const format = any_cast<hldb::Constant>(display->getArguments()->at(0));
    ASSERT_NE(format, nullptr) << "arg[0] is the format string literal";
    EXPECT_EQ(format->getConstType(), vpiStringConst);
    EXPECT_EQ(format->getValue(), expectedFormat);
    EXPECT_EQ(format->getSize(), expectedFormatSize) << "Sec 5.9: 8 bits per character";

    // Sec 21.3.5: $ftell returns the current position, so in this argument
    // position it must be a function call, never a task call.
    EXPECT_EQ(any_cast<hldb::SysTaskCall>(display->getArguments()->at(1)), nullptr)
        << "IEEE 1800-2023 Sec 21.3.5: $ftell returns an integer position, so it is a system "
           "function, not a system task";
    const hldb::SysFuncCall *const ftell = any_cast<hldb::SysFuncCall>(display->getArguments()->at(1));
    ASSERT_NE(ftell, nullptr) << "arg[1] is the '$ftell(fd)' call";
    EXPECT_EQ(ftell->getName(), "$ftell");
    ASSERT_NE(ftell->getArguments(), nullptr);
    ASSERT_EQ(ftell->getArguments()->size(), 1u) << "'$ftell(fd)' takes the descriptor alone";
    checkArgumentIsFd(ftell->getArguments()->at(0), "the $ftell descriptor argument");
  }
};

// --- module scope ------------------------------------------------------------

TEST_F(FposTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr); }

// Sec 6.21: the begin-end block is a scope, so "int fd;" is local to it and
// must not surface on the module. The module body contains nothing else --
// no net-type keyword (Sec 6.7), no port list, no "assign".
TEST_F(FposTest, ModuleTopDeclaresNothingOfItsOwn) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getVariables() == nullptr || top->getVariables()->empty())
      << "IEEE 1800-2023 Sec 6.21: 'int fd;' is declared inside 'initial begin ... end' and is local "
         "to that block -- it must not also appear as a module-level Variable";
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "no net-type keyword (Sec 6.7) appears anywhere in this file";
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top();' has an empty port list";
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "the file contains no 'assign' statement";
}

// Sec 9.2.2: one "initial" keyword, one Initial process -- and nothing else.
TEST_F(FposTest, ModuleHasExactlyOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u) << "the file writes exactly one procedure";
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr) << "that procedure is an 'initial'";
  EXPECT_EQ(any_cast<hldb::FinalStmt>(top->getProcesses()->at(0)), nullptr)
      << "no 'final' keyword is written in this file";
}

// --- block scope: "int fd;" is a declaration, not a statement -----------------

// Sec 9.3.1: block_item_declarations are a separate grammar slot from
// statement_or_null, so "fd" belongs to the Begin's variables.
TEST_F(FposTest, BeginScopeOwnsTheIntVariableFd) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' body should be a Begin";
  ASSERT_NE(body->getVariables(), nullptr) << "'int fd;' should live in the enclosing Begin's scope";
  ASSERT_EQ(body->getVariables()->size(), 1u) << "the block declares exactly one variable";

  const hldb::Variable *const fd = getBlockScopeFd();
  ASSERT_NE(fd, nullptr) << "Variable 'fd' not found in the Begin's scope";
  ASSERT_NE(fd->getTypespec<hldb::RefTypespec>(), nullptr);
  const hldb::IntTypespec *const ts = fd->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr) << "'int fd;' must resolve to an IntTypespec";
  EXPECT_TRUE(ts->getSigned()) << "IEEE 1800-2023 Sec 6.11.1: 'int' is a signed type";
  EXPECT_EQ(fd->getValue(), nullptr) << "'int fd;' is declared with no '=' initializer";
}

// Sec 9.3.1 again, from the other side: because the declaration is not a
// statement_or_null, the seven executable lines are all that is counted.
TEST_F(FposTest, InitialBodyIsBeginWithSevenStatements) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 7u)
      << "IEEE 1800-2023 Sec 9.3.1: 'int fd;' is a block_item_declaration, not a statement_or_null, "
         "so the block holds the seven executable lines only";
}

// --- stmt[0]: fd = $fopen("tmp.txt", "w"); -----------------------------------

TEST_F(FposTest, FirstStatementAssignsFopenResultToFd) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "stmt[0] should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "Sec 10.4.1: 'fd = ...' uses the blocking '=' operator";

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "the lhs 'fd' should be a RefObj";
  EXPECT_EQ(lhs->getName(), "fd");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getBlockScopeFd())
      << "'fd' must bind to the very Variable declared in this block (Sec 6.21)";

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
TEST_F(FposTest, FopenArgumentsAreFilenameAndMode) {
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

// --- stmt[1], stmt[3], stmt[5]: $display(<fmt>, $ftell(fd)); -----------------
//
// The three $display calls are checked separately rather than in a loop over
// one shared format string, because their format strings genuinely differ and
// the difference is the file's own record of what it expects the position to
// be at each point. Each test names its own literal and width.

TEST_F(FposTest, DisplayAfterOpenExpectsPositionZero) { checkDisplayOfFtell(1u, ":assert: (%d == 0)", 144); }

TEST_F(FposTest, DisplayAfterSeekExpectsPositionTwelve) { checkDisplayOfFtell(3u, ":assert: (%d == 12)", 152); }

TEST_F(FposTest, DisplayAfterRewindExpectsPositionZeroAgain) { checkDisplayOfFtell(5u, ":assert: (%d == 0)", 144); }

// --- stmt[2]: $fseek(fd, 12, 0); ---------------------------------------------

// Sec 21.3.5: "integer code = $fseek(fd, offset, operation);". The return
// code is discarded here, which does not make $fseek a task.
TEST_F(FposTest, FseekIsASysFuncCallInStatementPosition) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  EXPECT_EQ(any_cast<hldb::SysTaskCall>(body->getStmts()->at(2)), nullptr)
      << "IEEE 1800-2023 Sec 21.3.5: $fseek returns an integer code, so it is a system function even "
         "when its result is discarded in statement position";
  const hldb::SysFuncCall *const fseek = any_cast<hldb::SysFuncCall>(body->getStmts()->at(2));
  ASSERT_NE(fseek, nullptr) << "stmt[2] should be a SysFuncCall";
  EXPECT_EQ(fseek->getName(), "$fseek");
}

// Sec 21.3.5 fixes the argument order as (fd, offset, operation) and gives
// operation 0 the meaning "set the position to offset from the start of the
// file". Sec 5.7.1 makes both numbers unsized unsigned decimal literals.
TEST_F(FposTest, FseekArgumentsAreFdOffsetTwelveAndWhenceZero) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  const hldb::SysFuncCall *const fseek = any_cast<hldb::SysFuncCall>(body->getStmts()->at(2));
  ASSERT_NE(fseek, nullptr);
  ASSERT_NE(fseek->getArguments(), nullptr);
  ASSERT_EQ(fseek->getArguments()->size(), 3u) << "'$fseek(fd, 12, 0)' passes descriptor, offset and operation";

  checkArgumentIsFd(fseek->getArguments()->at(0), "the $fseek descriptor argument");

  const hldb::Constant *const offset = any_cast<hldb::Constant>(fseek->getArguments()->at(1));
  ASSERT_NE(offset, nullptr) << "arg[1] is the byte offset literal 12";
  EXPECT_EQ(offset->getDecompile(), "12");
  EXPECT_EQ(offset->getConstType(), vpiUIntConst) << "Sec 5.7.1: a bare decimal literal is unsigned";

  const hldb::Constant *const operation = any_cast<hldb::Constant>(fseek->getArguments()->at(2));
  ASSERT_NE(operation, nullptr) << "arg[2] is the operation literal 0";
  EXPECT_EQ(operation->getDecompile(), "0") << "Sec 21.3.5: operation 0 seeks from the start of the file";
  EXPECT_EQ(operation->getConstType(), vpiUIntConst) << "Sec 5.7.1: a bare decimal literal is unsigned";
}

// --- stmt[4]: $rewind(fd); ----------------------------------------------------

// Sec 21.3.5: "integer code = $rewind(fd);" -- another system function whose
// code is discarded here. The clause notes $rewind(fd) is equivalent to
// $fseek(fd, 0, 0), but that equivalence is semantic: the source wrote
// $rewind, so the model must carry $rewind with its single argument.
TEST_F(FposTest, RewindIsItsOwnCallOnFd) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  EXPECT_EQ(any_cast<hldb::SysTaskCall>(body->getStmts()->at(4)), nullptr)
      << "IEEE 1800-2023 Sec 21.3.5: $rewind returns an integer code, so it is a system function even "
         "when its result is discarded in statement position";
  const hldb::SysFuncCall *const rewind = any_cast<hldb::SysFuncCall>(body->getStmts()->at(4));
  ASSERT_NE(rewind, nullptr) << "stmt[4] should be a SysFuncCall";
  EXPECT_EQ(rewind->getName(), "$rewind")
      << "Sec 21.3.5 calls $rewind(fd) equivalent to $fseek(fd, 0, 0), but the source wrote $rewind "
         "and the model must not rewrite it into a $fseek";

  ASSERT_NE(rewind->getArguments(), nullptr);
  ASSERT_EQ(rewind->getArguments()->size(), 1u) << "'$rewind(fd)' takes the descriptor alone";
  checkArgumentIsFd(rewind->getArguments()->at(0), "the $rewind descriptor argument");
}

// --- stmt[6]: $fclose(fd); ----------------------------------------------------

// Sec 21.3.1 introduces $fclose as a system TASK -- it yields no value, the
// deliberate contrast with the $fopen, $ftell, $fseek and $rewind functions
// above, all of which return an integer.
TEST_F(FposTest, LastStatementIsFcloseOnTheSameFd) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  EXPECT_EQ(any_cast<hldb::SysFuncCall>(body->getStmts()->at(6)), nullptr)
      << "IEEE 1800-2023 Sec 21.3.1: $fclose returns nothing, so it is a system task, not a system "
         "function";
  const hldb::SysTaskCall *const fclose = any_cast<hldb::SysTaskCall>(body->getStmts()->at(6));
  ASSERT_NE(fclose, nullptr) << "stmt[6] should be a SysTaskCall";
  EXPECT_EQ(fclose->getName(), "$fclose");

  ASSERT_NE(fclose->getArguments(), nullptr);
  ASSERT_EQ(fclose->getArguments()->size(), 1u) << "'$fclose(fd)' passes exactly one argument";
  checkArgumentIsFd(fclose->getArguments()->at(0), "the $fclose descriptor argument");
}

// --- compiler diagnostics -----------------------------------------------------

// The source carries no ":should_fail_because:" tag and every construct in it
// is legal per the clauses cited above, so nothing may be reported as an
// error. (The warning count is deliberately not asserted -- see the header.)
TEST_F(FposTest, CompilerReportsZeroErrors) {
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
