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
//   tests/Google/chapter-21/21.3--fmonitor.sv   (:name: fmonitor_task,
//   :description: $fmonitor test, :tags: 21.3,
//   :type: simulation parsing)
// ----------------------------------------------------------------------------
//   module top();
//
//   logic a;
//
//   int fd;
//   string str = "abc";
//
//   initial begin
//       fd = $fopen("tmp.txt", "w");
//       $fmonitor(fd, a);
//       $fmonitorb(fd, a);
//       $fmonitoro(fd, a);
//       $fmonitorh(fd, a);
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
//   Sec 6.8 "Variable declarations" / Sec 6.7 "Net declarations" -- "logic",
//       "int" and "string" are all data types, none of them a net_type
//       keyword, and the module has no port list. All three of "a", "fd" and
//       "str" are therefore module-level Variables and none of them is a Net.
//   Sec 6.11.1 "Integer data types" -- "int" is a 2-state 32-bit SIGNED
//       integer type, so "fd" carries a signed IntTypespec.
//   Sec 6.8 again -- "logic a;" declares no packed dimensions and no "signed"
//       keyword, so "a" is an unsigned scalar: a LogicTypespec with no
//       declared ranges.
//   Sec 6.16 "String data type" -- "str" carries a StringTypespec, and unlike
//       the other two it is written WITH a declaration assignment, so it is
//       the only variable in the file whose vpiValue is populated.
//   Sec 5.9 "String literals" -- a string literal is an unsigned integer
//       constant 8 bits wide per character: "abc" is 3 * 8 = 24 bits,
//       "tmp.txt" is 7 * 8 = 56 bits, and "w" is 1 * 8 = 8 bits.
//   Sec 9.2.2 "Initial procedures" / Sec 9.3.1 "Sequential blocks" -- the
//       one "initial begin ... end" gives one Initial process whose body is a
//       Begin holding the five statements in source order. Nothing is
//       declared inside the block, so the Begin owns no variables of its own.
//   Sec 9.2.3 "Final procedures" -- "final" is a distinct procedure kind from
//       "initial", and its body here is a single statement written WITHOUT
//       begin/end, so it must bind directly rather than through a Begin.
//   Sec 10.4.1 "Blocking procedural assignments" -- "fd = $fopen(...)" uses
//       the "=" operator, so the Assignment is blocking.
//   Sec 21.3.1 "Opening and closing files" -- $fopen is a system FUNCTION
//       returning an integer file descriptor, which is why it may sit on the
//       right-hand side of an assignment into "int fd"; $fclose is a system
//       TASK and yields nothing. The object model mirrors that split: a
//       system function is a SysFuncCall, a system task is a SysTaskCall.
//   Sec 21.3.2 "File output system tasks" -- this is the clause the file is
//       named for. $fmonitor, $fmonitorb, $fmonitoro and $fmonitorh are four
//       DISTINCT system tasks (not one task with a modifier), each taking a
//       file descriptor as its first argument followed by the same argument
//       list the corresponding non-"f" display task accepts. All four are
//       tasks, so all four are SysTaskCalls, never SysFuncCalls. The four
//       suffixes differ only in the default radix used for arguments written
//       without a format specification, which is a formatting decision made
//       while time advances -- see "WHAT IS NOT CHECKED" below.
//
// ----------------------------------------------------------------------------
// WHAT IS CHECKED (every assertion below names a concrete value):
//   - module "top" exists, has no Nets, no ports and no continuous
//     assignments, and declares exactly 3 Variables: "a", "fd" and "str".
//   - "a" resolves to a LogicTypespec that is unsigned and has no declared
//     ranges, and has no initializer.
//   - "fd" resolves to a signed IntTypespec and has no initializer.
//   - "str" resolves to a StringTypespec and is the one variable that DOES
//     carry a declaration initializer: a vpiStringConst Constant whose value
//     is "abc" and whose size is 24 bits.
//   - "top" has exactly 2 processes: the Initial at index 0 and the
//     FinalStmt at index 1, matching source order, each cross-checked as not
//     being the other kind.
//   - the Initial's body is a Begin that declares no variables of its own and
//     holds exactly 5 statements.
//   - stmt[0] is a blocking Assignment whose lhs is RefObj "fd" bound by
//     object identity to the declared Variable "fd", and whose rhs is a
//     SysFuncCall named "$fopen" and explicitly not a SysTaskCall, with
//     exactly 2 arguments: string Constants "tmp.txt" (56 bits) and "w"
//     (8 bits), in that order.
//   - stmt[1] through stmt[4] are SysTaskCalls named, in source order,
//     "$fmonitor", "$fmonitorb", "$fmonitoro" and "$fmonitorh" -- four
//     distinct names, checked one by one, which is the point of this file.
//     None of them is a SysFuncCall.
//   - each of those four carries exactly 2 arguments: RefObj "fd" bound to
//     the Variable "fd", then RefObj "a" bound to the Variable "a" -- the
//     Sec 21.3.2 shape of "descriptor first, then the values to monitor".
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
//   - The default output radix each suffix selects: binary for $fmonitorb,
//     octal for $fmonitoro, hexadecimal for $fmonitorh, and the Sec 21.2
//     default for plain $fmonitor. A radix is applied when a value is
//     formatted for output, which only happens while time advances. The
//     nearest real assertions are SecondStatementIsPlainFmonitor,
//     ThirdStatementIsFmonitorB, FourthStatementIsFmonitorO and
//     FifthStatementIsFmonitorH, backed by
//     FourFmonitorVariantsHaveFourDistinctNames: together they pin that the
//     source really did name four distinct tasks in that order -- the static
//     half of the same concern.
//   - Whether any of the monitors ever fires, and what it writes. Continuous
//     monitoring is by definition event behavior: Sec 21.2.3 has a monitor
//     report only when one of its arguments changes. Nothing in the model
//     records a printed line. The nearest real assertions are the same four
//     per-variant tests, whose shared body checkFmonitorVariant() checks
//     which objects each monitor is watching.
//   - That issuing four monitors in a row leaves only the last one active.
//     Sec 21.2.3 allows only one monitor list to be active at a time, so at
//     runtime $fmonitorh would supersede the three before it -- but that is a
//     time-domain consequence, not a compile-time error, and no LRM clause
//     makes the sequence illegal to compile. The nearest real assertion is
//     InitialBodyIsBeginWithFiveStatements together with the per-variant
//     tests, which pin all four calls as present and ordered.
//   - Whether $fopen actually opens "tmp.txt" and therefore whether "fd"
//     holds a real descriptor or 0, and whether $fclose then succeeds. Those
//     values exist only once time advances; Variable::getValue() exposes only
//     a declaration-time initializer, and "int fd;" has none. The nearest
//     real assertion is FirstStatementAssignsFopenResultToFd.
//   - The runtime value of "a". It is declared and monitored but never
//     assigned, so it would hold x throughout; no field records that. The
//     nearest real assertion is VariableAIsUnsignedScalarLogic, which pins
//     its declared type instead.
//   - That "str" is declared but never read. Being unused is not an LRM
//     violation and produces no node of its own; what the source does
//     determine is its type and its initializer, and both are asserted by
//     VariableStrIsStringInitializedToAbc.
//   - The design-level typespec collection's size and which scope owns the
//     shared "logic" / "int" / string typespec nodes. Typespec sharing is a
//     tool-internal convention that the source text does not determine, so it
//     is left unasserted rather than guessed.
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
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/final_stmt.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
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

class FmonitorTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.3--fmonitor.hlc"}); }
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

  // Shared body for the four "$fmonitor<suffix>(fd, a);" statements, which
  // differ only in name and position. Sec 21.3.2 gives them all the same
  // argument shape: the file descriptor first, then the values to monitor.
  static void checkFmonitorVariant(uint32_t stmtIndex, const char *expectedName) {
    const hldb::Begin *const body = getInitialBody();
    ASSERT_NE(body, nullptr);
    ASSERT_NE(body->getStmts(), nullptr);
    ASSERT_GT(body->getStmts()->size(), stmtIndex);

    EXPECT_EQ(any_cast<hldb::SysFuncCall>(body->getStmts()->at(stmtIndex)), nullptr)
        << expectedName << " is a system task (IEEE 1800-2023 Sec 21.3.2), so it must not be a SysFuncCall";
    const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(body->getStmts()->at(stmtIndex));
    ASSERT_NE(call, nullptr) << "stmt[" << stmtIndex << "] should be a SysTaskCall";
    EXPECT_EQ(call->getName(), expectedName);

    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 2u) << expectedName << "(fd, a) passes exactly two arguments";

    const hldb::RefObj *const descriptor = any_cast<hldb::RefObj>(call->getArguments()->at(0));
    ASSERT_NE(descriptor, nullptr) << "arg[0] is the file descriptor 'fd'";
    EXPECT_EQ(descriptor->getName(), "fd");
    EXPECT_EQ(descriptor->getActual<hldb::Variable>(), getVariable("fd"))
        << "Sec 21.3.2: the first argument of an 'f' output task is the descriptor";

    const hldb::RefObj *const signal = any_cast<hldb::RefObj>(call->getArguments()->at(1));
    ASSERT_NE(signal, nullptr) << "arg[1] is the monitored signal 'a'";
    EXPECT_EQ(signal->getName(), "a");
    EXPECT_EQ(signal->getActual<hldb::Variable>(), getVariable("a"))
        << "the monitored object must bind to the declared Variable 'a'";
  }
};

// --- module scope and its three variable declarations ------------------------

TEST_F(FmonitorTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr); }

// Sec 6.7 lists the net_type keywords; "logic", "int" and "string" are none of
// them, and "module top();" has an empty port list, so nothing in this file
// declares a net or drives one continuously.
TEST_F(FmonitorTest, ModuleHasNoNetsNoPortsAndNoContinuousAssignments) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'logic', 'int' and 'string' are not net-type keywords (IEEE 1800-2023 Sec 6.7)";
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top();' has an empty port list";
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "the file contains no 'assign' statement";
}

TEST_F(FmonitorTest, ModuleHasExactlyThreeVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr) << "'logic a', 'int fd' and 'string str' are all Variables (Sec 6.8)";
  ASSERT_EQ(top->getVariables()->size(), 3u);
  EXPECT_NE(getVariable("a"), nullptr);
  EXPECT_NE(getVariable("fd"), nullptr);
  EXPECT_NE(getVariable("str"), nullptr);
}

// Sec 6.8: "logic a;" declares neither a packed range nor the "signed"
// keyword, so it is an unsigned scalar.
TEST_F(FmonitorTest, VariableAIsUnsignedScalarLogic) {
  const hldb::Variable *const a = getVariable("a");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getTypespec<hldb::RefTypespec>(), nullptr);
  const hldb::LogicTypespec *const ts = a->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ts, nullptr) << "'logic a;' must resolve to a LogicTypespec";
  EXPECT_FALSE(ts->getSigned()) << "Sec 6.8: 'logic' with no 'signed' keyword defaults to unsigned";
  EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty())
      << "'logic a;' declares no '[msb:lsb]' -- it is an implicit scalar bit";
  EXPECT_EQ(a->getValue(), nullptr) << "'logic a;' is declared with no '=' initializer";
}

// Sec 6.11.1: "int" is a 2-state 32-bit signed integer type.
TEST_F(FmonitorTest, VariableFdIsSignedIntWithNoInitializer) {
  const hldb::Variable *const fd = getVariable("fd");
  ASSERT_NE(fd, nullptr);
  ASSERT_NE(fd->getTypespec<hldb::RefTypespec>(), nullptr);
  const hldb::IntTypespec *const ts = fd->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr) << "'int fd;' must resolve to an IntTypespec";
  EXPECT_TRUE(ts->getSigned()) << "IEEE 1800-2023 Sec 6.11.1: 'int' is a signed type";
  EXPECT_EQ(fd->getValue(), nullptr) << "'int fd;' is declared with no '=' initializer";
}

// Sec 6.16 for the type, Sec 5.9 for the literal's width. "str" is the only
// variable in the file written with a declaration assignment, so it is the
// only one whose vpiValue may be populated.
TEST_F(FmonitorTest, VariableStrIsStringInitializedToAbc) {
  const hldb::Variable *const str = getVariable("str");
  ASSERT_NE(str, nullptr);
  ASSERT_NE(str->getTypespec<hldb::RefTypespec>(), nullptr);
  EXPECT_NE(str->getTypespec<hldb::RefTypespec>()->getActual<hldb::StringTypespec>(), nullptr)
      << "'string str' must resolve to a StringTypespec (IEEE 1800-2023 Sec 6.16)";

  const hldb::Constant *const init = str->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'string str = \"abc\";' carries a declaration-time initializer";
  EXPECT_EQ(init->getConstType(), vpiStringConst);
  EXPECT_EQ(init->getValue(), "abc");
  EXPECT_EQ(init->getSize(), 24) << "Sec 5.9: \"abc\" = 3 chars x 8 bits";
}

// --- the two procedures ------------------------------------------------------

// Sec 9.2.2 and Sec 9.2.3 define "initial" and "final" as distinct procedure
// kinds, so the module owns two processes of two different node types, in the
// order they are written.
TEST_F(FmonitorTest, ModuleHasInitialThenFinalProcess) {
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

// Sec 9.3.1: the "begin ... end" holds the five statements written, and
// nothing is declared inside it, so it owns no variables of its own -- every
// name used in the block comes from the enclosing module scope.
TEST_F(FmonitorTest, InitialBodyIsBeginWithFiveStatements) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' body should be a Begin";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "no block_item_declaration is written inside the begin-end (Sec 9.3.1)";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 5u) << "'fd = $fopen(...)' plus the four '$fmonitor*' calls";
}

// --- stmt[0]: fd = $fopen("tmp.txt", "w"); -----------------------------------

TEST_F(FmonitorTest, FirstStatementAssignsFopenResultToFd) {
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
TEST_F(FmonitorTest, FopenArgumentsAreFilenameAndMode) {
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

// --- stmt[1..4]: the four distinct $fmonitor tasks ---------------------------
//
// Sec 21.3.2 lists $fmonitor, $fmonitorb, $fmonitoro and $fmonitorh as four
// separate system tasks. Each is checked by name at its own source position,
// because "four distinct task names were written, in this order" is precisely
// what this fixture exists to pin down; a model that collapsed them into one
// task, or that reordered them, would pass a laxer check.

TEST_F(FmonitorTest, SecondStatementIsPlainFmonitor) { checkFmonitorVariant(1u, "$fmonitor"); }

TEST_F(FmonitorTest, ThirdStatementIsFmonitorB) { checkFmonitorVariant(2u, "$fmonitorb"); }

TEST_F(FmonitorTest, FourthStatementIsFmonitorO) { checkFmonitorVariant(3u, "$fmonitoro"); }

TEST_F(FmonitorTest, FifthStatementIsFmonitorH) { checkFmonitorVariant(4u, "$fmonitorh"); }

// The four names must all be different from one another. Checking them
// pairwise as a set complements the positional checks above: it fails loudly
// if the model resolved every suffix back to the same canonical "$fmonitor".
TEST_F(FmonitorTest, FourFmonitorVariantsHaveFourDistinctNames) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 5u);

  const hldb::SysTaskCall *calls[4] = {nullptr, nullptr, nullptr, nullptr};
  for (uint32_t i = 0; i < 4u; ++i) {
    calls[i] = any_cast<hldb::SysTaskCall>(body->getStmts()->at(i + 1u));
    ASSERT_NE(calls[i], nullptr) << "stmt[" << (i + 1u) << "] should be a SysTaskCall";
  }
  for (uint32_t i = 0; i < 4u; ++i) {
    for (uint32_t j = i + 1u; j < 4u; ++j) {
      EXPECT_NE(calls[i]->getName(), calls[j]->getName())
          << "IEEE 1800-2023 Sec 21.3.2: $fmonitor, $fmonitorb, $fmonitoro and $fmonitorh are four "
             "separate system tasks, not one task with a radix modifier -- stmt["
          << (i + 1u) << "] and stmt[" << (j + 1u) << "] must not share a name";
    }
  }
}

// --- final $fclose(fd); -------------------------------------------------------

// Sec 9.2.3: a "final" body written without begin/end is a single
// statement_or_null and must bind directly, not through a Begin wrapper.
// Sec 21.3.1: $fclose returns nothing, so it is a task, the deliberate
// contrast with the $fopen SysFuncCall above.
TEST_F(FmonitorTest, FinalBodyIsFcloseSysTaskCallOnTheSameFd) {
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
  const hldb::RefObj *const descriptor = any_cast<hldb::RefObj>(fcloseCall->getArguments()->at(0));
  ASSERT_NE(descriptor, nullptr) << "the argument is the descriptor 'fd'";
  EXPECT_EQ(descriptor->getName(), "fd");
  EXPECT_EQ(descriptor->getActual<hldb::Variable>(), getVariable("fd"))
      << "the 'fd' closed in 'final' is the same declared Variable that '$fopen' wrote and the four "
         "monitors watched";
}

// --- compiler diagnostics -----------------------------------------------------

// The source carries no ":should_fail_because:" tag and every construct in it
// is legal per the clauses cited above, so nothing may be reported as an
// error. (The warning count is deliberately not asserted -- see the header.)
TEST_F(FmonitorTest, CompilerReportsZeroErrors) {
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
