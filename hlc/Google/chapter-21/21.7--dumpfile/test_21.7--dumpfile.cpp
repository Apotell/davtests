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
//   tests/Google/chapter-21/21.7--dumpfile.sv   (:name: vcd_dump_test,
//   :description: vcd dump tests, :tags: 21.7, :type: simulation parsing)
// ----------------------------------------------------------------------------
//   module top();
//
//   integer i;
//
//   initial begin
//       $dumpfile("out.vcd");
//       $dumpvars;
//       $dumplimit(1024*1024);
//
//       i = 1;
//       #100 i = 2;
//       #200 $dumpoff;
//       i = 3;
//       #800 $dumpon;
//       i = 4;
//       #100 $dumpflush;
//       i = 5;
//       #300 $dumpall;
//       i = 6;
//   end
//
//   endmodule
// ============================================================================
//
// IEEE 1800-2023 constructs present in this file, and what each one pins down
// statically (all of this was derived by reading the .sv text above and the
// LRM; nothing here was taken from tool output):
//
//   Sec 6.11.1 "Integer data types" -- "integer" is a 4-state 32-bit SIGNED
//       type, and an integer_atom_type in the Sec 6.8 variable declaration
//       grammar, never a net_type (Sec 6.7). "i" is a Variable, and it is the
//       file's only declaration.
//   Sec 9.2.2 "Initial procedures" / Sec 9.3.1 "Sequential blocks" -- the one
//       "initial begin ... end" gives exactly one Initial process whose body
//       is a Begin. Nothing is declared inside the block, so "i" resolves
//       outward to the module scope. The blank line in the source is not a
//       statement, so the block holds exactly the thirteen statements written.
//   Sec 9.4.1 "Delay control" -- "#100 i = 2;" is a single statement carrying
//       a delay control: the "#100" and the statement it prefixes are ONE
//       entry in the block, not two, and the model wraps the delayed
//       statement rather than leaving it bare. Five of the thirteen
//       statements are written this way, with the delays 100, 200, 800, 100
//       and 300 in that order. This is the fixture's second structural point
//       after the dump tasks themselves: four of the seven VCD calls are
//       reached only through a delay control, so a model that dropped the
//       wrapper would also change how many statements the block appears to
//       hold.
//   Sec 10.4.1 "Blocking procedural assignments" -- all six assignments to
//       "i" use the "=" operator, so all six are blocking, and they count up
//       1, 2, 3, 4, 5, 6 in source order.
//   Sec 21.7 "VCD files" -- this is the clause the file is named for, and it
//       exercises SEVEN of its system tasks: $dumpfile (Sec 21.7.1, names the
//       output file), $dumpvars (Sec 21.7.2, selects what to dump),
//       $dumplimit, $dumpoff, $dumpon, $dumpflush and $dumpall. Every one of
//       them is a system TASK with no return value, which puts the whole
//       family on the opposite side of the task/function split from the
//       Sec 21.3 file-I/O routines and from Sec 21.6's $test$plusargs and
//       $value$plusargs -- all of which return an integer and can therefore
//       stand in an expression. None of these seven may be a SysFuncCall, and
//       the seven names must stay seven distinct names.
//       Two further facts come straight from the source text:
//         * "$dumpvars;" is written with NO parentheses and NO arguments,
//           which is the Sec 21.7.2 form that selects the whole design. The
//           argument-less form is a real distinction from the others here,
//           so the absence is asserted rather than assumed.
//         * "$dumplimit(1024*1024);" writes a MULTIPLICATION, not the number
//           1048576. A system task argument is an ordinary runtime
//           expression, so nothing requires it to be folded, and the faithful
//           reading of the source is the operation itself.
//   Sec 11.4.2 "Arithmetic operators" -- "1024*1024" is a binary
//       multiplication over two identical operands.
//   Sec 5.7.1 "Integer literal constants" -- 1024, 100, 200, 800, 300 and the
//       six assigned values are unsized, unbased, unsigned decimal literals.
//   Sec 5.9 "String literals" -- a string literal is an unsigned integer
//       constant 8 bits wide per character, so "out.vcd" is 7 * 8 = 56 bits.
//
// ----------------------------------------------------------------------------
// WHAT IS CHECKED (every assertion below names a concrete value):
//   - module "top" exists, has no Nets, no ports, no parameters and no
//     continuous assignments, and declares exactly ONE Variable, "i",
//     resolving to an IntegerTypespec with no declaration initializer.
//   - "top" has exactly one process, and it is an Initial and explicitly not
//     a FinalStmt.
//   - the Initial's body is a Begin that declares no variables of its own and
//     holds exactly THIRTEEN statements -- the count Sec 9.4.1 gives once a
//     delayed statement is understood as one entry rather than two.
//   - stmt[0] is a SysTaskCall "$dumpfile" and explicitly not a SysFuncCall,
//     with exactly one argument: a Constant with constType vpiStringConst,
//     value "out.vcd" and size 56 bits.
//   - stmt[1] is a SysTaskCall "$dumpvars" carrying ZERO arguments.
//   - stmt[2] is a SysTaskCall "$dumplimit" with exactly one argument, an
//     Operation with opType vpiMultOp carrying exactly two operands, both
//     Constants decompiling to "1024".
//   - the six assignments to "i" -- at stmt[3], inside stmt[4]'s delay
//     control, and at stmt[6], stmt[8], stmt[10] and stmt[12] -- are all
//     blocking, all bind their lhs by object identity to the one declared
//     Variable, and their rhs Constants decompile to "1", "2", "3", "4", "5"
//     and "6" in that order.
//   - stmt[4], stmt[5], stmt[7], stmt[9] and stmt[11] are DelayControls whose
//     delays are Constants decompiling to "100", "200", "800", "100" and
//     "300" in that order.
//   - the statement inside stmt[4]'s delay control is the "i = 2" Assignment,
//     while the statements inside stmt[5], stmt[7], stmt[9] and stmt[11] are
//     SysTaskCalls named "$dumpoff", "$dumpon", "$dumpflush" and "$dumpall"
//     respectively, each carrying zero arguments.
//   - all seven VCD calls are SysTaskCalls, none of them is a SysFuncCall,
//     and their seven names are pairwise distinct.
//   - the compiler reports zero fatal / syntax / error diagnostics: every
//     construct above is legal per the clauses cited, and the source carries
//     no ":should_fail_because:" tag.
//
// ----------------------------------------------------------------------------
// WHAT IS NOT CHECKED, AND WHY (permanently out of scope -- HLC is a static
// compiler/elaborator and never a simulator):
//   - Everything the dump tasks would actually do: whether "out.vcd" is
//     created, what values land in it, when dumping stops and resumes, when
//     the checkpoint is written and when the buffer is flushed. All of that
//     is behavior in the time domain, and a VCD file is by definition a
//     record of simulation. Nothing in the model records it. The nearest real
//     assertions are DumpfileNamesOutVcd, which pins the file name that would
//     be opened, and TheFourDelayedDumpTasksAreOffOnFlushAll, which pins
//     which task each delay leads to.
//   - The times at which anything happens. The delays are asserted as the
//     written literals 100, 200, 800, 100 and 300; converting those into
//     simulation time requires a time unit and a running clock, neither of
//     which exists before something runs. The nearest real assertion is
//     TheFiveDelayControlsCarryTheWrittenDelays.
//   - The order in which the thirteen statements take effect, and the value
//     "i" holds at any moment. Execution order is a time-domain fact, and
//     Variable::getValue() exposes only a declaration-time initializer, which
//     "integer i;" does not have. The nearest real assertion is
//     TheSixAssignmentsCountUpFromOneToSix, which pins the six values the
//     source writes and the order it writes them in.
//   - Whether the size limit is reached, and what happens if it is. That is a
//     property of the dump as it grows. The nearest real assertion is
//     DumplimitArgumentIsTheWrittenMultiplication.
//   - Whether an implementation constant-folds "1024*1024" into 1048576. The
//     argument of a system task is an ordinary runtime expression, so no LRM
//     rule requires it to be reduced; the test therefore asserts the
//     operation the source actually wrote rather than a number the source
//     never spells out.
//   - The bit widths of the unsized decimal literals. Sec 5.7.1 requires only
//     "at least 32 bits" for an unsized literal, so the exact width is
//     implementation-determined; each constant's decompiled value is asserted
//     instead. (The string literal's width IS asserted, because Sec 5.9 fixes
//     it exactly at 8 bits per character.)
//   - The Begin's own name. No ": label" is written, so the block is unnamed;
//     whether the model leaves the name empty or synthesizes an implicit
//     scope name is a tool-internal convention the source does not determine.
//     The scoping fact that matters is asserted instead, by
//     InitialBodyIsBeginWithThirteenStatements, which requires the block to
//     own no variables of its own.
//   - The design-level typespec collection's size and which scope owns the
//     shared IntegerTypespec / string typespec nodes. Typespec sharing is
//     likewise a tool-internal convention.
//   - The warning count. Whether a legal file also draws advisory warnings is
//     tool policy, not something the LRM fixes, so only the fatal/syntax/
//     error counts are asserted below.
//
// No getElaborated() gating is used anywhere in this file. That is the
// correct answer here rather than an omission: the fixture has no parameters,
// no generate constructs and no module instantiations, and its one arithmetic
// expression sits in a runtime argument position that nothing requires to be
// reduced. Every value asserted below is established by parsing and name
// resolution alone.
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
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/final_stmt.h>
#include <hldb/initial.h>
#include <hldb/integer_typespec.h>
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

class DumpfileTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.7--dumpfile.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  // "integer i;" -- the file's one and only declaration.
  static const hldb::Variable *getI() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("i", top->getVariables());
  }

  // "initial begin ... end" -- the module's only process.
  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }

  static const hldb::Any *getStmt(uint32_t index) {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() <= index) return nullptr;
    return body->getStmts()->at(index);
  }

  // Sec 9.4.1: "#100 i = 2;" is one statement wrapping another, so an
  // assignment may sit either directly in the block or inside a delay
  // control. This reaches the assignment either way.
  static const hldb::Assignment *getAssignmentAt(uint32_t index) {
    const hldb::Any *const stmt = getStmt(index);
    if (stmt == nullptr) return nullptr;
    const hldb::Assignment *const direct = any_cast<hldb::Assignment>(stmt);
    if (direct != nullptr) return direct;
    const hldb::DelayControl *const delayed = any_cast<hldb::DelayControl>(stmt);
    return (delayed == nullptr) ? nullptr : delayed->getStmt<hldb::Assignment>();
  }

  // Sec 21.7: a VCD call may sit either directly in the block or inside a
  // delay control. This reaches the call either way.
  static const hldb::SysTaskCall *getDumpCallAt(uint32_t index) {
    const hldb::Any *const stmt = getStmt(index);
    if (stmt == nullptr) return nullptr;
    const hldb::SysTaskCall *const direct = any_cast<hldb::SysTaskCall>(stmt);
    if (direct != nullptr) return direct;
    const hldb::DelayControl *const delayed = any_cast<hldb::DelayControl>(stmt);
    return (delayed == nullptr) ? nullptr : delayed->getStmt<hldb::SysTaskCall>();
  }

  // Shared body for the five delayed statements.
  static void checkDelayIs(uint32_t index, const char *expectedDelay) {
    const hldb::DelayControl *const delayed = any_cast<hldb::DelayControl>(getStmt(index));
    ASSERT_NE(delayed, nullptr) << "stmt[" << index
                                << "]: Sec 9.4.1 -- a '#<delay> <statement>' is a "
                                   "DelayControl wrapping the statement it prefixes";
    const hldb::Constant *const delay = delayed->getDelay<hldb::Constant>();
    ASSERT_NE(delay, nullptr) << "stmt[" << index << "]: the delay is written as a plain literal";
    EXPECT_EQ(delay->getDecompile(), expectedDelay) << "stmt[" << index << "]";
  }

  // Shared body for the argument-less VCD tasks.
  static void checkDumpTaskTakesNoArguments(const hldb::SysTaskCall *call, const char *expectedName,
                                            const char *where) {
    ASSERT_NE(call, nullptr) << where << " should be a SysTaskCall";
    EXPECT_EQ(call->getName(), expectedName) << where;
    EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty())
        << where << ": '" << expectedName << ";' is written with no parentheses and no arguments";
  }
};

// --- module scope and its single declaration ---------------------------------

TEST_F(DumpfileTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr); }

// Sec 6.7 lists the net_type keywords; "integer" is not one of them, and
// "module top();" has an empty port list, so nothing here declares a net or
// drives one continuously.
TEST_F(DumpfileTest, ModuleHasNoNetsNoPortsNoParametersAndNoContinuousAssignments) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'integer' is not a net-type keyword (IEEE 1800-2023 Sec 6.7)";
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top();' has an empty port list";
  EXPECT_TRUE(top->getParameters() == nullptr || top->getParameters()->empty())
      << "no parameter is declared anywhere in this file";
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "the file contains no 'assign' statement";
}

// Sec 6.11.1: "integer" is the 4-state 32-bit signed type.
TEST_F(DumpfileTest, ModuleHasExactlyOneIntegerVariableI) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr) << "'integer i;' must be a Variable (Sec 6.8)";
  ASSERT_EQ(top->getVariables()->size(), 1u) << "'integer i;' is the file's only declaration";

  const hldb::Variable *const i = getI();
  ASSERT_NE(i, nullptr);
  ASSERT_NE(i->getTypespec<hldb::RefTypespec>(), nullptr);
  EXPECT_NE(i->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntegerTypespec>(), nullptr)
      << "Sec 6.11.1: 'integer' is the 4-state type, so it must resolve to an IntegerTypespec, not an "
         "IntTypespec";
  EXPECT_EQ(i->getValue(), nullptr) << "'integer i;' is declared with no '=' initializer";
}

// --- the single procedure -----------------------------------------------------

// Sec 9.2.2: one "initial" keyword, one Initial process -- and nothing else.
TEST_F(DumpfileTest, ModuleHasExactlyOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u) << "the file writes exactly one procedure";
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr) << "that procedure is an 'initial'";
  EXPECT_EQ(any_cast<hldb::FinalStmt>(top->getProcesses()->at(0)), nullptr)
      << "no 'final' keyword is written in this file";
}

// Sec 9.4.1: a delayed statement is ONE entry, not two, so the thirteen
// written statements stay thirteen -- eight undelayed and five delayed.
TEST_F(DumpfileTest, InitialBodyIsBeginWithThirteenStatements) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' body should be a Begin";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "no block_item_declaration is written inside the begin-end (Sec 9.3.1); 'i' resolves outward "
         "to the module scope";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 13u)
      << "Sec 9.4.1: '#100 i = 2;' is a single statement carrying a delay, not a delay plus a "
         "statement, so the thirteen written statements stay thirteen";
}

// --- the three undelayed VCD calls -------------------------------------------

// Sec 21.7.1: $dumpfile names the output file. Sec 5.9 fixes the literal's
// width at 8 bits per character.
TEST_F(DumpfileTest, DumpfileNamesOutVcd) {
  EXPECT_EQ(any_cast<hldb::SysFuncCall>(getStmt(0)), nullptr)
      << "IEEE 1800-2023 Sec 21.7: every VCD dump routine is a system TASK with no return value";
  const hldb::SysTaskCall *const call = getDumpCallAt(0);
  ASSERT_NE(call, nullptr) << "stmt[0] should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$dumpfile");

  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "'$dumpfile(\"out.vcd\")' passes just the file name";
  const hldb::Constant *const name = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(name, nullptr) << "the argument is the string literal \"out.vcd\"";
  EXPECT_EQ(name->getConstType(), vpiStringConst);
  EXPECT_EQ(name->getValue(), "out.vcd");
  EXPECT_EQ(name->getSize(), 56) << "Sec 5.9: \"out.vcd\" = 7 chars x 8 bits";
}

// Sec 21.7.2: written with no parentheses at all, which is the form that
// selects the whole design. The absence of arguments is a real distinction
// from the two calls around it, so it is asserted rather than assumed.
TEST_F(DumpfileTest, DumpvarsIsCalledWithNoArguments) {
  EXPECT_EQ(any_cast<hldb::SysFuncCall>(getStmt(1)), nullptr)
      << "IEEE 1800-2023 Sec 21.7: every VCD dump routine is a system TASK with no return value";
  checkDumpTaskTakesNoArguments(getDumpCallAt(1), "$dumpvars", "stmt[1]");
}

// Sec 11.4.2: the source wrote a multiplication, not the number 1048576, and
// a system task argument is an ordinary runtime expression that nothing
// requires to be folded -- so the operation itself is the faithful reading.
TEST_F(DumpfileTest, DumplimitArgumentIsTheWrittenMultiplication) {
  EXPECT_EQ(any_cast<hldb::SysFuncCall>(getStmt(2)), nullptr)
      << "IEEE 1800-2023 Sec 21.7: every VCD dump routine is a system TASK with no return value";
  const hldb::SysTaskCall *const call = getDumpCallAt(2);
  ASSERT_NE(call, nullptr) << "stmt[2] should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$dumplimit");

  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "'$dumplimit(1024*1024)' passes exactly one argument";
  const hldb::Operation *const product = any_cast<hldb::Operation>(call->getArguments()->at(0));
  ASSERT_NE(product, nullptr) << "the source wrote '1024*1024', so the argument is an Operation";
  EXPECT_EQ(product->getOpType(), vpiMultOp) << "Sec 11.4.2: '*' is the multiplication operator";
  ASSERT_NE(product->getOperands(), nullptr);
  ASSERT_EQ(product->getOperands()->size(), 2u) << "'*' is a binary operator";
  for (uint32_t k = 0; k < 2u; ++k) {
    const hldb::Constant *const operand = any_cast<hldb::Constant>(product->getOperands()->at(k));
    ASSERT_NE(operand, nullptr) << "operand " << k << " should be a Constant";
    EXPECT_EQ(operand->getDecompile(), "1024") << "operand " << k;
    EXPECT_EQ(operand->getConstType(), vpiUIntConst) << "Sec 5.7.1: a bare decimal literal is unsigned";
  }
}

// --- the six assignments ------------------------------------------------------

// Sec 10.4.1: all six use "=", so all six are blocking, and they count up in
// source order. One of them sits inside a delay control, which is why the
// helper reaches through it.
TEST_F(DumpfileTest, TheSixAssignmentsCountUpFromOneToSix) {
  const uint32_t indexes[6] = {3u, 4u, 6u, 8u, 10u, 12u};
  const char *const values[6] = {"1", "2", "3", "4", "5", "6"};
  for (uint32_t k = 0; k < 6u; ++k) {
    const hldb::Assignment *const assign = getAssignmentAt(indexes[k]);
    ASSERT_NE(assign, nullptr) << "stmt[" << indexes[k] << "] should hold an Assignment";
    EXPECT_TRUE(assign->getBlocking()) << "stmt[" << indexes[k] << "]: Sec 10.4.1 -- '=' is blocking";

    const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr) << "stmt[" << indexes[k] << "] lhs should be a RefObj";
    EXPECT_EQ(lhs->getName(), "i");
    EXPECT_EQ(lhs->getActual<hldb::Variable>(), getI())
        << "stmt[" << indexes[k] << "]: every assignment writes the one declared Variable 'i'";

    const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr) << "stmt[" << indexes[k] << "] rhs should be a Constant";
    EXPECT_EQ(rhs->getDecompile(), values[k]) << "stmt[" << indexes[k] << "]: the six values count up in order";
  }
}

// --- the five delay controls ---------------------------------------------------

// Sec 9.4.1: the delays are written as plain literals, in this order. Note
// that 100 appears twice, so the sequence is checked position by position
// rather than as a set.
TEST_F(DumpfileTest, TheFiveDelayControlsCarryTheWrittenDelays) {
  const uint32_t indexes[5] = {4u, 5u, 7u, 9u, 11u};
  const char *const delays[5] = {"100", "200", "800", "100", "300"};
  for (uint32_t k = 0; k < 5u; ++k) {
    checkDelayIs(indexes[k], delays[k]);
  }
}

// Sec 9.4.1 again: the first delayed statement wraps an assignment, not a
// dump call -- so the wrapper is not exclusive to the VCD tasks.
TEST_F(DumpfileTest, FirstDelayControlWrapsTheAssignmentOfTwo) {
  const hldb::DelayControl *const delayed = any_cast<hldb::DelayControl>(getStmt(4));
  ASSERT_NE(delayed, nullptr) << "stmt[4] should be a DelayControl";
  EXPECT_EQ(delayed->getStmt<hldb::SysTaskCall>(), nullptr) << "'#100 i = 2;' delays an assignment, not a dump call";

  const hldb::Assignment *const assign = delayed->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "the delayed statement should be an Assignment";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getI());
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "2");
}

// Sec 21.7: the remaining four delayed statements each wrap one of the VCD
// tasks, in this order, and each is written with no arguments.
TEST_F(DumpfileTest, TheFourDelayedDumpTasksAreOffOnFlushAll) {
  const uint32_t indexes[4] = {5u, 7u, 9u, 11u};
  const char *const names[4] = {"$dumpoff", "$dumpon", "$dumpflush", "$dumpall"};
  for (uint32_t k = 0; k < 4u; ++k) {
    const hldb::DelayControl *const delayed = any_cast<hldb::DelayControl>(getStmt(indexes[k]));
    ASSERT_NE(delayed, nullptr) << "stmt[" << indexes[k] << "] should be a DelayControl";
    EXPECT_EQ(delayed->getStmt<hldb::SysFuncCall>(), nullptr)
        << "stmt[" << indexes[k] << "]: Sec 21.7 -- a VCD dump routine is a system task, not a function";
    EXPECT_EQ(delayed->getStmt<hldb::Assignment>(), nullptr)
        << "stmt[" << indexes[k] << "]: this delay wraps a dump call, not an assignment";
    checkDumpTaskTakesNoArguments(delayed->getStmt<hldb::SysTaskCall>(), names[k], "the delayed dump call");
  }
}

// All seven VCD routines in this file are tasks, and the seven names stay
// seven distinct names -- a model that canonicalized the family down to one
// entry point would fail here.
TEST_F(DumpfileTest, AllSevenVcdRoutinesAreTasksWithSevenDistinctNames) {
  const uint32_t indexes[7] = {0u, 1u, 2u, 5u, 7u, 9u, 11u};
  const hldb::SysTaskCall *calls[7] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
  for (uint32_t k = 0; k < 7u; ++k) {
    calls[k] = getDumpCallAt(indexes[k]);
    ASSERT_NE(calls[k], nullptr) << "stmt[" << indexes[k] << "] should hold a SysTaskCall";
  }
  for (uint32_t a = 0; a < 7u; ++a) {
    for (uint32_t b = a + 1u; b < 7u; ++b) {
      EXPECT_NE(calls[a]->getName(), calls[b]->getName())
          << "IEEE 1800-2023 Sec 21.7 lists these as separate system tasks -- stmt[" << indexes[a] << "] and stmt["
          << indexes[b] << "] must not share a name";
      EXPECT_NE(calls[a], calls[b]) << "stmt[" << indexes[a] << "] and stmt[" << indexes[b]
                                    << "] are two separately written calls and must be two separate nodes";
    }
  }
}

// --- compiler diagnostics -----------------------------------------------------

// The source carries no ":should_fail_because:" tag and every construct in it
// is legal per the clauses cited above, so nothing may be reported as an
// error. (The warning count is deliberately not asserted -- see the header.)
TEST_F(DumpfileTest, CompilerReportsZeroErrors) {
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
