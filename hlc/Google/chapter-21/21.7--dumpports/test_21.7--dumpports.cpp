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
//   tests/Google/chapter-21/21.7--dumpports.sv   (:name: vcd_dumpports_test,
//   :description: vcd dump ports tests, :tags: 21.7,
//   :type: simulation parsing)
// ----------------------------------------------------------------------------
//   module top();
//
//   integer i;
//   string fname = "out.vcd";
//
//   initial begin
//       $dumpports(top, fname);
//       $dumpportslimit(1024*1024, fname);
//
//       i = 1;
//       #100 i = 2;
//       #200 $dumpportsoff(fname);
//       i = 3;
//       #800 $dumpportson(fname);
//       i = 4;
//       #100 $dumpportsflush(fname);
//       i = 5;
//       #300 $dumpportsall(fname);
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
//   Sec 6.8 "Variable declarations" / Sec 6.7 "Net declarations" -- "integer"
//       and "string" are data types, neither is a net_type keyword, and
//       "module top();" has an empty port list. Both "i" and "fname" are
//       therefore module-level Variables and neither is a Net.
//   Sec 6.11.1 "Integer data types" -- "integer" is a 4-state 32-bit SIGNED
//       type, so "i" carries an IntegerTypespec, a different node kind from
//       the IntTypespec that "int" would produce.
//   Sec 6.16 "String data type" -- "fname" carries a StringTypespec, and
//       unlike "i" it is written with a declaration assignment, so it is the
//       only variable in the file whose vpiValue is populated.
//   Sec 5.9 "String literals" -- a string literal is an unsigned integer
//       constant 8 bits wide per character, so "out.vcd" is 7 * 8 = 56 bits.
//   Sec 9.2.2 "Initial procedures" / Sec 9.3.1 "Sequential blocks" -- the one
//       "initial begin ... end" gives exactly one Initial process whose body
//       is a Begin. Nothing is declared inside the block, so both names
//       resolve outward to the module scope, and the blank line in the source
//       is not a statement -- the block holds exactly the twelve statements
//       written.
//   Sec 9.4.1 "Delay control" -- "#100 i = 2;" is a single statement carrying
//       a delay control: the "#100" and the statement it prefixes are ONE
//       entry in the block, not two. Five of the twelve statements are
//       written this way, with the delays 100, 200, 800, 100 and 300 in that
//       order, and four of the six dump calls are reached only through one.
//   Sec 10.4.1 "Blocking procedural assignments" -- all six assignments to
//       "i" use the "=" operator, so all six are blocking, and they count up
//       1, 2, 3, 4, 5, 6 in source order.
//   Sec 21.7 "Value change dump (VCD) files", extended VCD -- this is the
//       clause the file is named for. It exercises SIX of the extended-VCD
//       system tasks: $dumpports, $dumpportslimit, $dumpportsoff,
//       $dumpportson, $dumpportsflush and $dumpportsall. Every one of them is
//       a system TASK with no return value, so none may be a SysFuncCall, and
//       the six names must stay six distinct names.
//       The interesting contrast is with the four-state family in the sibling
//       21.7--dumpfile.sv, and it is entirely about ARGUMENTS:
//         * there, $dumpvars, $dumpoff, $dumpon, $dumpflush and $dumpall are
//           written with NO arguments at all, because a four-state dump has a
//           single implicit dumpfile. Here every one of the six calls names
//           the file explicitly, passing "fname" -- so the same six
//           references must all resolve to the ONE declared string Variable.
//         * $dumpports additionally takes a SCOPE as its first argument.
//           "top" there is the module's own name, not a variable and not a
//           literal, which makes it the only argument in either fixture that
//           is neither.
//         * $dumpportslimit takes the limit FIRST and the file name second,
//           the same "payload then file" order the four-state $dumplimit
//           would have had if it named a file at all.
//       "$dumpportslimit(1024*1024, ...)" writes a MULTIPLICATION, not the
//       number 1048576. A system task argument is an ordinary runtime
//       expression, so nothing requires it to be folded, and the faithful
//       reading of the source is the operation itself.
//   Sec 11.4.2 "Arithmetic operators" -- "1024*1024" is a binary
//       multiplication over two identical operands.
//   Sec 5.7.1 "Integer literal constants" -- 1024, 100, 200, 800, 300 and the
//       six assigned values are unsized, unbased, unsigned decimal literals.
//
// ----------------------------------------------------------------------------
// WHAT IS CHECKED (every assertion below names a concrete value):
//   - module "top" exists, has no Nets, no ports, no parameters and no
//     continuous assignments, and declares exactly 2 Variables, "i" and
//     "fname", which are distinct objects.
//   - "i" resolves to an IntegerTypespec and NOT to a StringTypespec, and has
//     no declaration initializer; "fname" resolves to a StringTypespec and
//     NOT to an IntegerTypespec, and its initializer is a Constant with
//     constType vpiStringConst, value "out.vcd" and size 56 bits. Checking
//     each type both positively and negatively is what proves the two
//     declarations were not transposed.
//   - "top" has exactly one process, and it is an Initial and explicitly not
//     a FinalStmt.
//   - the Initial's body is a Begin that declares no variables of its own and
//     holds exactly TWELVE statements -- the count Sec 9.4.1 gives once a
//     delayed statement is understood as one entry rather than two.
//   - stmt[0] is a SysTaskCall "$dumpports" and explicitly not a SysFuncCall,
//     with exactly 2 arguments: a RefObj named "top" that is neither a
//     Constant nor bound to any declared Variable -- it names the module
//     scope -- followed by a RefObj "fname" bound by object identity to the
//     declared string.
//   - stmt[1] is a SysTaskCall "$dumpportslimit" with exactly 2 arguments: an
//     Operation with opType vpiMultOp carrying exactly two operands, both
//     Constants decompiling to "1024", followed by a RefObj "fname" bound to
//     the same declared string.
//   - the six assignments to "i" -- at stmt[2], inside stmt[3]'s delay
//     control, and at stmt[5], stmt[7], stmt[9] and stmt[11] -- are all
//     blocking, all bind their lhs by object identity to the declared "i",
//     and their rhs Constants decompile to "1" through "6" in that order.
//   - stmt[3], stmt[4], stmt[6], stmt[8] and stmt[10] are DelayControls whose
//     delays are Constants decompiling to "100", "200", "800", "100" and
//     "300" in that order.
//   - the statement inside stmt[3]'s delay control is the "i = 2" Assignment,
//     while the statements inside stmt[4], stmt[6], stmt[8] and stmt[10] are
//     SysTaskCalls named "$dumpportsoff", "$dumpportson", "$dumpportsflush"
//     and "$dumpportsall" respectively, each carrying exactly ONE argument,
//     a RefObj "fname" bound to the declared string.
//   - all six extended-VCD calls are SysTaskCalls, none is a SysFuncCall,
//     their six names are pairwise distinct, they are six distinct nodes, and
//     every one of their "fname" arguments resolves to the SAME single
//     Variable object.
//   - the compiler reports zero fatal / syntax / error diagnostics: every
//     construct above is legal per the clauses cited, and the source carries
//     no ":should_fail_because:" tag.
//
// ----------------------------------------------------------------------------
// WHAT IS NOT CHECKED, AND WHY (permanently out of scope -- HLC is a static
// compiler/elaborator and never a simulator):
//   - Everything the dump tasks would actually do: whether "out.vcd" is
//     created, which ports of "top" get recorded, what values land in the
//     file, and when dumping stops, resumes, checkpoints or flushes. All of
//     that is behavior in the time domain, and an extended VCD file is by
//     definition a record of simulation. Nothing in the model records it. The
//     nearest real assertions are DumpportsNamesTheTopScopeAndTheFile, which
//     pins the scope and the file that would be dumped, and
//     TheFourDelayedDumpportsCallsAreOffOnFlushAll, which pins which task
//     each delay leads to.
//   - The times at which anything happens. The delays are asserted as the
//     written literals 100, 200, 800, 100 and 300; turning those into
//     simulation time needs a time unit and a running clock, neither of which
//     exists before something runs. The nearest real assertion is
//     TheFiveDelayControlsCarryTheWrittenDelays.
//   - The order in which the twelve statements take effect, and the value "i"
//     holds at any moment. Execution order is a time-domain fact, and
//     Variable::getValue() exposes only a declaration-time initializer, which
//     "integer i;" does not have. The nearest real assertion is
//     TheSixAssignmentsCountUpFromOneToSix, which pins the six values the
//     source writes and the order it writes them in.
//   - Whether "fname" still holds "out.vcd" by the time any of the six calls
//     reads it. There is no field that records a later value, and nothing in
//     this file reassigns it in any case. The nearest real assertion is
//     StringVariableIsInitializedToOutVcd.
//   - Whether the size limit is reached, and what happens if it is. That is a
//     property of the dump as it grows. The nearest real assertion is
//     DumpportslimitTakesTheWrittenMultiplicationThenTheFile.
//   - Whether an implementation constant-folds "1024*1024" into 1048576. The
//     argument of a system task is an ordinary runtime expression, so no LRM
//     rule requires it to be reduced; the test therefore asserts the
//     operation the source actually wrote rather than a number the source
//     never spells out.
//   - What object "top" resolves to beyond the fact that it is a reference
//     rather than a literal, and that it is not one of the two declared
//     variables. The source determines that it names the enclosing module
//     scope; which node kind an implementation uses to represent a scope
//     reference in an argument list is a tool-internal convention, so the
//     test pins what the source fixes and stops there.
//   - The bit widths of the unsized decimal literals. Sec 5.7.1 requires only
//     "at least 32 bits" for an unsized literal, so the exact width is
//     implementation-determined; each constant's decompiled value is asserted
//     instead. (The string literal's width IS asserted, because Sec 5.9 fixes
//     it exactly at 8 bits per character.)
//   - The Begin's own name. No ": label" is written, so the block is unnamed;
//     whether the model leaves the name empty or synthesizes an implicit
//     scope name is a tool-internal convention the source does not determine.
//     The scoping fact that matters is asserted instead, by
//     InitialBodyIsBeginWithTwelveStatements, which requires the block to own
//     no variables of its own.
//   - The design-level typespec collection's size and which scope owns the
//     shared IntegerTypespec / StringTypespec nodes. Typespec sharing is
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
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class DumpportsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.7--dumpports.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Variable *getVariable(const char *name) {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, top->getVariables());
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

  // Sec 21.7: an extended-VCD call may sit either directly in the block or
  // inside a delay control. This reaches the call either way.
  static const hldb::SysTaskCall *getDumpCallAt(uint32_t index) {
    const hldb::Any *const stmt = getStmt(index);
    if (stmt == nullptr) return nullptr;
    const hldb::SysTaskCall *const direct = any_cast<hldb::SysTaskCall>(stmt);
    if (direct != nullptr) return direct;
    const hldb::DelayControl *const delayed = any_cast<hldb::DelayControl>(stmt);
    return (delayed == nullptr) ? nullptr : delayed->getStmt<hldb::SysTaskCall>();
  }

  // Every one of the six calls names the file explicitly. This checks that a
  // given argument is a reference bound to the ONE declared string Variable,
  // not merely something spelled "fname".
  static void checkArgumentIsFname(const hldb::Any *argument, const char *where) {
    EXPECT_EQ(any_cast<hldb::Constant>(argument), nullptr)
        << where
        << ": the source wrote the identifier 'fname', so the argument must be a reference, "
           "not a Constant carrying an inlined copy of its initializer";
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(argument);
    ASSERT_NE(ref, nullptr) << where << " should be a RefObj";
    EXPECT_EQ(ref->getName(), "fname");
    EXPECT_EQ(ref->getActual<hldb::Variable>(), getVariable("fname"))
        << where << " must bind to the one declared string Variable 'fname'";
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
};

// --- module scope and its two declarations -----------------------------------

TEST_F(DumpportsTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr); }

// Sec 6.7 lists the net_type keywords; neither "integer" nor "string" is one
// of them, and "module top();" has an empty port list, so nothing here
// declares a net or drives one continuously.
TEST_F(DumpportsTest, ModuleHasNoNetsNoPortsNoParametersAndNoContinuousAssignments) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'integer' and 'string' are not net-type keywords (IEEE 1800-2023 Sec 6.7)";
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top();' has an empty port list";
  EXPECT_TRUE(top->getParameters() == nullptr || top->getParameters()->empty())
      << "no parameter is declared anywhere in this file";
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "the file contains no 'assign' statement";
}

TEST_F(DumpportsTest, ModuleHasExactlyTwoDistinctVariablesIAndFname) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr) << "'integer i;' and 'string fname' must both be Variables (Sec 6.8)";
  ASSERT_EQ(top->getVariables()->size(), 2u);

  const hldb::Variable *const i = getVariable("i");
  const hldb::Variable *const fname = getVariable("fname");
  ASSERT_NE(i, nullptr);
  ASSERT_NE(fname, nullptr);
  EXPECT_NE(i, fname) << "the counter and the file name are two separate declarations and must be two "
                         "separate Variable objects";
}

// Sec 6.11.1: "integer" is the 4-state 32-bit signed type. The negative check
// against StringTypespec proves "i" did not pick up "fname"'s type.
TEST_F(DumpportsTest, VariableIIsIntegerAndNotAString) {
  const hldb::Variable *const i = getVariable("i");
  ASSERT_NE(i, nullptr);
  ASSERT_NE(i->getTypespec<hldb::RefTypespec>(), nullptr);

  EXPECT_NE(i->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntegerTypespec>(), nullptr)
      << "Sec 6.11.1: 'integer' is the 4-state type, so it must resolve to an IntegerTypespec, not an "
         "IntTypespec";
  EXPECT_EQ(i->getTypespec<hldb::RefTypespec>()->getActual<hldb::StringTypespec>(), nullptr)
      << "'integer i;' is not the string declaration -- the two must not be transposed";
  EXPECT_EQ(i->getValue(), nullptr) << "'integer i;' is declared with no '=' initializer";
}

// Sec 6.16 for the type, Sec 6.8 for the declaration assignment and Sec 5.9
// for the literal's width. "fname" is the only variable written with an "=",
// so it is the only one whose vpiValue may be populated.
TEST_F(DumpportsTest, StringVariableIsInitializedToOutVcd) {
  const hldb::Variable *const fname = getVariable("fname");
  ASSERT_NE(fname, nullptr);
  ASSERT_NE(fname->getTypespec<hldb::RefTypespec>(), nullptr);

  EXPECT_NE(fname->getTypespec<hldb::RefTypespec>()->getActual<hldb::StringTypespec>(), nullptr)
      << "Sec 6.16: 'string fname' must resolve to a StringTypespec";
  EXPECT_EQ(fname->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntegerTypespec>(), nullptr)
      << "'string fname' is not the integer declaration -- the two must not be transposed";

  const hldb::Constant *const init = fname->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'string fname = \"out.vcd\";' carries a declaration-time initializer";
  EXPECT_EQ(init->getConstType(), vpiStringConst);
  EXPECT_EQ(init->getValue(), "out.vcd");
  EXPECT_EQ(init->getSize(), 56) << "Sec 5.9: \"out.vcd\" = 7 chars x 8 bits";
}

// --- the single procedure -----------------------------------------------------

// Sec 9.2.2: one "initial" keyword, one Initial process -- and nothing else.
TEST_F(DumpportsTest, ModuleHasExactlyOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u) << "the file writes exactly one procedure";
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr) << "that procedure is an 'initial'";
  EXPECT_EQ(any_cast<hldb::FinalStmt>(top->getProcesses()->at(0)), nullptr)
      << "no 'final' keyword is written in this file";
}

// Sec 9.4.1: a delayed statement is ONE entry, not two, so the twelve written
// statements stay twelve -- seven undelayed and five delayed.
TEST_F(DumpportsTest, InitialBodyIsBeginWithTwelveStatements) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' body should be a Begin";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "no block_item_declaration is written inside the begin-end (Sec 9.3.1); both names resolve "
         "outward to the module scope";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 12u)
      << "Sec 9.4.1: '#100 i = 2;' is a single statement carrying a delay, not a delay plus a "
         "statement, so the twelve written statements stay twelve";
}

// --- the two undelayed extended-VCD calls ------------------------------------

// THE POINT OF THIS FIXTURE, and its sharpest difference from the four-state
// family in 21.7--dumpfile.sv: $dumpports names a SCOPE as well as a file.
// "top" is the module's own name -- neither a literal nor a declared
// variable -- and it is the only argument in either fixture that is neither.
TEST_F(DumpportsTest, DumpportsNamesTheTopScopeAndTheFile) {
  EXPECT_EQ(any_cast<hldb::SysFuncCall>(getStmt(0)), nullptr)
      << "IEEE 1800-2023 Sec 21.7: every extended-VCD dump routine is a system TASK with no return value";
  const hldb::SysTaskCall *const call = getDumpCallAt(0);
  ASSERT_NE(call, nullptr) << "stmt[0] should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$dumpports");

  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u) << "'$dumpports(top, fname)' passes a scope and a file name";

  EXPECT_EQ(any_cast<hldb::Constant>(call->getArguments()->at(0)), nullptr)
      << "'top' is an identifier naming the enclosing module scope, so it cannot be a literal";
  const hldb::RefObj *const scope = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(scope, nullptr) << "the scope argument should be a RefObj";
  EXPECT_EQ(scope->getName(), "top");
  EXPECT_EQ(scope->getActual<hldb::Variable>(), nullptr)
      << "'top' names the module scope, not a variable -- it must not resolve to 'i' or 'fname'";

  checkArgumentIsFname(call->getArguments()->at(1), "the $dumpports file-name argument");
}

// Sec 11.4.2: the source wrote a multiplication, not the number 1048576, and
// a system task argument is an ordinary runtime expression that nothing
// requires to be folded. The limit comes FIRST and the file name second.
TEST_F(DumpportsTest, DumpportslimitTakesTheWrittenMultiplicationThenTheFile) {
  EXPECT_EQ(any_cast<hldb::SysFuncCall>(getStmt(1)), nullptr)
      << "IEEE 1800-2023 Sec 21.7: every extended-VCD dump routine is a system TASK with no return value";
  const hldb::SysTaskCall *const call = getDumpCallAt(1);
  ASSERT_NE(call, nullptr) << "stmt[1] should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$dumpportslimit");

  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u) << "'$dumpportslimit(1024*1024, fname)' passes a limit and a file";

  const hldb::Operation *const product = any_cast<hldb::Operation>(call->getArguments()->at(0));
  ASSERT_NE(product, nullptr) << "the source wrote '1024*1024', so the limit argument is an Operation";
  EXPECT_EQ(product->getOpType(), vpiMultOp) << "Sec 11.4.2: '*' is the multiplication operator";
  ASSERT_NE(product->getOperands(), nullptr);
  ASSERT_EQ(product->getOperands()->size(), 2u) << "'*' is a binary operator";
  for (uint32_t k = 0; k < 2u; ++k) {
    const hldb::Constant *const operand = any_cast<hldb::Constant>(product->getOperands()->at(k));
    ASSERT_NE(operand, nullptr) << "operand " << k << " should be a Constant";
    EXPECT_EQ(operand->getDecompile(), "1024") << "operand " << k;
    EXPECT_EQ(operand->getConstType(), vpiUIntConst) << "Sec 5.7.1: a bare decimal literal is unsigned";
  }

  checkArgumentIsFname(call->getArguments()->at(1), "the $dumpportslimit file-name argument");
}

// --- the six assignments ------------------------------------------------------

// Sec 10.4.1: all six use "=", so all six are blocking, and they count up in
// source order. One of them sits inside a delay control, which is why the
// helper reaches through it.
TEST_F(DumpportsTest, TheSixAssignmentsCountUpFromOneToSix) {
  const uint32_t indexes[6] = {2u, 3u, 5u, 7u, 9u, 11u};
  const char *const values[6] = {"1", "2", "3", "4", "5", "6"};
  for (uint32_t k = 0; k < 6u; ++k) {
    const hldb::Assignment *const assign = getAssignmentAt(indexes[k]);
    ASSERT_NE(assign, nullptr) << "stmt[" << indexes[k] << "] should hold an Assignment";
    EXPECT_TRUE(assign->getBlocking()) << "stmt[" << indexes[k] << "]: Sec 10.4.1 -- '=' is blocking";

    const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr) << "stmt[" << indexes[k] << "] lhs should be a RefObj";
    EXPECT_EQ(lhs->getName(), "i");
    EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariable("i"))
        << "stmt[" << indexes[k] << "]: every assignment writes the declared Variable 'i'";

    const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr) << "stmt[" << indexes[k] << "] rhs should be a Constant";
    EXPECT_EQ(rhs->getDecompile(), values[k]) << "stmt[" << indexes[k] << "]: the six values count up in order";
  }
}

// --- the five delay controls ---------------------------------------------------

// Sec 9.4.1: the delays are written as plain literals, in this order. Note
// that 100 appears twice, so the sequence is checked position by position
// rather than as a set.
TEST_F(DumpportsTest, TheFiveDelayControlsCarryTheWrittenDelays) {
  const uint32_t indexes[5] = {3u, 4u, 6u, 8u, 10u};
  const char *const delays[5] = {"100", "200", "800", "100", "300"};
  for (uint32_t k = 0; k < 5u; ++k) {
    checkDelayIs(indexes[k], delays[k]);
  }
}

// Sec 9.4.1 again: the first delayed statement wraps an assignment, not a
// dump call -- so the wrapper is not exclusive to the extended-VCD tasks.
TEST_F(DumpportsTest, FirstDelayControlWrapsTheAssignmentOfTwo) {
  const hldb::DelayControl *const delayed = any_cast<hldb::DelayControl>(getStmt(3));
  ASSERT_NE(delayed, nullptr) << "stmt[3] should be a DelayControl";
  EXPECT_EQ(delayed->getStmt<hldb::SysTaskCall>(), nullptr) << "'#100 i = 2;' delays an assignment, not a dump call";

  const hldb::Assignment *const assign = delayed->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "the delayed statement should be an Assignment";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariable("i"));
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "2");
}

// Sec 21.7: the remaining four delayed statements each wrap one of the
// extended-VCD tasks, in this order, and -- unlike their four-state
// counterparts in 21.7--dumpfile.sv, which take no arguments at all -- each
// names the file explicitly.
TEST_F(DumpportsTest, TheFourDelayedDumpportsCallsAreOffOnFlushAll) {
  const uint32_t indexes[4] = {4u, 6u, 8u, 10u};
  const char *const names[4] = {"$dumpportsoff", "$dumpportson", "$dumpportsflush", "$dumpportsall"};
  for (uint32_t k = 0; k < 4u; ++k) {
    const hldb::DelayControl *const delayed = any_cast<hldb::DelayControl>(getStmt(indexes[k]));
    ASSERT_NE(delayed, nullptr) << "stmt[" << indexes[k] << "] should be a DelayControl";
    EXPECT_EQ(delayed->getStmt<hldb::SysFuncCall>(), nullptr)
        << "stmt[" << indexes[k] << "]: Sec 21.7 -- an extended-VCD routine is a system task, not a function";
    EXPECT_EQ(delayed->getStmt<hldb::Assignment>(), nullptr)
        << "stmt[" << indexes[k] << "]: this delay wraps a dump call, not an assignment";

    const hldb::SysTaskCall *const call = delayed->getStmt<hldb::SysTaskCall>();
    ASSERT_NE(call, nullptr) << "stmt[" << indexes[k] << "]: the delayed statement should be a SysTaskCall";
    EXPECT_EQ(call->getName(), names[k]);
    ASSERT_NE(call->getArguments(), nullptr) << "stmt[" << indexes[k] << "]";
    ASSERT_EQ(call->getArguments()->size(), 1u)
        << "stmt[" << indexes[k]
        << "]: each of these names the file, unlike the argument-less four-state "
           "$dumpoff / $dumpon / $dumpflush / $dumpall";
    checkArgumentIsFname(call->getArguments()->at(0), "the delayed dump call's file-name argument");
  }
}

// All six extended-VCD routines in this file are tasks, the six names stay six
// distinct names, they are six separate nodes, and every one of them points at
// the SAME declared file-name Variable.
TEST_F(DumpportsTest, AllSixRoutinesAreDistinctTasksSharingOneFilenameVariable) {
  const uint32_t indexes[6] = {0u, 1u, 4u, 6u, 8u, 10u};
  const hldb::SysTaskCall *calls[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
  for (uint32_t k = 0; k < 6u; ++k) {
    calls[k] = getDumpCallAt(indexes[k]);
    ASSERT_NE(calls[k], nullptr) << "stmt[" << indexes[k] << "] should hold a SysTaskCall";
    ASSERT_NE(calls[k]->getArguments(), nullptr) << "stmt[" << indexes[k] << "]";
    ASSERT_FALSE(calls[k]->getArguments()->empty()) << "stmt[" << indexes[k] << "]";

    // The file name is the LAST argument of every one of the six calls.
    const uint32_t last = static_cast<uint32_t>(calls[k]->getArguments()->size()) - 1u;
    const hldb::RefObj *const file = any_cast<hldb::RefObj>(calls[k]->getArguments()->at(last));
    ASSERT_NE(file, nullptr) << "stmt[" << indexes[k] << "]: the trailing argument names the file";
    EXPECT_EQ(file->getActual<hldb::Variable>(), getVariable("fname"))
        << "stmt[" << indexes[k] << "]: all six calls must name the one declared 'fname'";
  }

  for (uint32_t a = 0; a < 6u; ++a) {
    for (uint32_t b = a + 1u; b < 6u; ++b) {
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
TEST_F(DumpportsTest, CompilerReportsZeroErrors) {
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
