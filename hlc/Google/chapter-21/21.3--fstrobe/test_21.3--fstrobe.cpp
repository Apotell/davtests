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
//   tests/Google/chapter-21/21.3--fstrobe.sv   (:name: fstrobe_task,
//   :description: $fstrobe test, :tags: 21.3, :type: simulation parsing)
// ----------------------------------------------------------------------------
//   module top();
//
//   logic clk;
//   logic a;
//
//   int fd;
//
//   always @(posedge clk) begin
//       $fstrobe(fd, a);
//       $fstrobe(fd, a);
//       $fstrobe(fd, a);
//       $fstrobe(fd, a);
//   end
//
//   initial begin
//       fd = $fopen("tmp.txt", "w");
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
//   Sec 6.8 "Variable declarations" / Sec 6.7 "Net declarations" -- "logic"
//       and "int" are data types, neither is a net_type keyword, and
//       "module top();" has an empty port list. All three of "clk", "a" and
//       "fd" are therefore module-level Variables and none is a Net.
//   Sec 6.8 again -- "logic clk;" and "logic a;" declare no packed dimensions
//       and no "signed" keyword, so each is an unsigned scalar: a
//       LogicTypespec with no declared ranges.
//   Sec 6.11.1 "Integer data types" -- "int" is a 2-state 32-bit SIGNED
//       integer type, so "fd" carries a signed IntTypespec.
//   Sec 9.2.2.1 "Always procedures" -- the plain "always" keyword produces an
//       Always process whose AlwaysType is vpiAlways, distinct from the
//       always_comb / always_latch / always_ff forms of Sec 9.2.2.2-9.2.2.4,
//       none of which is written here.
//   Sec 9.4.2 "Event control" -- "@(posedge clk)" is an event control
//       wrapping the procedure's statement, so the Always's own statement is
//       an EventControl whose condition is a posedge operation over a single
//       operand, the reference to "clk".
//   Sec 9.3.1 "Sequential blocks" -- both begin-end blocks are Begins. The
//       always block's Begin holds the four statements written; the initial
//       block's Begin holds exactly one, because the source wrapped a single
//       statement in begin/end even though it did not have to.
//   Sec 9.2.2 "Initial procedures" and Sec 9.2.3 "Final procedures" -- the
//       "initial" and the "final" are two further, distinct procedure kinds.
//       The "final" body is a single statement written WITHOUT begin/end, so
//       it must bind directly rather than through a Begin -- the deliberate
//       contrast with the "initial begin ... end" just above it. This file
//       therefore has THREE processes, one of each kind, in source order.
//   Sec 10.4.1 "Blocking procedural assignments" -- "fd = $fopen(...)" uses
//       the "=" operator, so the Assignment is blocking.
//   Sec 21.3.1 "Opening and closing files" -- $fopen is a system FUNCTION
//       returning an integer file descriptor, which is why it may sit on an
//       assignment's right-hand side; $fclose is a system TASK and yields
//       nothing. The object model mirrors that split: a system function is a
//       SysFuncCall, a system task is a SysTaskCall.
//   Sec 21.3.2 "File output system tasks" -- this is the clause the file is
//       named for. $fstrobe is one of the file output system TASKS (alongside
//       $fdisplay, $fwrite and $fmonitor), so it is a SysTaskCall, never a
//       SysFuncCall, and it takes the file descriptor as its FIRST argument
//       followed by the values to write -- the descriptor-first order shared
//       by all the Sec 21.3.2 output tasks, and the opposite of the
//       destination-first order of Sec 21.3.4.2's $fgets and Sec 21.3.4.4's
//       $fread.
//       The file writes the SAME call four times over. Nothing in the LRM
//       makes repeated identical output-task calls redundant or collapsible:
//       per Sec 21.2.2, a strobe is a one-shot request serviced at the end of
//       the current time step, not a standing registration like Sec 21.2.3's
//       $monitor. Four calls written means four calls in the model, so the
//       four must survive as four DISTINCT nodes.
//   Sec 5.9 "String literals" -- a string literal is an unsigned integer
//       constant 8 bits wide per character: "tmp.txt" is 7 * 8 = 56 bits and
//       "w" is 1 * 8 = 8 bits.
//
// ----------------------------------------------------------------------------
// WHAT IS CHECKED (every assertion below names a concrete value):
//   - module "top" exists, has no Nets, no ports and no continuous
//     assignments, and declares exactly 3 Variables: "clk", "a" and "fd".
//   - "clk" and "a" each resolve to a LogicTypespec that is unsigned and has
//     no declared ranges, and neither has an initializer; they are two
//     distinct Variable objects.
//   - "fd" resolves to a signed IntTypespec and has no initializer.
//   - "top" has exactly 3 processes, in source order: an Always at index 0,
//     an Initial at index 1 and a FinalStmt at index 2, each cross-checked as
//     not being either of the other two kinds.
//   - the Always has AlwaysType vpiAlways, and explicitly not vpiAlwaysComb,
//     vpiAlwaysLatch or vpiAlwaysFF.
//   - the Always's statement is an EventControl whose condition is an
//     Operation with opType vpiPosedgeOp carrying exactly 1 operand: RefObj
//     "clk" bound by object identity to the declared Variable "clk".
//   - the EventControl's body is a Begin holding exactly 4 statements and
//     declaring no variables of its own.
//   - each of those 4 statements is a SysTaskCall named "$fstrobe" and
//     explicitly not a SysFuncCall, with exactly 2 arguments: RefObj "fd"
//     bound to Variable "fd" first, then RefObj "a" bound to Variable "a" --
//     the Sec 21.3.2 descriptor-first order.
//   - the four calls are four DISTINCT nodes, checked pairwise, so a model
//     that deduplicated the repeated line would fail.
//   - the Initial's body is a Begin holding exactly 1 statement and declaring
//     no variables: a blocking Assignment whose lhs is RefObj "fd" bound to
//     the declared Variable "fd", and whose rhs is a SysFuncCall named
//     "$fopen" and explicitly not a SysTaskCall, with exactly 2 arguments --
//     string Constants "tmp.txt" (56 bits) and "w" (8 bits), in that order.
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
//   - Whether the always block ever runs. "clk" is declared but never
//     driven anywhere in this file, so no posedge would ever occur and the
//     four strobes would never fire. That is event behavior in the time
//     domain; what the source DOES determine is which object the edge is
//     watching, and that is asserted by
//     AlwaysIsTriggeredByPosedgeOfClk.
//   - How many lines the four $fstrobe calls would write, and what they would
//     contain. Sec 21.2.2 defers a strobe to the end of the time step, which
//     only exists while time advances. The nearest real assertions are
//     AlwaysBodyHasFourStrobeStatements and
//     TheFourStrobesAreFourDistinctNodes, which pin that four calls were
//     written and survive as four.
//   - The order in which the four strobes, the initial block and the final
//     block execute relative to one another. Execution order is a time-domain
//     fact. The nearest real assertion is
//     ModuleHasAlwaysThenInitialThenFinalProcess, which pins that the three
//     procedures exist as distinct kinds in source order in the model.
//   - The runtime value of "a", which is strobed but never assigned, and of
//     "clk". Both would hold x throughout; no field records that. The nearest
//     real assertion is VariablesClkAndAAreUnsignedScalarLogic, which pins
//     their declared types instead.
//   - Whether $fopen actually opens "tmp.txt", and therefore whether "fd"
//     holds a real descriptor or 0, and whether $fclose then succeeds. The
//     nearest real assertion is InitialBodyAssignsFopenResultToFd.
//   - The output radix used for "a" in the absence of a format specification.
//     A radix is applied when a value is formatted for output, which only
//     happens while time advances. (Note that unlike $fmonitor in
//     21.3--fmonitor.sv, this file writes no b/o/h suffixed variants at all,
//     so there is not even a static name difference to record here.) The
//     nearest real assertion is FstrobeArgumentsAreDescriptorThenSignal.
//   - The two Begins' own names. Neither block is given a ": label", so both
//     are unnamed; whether the model leaves the name empty or synthesizes an
//     implicit scope name is a tool-internal convention the source does not
//     determine. The scoping fact that matters is asserted instead: both
//     blocks are required to own no variables of their own.
//   - The design-level typespec collection's size and which scope owns the
//     shared "logic" / "int" / string typespec nodes. Typespec sharing is a
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
#include <hldb/always.h>
#include <hldb/any_type.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/final_stmt.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FstrobeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.3--fstrobe.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Variable *getVariable(const char *name) {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, top->getVariables());
  }

  // "always @(posedge clk) begin ... end" -- written first, so index 0.
  static const hldb::Always *getAlwaysProcess() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    return any_cast<hldb::Always>(top->getProcesses()->at(0));
  }

  // Sec 9.4.2: the "@(posedge clk)" wrapping the always block's body.
  static const hldb::EventControl *getEventControl() {
    const hldb::Always *const always = getAlwaysProcess();
    return (always == nullptr) ? nullptr : always->getStmt<hldb::EventControl>();
  }

  // The "begin ... end" holding the four $fstrobe calls.
  static const hldb::Begin *getAlwaysBody() {
    const hldb::EventControl *const ec = getEventControl();
    return (ec == nullptr) ? nullptr : ec->getStmt<hldb::Begin>();
  }

  // "initial begin ... end" -- written second, so index 1.
  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->size() < 2u) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(1));
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }

  // "final $fclose(fd);" -- written third and last, so index 2.
  static const hldb::FinalStmt *getFinalProcess() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->size() < 3u) return nullptr;
    return any_cast<hldb::FinalStmt>(top->getProcesses()->at(2));
  }

  // Every reference in this file names one of the three declared variables.
  // This checks that a given argument is bound to the declared Variable of
  // that name, not merely to something spelled alike.
  static void checkArgumentRefersTo(const hldb::Any *argument, const char *name, const char *where) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(argument);
    ASSERT_NE(ref, nullptr) << where << " should be a RefObj";
    EXPECT_EQ(ref->getName(), name);
    EXPECT_EQ(ref->getActual<hldb::Variable>(), getVariable(name))
        << where << " must bind to the module-level Variable '" << name << "'";
  }
};

// --- module scope and its three variable declarations ------------------------

TEST_F(FstrobeTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr); }

// Sec 6.7 lists the net_type keywords; neither "logic" nor "int" is one of
// them, and "module top();" has an empty port list, so nothing here declares
// a net or drives one continuously.
TEST_F(FstrobeTest, ModuleHasNoNetsNoPortsAndNoContinuousAssignments) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'logic' and 'int' are not net-type keywords (IEEE 1800-2023 Sec 6.7)";
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top();' has an empty port list";
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "the file contains no 'assign' statement";
}

TEST_F(FstrobeTest, ModuleHasExactlyThreeVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr) << "'logic clk', 'logic a' and 'int fd' are all Variables (Sec 6.8)";
  ASSERT_EQ(top->getVariables()->size(), 3u);
  EXPECT_NE(getVariable("clk"), nullptr);
  EXPECT_NE(getVariable("a"), nullptr);
  EXPECT_NE(getVariable("fd"), nullptr);
}

// Sec 6.8: a "logic" declaration with no packed range and no "signed" keyword
// is an unsigned scalar. The two are written separately, so they must be two
// distinct Variable objects.
TEST_F(FstrobeTest, VariablesClkAndAAreUnsignedScalarLogic) {
  const hldb::Variable *const clk = getVariable("clk");
  const hldb::Variable *const a = getVariable("a");
  ASSERT_NE(clk, nullptr);
  ASSERT_NE(a, nullptr);
  EXPECT_NE(clk, a) << "'logic clk;' and 'logic a;' are two separate declarations and must be two "
                       "separate Variable objects";

  const hldb::Variable *const declarations[2] = {clk, a};
  const char *const names[2] = {"clk", "a"};
  for (uint32_t i = 0; i < 2u; ++i) {
    ASSERT_NE(declarations[i]->getTypespec<hldb::RefTypespec>(), nullptr) << "variable " << names[i];
    const hldb::LogicTypespec *const ts =
        declarations[i]->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(ts, nullptr) << "'logic " << names[i] << ";' must resolve to a LogicTypespec";
    EXPECT_FALSE(ts->getSigned()) << "Sec 6.8: 'logic' with no 'signed' keyword defaults to unsigned (" << names[i]
                                  << ")";
    EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty())
        << "'logic " << names[i] << ";' declares no '[msb:lsb]' -- it is an implicit scalar bit";
    EXPECT_EQ(declarations[i]->getValue(), nullptr)
        << "'logic " << names[i] << ";' is declared with no '=' initializer";
  }
}

// Sec 6.11.1: "int" is a 2-state 32-bit signed integer type.
TEST_F(FstrobeTest, VariableFdIsSignedIntWithNoInitializer) {
  const hldb::Variable *const fd = getVariable("fd");
  ASSERT_NE(fd, nullptr);
  ASSERT_NE(fd->getTypespec<hldb::RefTypespec>(), nullptr);
  const hldb::IntTypespec *const ts = fd->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr) << "'int fd;' must resolve to an IntTypespec";
  EXPECT_TRUE(ts->getSigned()) << "IEEE 1800-2023 Sec 6.11.1: 'int' is a signed type";
  EXPECT_EQ(fd->getValue(), nullptr) << "'int fd;' is declared with no '=' initializer";
}

// --- the three procedures ----------------------------------------------------

// Sec 9.2.2.1, Sec 9.2.2 and Sec 9.2.3 define "always", "initial" and "final"
// as three distinct procedure kinds, so the module owns three processes of
// three different node types, in the order they are written.
TEST_F(FstrobeTest, ModuleHasAlwaysThenInitialThenFinalProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 3u) << "one 'always', one 'initial' and one 'final'";

  EXPECT_NE(any_cast<hldb::Always>(top->getProcesses()->at(0)), nullptr) << "the 'always' procedure is written first";
  EXPECT_EQ(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
  EXPECT_EQ(any_cast<hldb::FinalStmt>(top->getProcesses()->at(0)), nullptr);

  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(1)), nullptr)
      << "the 'initial' procedure is written second";
  EXPECT_EQ(any_cast<hldb::Always>(top->getProcesses()->at(1)), nullptr);
  EXPECT_EQ(any_cast<hldb::FinalStmt>(top->getProcesses()->at(1)), nullptr);

  EXPECT_NE(any_cast<hldb::FinalStmt>(top->getProcesses()->at(2)), nullptr)
      << "the 'final' procedure is written third and is its own node kind (Sec 9.2.3)";
  EXPECT_EQ(any_cast<hldb::Always>(top->getProcesses()->at(2)), nullptr);
  EXPECT_EQ(any_cast<hldb::Initial>(top->getProcesses()->at(2)), nullptr);
}

// Sec 9.2.2.1: the bare "always" keyword, not one of the Sec 9.2.2.2-9.2.2.4
// specialized forms.
TEST_F(FstrobeTest, AlwaysIsThePlainAlwaysForm) {
  const hldb::Always *const always = getAlwaysProcess();
  ASSERT_NE(always, nullptr);
  EXPECT_EQ(always->getAlwaysType(), vpiAlways) << "Sec 9.2.2.1: the 'always' keyword gives AlwaysType vpiAlways";
  EXPECT_NE(always->getAlwaysType(), vpiAlwaysComb) << "Sec 9.2.2.2: this is not 'always_comb'";
  EXPECT_NE(always->getAlwaysType(), vpiAlwaysLatch) << "Sec 9.2.2.3: this is not 'always_latch'";
  EXPECT_NE(always->getAlwaysType(), vpiAlwaysFF) << "Sec 9.2.2.4: this is not 'always_ff'";
}

// Sec 9.4.2: "@(posedge clk)" is an event control over a single posedge
// operand.
TEST_F(FstrobeTest, AlwaysIsTriggeredByPosedgeOfClk) {
  const hldb::EventControl *const ec = getEventControl();
  ASSERT_NE(ec, nullptr) << "'always @(...)' should produce an EventControl as the process' statement";

  const hldb::Operation *const cond = ec->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "'posedge clk' should be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiPosedgeOp) << "Sec 9.4.2: 'posedge' is the positive-edge event operator";
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 1u) << "'posedge clk' has exactly one operand";
  checkArgumentRefersTo(cond->getOperands()->at(0), "clk", "the posedge operand");
}

// --- the always body: four identical $fstrobe calls --------------------------

TEST_F(FstrobeTest, AlwaysBodyHasFourStrobeStatements) {
  const hldb::Begin *const body = getAlwaysBody();
  ASSERT_NE(body, nullptr) << "the always block's 'begin ... end' should be a Begin";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "no block_item_declaration is written inside the always block (Sec 9.3.1)";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 4u) << "the source writes '$fstrobe(fd, a);' four times";
}

// Sec 21.3.2: $fstrobe is a file output system TASK, and every task in that
// clause takes the descriptor FIRST, then the values to write.
TEST_F(FstrobeTest, FstrobeArgumentsAreDescriptorThenSignal) {
  const hldb::Begin *const body = getAlwaysBody();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 4u);

  for (uint32_t i = 0; i < 4u; ++i) {
    EXPECT_EQ(any_cast<hldb::SysFuncCall>(body->getStmts()->at(i)), nullptr)
        << "stmt[" << i
        << "]: IEEE 1800-2023 Sec 21.3.2 lists $fstrobe among the file output system "
           "TASKS, so it must not be a SysFuncCall";
    const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(body->getStmts()->at(i));
    ASSERT_NE(call, nullptr) << "stmt[" << i << "] should be a SysTaskCall";
    EXPECT_EQ(call->getName(), "$fstrobe");

    ASSERT_NE(call->getArguments(), nullptr) << "stmt[" << i << "]";
    ASSERT_EQ(call->getArguments()->size(), 2u) << "stmt[" << i << "]: '$fstrobe(fd, a)' passes two arguments";
    checkArgumentRefersTo(call->getArguments()->at(0), "fd", "the $fstrobe descriptor argument");
    checkArgumentRefersTo(call->getArguments()->at(1), "a", "the $fstrobe value argument");
  }
}

// Sec 21.2.2 makes a strobe a one-shot request serviced at the end of the
// current time step, not a standing registration like Sec 21.2.3's $monitor.
// Four identical calls are therefore four independent requests, and the model
// must carry four separate nodes rather than folding the repeated line into
// one shared node.
TEST_F(FstrobeTest, TheFourStrobesAreFourDistinctNodes) {
  const hldb::Begin *const body = getAlwaysBody();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 4u);

  const hldb::SysTaskCall *calls[4] = {nullptr, nullptr, nullptr, nullptr};
  for (uint32_t i = 0; i < 4u; ++i) {
    calls[i] = any_cast<hldb::SysTaskCall>(body->getStmts()->at(i));
    ASSERT_NE(calls[i], nullptr) << "stmt[" << i << "] should be a SysTaskCall";
  }
  for (uint32_t i = 0; i < 4u; ++i) {
    for (uint32_t j = i + 1u; j < 4u; ++j) {
      EXPECT_NE(calls[i], calls[j]) << "stmt[" << i << "] and stmt[" << j
                                    << "] are two separately written '$fstrobe(fd, a);' statements and must be "
                                       "two separate nodes -- the repeated line must not be deduplicated";
    }
  }
}

// --- the initial block: fd = $fopen("tmp.txt", "w"); -------------------------

// Sec 9.3.1: the source wrapped a single statement in begin/end, so a Begin
// is present and holds exactly one statement.
TEST_F(FstrobeTest, InitialBodyAssignsFopenResultToFd) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' body should be a Begin even around one statement";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "no block_item_declaration is written inside the initial block (Sec 9.3.1)";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u) << "the initial block holds the single $fopen assignment";

  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "the one statement should be an Assignment";
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
TEST_F(FstrobeTest, FopenArgumentsAreFilenameAndMode) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u);
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

// --- final $fclose(fd); -------------------------------------------------------

// Sec 9.2.3: a "final" body written without begin/end is a single
// statement_or_null and must bind directly -- the deliberate contrast with
// the "initial begin ... end" above, which does produce a Begin around its
// one statement. Sec 21.3.1: $fclose returns nothing, so it is a task.
TEST_F(FstrobeTest, FinalBodyIsFcloseSysTaskCallWithNoBeginWrapper) {
  const hldb::FinalStmt *const fin = getFinalProcess();
  ASSERT_NE(fin, nullptr);
  EXPECT_EQ(fin->getStmt<hldb::Begin>(), nullptr)
      << "Sec 9.2.3: no begin/end was written on the 'final', so its body must not be wrapped in a "
         "Begin -- unlike the 'initial begin ... end' in the same file";
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
TEST_F(FstrobeTest, CompilerReportsZeroErrors) {
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
