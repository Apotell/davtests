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
//   tests/Google/chapter-21/21.3--file.sv   (:name: file_tasks,
//   :description: $fopen and $fclose test, :tags: 21.3,
//   :type: simulation parsing)
// ----------------------------------------------------------------------------
//   module top();
//
//   initial begin
//       int fd;
//       fd = $fopen("tmp.txt", "w");
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
//       A block_item_declaration is NOT a statement_or_null. "int fd;" is
//       therefore a declaration of the block, and the block holds exactly TWO
//       statements, not three. This is one of the two structural points that
//       separate this file from its siblings 21.3--fgets.sv, 21.3--fread.sv
//       and 21.3--fscanf.sv, where the descriptor is declared at module level
//       instead.
//   Sec 6.21 "Scope and lifetime" -- a begin-end block is a scope, and a
//       declaration inside it is local to that scope. So "fd" belongs to the
//       Begin, and the enclosing module "top" declares nothing at all: no
//       variables, no nets, no ports.
//   Sec 6.11.1 "Integer data types" -- "int" is a 2-state 32-bit SIGNED
//       integer type, and an integer_atom_type in the Sec 6.8 variable
//       declaration grammar, never a net_type (Sec 6.7). "fd" is a Variable.
//   Sec 9.2.2 "Initial procedures" -- the one "initial" gives exactly one
//       Initial process, the module's only process. This file writes no
//       "final" procedure, unlike 21.3--fgets.sv and 21.3--fread.sv; the
//       whole open/close pair lives in the one initial block.
//   Sec 10.4.1 "Blocking procedural assignments" -- "fd = $fopen(...)" uses
//       the "=" operator, so the Assignment is blocking.
//   Sec 21.3.1 "Opening and closing files" -- this is the clause the file is
//       named for, and its two subjects are the file's entire content.
//       $fopen is a system FUNCTION returning an integer file descriptor,
//       which is exactly why it may sit on the right-hand side of an
//       assignment into "int fd"; its two-argument form is
//       $fopen(filename, mode). $fclose is a system TASK: it returns nothing
//       and is written in statement position. The object model mirrors that
//       split -- a system function is a SysFuncCall, a system task is a
//       SysTaskCall -- so checking both kinds against each other in one file
//       is the substance of ":description: $fopen and $fclose test".
//   Sec 5.9 "String literals" -- a string literal is an unsigned integer
//       constant 8 bits wide per character: "tmp.txt" is 7 * 8 = 56 bits and
//       "w" is 1 * 8 = 8 bits.
//
// ----------------------------------------------------------------------------
// WHAT IS CHECKED (every assertion below names a concrete value):
//   - module "top" exists and declares nothing of its own: no Nets, no
//     Variables, no ports, no continuous assignments. "fd" must NOT leak out
//     of the begin-end scope into the module (Sec 6.21).
//   - "top" has exactly one process, and it is an Initial and explicitly not
//     a FinalStmt.
//   - the Initial's body is a Begin that owns exactly one Variable, "fd",
//     resolving to a signed IntTypespec and carrying no initializer
//     ("int fd;" is written without "=").
//   - that same Begin has exactly TWO statements, because Sec 9.3.1 makes
//     "int fd;" a block_item_declaration rather than a statement.
//   - stmt[0] is a blocking Assignment whose lhs is RefObj "fd" bound by
//     object identity to the block-scope Variable (so a stray module-level or
//     implicitly created "fd" would fail), and whose rhs is a SysFuncCall
//     named "$fopen" and explicitly not a SysTaskCall, with exactly 2
//     arguments: string Constants "tmp.txt" (56 bits) and "w" (8 bits), in
//     that order.
//   - stmt[1] is a SysTaskCall named "$fclose" and explicitly not a
//     SysFuncCall, with exactly 1 argument: RefObj "fd" bound to the same
//     block-scope Variable object that $fopen wrote.
//   - the descriptor assigned by $fopen and the descriptor passed to $fclose
//     are the SAME Variable object, which is what makes the open/close pair a
//     pair rather than two unrelated calls.
//   - the compiler reports zero fatal / syntax / error diagnostics: the file
//     carries no ":should_fail_because:" tag and is legal per the clauses
//     above.
//
// ----------------------------------------------------------------------------
// WHAT IS NOT CHECKED, AND WHY (permanently out of scope -- HLC is a static
// compiler/elaborator and never a simulator):
//   - Whether $fopen actually opens "tmp.txt", and therefore whether "fd"
//     ends up holding a nonzero descriptor or 0 for failure. That value only
//     comes into being once time advances; Variable::getValue() exposes only
//     a declaration-time initializer, and "int fd;" has none, so no field
//     anywhere records it. The nearest real assertion is
//     FirstStatementAssignsFopenResultToFd, which pins the static shape that
//     would produce it: a blocking Assignment from SysFuncCall "$fopen" into
//     the block-scope Variable "fd".
//   - Whether $fclose succeeds, and what happens if it is handed a descriptor
//     from a failed open. Both are runtime I/O outcomes, not compile-time
//     facts, and no LRM clause makes the pairing illegal to compile. The
//     nearest real assertion is OpenAndCloseShareOneDescriptorObject, which
//     checks that the descriptor closed is object-identical to the one
//     $fopen assigned.
//   - That the two statements execute in the order written, so that the file
//     is opened before it is closed. Execution order is a time-domain fact.
//     The nearest real assertions are InitialBodyIsBeginWithTwoStatements
//     plus the two per-statement tests, which pin the source order of the
//     statements in the model.
//   - The Begin's own name. No ": label" is written, so the block is
//     unnamed; whether the model leaves the name empty or synthesizes an
//     implicit scope name is a tool-internal convention that the source text
//     does not determine, so it is left unasserted rather than guessed. The
//     scoping fact that actually matters is asserted instead, by
//     ModuleTopDeclaresNothingOfItsOwn together with
//     BeginScopeOwnsTheIntVariableFd.
//   - The design-level typespec collection's size and which scope owns the
//     shared "int" / string typespec nodes. Typespec sharing is likewise a
//     tool-internal convention that the source text does not determine.
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

class FileTasksTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.3--file.hlc"}); }
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

  // Both calls in this file take "fd". This checks that a given argument is a
  // reference bound to the one block-scope Variable, not merely something
  // spelled "fd".
  static void checkArgumentIsFd(const hldb::Any *argument, const char *where) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(argument);
    ASSERT_NE(ref, nullptr) << where << " should be a RefObj";
    EXPECT_EQ(ref->getName(), "fd");
    EXPECT_EQ(ref->getActual<hldb::Variable>(), getBlockScopeFd())
        << where << " must bind to the Variable declared in this block (Sec 6.21)";
  }

  // "fd = $fopen(...);" -- stmt[0] of the initial block.
  static const hldb::Assignment *getOpenAssignment() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) return nullptr;
    return any_cast<hldb::Assignment>(body->getStmts()->at(0));
  }

  // "$fclose(fd);" -- stmt[1] of the initial block.
  static const hldb::SysTaskCall *getCloseCall() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() < 2u) return nullptr;
    return any_cast<hldb::SysTaskCall>(body->getStmts()->at(1));
  }
};

// --- module scope ------------------------------------------------------------

TEST_F(FileTasksTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr); }

// Sec 6.21: the begin-end block is a scope, so "int fd;" is local to it and
// must not surface on the module. The module body contains nothing else --
// no net-type keyword (Sec 6.7), no port list, no "assign".
TEST_F(FileTasksTest, ModuleTopDeclaresNothingOfItsOwn) {
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
// Unlike 21.3--fgets.sv and 21.3--fread.sv, this file writes no "final".
TEST_F(FileTasksTest, ModuleHasExactlyOneInitialProcess) {
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
TEST_F(FileTasksTest, BeginScopeOwnsTheIntVariableFd) {
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
// statement_or_null, only "fd = $fopen(...);" and "$fclose(fd);" are counted.
TEST_F(FileTasksTest, InitialBodyIsBeginWithTwoStatements) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 2u)
      << "IEEE 1800-2023 Sec 9.3.1: 'int fd;' is a block_item_declaration, not a statement_or_null, "
         "so the block holds two statements, not three";
}

// --- stmt[0]: fd = $fopen("tmp.txt", "w"); -----------------------------------

TEST_F(FileTasksTest, FirstStatementAssignsFopenResultToFd) {
  const hldb::Assignment *const assign = getOpenAssignment();
  ASSERT_NE(assign, nullptr) << "stmt[0] should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "Sec 10.4.1: 'fd = ...' uses the blocking '=' operator";

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "the lhs 'fd' should be a RefObj";
  EXPECT_EQ(lhs->getName(), "fd");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getBlockScopeFd())
      << "'fd' must bind to the very Variable declared in this block (Sec 6.21), not to some other "
         "object of the same name";

  // Sec 21.3.1: $fopen is a system FUNCTION returning a descriptor. That is
  // precisely why it may appear as an rhs, and why it is not a SysTaskCall.
  EXPECT_EQ(assign->getRhs<hldb::SysTaskCall>(), nullptr)
      << "IEEE 1800-2023 Sec 21.3.1: $fopen returns a file descriptor, so it is a system function, "
         "not a system task";
  const hldb::SysFuncCall *const fopenCall = assign->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(fopenCall, nullptr) << "the rhs should be a SysFuncCall";
  EXPECT_EQ(fopenCall->getName(), "$fopen");
}

// Sec 21.3.1: the two-argument form is $fopen(filename, mode). Sec 5.9 fixes
// each literal's width at 8 bits per character.
TEST_F(FileTasksTest, FopenArgumentsAreFilenameAndMode) {
  const hldb::Assignment *const assign = getOpenAssignment();
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

// --- stmt[1]: $fclose(fd); ----------------------------------------------------

// Sec 21.3.1 introduces $fclose as a system TASK -- it yields no value, the
// deliberate contrast with $fopen above and the other half of this file's
// ":description: $fopen and $fclose test".
TEST_F(FileTasksTest, SecondStatementIsFcloseSysTaskCall) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  EXPECT_EQ(any_cast<hldb::SysFuncCall>(body->getStmts()->at(1)), nullptr)
      << "IEEE 1800-2023 Sec 21.3.1: $fclose returns nothing, so it is a system task, not a system "
         "function";
  const hldb::SysTaskCall *const fcloseCall = getCloseCall();
  ASSERT_NE(fcloseCall, nullptr) << "stmt[1] should be a SysTaskCall";
  EXPECT_EQ(fcloseCall->getName(), "$fclose");

  ASSERT_NE(fcloseCall->getArguments(), nullptr);
  ASSERT_EQ(fcloseCall->getArguments()->size(), 1u) << "'$fclose(fd)' passes exactly one argument";
  checkArgumentIsFd(fcloseCall->getArguments()->at(0), "the $fclose descriptor argument");
}

// The two statements are an open/close PAIR, which in the model means the
// descriptor $fopen assigns and the descriptor $fclose receives are one and
// the same declared object -- not two same-named references that happen to
// read alike.
TEST_F(FileTasksTest, OpenAndCloseShareOneDescriptorObject) {
  const hldb::Assignment *const assign = getOpenAssignment();
  const hldb::SysTaskCall *const fcloseCall = getCloseCall();
  ASSERT_NE(assign, nullptr);
  ASSERT_NE(fcloseCall, nullptr);
  ASSERT_NE(fcloseCall->getArguments(), nullptr);
  ASSERT_EQ(fcloseCall->getArguments()->size(), 1u);

  const hldb::RefObj *const assigned = assign->getLhs<hldb::RefObj>();
  const hldb::RefObj *const closed = any_cast<hldb::RefObj>(fcloseCall->getArguments()->at(0));
  ASSERT_NE(assigned, nullptr);
  ASSERT_NE(closed, nullptr);

  const hldb::Variable *const assignedVar = assigned->getActual<hldb::Variable>();
  ASSERT_NE(assignedVar, nullptr) << "the assignment lhs must resolve to a declared Variable";
  EXPECT_EQ(assignedVar, closed->getActual<hldb::Variable>())
      << "the descriptor written by '$fopen' and the one passed to '$fclose' must be the same "
         "block-scope Variable object";
  EXPECT_EQ(assignedVar, getBlockScopeFd()) << "and that object is the 'int fd;' declared in this block";
}

// --- compiler diagnostics -----------------------------------------------------

// The source carries no ":should_fail_because:" tag and every construct in it
// is legal per the clauses cited above, so nothing may be reported as an
// error. (The warning count is deliberately not asserted -- see the header.)
TEST_F(FileTasksTest, CompilerReportsZeroErrors) {
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
