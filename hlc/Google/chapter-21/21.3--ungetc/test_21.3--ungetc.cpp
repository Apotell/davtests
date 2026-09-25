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
//   tests/Google/chapter-21/21.3--ungetc.sv   (:name: ungetc_function,
//   :description: $ungetc test, :tags: 21.3, :type: simulation parsing)
// ----------------------------------------------------------------------------
//   module top();
//
//   int fd;
//
//   initial begin
//       fd = $fopen("tmp.txt", "w");
//       $ungetc(123, fd);
//       $display(":assert: (%d == %d)", 123, $fgetc(fd));
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
//   Sec 6.11.1 "Integer data types" -- "int" is a 2-state 32-bit SIGNED
//       integer type, an integer_atom_type in the Sec 6.8 variable
//       declaration grammar and never a net_type (Sec 6.7). "fd" is a
//       Variable, and it is the ONLY declaration in this file: everything
//       else the calls below pass is a literal.
//   Sec 9.2.2 "Initial procedures" / Sec 9.3.1 "Sequential blocks" -- the
//       "initial begin ... end" gives one Initial process whose body is a
//       Begin holding the three statements in source order. Nothing is
//       declared inside the block, so the Begin owns no variables of its own.
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
//   Sec 21.3.5 "File positioning" -- this is the clause the file is named
//       for. Its prototype is
//           integer code = $ungetc(c, fd);
//       so $ungetc is a system FUNCTION returning an integer code, and it is
//       a SysFuncCall even here, where the file writes it in statement
//       position and throws the code away -- discarding a result does not
//       turn a system function into a system task. The argument order puts
//       the CHARACTER first and the descriptor second.
//       That first argument is the sharpest structural point in this file.
//       Unlike the destination-first input routines -- Sec 21.3.4.2's
//       $fgets(str, fd) and Sec 21.3.4.4's $fread(var, fd), whose leading
//       argument must be a writable variable -- $ungetc's leading argument is
//       a VALUE being pushed back, so the source may legally write a bare
//       literal there, and this file does. It must therefore appear as a
//       Constant, never as a reference to some object.
//   Sec 21.3.4.3 "Reading a character at a time - $fgetc" -- its prototype is
//           integer code = $fgetc(fd);
//       another system FUNCTION, taking the descriptor alone. Here it is
//       written inside $display's argument list, which is an expression
//       context and makes its SysFuncCall nature directly observable rather
//       than merely inferred from a discarded result.
//   Sec 21.2.1 "$display and $write" -- $display is a system TASK, hence a
//       SysTaskCall, and takes a format string followed by the values to
//       print, here the literal 123 and the result of the $fgetc call, to
//       match the two "%d" specifications.
//   Sec 5.7.1 "Integer literal constants" -- a bare decimal such as 123 is an
//       unsized, unbased, unsigned integer literal. The file writes 123 twice
//       over, once as the character handed to $ungetc and once as the value
//       $display prints beside what $fgetc returns.
//   Sec 5.9 "String literals" -- a string literal is an unsigned integer
//       constant 8 bits wide per character: "tmp.txt" is 7 * 8 = 56 bits, "w"
//       is 1 * 8 = 8 bits and ":assert: (%d == %d)" is 19 * 8 = 152 bits.
//
// ----------------------------------------------------------------------------
// WHAT IS CHECKED (every assertion below names a concrete value):
//   - module "top" exists, has no Nets, no ports and no continuous
//     assignments, and declares exactly ONE Variable, "fd", resolving to a
//     signed IntTypespec with no declaration initializer.
//   - "top" has exactly 2 processes: the Initial at index 0 and the
//     FinalStmt at index 1, matching source order, each cross-checked as not
//     being the other kind.
//   - the Initial's body is a Begin that declares no variables of its own and
//     holds exactly 3 statements.
//   - stmt[0] is a blocking Assignment whose lhs is RefObj "fd" bound by
//     object identity to the declared Variable "fd", and whose rhs is a
//     SysFuncCall named "$fopen" and explicitly not a SysTaskCall, with
//     exactly 2 arguments: string Constants "tmp.txt" (56 bits) and "w"
//     (8 bits), in that order.
//   - stmt[1] is a SysFuncCall named "$ungetc" and explicitly not a
//     SysTaskCall, with exactly 2 arguments: a Constant decompiling to "123"
//     with constType vpiUIntConst -- and explicitly NOT a RefObj, since a
//     pushed-back character is a value rather than a destination -- followed
//     by RefObj "fd" bound to the declared Variable "fd".
//   - stmt[2] is a SysTaskCall named "$display" and explicitly not a
//     SysFuncCall, with exactly 3 arguments: a vpiStringConst Constant whose
//     text is ":assert: (%d == %d)" and whose width is 152 bits, then a
//     Constant decompiling to "123" with constType vpiUIntConst, then the
//     nested "$fgetc(fd)" call.
//   - that nested call is a SysFuncCall named "$fgetc" and explicitly not a
//     SysTaskCall, with exactly 1 argument: RefObj "fd" bound to the declared
//     Variable "fd" -- the Sec 21.3.4.3 descriptor-only form.
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
//   - Whether the claim the ":assert:" format string makes actually holds --
//     that the character $ungetc pushed back is the one $fgetc then reads
//     back, so that the two "%d" values print equal. Pushing a character back
//     onto a stream and reading it again are stream operations that only
//     exist while time advances. What the source DOES determine is that the
//     same literal 123 appears on both sides of that comparison, and both
//     occurrences are asserted, by UngetcArgumentsAreCharacterThenDescriptor
//     and DisplayComparesLiteral123AgainstFgetc.
//   - The integer codes $ungetc and $fgetc return. Nothing in the model
//     records a call's result. The nearest real assertions are the same two
//     tests plus FgetcTakesTheDescriptorAlone, which pin the calls and their
//     operands.
//   - Whether $ungetc against a descriptor opened in "w" (write-only) mode
//     does anything at all, and whether $fgetc can then read from it. Those
//     are runtime I/O outcomes, not compile-time errors -- Sec 21.3.1 makes
//     the mode a runtime property of the opened stream, and no LRM clause
//     makes this combination illegal to compile. The nearest real assertions
//     are FopenArgumentsAreFilenameAndMode (which pins the literal mode
//     string "w") and FgetcTakesTheDescriptorAlone.
//   - Whether $fopen actually opens "tmp.txt", and therefore whether "fd"
//     holds a real descriptor or 0, and whether $fclose then succeeds.
//     Variable::getValue() exposes only a declaration-time initializer, and
//     "int fd;" has none. The nearest real assertion is
//     FirstStatementAssignsFopenResultToFd.
//   - The printed output itself, the "%d" radix and any trailing newline.
//     Formatting happens while time advances. The nearest real assertion is
//     DisplayComparesLiteral123AgainstFgetc, which pins the format string
//     that would drive it.
//   - That the three statements execute in the order written, and that the
//     "final" runs after the "initial". Execution order is a time-domain
//     fact. The nearest real assertions are ModuleHasInitialThenFinalProcess
//     and InitialBodyIsBeginWithThreeStatements plus the per-statement tests,
//     which pin the source order in the model.
//   - The bit width of the unsized decimal literals 123. Sec 5.7.1 requires
//     only "at least 32 bits" for an unsized literal, so the exact width is
//     implementation-determined rather than fixed by the source text; the
//     constants' type and value are asserted instead. (The string literals'
//     widths ARE asserted, because Sec 5.9 fixes them exactly at 8 bits per
//     character.)
//   - Whether the two written occurrences of 123 are one shared Constant node
//     or two separate ones. Constant pooling is a tool-internal convention
//     that the source text does not determine, so each occurrence is checked
//     for its own type and value and no identity relation between them is
//     asserted either way.
//   - The Begin's own name. No ": label" is written, so the block is unnamed;
//     whether the model leaves the name empty or synthesizes an implicit
//     scope name is a tool-internal convention the source does not determine.
//     The scoping fact that matters is asserted instead, by
//     InitialBodyIsBeginWithThreeStatements, which requires the block to own
//     no variables of its own.
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

class UngetcTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.3--ungetc.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  // "int fd;" -- the file's one and only declaration.
  static const hldb::Variable *getFd() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("fd", top->getVariables());
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

  // "$ungetc(123, fd);" -- stmt[1] of the initial block.
  static const hldb::SysFuncCall *getUngetcCall() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() < 2u) return nullptr;
    return any_cast<hldb::SysFuncCall>(body->getStmts()->at(1));
  }

  // "$display(...);" -- stmt[2] of the initial block.
  static const hldb::SysTaskCall *getDisplayCall() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() < 3u) return nullptr;
    return any_cast<hldb::SysTaskCall>(body->getStmts()->at(2));
  }

  // Every reference in this file names the one declared variable "fd". This
  // checks that a given argument is bound to it, not merely spelled alike.
  static void checkArgumentIsFd(const hldb::Any *argument, const char *where) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(argument);
    ASSERT_NE(ref, nullptr) << where << " should be a RefObj";
    EXPECT_EQ(ref->getName(), "fd");
    EXPECT_EQ(ref->getActual<hldb::Variable>(), getFd()) << where << " must bind to the one declared Variable 'fd'";
  }

  // Sec 5.7.1: a bare decimal literal is unsized and unsigned. The width is
  // deliberately not asserted -- see the header.
  static void checkIsDecimalLiteral(const hldb::Any *argument, const char *expectedText, const char *where) {
    const hldb::Constant *const literal = any_cast<hldb::Constant>(argument);
    ASSERT_NE(literal, nullptr) << where << " should be a Constant";
    EXPECT_EQ(literal->getDecompile(), expectedText);
    EXPECT_EQ(literal->getConstType(), vpiUIntConst)
        << where << ": Sec 5.7.1 makes a bare decimal literal unsized and unsigned";
  }
};

// --- module scope and its single variable declaration ------------------------

TEST_F(UngetcTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr); }

// Sec 6.7 lists the net_type keywords; "int" is not one of them, and
// "module top();" has an empty port list, so nothing here declares a net or
// drives one continuously.
TEST_F(UngetcTest, ModuleHasNoNetsNoPortsAndNoContinuousAssignments) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'int' is not a net-type keyword (IEEE 1800-2023 Sec 6.7)";
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top();' has an empty port list";
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "the file contains no 'assign' statement";
}

// Sec 6.11.1: "int" is a 2-state 32-bit signed integer type. "fd" is the only
// object this file declares -- every other operand below is a literal.
TEST_F(UngetcTest, ModuleHasExactlyOneSignedIntVariableFd) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr) << "'int fd;' must be a Variable (Sec 6.8)";
  ASSERT_EQ(top->getVariables()->size(), 1u) << "'int fd;' is the file's only declaration";

  const hldb::Variable *const fd = getFd();
  ASSERT_NE(fd, nullptr);
  ASSERT_NE(fd->getTypespec<hldb::RefTypespec>(), nullptr);
  const hldb::IntTypespec *const ts = fd->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr) << "'int fd;' must resolve to an IntTypespec";
  EXPECT_TRUE(ts->getSigned()) << "IEEE 1800-2023 Sec 6.11.1: 'int' is a signed type";
  EXPECT_EQ(fd->getValue(), nullptr) << "'int fd;' is declared with no '=' initializer";
}

// --- the two procedures ------------------------------------------------------

// Sec 9.2.2 and Sec 9.2.3 define "initial" and "final" as distinct procedure
// kinds, so the module owns two processes of two different node types, in the
// order they are written.
TEST_F(UngetcTest, ModuleHasInitialThenFinalProcess) {
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

// Sec 9.3.1: the "begin ... end" holds the three statements written and
// declares nothing of its own.
TEST_F(UngetcTest, InitialBodyIsBeginWithThreeStatements) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' body should be a Begin";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "no block_item_declaration is written inside the begin-end (Sec 9.3.1); 'fd' resolves "
         "outward to the module scope";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 3u) << "the $fopen assignment, the $ungetc call and the $display";
}

// --- stmt[0]: fd = $fopen("tmp.txt", "w"); -----------------------------------

TEST_F(UngetcTest, FirstStatementAssignsFopenResultToFd) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "stmt[0] should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "Sec 10.4.1: 'fd = ...' uses the blocking '=' operator";

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "the lhs 'fd' should be a RefObj";
  EXPECT_EQ(lhs->getName(), "fd");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getFd()) << "'fd' must bind to the declared Variable 'fd'";

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
TEST_F(UngetcTest, FopenArgumentsAreFilenameAndMode) {
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

// --- stmt[1]: $ungetc(123, fd); ----------------------------------------------

// Sec 21.3.5: "integer code = $ungetc(c, fd);". The return code is discarded
// here, which does not make $ungetc a system task.
TEST_F(UngetcTest, SecondStatementIsUngetcSysFuncCall) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  EXPECT_EQ(any_cast<hldb::SysTaskCall>(body->getStmts()->at(1)), nullptr)
      << "IEEE 1800-2023 Sec 21.3.5: $ungetc returns an integer code, so it is a system function even "
         "when its result is discarded in statement position";
  const hldb::SysFuncCall *const ungetcCall = getUngetcCall();
  ASSERT_NE(ungetcCall, nullptr) << "stmt[1] should be a SysFuncCall";
  EXPECT_EQ(ungetcCall->getName(), "$ungetc");
}

// Sec 21.3.5 fixes the order as (c, fd): the character first, the descriptor
// second. Unlike the destination-first input routines $fgets and $fread,
// whose leading argument must be a writable variable, $ungetc's leading
// argument is a value being pushed back -- which is exactly why the source
// may write a bare literal there, and why it must appear as a Constant and
// not as a reference.
TEST_F(UngetcTest, UngetcArgumentsAreCharacterThenDescriptor) {
  const hldb::SysFuncCall *const ungetcCall = getUngetcCall();
  ASSERT_NE(ungetcCall, nullptr);
  ASSERT_NE(ungetcCall->getArguments(), nullptr);
  ASSERT_EQ(ungetcCall->getArguments()->size(), 2u) << "'$ungetc(123, fd)' passes the character then the descriptor";

  EXPECT_EQ(any_cast<hldb::RefObj>(ungetcCall->getArguments()->at(0)), nullptr)
      << "Sec 21.3.5: $ungetc's first argument is a value pushed back, not a destination -- the "
         "source wrote the literal 123, so it must not appear as a reference";
  checkIsDecimalLiteral(ungetcCall->getArguments()->at(0), "123", "the $ungetc character argument");

  checkArgumentIsFd(ungetcCall->getArguments()->at(1), "the $ungetc descriptor argument");
}

// --- stmt[2]: $display(":assert: (%d == %d)", 123, $fgetc(fd)); --------------

// Sec 21.2.1: $display is a system TASK taking a format string followed by
// the values to print, here the same literal 123 the file pushed back and the
// result of reading a character straight back out.
TEST_F(UngetcTest, DisplayComparesLiteral123AgainstFgetc) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  EXPECT_EQ(any_cast<hldb::SysFuncCall>(body->getStmts()->at(2)), nullptr)
      << "IEEE 1800-2023 Sec 21.2.1: $display is a system task, so it must not be a SysFuncCall";
  const hldb::SysTaskCall *const display = getDisplayCall();
  ASSERT_NE(display, nullptr) << "stmt[2] should be a SysTaskCall";
  EXPECT_EQ(display->getName(), "$display");

  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 3u) << "the call passes a format string and two values";

  const hldb::Constant *const format = any_cast<hldb::Constant>(display->getArguments()->at(0));
  ASSERT_NE(format, nullptr) << "arg[0] is the format string literal";
  EXPECT_EQ(format->getConstType(), vpiStringConst);
  EXPECT_EQ(format->getValue(), ":assert: (%d == %d)");
  EXPECT_EQ(format->getSize(), 152) << "Sec 5.9: \":assert: (%d == %d)\" = 19 chars x 8 bits";

  checkIsDecimalLiteral(display->getArguments()->at(1), "123", "the first value printed");
}

// Sec 21.3.4.3: "integer code = $fgetc(fd);" -- a system function taking the
// descriptor alone. Written inside $display's argument list, this is an
// expression context, so its SysFuncCall nature is directly observable here
// rather than inferred from a discarded result.
TEST_F(UngetcTest, FgetcTakesTheDescriptorAlone) {
  const hldb::SysTaskCall *const display = getDisplayCall();
  ASSERT_NE(display, nullptr);
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 3u);

  EXPECT_EQ(any_cast<hldb::SysTaskCall>(display->getArguments()->at(2)), nullptr)
      << "IEEE 1800-2023 Sec 21.3.4.3: $fgetc returns the character read, so it is a system "
         "function, not a system task";
  const hldb::SysFuncCall *const fgetc = any_cast<hldb::SysFuncCall>(display->getArguments()->at(2));
  ASSERT_NE(fgetc, nullptr) << "arg[2] is the '$fgetc(fd)' call";
  EXPECT_EQ(fgetc->getName(), "$fgetc");

  ASSERT_NE(fgetc->getArguments(), nullptr);
  ASSERT_EQ(fgetc->getArguments()->size(), 1u) << "'$fgetc(fd)' takes the descriptor alone";
  checkArgumentIsFd(fgetc->getArguments()->at(0), "the $fgetc descriptor argument");
}

// --- final $fclose(fd); -------------------------------------------------------

// Sec 9.2.3: a "final" body written without begin/end is a single
// statement_or_null and must bind directly, not through a Begin wrapper.
// Sec 21.3.1: $fclose returns nothing, so it is a task -- the deliberate
// contrast with the $fopen, $ungetc and $fgetc functions above, all of which
// return an integer.
TEST_F(UngetcTest, FinalBodyIsFcloseSysTaskCallOnTheSameFd) {
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
  checkArgumentIsFd(fcloseCall->getArguments()->at(0), "the $fclose descriptor argument");
}

// --- compiler diagnostics -----------------------------------------------------

// The source carries no ":should_fail_because:" tag and every construct in it
// is legal per the clauses cited above, so nothing may be reported as an
// error. (The warning count is deliberately not asserted -- see the header.)
TEST_F(UngetcTest, CompilerReportsZeroErrors) {
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
