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
//   tests/Google/chapter-21/21.3--sscanf.sv   (:name: sscanf_task,
//   :description: $sscanf test, :tags: 21.3, :type: simulation parsing)
// ----------------------------------------------------------------------------
//   module top();
//
//   string str = "1234";
//   int c;
//
//   initial begin
//       $sscanf(str, "%d", c);
//       $display(":assert: (%d == %s)", c, str);
//   end
//
//   endmodule
// ============================================================================
//
// IEEE 1800-2023 constructs present in this file, and what each one pins down
// statically (all of this was derived by reading the .sv text above and the
// LRM; nothing here was taken from tool output):
//
//   Sec 6.8 "Variable declarations" / Sec 6.7 "Net declarations" -- "string"
//       and "int" are both data types, neither is a net_type keyword, and
//       "module top();" has an empty port list. Both "str" and "c" are
//       therefore module-level Variables and neither is a Net.
//   Sec 6.16 "String data type" -- "string" is its own data type, not a
//       packed vector, so "str" carries a StringTypespec.
//   Sec 6.11.1 "Integer data types" -- "int" is a 2-state 32-bit SIGNED
//       integer type, so "c" carries a signed IntTypespec. The two
//       declarations therefore differ in TYPE as well as in name, which is
//       what makes the $sscanf argument-order check below decisive.
//   Sec 6.8 also gives the declaration assignment form. "str" is the only
//       variable here written WITH an "=", so it is the only one whose
//       vpiValue is populated; "int c;" must have none.
//   Sec 9.2.2 "Initial procedures" / Sec 9.3.1 "Sequential blocks" -- the one
//       "initial begin ... end" gives exactly one Initial process whose body
//       is a Begin holding the two statements in source order. Nothing is
//       declared inside the block, so the Begin owns no variables of its own
//       and both names resolve outward to the module scope. This file writes
//       no "final" procedure.
//   Sec 21.3.4.1 "Reading formatted data" -- this is the clause the file is
//       named for. The LRM gives $fscanf and $sscanf side by side:
//           integer code = $fscanf(fd,  format, args);
//           integer code = $sscanf(str, format, args);
//       so $sscanf is a system FUNCTION returning the number of items it
//       successfully matched, and it is a SysFuncCall even here, where the
//       file writes it in statement position and throws the count away --
//       discarding a result does not turn a system function into a system
//       task. The two prototypes share one positional shape: SOURCE FIRST,
//       then the format string, then the destination(s). What differs is only
//       what the source is -- $fscanf reads a file descriptor, $sscanf reads
//       a STRING. Nothing in this file is ever opened or closed, and that is
//       not an omission: $sscanf needs no descriptor, so unlike every other
//       21.3 fixture there is no $fopen and no $fclose here at all, and no
//       assignment statement of any kind.
//       Note also that the source argument is the identifier "str", not a
//       literal, so it must survive as a REFERENCE to the declared string
//       variable rather than being folded into a fresh Constant carrying
//       "1234" from the declaration.
//   Sec 21.2.1 "$display and $write" -- $display is a system TASK, hence a
//       SysTaskCall, and takes a format string followed by the values to
//       print, here "c" then "str" matching the "%d" then "%s" of its format.
//   Sec 5.9 "String literals" -- a string literal is an unsigned integer
//       constant 8 bits wide per character: "1234" is 4 * 8 = 32 bits, "%d"
//       is 2 * 8 = 16 bits and ":assert: (%d == %s)" is 19 * 8 = 152 bits.
//
// ----------------------------------------------------------------------------
// WHAT IS CHECKED (every assertion below names a concrete value):
//   - module "top" exists, has no Nets, no ports and no continuous
//     assignments, and declares exactly 2 Variables, "str" and "c", which are
//     distinct objects.
//   - "str" resolves to a StringTypespec and NOT to an IntTypespec, and is
//     the one variable that DOES carry a declaration initializer: a
//     vpiStringConst Constant whose value is "1234" and whose size is 32
//     bits.
//   - "c" resolves to a signed IntTypespec and NOT to a StringTypespec, and
//     carries no declaration initializer. Checking each type both positively
//     and negatively is what proves the two declarations were not transposed.
//   - "top" has exactly 1 process, and it is an Initial and explicitly not a
//     FinalStmt.
//   - the Initial's body is a Begin that declares no variables of its own and
//     holds exactly 2 statements, neither of which is an Assignment -- there
//     is no descriptor to open, so nothing is assigned anywhere in this file.
//   - stmt[0] is a SysFuncCall named "$sscanf" and explicitly not a
//     SysTaskCall, with exactly 3 arguments, in this order: RefObj "str"
//     bound to Variable "str" (and NOT a Constant), a vpiStringConst Constant
//     whose text is "%d" and whose width is 16 bits, and RefObj "c" bound to
//     Variable "c".
//   - within that call, the scanned source at arg[0] and the destination at
//     arg[2] resolve to two DIFFERENT Variable objects; arg[0] resolves to
//     the StringTypespec declaration and arg[2] to the IntTypespec one -- the
//     source-first order of Sec 21.3.4.1, pinned by type as well as by name.
//   - stmt[1] is a SysTaskCall named "$display" and explicitly not a
//     SysFuncCall, with exactly 3 arguments: a vpiStringConst Constant whose
//     text is ":assert: (%d == %s)" and whose width is 152 bits, then RefObj
//     "c" bound to Variable "c", then RefObj "str" bound to Variable "str" --
//     the order matching the format's "%d" then "%s".
//   - the compiler reports zero fatal / syntax / error diagnostics: the file
//     carries no ":should_fail_because:" tag and is legal per the clauses
//     above.
//
// ----------------------------------------------------------------------------
// WHAT IS NOT CHECKED, AND WHY (permanently out of scope -- HLC is a static
// compiler/elaborator and never a simulator):
//   - The value $sscanf parses into "c", and the match count it returns.
//     Both exist only once time advances; Variable::getValue() exposes only a
//     declaration-time initializer, and "int c;" has none, so no field
//     anywhere records a post-execution value. The nearest real assertion is
//     SscanfArgumentsAreSourceFormatThenDestination, which pins which
//     declared object the parsed value would be written into.
//   - Whether the claim the ":assert:" format string makes actually holds --
//     that "c" ends up equal to "str", i.e. that "1234" parses to 1234. That
//     is the file's own record of a runtime expectation. What the source DOES
//     determine is the exact text of both literals, and both are asserted
//     verbatim by DisplayFormatsCThenStrAgainstTheAssertText and
//     SscanfArgumentsAreSourceFormatThenDestination.
//   - What "%d" and "%s" mean when the calls run: consuming a decimal field,
//     and formatting a value as text. Interpreting a conversion specification
//     is work done while time advances. The nearest real assertions are the
//     same two tests, which pin the specifications' text.
//   - The printed output itself and any trailing newline. Formatting happens
//     while time advances. The nearest real assertion is
//     DisplayFormatsCThenStrAgainstTheAssertText.
//   - Whether "str" still holds "1234" by the time $sscanf reads it.
//     Variable::getValue() exposes only the declaration-time initializer;
//     there is no field that records a later value, and nothing in this file
//     reassigns "str" in any case. The nearest real assertion is
//     VariableStrIsAStringInitializedTo1234.
//   - That the two statements execute in the order written, so that "c" is
//     parsed before it is displayed. Execution order is a time-domain fact.
//     The nearest real assertion is InitialBodyIsBeginWithTwoStatements plus
//     the two per-statement tests, which pin the source order in the model.
//   - The Begin's own name. No ": label" is written, so the block is unnamed;
//     whether the model leaves the name empty or synthesizes an implicit
//     scope name is a tool-internal convention the source does not determine.
//     The scoping fact that matters is asserted instead, by
//     InitialBodyIsBeginWithTwoStatements, which requires the block to own no
//     variables of its own.
//   - The design-level typespec collection's size and which scope owns the
//     shared string and "int" typespec nodes. Typespec sharing is a
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
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class SscanfTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.3--sscanf.hlc"}); }
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

  // "$sscanf(str, "%d", c);" -- stmt[0] of the initial block.
  static const hldb::SysFuncCall *getSscanfCall() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) return nullptr;
    return any_cast<hldb::SysFuncCall>(body->getStmts()->at(0));
  }

  // "$display(...);" -- stmt[1] of the initial block.
  static const hldb::SysTaskCall *getDisplayCall() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() < 2u) return nullptr;
    return any_cast<hldb::SysTaskCall>(body->getStmts()->at(1));
  }

  // Both variables in this file are referenced by name from inside the
  // initial block. This checks that a given argument is a reference bound to
  // the one declared Variable of that name, not merely something spelled
  // alike.
  static void checkArgumentRefersTo(const hldb::Any *argument, const char *name, const char *where) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(argument);
    ASSERT_NE(ref, nullptr) << where << " should be a RefObj";
    EXPECT_EQ(ref->getName(), name);
    EXPECT_EQ(ref->getActual<hldb::Variable>(), getVariable(name))
        << where << " must bind to the module-level Variable '" << name << "'";
  }
};

// --- module scope and its two variable declarations --------------------------

TEST_F(SscanfTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr); }

// Sec 6.7 lists the net_type keywords; neither "string" nor "int" is one of
// them, and "module top();" has an empty port list, so nothing here declares
// a net or drives one continuously.
TEST_F(SscanfTest, ModuleHasNoNetsNoPortsAndNoContinuousAssignments) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'string' and 'int' are not net-type keywords (IEEE 1800-2023 Sec 6.7)";
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top();' has an empty port list";
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "the file contains no 'assign' statement";
}

TEST_F(SscanfTest, ModuleHasExactlyTwoDistinctVariablesStrAndC) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr) << "'string str' and 'int c;' must both be Variables (Sec 6.8)";
  ASSERT_EQ(top->getVariables()->size(), 2u);

  const hldb::Variable *const str = getVariable("str");
  const hldb::Variable *const c = getVariable("c");
  ASSERT_NE(str, nullptr);
  ASSERT_NE(c, nullptr);
  EXPECT_NE(str, c) << "'string str = \"1234\";' and 'int c;' are two separate declarations and must "
                       "be two separate Variable objects";
}

// Sec 6.16 for the type, Sec 6.8 for the declaration assignment and Sec 5.9
// for the literal's width. "str" is the only variable in the file written
// with an "=", so it is the only one whose vpiValue may be populated.
TEST_F(SscanfTest, VariableStrIsAStringInitializedTo1234) {
  const hldb::Variable *const str = getVariable("str");
  ASSERT_NE(str, nullptr);
  ASSERT_NE(str->getTypespec<hldb::RefTypespec>(), nullptr);

  EXPECT_NE(str->getTypespec<hldb::RefTypespec>()->getActual<hldb::StringTypespec>(), nullptr)
      << "'string str' must resolve to a StringTypespec (IEEE 1800-2023 Sec 6.16)";
  EXPECT_EQ(str->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr)
      << "'string str' is not the int declaration -- the two declarations must not be transposed";

  const hldb::Constant *const init = str->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'string str = \"1234\";' carries a declaration-time initializer";
  EXPECT_EQ(init->getConstType(), vpiStringConst)
      << "\"1234\" is written in double quotes, so it is a string literal, not a decimal number";
  EXPECT_EQ(init->getValue(), "1234");
  EXPECT_EQ(init->getSize(), 32) << "Sec 5.9: \"1234\" = 4 chars x 8 bits";
}

// Sec 6.11.1: "int" is a 2-state 32-bit signed integer type. The negative
// check against StringTypespec is the mirror image of the one above.
TEST_F(SscanfTest, VariableCIsSignedIntAndNotAString) {
  const hldb::Variable *const c = getVariable("c");
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getTypespec<hldb::RefTypespec>(), nullptr);

  const hldb::IntTypespec *const ts = c->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr) << "'int c;' must resolve to an IntTypespec";
  EXPECT_TRUE(ts->getSigned()) << "IEEE 1800-2023 Sec 6.11.1: 'int' is a signed type";
  EXPECT_EQ(c->getTypespec<hldb::RefTypespec>()->getActual<hldb::StringTypespec>(), nullptr)
      << "'int c;' is not the string declaration -- the two declarations must not be transposed";
  EXPECT_EQ(c->getValue(), nullptr) << "'int c;' is declared with no '=' initializer";
}

// --- the single procedure ----------------------------------------------------

// Sec 9.2.2: one "initial" keyword, one Initial process -- and nothing else.
TEST_F(SscanfTest, ModuleHasExactlyOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u) << "the file writes exactly one procedure";
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr) << "that procedure is an 'initial'";
  EXPECT_EQ(any_cast<hldb::FinalStmt>(top->getProcesses()->at(0)), nullptr)
      << "no 'final' keyword is written in this file";
}

// Sec 9.3.1: the "begin ... end" holds the two statements written and
// declares nothing. Because $sscanf reads a string rather than a descriptor
// (Sec 21.3.4.1), this file opens no file and assigns nothing -- so neither
// statement may be an Assignment.
TEST_F(SscanfTest, InitialBodyIsBeginWithTwoStatements) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' body should be a Begin";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "no block_item_declaration is written inside the begin-end (Sec 9.3.1); both names resolve "
         "outward to the module scope";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u) << "'$sscanf(str, \"%d\", c);' and the '$display(...)'";

  for (uint32_t i = 0; i < 2u; ++i) {
    EXPECT_EQ(any_cast<hldb::Assignment>(body->getStmts()->at(i)), nullptr)
        << "stmt[" << i
        << "]: $sscanf reads a string, not a file, so this fixture opens nothing and "
           "contains no assignment at all";
  }
}

// --- stmt[0]: $sscanf(str, "%d", c); -----------------------------------------

// Sec 21.3.4.1: "integer code = $sscanf(str, format, args);". The match count
// is discarded here by calling $sscanf in statement position, which does not
// make it a system task.
TEST_F(SscanfTest, FirstStatementIsSscanfSysFuncCall) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  EXPECT_EQ(any_cast<hldb::SysTaskCall>(body->getStmts()->at(0)), nullptr)
      << "IEEE 1800-2023 Sec 21.3.4.1: $sscanf returns the number of items matched, so it is a "
         "system function even when its result is discarded in statement position";
  const hldb::SysFuncCall *const sscanfCall = getSscanfCall();
  ASSERT_NE(sscanfCall, nullptr) << "stmt[0] should be a SysFuncCall";
  EXPECT_EQ(sscanfCall->getName(), "$sscanf")
      << "the name must be '$sscanf', not '$fscanf' -- the two share an argument shape but differ in "
         "what they read from (Sec 21.3.4.1)";
}

// Sec 21.3.4.1 fixes the order as (str, format, args): the SOURCE FIRST, then
// the format string, then the destinations. Sec 5.9 fixes the format
// literal's width at 8 bits per character.
TEST_F(SscanfTest, SscanfArgumentsAreSourceFormatThenDestination) {
  const hldb::SysFuncCall *const sscanfCall = getSscanfCall();
  ASSERT_NE(sscanfCall, nullptr);
  ASSERT_NE(sscanfCall->getArguments(), nullptr);
  ASSERT_EQ(sscanfCall->getArguments()->size(), 3u)
      << "'$sscanf(str, \"%d\", c)' passes the scanned string, the format and one destination";

  // The scanned source is the identifier "str", so it must survive as a
  // reference and must not be folded into a Constant carrying "1234".
  EXPECT_EQ(any_cast<hldb::Constant>(sscanfCall->getArguments()->at(0)), nullptr)
      << "the source wrote the identifier 'str', so arg[0] must be a reference, not a Constant "
         "carrying an inlined copy of its initializer";
  checkArgumentRefersTo(sscanfCall->getArguments()->at(0), "str", "the $sscanf scanned-string argument");

  const hldb::Constant *const format = any_cast<hldb::Constant>(sscanfCall->getArguments()->at(1));
  ASSERT_NE(format, nullptr) << "arg[1] is the format string literal";
  EXPECT_EQ(format->getConstType(), vpiStringConst);
  EXPECT_EQ(format->getValue(), "%d");
  EXPECT_EQ(format->getSize(), 16) << "Sec 5.9: \"%d\" = 2 chars x 8 bits";

  checkArgumentRefersTo(sscanfCall->getArguments()->at(2), "c", "the $sscanf destination argument");
}

// Because "str" and "c" differ in TYPE as well as in name, a swap of the
// source and the destination is detectable here by the typespec each
// argument's actual resolves to, not only by the name -- so this pins the
// Sec 21.3.4.1 order from both directions.
TEST_F(SscanfTest, SscanfSourceAndDestinationAreNotSwapped) {
  const hldb::SysFuncCall *const sscanfCall = getSscanfCall();
  ASSERT_NE(sscanfCall, nullptr);
  ASSERT_NE(sscanfCall->getArguments(), nullptr);
  ASSERT_EQ(sscanfCall->getArguments()->size(), 3u);

  const hldb::RefObj *const first = any_cast<hldb::RefObj>(sscanfCall->getArguments()->at(0));
  const hldb::RefObj *const last = any_cast<hldb::RefObj>(sscanfCall->getArguments()->at(2));
  ASSERT_NE(first, nullptr);
  ASSERT_NE(last, nullptr);

  const hldb::Variable *const scanned = first->getActual<hldb::Variable>();
  const hldb::Variable *const destination = last->getActual<hldb::Variable>();
  ASSERT_NE(scanned, nullptr);
  ASSERT_NE(destination, nullptr);
  EXPECT_NE(scanned, destination) << "the scanned string and the destination are distinct declared variables";

  ASSERT_NE(scanned->getTypespec<hldb::RefTypespec>(), nullptr);
  EXPECT_NE(scanned->getTypespec<hldb::RefTypespec>()->getActual<hldb::StringTypespec>(), nullptr)
      << "Sec 21.3.4.1: $sscanf's first argument is the string being scanned, so it must resolve to "
         "the StringTypespec declaration";
  ASSERT_NE(destination->getTypespec<hldb::RefTypespec>(), nullptr);
  EXPECT_NE(destination->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr)
      << "Sec 21.3.4.1: the trailing argument is the destination the matched item is written into, "
         "so it must resolve to the int declaration";
}

// --- stmt[1]: $display(":assert: (%d == %s)", c, str); -----------------------

// Sec 21.2.1: $display is a system TASK taking a format string followed by
// the values to print, here "c" then "str" to match "%d" then "%s".
TEST_F(SscanfTest, DisplayFormatsCThenStrAgainstTheAssertText) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  EXPECT_EQ(any_cast<hldb::SysFuncCall>(body->getStmts()->at(1)), nullptr)
      << "IEEE 1800-2023 Sec 21.2.1: $display is a system task, so it must not be a SysFuncCall";
  const hldb::SysTaskCall *const display = getDisplayCall();
  ASSERT_NE(display, nullptr) << "stmt[1] should be a SysTaskCall";
  EXPECT_EQ(display->getName(), "$display");

  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 3u) << "the call passes a format string and two values";

  const hldb::Constant *const format = any_cast<hldb::Constant>(display->getArguments()->at(0));
  ASSERT_NE(format, nullptr) << "arg[0] is the format string literal";
  EXPECT_EQ(format->getConstType(), vpiStringConst);
  EXPECT_EQ(format->getValue(), ":assert: (%d == %s)");
  EXPECT_EQ(format->getSize(), 152) << "Sec 5.9: \":assert: (%d == %s)\" = 19 chars x 8 bits";

  checkArgumentRefersTo(display->getArguments()->at(1), "c", "the value printed for '%d'");
  checkArgumentRefersTo(display->getArguments()->at(2), "str", "the value printed for '%s'");
}

// --- compiler diagnostics -----------------------------------------------------

// The source carries no ":should_fail_because:" tag and every construct in it
// is legal per the clauses cited above, so nothing may be reported as an
// error. (The warning count is deliberately not asserted -- see the header.)
TEST_F(SscanfTest, CompilerReportsZeroErrors) {
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
