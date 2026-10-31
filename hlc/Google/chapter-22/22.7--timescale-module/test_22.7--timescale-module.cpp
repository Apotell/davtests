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

// Test for tests/Google/chapter-22/22.7--timescale-module.sv
//
// THE SOURCE
//
// Lines 1-15 are license and metadata comments. The SystemVerilog text is,
// word for word, the IEEE 1800-2023 22.7 example of "a `timescale directive
// in the context of a module":
//
//   16: `timescale 10 ns / 1 ns
//   17: module test;
//   18:   logic set;
//   19:   parameter d = 1.55;
//   20:   initial begin
//   21:     #d set = 0;
//   22:     #d set = 1;
//   23:   end
//   24: endmodule
//
// The run is assumed to pass no -timescale option; it is not part of the
// source.
//
// The repository stores the file with LF line endings; a Windows checkout
// (core.autocrlf) turns them into CR LF. 5.3 lists "newlines" as white space
// but does not name the carriage return, so this test treats CR LF as one
// line ending and asserts nothing about the CR itself.
//
// WHAT THE LRM SAYS ABOUT IT
//
// Line 16 is a legal `timescale (Syntax 22-6). 10 and 1 are valid integers
//   ("the valid integers are 1, 10, and 100"), ns is a valid unit, the
//   precision 1 ns "shall be at least as precise as" the unit 10 ns, and the
//   directive is not "specified within a design element" (22.7).
//
// Module test follows line 16 and declares no timeunit or timeprecision, so
//   "the time unit shall be set to the units of the last `timescale
//   directive", and the precision follows "the same precedence" (3.14.2.3
//   b)): time unit 10 ns, time precision 1 ns. 22.7 says it for this very
//   example: "the time unit for module test is 10 ns". They are the instance
//   properties vpiTimeUnit and vpiTimePrecision (37.10), written as in
//   Table 20-2 (20.4.1): the power of ten in seconds, so 10 ns is -8 and
//   1 ns is -9.
//
// Line 18: set is a variable of the data type logic, a "4-state data type,
//   user-defined vector size, unsigned" (Table 6-8), with no packed
//   dimension.
//
// Line 19: d is a value parameter (6.20.1). test has no parameter port list,
//   so d is not a local parameter (23.10); it has no explicit range, so its
//   range handles are NULL (37.28 detail 5). 1.55 is a real literal (5.7.2),
//   and "If the expression is real, the parameter is real" (6.20.2).
//   "Parameter d retains its value no matter what timescale is in effect"
//   (22.7): the precision rounds the delay made from d to 1.6, while d itself
//   stays 1.55.
//
// Lines 20-23: one initial procedure (9.2.1) whose statement is a begin-end
//   block (9.3.1) of two statements, line 21 first. Each is the delay control
//   #d (9.4.1) followed by the blocking assignment it delays (10.4.1). In the
//   VPI model the delay control's vpiDelay is the expression d and its stmt
//   is the assignment (37.68); the assignment has no delay control of its own
//   (37.64), unlike "set = #d 0;" (9.4.5). The numbers 0 and 1 "shall be
//   treated as signed integers" (5.7.1).
//
// test is never instantiated, so it is a top-level module: "A top-level
//   module is implicitly instantiated once, and its instance name is the same
//   as the module name" (23.3.1).
//
// Some checks read an LRM fact through an HLDB code or class: vpiInitial,
// vpiConstType, vpiRealConst, vpiDecConst and vpiIntConst are VPI codes
// (Annex K); a LogicTypespec means "declared logic"; PaNotimescaleInfo is
// HLC's diagnostic for a design element that has no timescale. Every
// expected value comes from the LRM, so a red test means HLC deviates from
// it.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors                no fatal, syntax or error diagnostic
//   TimescaleDirectiveIsAccepted         no diagnostic rejecting line 16
//   TestIsNotReportedWithoutATimescale   no "no timescale" diagnostic for test
//   SourceFileHasNoIncludesOrMacros      one SourceFile, no includes, no macros
//   ModuleTestIsDeclaredOnLine17         one module, test; one top instance
//   ModuleTestItems                      no ports; one variable, parameter,
//                                        parameter assignment and process
//   TimeUnitOfTestIs10Nanoseconds        vpiTimeUnit is -8
//   TimePrecisionOfTestIs1Nanosecond     vpiTimePrecision is -9
//   SetIsAnUnsignedLogicVariable         line 18, logic, unsigned, no range
//   ParameterDIsNotLocalAndHasNoRange    line 19, not local, no range
//   ParameterDIsReal                     1.55 is a vpiRealConst, d is real
//   ParameterDKeepsTheValue1_55          1.55, also after elaboration
//   InitialProcedureHoldsABeginEndBlock  initial on line 20, two statements
//   Line21IsDelayedByD                   #d on line 21 delays an assignment
//   Line21AssignsZeroToSet               blocking set = 0, no delay of its own
//   Line22IsDelayedByD                   #d on line 22 delays an assignment
//   Line22AssignsOneToSet                blocking set = 1, no delay of its own
//   AssignedNumbersAreSignedIntegers     0 and 1 are signed (5.7.1)
//   NothingElseIsDeclared                no other design element or typedef
//
// Only facts that exist only after elaboration are gated on
// Design::getElaborated(): the top-level instance (23.3.1), its time unit and
// precision (37.10), and the type and final value of d (6.20.2, 37.28
// detail 1). Each gated block sits in a test whose ungated part checks the
// same fact on what parsing already establishes, so no test is empty on a run
// that does not elaborate. These instance facts have no pre-elaboration
// value, so the gates have no else branch.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The rounding of d from 1.55 to 1.6, its scaling to a 16 ns delay, and
//     the assignments at 16 ns and 32 ns (22.7 a) to c)). Delays are rounded
//     "before being used in simulation", and the schedule exists only while
//     simulation time advances, so this is permanently out of scope. Nearest
//     assertions: TimeUnitOfTestIs10Nanoseconds and
//     TimePrecisionOfTestIs1Nanosecond (the inputs of the rounding),
//     Line21IsDelayedByD and Line22IsDelayedByD (the delay is d), and
//     ParameterDKeepsTheValue1_55 (the value the delay is made from).
//   - The simulation time unit (3.14.3; 37.10 detail 7, vpi_get() with a
//     NULL handle). It belongs to the whole design rather than to test, and
//     the HLDB headers give it no field. Nearest assertion:
//     TimePrecisionOfTestIs1Nanosecond.
//   - A directive object for line 16 in any directive list (SourceFile,
//     Design or Instance). 22.7 requires the directive to take effect, not to
//     be recorded, and directives are not VPI objects, so neither the
//     contents nor the size of such a list is asserted. Nearest assertions:
//     TimeUnitOfTestIs10Nanoseconds and TimePrecisionOfTestIs1Nanosecond.
//   - The time unit and precision of the SourceFile or compilation unit.
//     "The time unit of the compilation-unit scope can only be set by a
//     timeunit declaration, not a `timescale directive", and its default is
//     implementation-specific (3.14.2.3). Nearest assertion:
//     TimeUnitOfTestIs10Nanoseconds.
//   - What the expressions d (lines 21 and 22) and set bind to. The binding
//     is a static fact (d is the parameter of line 19, set the variable of
//     line 18), but how HLDB models a reference, and the accessor to follow
//     it, could not be confirmed against the HLDB headers for this test.
//     Nearest assertions: the names checked in Line21IsDelayedByD,
//     Line21AssignsZeroToSet, Line22IsDelayedByD and Line22AssignsOneToSet.
//   - How HLDB represents the implied type of d (6.20.2) as a typespec. The
//     source writes no type, so whether a typespec node exists is a tool
//     convention. Nearest assertion: ParameterDIsReal (vpiConstType).
//   - Whether the Design-level parameter, parameter assignment and variable
//     lists hold only compilation-unit items or every item. The headers do
//     not say, so d and set are looked up in test, where they are declared.
//     Nearest assertions: ModuleTestItems and the tests for set and d.
//   - DelayControl::getVpiDelay(), a string whose content the headers do not
//     document. Nearest assertions: Line21IsDelayedByD and
//     Line22IsDelayedByD.
//   - The end line of module test. The LRM location property is vpiLineNo
//     (37.3.3), a start line; vpiEndLine exists only for assertions (37.49).
//     Nearest assertion: ModuleTestIsDeclaredOnLine17.
//   - The comments on lines 1-15 (5.4) and columns. The LRM gives comments no
//     meaning, how HLDB keeps them is a tool convention, and the LRM numbers
//     lines (22.13) but not columns. Nearest assertions: the line numbers
//     checked throughout, which hold only if lines 1-15 were read as whole
//     lines.
//   - Warnings, and informational diagnostics other than the two "no
//     timescale" ones. The LRM neither requires nor forbids them. Nearest
//     assertions: CompilesWithoutErrors and
//     TestIsNotReportedWithoutATimescale.

#include <hlc/Common/FileSystem.h>
#include <hlc/Common/PathId.h>
#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/checker_decl.h>
#include <hldb/class_defn.h>
#include <hldb/config_decl.h>
#include <hldb/constant.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/expr.h>
#include <hldb/initial.h>
#include <hldb/interface.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/process_stmt.h>
#include <hldb/program.h>
#include <hldb/ref_typespec.h>
#include <hldb/source_file.h>
#include <hldb/typedef.h>
#include <hldb/udp_defn.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "22.7--timescale-module.sv";

// Table 20-2 (20.4.1) writes a time unit or precision as its power of ten in
// seconds.
constexpr int32_t kTenNanoseconds = -8;
constexpr int32_t kOneNanosecond = -9;

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

// Size of an HLDB collection; a collection that was never created is empty.
template <typename T>
size_t sizeOf(const std::vector<T *> *collection) {
  return (collection == nullptr) ? 0 : collection->size();
}

// The objects of 'collection' whose source location is the file under test.
template <typename T>
std::vector<const T *> fromSourceFile(const std::vector<T *> *collection) {
  std::vector<const T *> objects;
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (endsWith(object->getFile(), kSourceFileName)) objects.emplace_back(object);
    }
  }
  return objects;
}

// The object of 'collection' named 'name', or nullptr unless there is exactly one.
template <typename T>
const T *findByName(const std::vector<T *> *collection, std::string_view name) {
  const T *found = nullptr;
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (object->getName() != name) continue;
      if (found != nullptr) return nullptr;
      found = object;
    }
  }
  return found;
}

// The object of 'collection' that starts on 'line', or nullptr unless there is exactly one.
template <typename T>
const T *findOnLine(const std::vector<T *> *collection, uint32_t line) {
  const T *found = nullptr;
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (object->getStartLine() != line) continue;
      if (found != nullptr) return nullptr;
      found = object;
    }
  }
  return found;
}

// The literal of 'constant' as written: its vpiDecompile text (37.59), or,
// when that is empty, its value text without the "<format>:" prefix.
std::string_view literalOf(const hldb::Constant *constant) {
  const std::string_view decompile = constant->getDecompile();
  if (!decompile.empty()) return decompile;
  const std::string_view value = constant->getValue();
  const size_t colon = value.find(':');
  return (colon == std::string_view::npos) ? value : value.substr(colon + 1);
}

// The number the real literal 'constant' stands for (5.7.2), or NaN unless
// its text is exactly one number. Comparing numbers, not text, also accepts
// 1.55 written with more digits.
double realValueOf(const hldb::Constant *constant) {
  const std::string text(literalOf(constant));
  char *end = nullptr;
  const double value = std::strtod(text.c_str(), &end);
  if (text.empty() || (end != text.c_str() + text.size())) return std::numeric_limits<double>::quiet_NaN();
  return value;
}

// The line a diagnostic points at, or 0 when it carries no location.
uint32_t lineOf(const Event &error) { return error.getArguments().empty() ? 0 : error.getArguments().front().m_line; }

// Every diagnostic of 'severity' as " [<code> line <n>: <message template>]",
// so that a red run shows what was reported and where.
std::string describeErrors(const EventListener *eventListener, EventSeverity severity) {
  std::string description;
  for (const Event &error : eventListener->getEvents()) {
    const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
    if ((errorInfo == nullptr) || (errorInfo->m_severity != severity)) continue;
    description += " [" + std::to_string(static_cast<int32_t>(error.getType()));
    description += " line " + std::to_string(lineOf(error));
    description += ": " + std::string(errorInfo->m_lines[0].m_text) + "]";
  }
  return description;
}

// Number of diagnostics of 'type' located in the file under test. Diagnostics
// about other files, such as the tool's built-in sources, are not counted.
size_t countInSourceFile(const EventListener *eventListener, uint32_t type) {
  size_t count = 0;
  for (const Event &error : eventListener->getEvents()) {
    if (error.getType() != type) continue;
    for (const EventArg &location : error.getArguments()) {
      if (endsWith(location.m_file, kSourceFileName)) {
        ++count;
        break;
      }
    }
  }
  return count;
}
}  // namespace

class TimescaleModuleTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.7--timescale-module.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // The SourceFile nodes of the file under test.
  static std::vector<const hldb::SourceFile *> findSourceFiles() {
    std::vector<const hldb::SourceFile *> sourceFiles;
    if (m_design != nullptr) {
      if (const hldb::SourceFileCollection *const collection = m_design->getSourceFiles()) {
        for (const hldb::SourceFile *sourceFile : *collection) {
          if (endsWith(sourceFile->getName(), kSourceFileName)) sourceFiles.emplace_back(sourceFile);
        }
      }
    }
    return sourceFiles;
  }

  // The definition of module test, or nullptr unless the file defines exactly one module.
  static const hldb::Module *getModuleTest() {
    if (m_design == nullptr) return nullptr;
    const std::vector<const hldb::Module *> modules = fromSourceFile(m_design->getAllModules());
    return (modules.size() == 1) ? modules.front() : nullptr;
  }

  // The top-level instance of test (23.3.1), or nullptr unless there is
  // exactly one. It exists only after elaboration.
  static const hldb::Module *getTopInstance() {
    if (m_design == nullptr) return nullptr;
    const std::vector<const hldb::Module *> topInstances = fromSourceFile(m_design->getTopModules());
    return (topInstances.size() == 1) ? topInstances.front() : nullptr;
  }

  // The parameter assignment in 'scope' whose left-hand side is 'name', or
  // nullptr unless there is exactly one.
  static const hldb::ParamAssign *findParamAssign(const hldb::Module *scope, std::string_view name) {
    const hldb::ParamAssign *found = nullptr;
    if (const hldb::ParamAssignCollection *const paramAssigns = scope->getParamAssigns()) {
      for (const hldb::ParamAssign *paramAssign : *paramAssigns) {
        const hldb::Any *const lhs = paramAssign->getLhs();
        if ((lhs == nullptr) || (lhs->getName() != name)) continue;
        if (found != nullptr) return nullptr;
        found = paramAssign;
      }
    }
    return found;
  }

  // The process of test that starts on line 20, or nullptr.
  static const hldb::Process *getInitialProcedure() {
    const hldb::Module *const test = getModuleTest();
    return (test == nullptr) ? nullptr : findOnLine(test->getProcesses(), 20);
  }

  // The begin-end block of lines 20-23, or nullptr.
  static const hldb::Begin *getBlock() {
    const hldb::Process *const process = getInitialProcedure();
    return (process == nullptr) ? nullptr : ::any_cast<hldb::Begin>(process->getStmt());
  }

  // Statement 'index' of the begin-end block as a delay control, or nullptr.
  static const hldb::DelayControl *getDelayControl(size_t index) {
    const hldb::Begin *const block = getBlock();
    if ((block == nullptr) || (sizeOf(block->getStmts()) <= index)) return nullptr;
    return ::any_cast<hldb::DelayControl>(block->getStmts()->at(index));
  }

  // The statement that delay control 'index' delays, as an assignment, or nullptr.
  static const hldb::Assignment *getDelayedAssignment(size_t index) {
    const hldb::DelayControl *const delayControl = getDelayControl(index);
    return (delayControl == nullptr) ? nullptr : ::any_cast<hldb::Assignment>(delayControl->getStmt());
  }

  // The value assignment 'index' assigns, as a constant, or nullptr.
  static const hldb::Constant *getAssignedConstant(size_t index) {
    const hldb::Assignment *const assignment = getDelayedAssignment(index);
    return (assignment == nullptr) ? nullptr : ::any_cast<hldb::Constant>(assignment->getRhs());
  }
};

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Every line of the file is legal, so nothing is fatal, a syntax error or an
// error.
TEST_F(TimescaleModuleTest, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  const EventListener::Stats stats = eventListener->getStats();
  EXPECT_EQ(stats.m_fatals, 0) << describeErrors(eventListener, EventSeverity::Fatal);
  EXPECT_EQ(stats.m_syntaxes, 0) << describeErrors(eventListener, EventSeverity::Syntax);
  EXPECT_EQ(stats.m_errors, 0) << describeErrors(eventListener, EventSeverity::Error);
}

// 22.7 and 3.14.2.3 b): line 16 gives test a time unit and precision, so no
// diagnostic may say test has none. test is the only design element, so the
// 3.14.2.3 error for a design that mixes elements with and without a time
// unit cannot apply either.
TEST_F(TimescaleModuleTest, TestIsNotReportedWithoutATimescale) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  EXPECT_EQ(countInSourceFile(eventListener, EventType::PaNotimescaleInfo), 0u)
      << describeErrors(eventListener, EventSeverity::Info);
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once. It has no `include (22.4) and no `define or macro
// usage (22.5.1): `timescale is a compiler directive, not a macro.
TEST_F(TimescaleModuleTest, SourceFileHasNoIncludesOrMacros) {
  const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
  ASSERT_EQ(sourceFiles.size(), 1u);
  const hldb::SourceFile *const sourceFile = sourceFiles.front();

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// ---------------------------------------------------------------------------
// Module test (lines 17-24)
// ---------------------------------------------------------------------------

// Lines 17-24 define one module, test. It is never instantiated, so it is a
// top-level module, implicitly instantiated once under its own name
// (23.3.1); that instance exists only after elaboration.
TEST_F(TimescaleModuleTest, ModuleTestIsDeclaredOnLine17) {
  ASSERT_NE(m_design, nullptr);
  const std::vector<const hldb::Module *> modules = fromSourceFile(m_design->getAllModules());
  ASSERT_EQ(modules.size(), 1u);
  EXPECT_EQ(modules.front()->getDefName(), "test");
  EXPECT_EQ(modules.front()->getStartLine(), 17u);

  if (m_design->getElaborated()) {
    const std::vector<const hldb::Module *> topInstances = fromSourceFile(m_design->getTopModules());
    ASSERT_EQ(topInstances.size(), 1u);
    EXPECT_EQ(topInstances.front()->getName(), "test");
    EXPECT_EQ(topInstances.front()->getDefName(), "test");
  }
}

// "module test;" has no list of ports (23.2.1). Its items are one variable
// (line 18), one parameter with its one parameter assignment (line 19) and
// one initial procedure (lines 20-23), and nothing else.
TEST_F(TimescaleModuleTest, ModuleTestItems) {
  const hldb::Module *const test = getModuleTest();
  ASSERT_NE(test, nullptr);

  EXPECT_EQ(sizeOf(test->getPorts()), 0u);
  EXPECT_EQ(sizeOf(test->getVariables()), 1u) << "logic set;";
  EXPECT_EQ(sizeOf(test->getParameters()), 1u) << "parameter d = 1.55;";
  EXPECT_EQ(sizeOf(test->getParamAssigns()), 1u) << "d = 1.55";
  EXPECT_EQ(sizeOf(test->getProcesses()), 1u) << "initial begin ... end";

  EXPECT_EQ(sizeOf(test->getNets()), 0u);
  EXPECT_EQ(sizeOf(test->getContAssigns()), 0u);
  EXPECT_EQ(sizeOf(test->getPrimitives()), 0u);
  EXPECT_EQ(sizeOf(test->getModules()), 0u);
}

// ---------------------------------------------------------------------------
// The effect of `timescale 10 ns / 1 ns on module test
// ---------------------------------------------------------------------------

// 3.14.2.3 b): test declares no timeunit, so its time unit is the unit of the
// last `timescale, 10 ns, which is -8 in Table 20-2. 22.7: "the time unit for
// module test is 10 ns". vpiTimeUnit is an instance property (37.10), so it
// is also checked on the top-level instance after elaboration.
TEST_F(TimescaleModuleTest, TimeUnitOfTestIs10Nanoseconds) {
  const hldb::Module *const test = getModuleTest();
  ASSERT_NE(test, nullptr);
  EXPECT_EQ(test->getTimeUnit(), kTenNanoseconds);

  if (m_design->getElaborated()) {
    const hldb::Module *const topInstance = getTopInstance();
    ASSERT_NE(topInstance, nullptr);
    EXPECT_EQ(topInstance->getTimeUnit(), kTenNanoseconds);
  }
}

// 3.14.2.3: test declares no timeprecision, so its precision follows the same
// precedence as its unit: the precision of the last `timescale, 1 ns, which
// is -9 in Table 20-2. vpiTimePrecision is an instance property (37.10), so
// it is also checked on the top-level instance after elaboration.
TEST_F(TimescaleModuleTest, TimePrecisionOfTestIs1Nanosecond) {
  const hldb::Module *const test = getModuleTest();
  ASSERT_NE(test, nullptr);
  EXPECT_EQ(test->getTimePrecision(), kOneNanosecond);

  if (m_design->getElaborated()) {
    const hldb::Module *const topInstance = getTopInstance();
    ASSERT_NE(topInstance, nullptr);
    EXPECT_EQ(topInstance->getTimePrecision(), kOneNanosecond);
  }
}

// ---------------------------------------------------------------------------
// Line 18: logic set;
// ---------------------------------------------------------------------------

// set is a variable of the data type logic, which is unsigned (Table 6-8).
// No packed dimension is written, so its type has no range.
TEST_F(TimescaleModuleTest, SetIsAnUnsignedLogicVariable) {
  const hldb::Module *const test = getModuleTest();
  ASSERT_NE(test, nullptr);
  const hldb::Variable *const set = findByName(test->getVariables(), "set");
  ASSERT_NE(set, nullptr) << "set is a variable of test";
  EXPECT_EQ(set->getStartLine(), 18u);

  const hldb::RefTypespec *const typespec = set->getTypespec();
  ASSERT_NE(typespec, nullptr) << "set is declared with the data type logic";
  const hldb::LogicTypespec *const logic = ::any_cast<hldb::LogicTypespec>(typespec->getActual());
  ASSERT_NE(logic, nullptr) << "set is declared logic";
  EXPECT_FALSE(logic->getSigned()) << "logic is unsigned";
  EXPECT_EQ(sizeOf(logic->getRanges()), 0u) << "no packed dimension";
}

// ---------------------------------------------------------------------------
// Line 19: parameter d = 1.55;
// ---------------------------------------------------------------------------

// d is a value parameter (6.20.1). test has no parameter port list, so d is
// not a local parameter (23.10). No range is written, so it has none (37.28
// detail 5).
TEST_F(TimescaleModuleTest, ParameterDIsNotLocalAndHasNoRange) {
  const hldb::Module *const test = getModuleTest();
  ASSERT_NE(test, nullptr);
  const hldb::Parameter *const d = ::any_cast<hldb::Parameter>(findByName(test->getParameters(), "d"));
  ASSERT_NE(d, nullptr) << "d is a value parameter of test";
  EXPECT_EQ(d->getStartLine(), 19u);
  EXPECT_FALSE(d->getLocalParam());
  EXPECT_EQ(sizeOf(d->getRanges()), 0u);
}

// 1.55 is a real literal (5.7.2), a vpiRealConst. d has no type of its own,
// so "If the expression is real, the parameter is real" (6.20.2): its
// vpiConstType (37.28) is vpiRealConst. That rule types d by its final value
// "after any value overrides have been applied", so the type of d is checked
// after elaboration only.
TEST_F(TimescaleModuleTest, ParameterDIsReal) {
  const hldb::Module *const test = getModuleTest();
  ASSERT_NE(test, nullptr);
  const hldb::ParamAssign *const paramAssign = findParamAssign(test, "d");
  ASSERT_NE(paramAssign, nullptr) << "d = 1.55 is a parameter assignment of test";
  const hldb::Constant *const value = ::any_cast<hldb::Constant>(paramAssign->getRhs());
  ASSERT_NE(value, nullptr) << "d is assigned a literal";
  EXPECT_EQ(value->getConstType(), vpiRealConst);

  if (m_design->getElaborated()) {
    const hldb::Module *const topInstance = getTopInstance();
    ASSERT_NE(topInstance, nullptr);
    const hldb::Any *const d = findByName(topInstance->getParameters(), "d");
    ASSERT_NE(d, nullptr) << "d is a parameter of the top-level instance";
    const hldb::Any::vpi_property_value_t constType = d->getVpiPropertyValue(vpiConstType);
    ASSERT_TRUE(std::holds_alternative<int64_t>(constType));
    EXPECT_EQ(std::get<int64_t>(constType), vpiRealConst);
  }
}

// d is assigned 1.55, and "Parameter d retains its value no matter what
// timescale is in effect" (22.7): only the delay made from d is rounded to
// 1.6. So after elaboration, where a parameter has its final value (37.28
// detail 1), d is still 1.55.
TEST_F(TimescaleModuleTest, ParameterDKeepsTheValue1_55) {
  const hldb::Module *const test = getModuleTest();
  ASSERT_NE(test, nullptr);
  const hldb::ParamAssign *const paramAssign = findParamAssign(test, "d");
  ASSERT_NE(paramAssign, nullptr) << "d = 1.55 is a parameter assignment of test";
  EXPECT_EQ(paramAssign->getStartLine(), 19u);
  const hldb::Constant *const value = ::any_cast<hldb::Constant>(paramAssign->getRhs());
  ASSERT_NE(value, nullptr) << "d is assigned a literal";
  EXPECT_DOUBLE_EQ(realValueOf(value), 1.55) << "literal: " << literalOf(value);

  if (m_design->getElaborated()) {
    const hldb::Module *const topInstance = getTopInstance();
    ASSERT_NE(topInstance, nullptr);
    const hldb::ParamAssign *const finalAssign = findParamAssign(topInstance, "d");
    ASSERT_NE(finalAssign, nullptr) << "d is a parameter of the top-level instance";
    const hldb::Constant *const finalValue = ::any_cast<hldb::Constant>(finalAssign->getRhs());
    ASSERT_NE(finalValue, nullptr) << "the final value of d is a constant";
    EXPECT_DOUBLE_EQ(realValueOf(finalValue), 1.55) << "literal: " << literalOf(finalValue);
  }
}

// ---------------------------------------------------------------------------
// Lines 20-23: initial begin ... end
// ---------------------------------------------------------------------------

// One initial procedure (9.2.1) starts on line 20. Its statement is the
// begin-end block (9.3.1) of the two statements on lines 21 and 22.
TEST_F(TimescaleModuleTest, InitialProcedureHoldsABeginEndBlock) {
  const hldb::Process *const process = getInitialProcedure();
  ASSERT_NE(process, nullptr) << "a process starts on line 20";
  EXPECT_EQ(process->getVpiType(), static_cast<uint32_t>(vpiInitial));

  const hldb::Begin *const block = getBlock();
  ASSERT_NE(block, nullptr) << "the statement of the initial procedure is begin ... end";
  EXPECT_EQ(block->getStartLine(), 20u);
  EXPECT_EQ(sizeOf(block->getStmts()), 2u);
}

// Line 21: "#d set = 0;" starts with the delay control #d (9.4.1). Its
// vpiDelay is the expression d, not the number d stands for (37.68). Its
// stmt is the assignment it delays: 37.68 detail 1 leaves stmt NULL only for
// a delay control that is part of an assignment.
TEST_F(TimescaleModuleTest, Line21IsDelayedByD) {
  const hldb::DelayControl *const delayControl = getDelayControl(0);
  ASSERT_NE(delayControl, nullptr) << "the first statement of the block is a delay control";
  EXPECT_EQ(delayControl->getStartLine(), 21u);

  const hldb::Expr *const delay = delayControl->getDelay();
  EXPECT_NE(delay, nullptr) << "#d has a delay expression";
  if (delay != nullptr) EXPECT_EQ(delay->getName(), "d");

  EXPECT_NE(::any_cast<hldb::Assignment>(delayControl->getStmt()), nullptr) << "#d delays the assignment set = 0";
}

// Line 21: the statement #d delays is "set = 0;", a blocking procedural
// assignment (10.4.1) of the literal 0 to set. The #d before it is not part
// of the assignment, so the assignment has no delay control of its own
// (37.64), as "set = #d 0;" would (9.4.5).
TEST_F(TimescaleModuleTest, Line21AssignsZeroToSet) {
  const hldb::Assignment *const assignment = getDelayedAssignment(0);
  ASSERT_NE(assignment, nullptr) << "#d on line 21 delays an assignment";
  EXPECT_EQ(assignment->getStartLine(), 21u);
  EXPECT_TRUE(assignment->getBlocking());
  EXPECT_EQ(assignment->getDelayControl(), nullptr) << "no intra-assignment delay";

  const hldb::Expr *const lhs = assignment->getLhs();
  EXPECT_NE(lhs, nullptr) << "set = 0 has a left-hand side";
  if (lhs != nullptr) EXPECT_EQ(lhs->getName(), "set");

  const hldb::Constant *const rhs = getAssignedConstant(0);
  EXPECT_NE(rhs, nullptr) << "set = 0 assigns a literal";
  if (rhs != nullptr) EXPECT_EQ(literalOf(rhs), "0");
}

// Line 22: "#d set = 1;" starts with the delay control #d (9.4.1). Its
// vpiDelay is the expression d (37.68), and its stmt is the assignment it
// delays (37.68 detail 1).
TEST_F(TimescaleModuleTest, Line22IsDelayedByD) {
  const hldb::DelayControl *const delayControl = getDelayControl(1);
  ASSERT_NE(delayControl, nullptr) << "the second statement of the block is a delay control";
  EXPECT_EQ(delayControl->getStartLine(), 22u);

  const hldb::Expr *const delay = delayControl->getDelay();
  EXPECT_NE(delay, nullptr) << "#d has a delay expression";
  if (delay != nullptr) EXPECT_EQ(delay->getName(), "d");

  EXPECT_NE(::any_cast<hldb::Assignment>(delayControl->getStmt()), nullptr) << "#d delays the assignment set = 1";
}

// Line 22: the statement #d delays is "set = 1;", a blocking procedural
// assignment (10.4.1) of the literal 1 to set, with no delay control of its
// own (37.64).
TEST_F(TimescaleModuleTest, Line22AssignsOneToSet) {
  const hldb::Assignment *const assignment = getDelayedAssignment(1);
  ASSERT_NE(assignment, nullptr) << "#d on line 22 delays an assignment";
  EXPECT_EQ(assignment->getStartLine(), 22u);
  EXPECT_TRUE(assignment->getBlocking());
  EXPECT_EQ(assignment->getDelayControl(), nullptr) << "no intra-assignment delay";

  const hldb::Expr *const lhs = assignment->getLhs();
  EXPECT_NE(lhs, nullptr) << "set = 1 has a left-hand side";
  if (lhs != nullptr) EXPECT_EQ(lhs->getName(), "set");

  const hldb::Constant *const rhs = getAssignedConstant(1);
  EXPECT_NE(rhs, nullptr) << "set = 1 assigns a literal";
  if (rhs != nullptr) EXPECT_EQ(literalOf(rhs), "1");
}

// 5.7.1: "Simple decimal numbers without the size and the base format shall
// be treated as signed integers." The signed VPI types of a decimal integer
// constant are vpiDecConst and vpiIntConst; HLDB's vpiUIntConst (marked "NOT
// Standard" in its headers) would make 0 and 1 unsigned.
TEST_F(TimescaleModuleTest, AssignedNumbersAreSignedIntegers) {
  const hldb::Constant *const zero = getAssignedConstant(0);
  EXPECT_NE(zero, nullptr) << "line 21 assigns the literal 0";
  if (zero != nullptr) {
    const int32_t constType = zero->getConstType();
    EXPECT_TRUE((constType == vpiDecConst) || (constType == vpiIntConst)) << "0 has vpiConstType " << constType;
  }

  const hldb::Constant *const one = getAssignedConstant(1);
  EXPECT_NE(one, nullptr) << "line 22 assigns the literal 1";
  if (one != nullptr) {
    const int32_t constType = one->getConstType();
    EXPECT_TRUE((constType == vpiDecConst) || (constType == vpiIntConst)) << "1 has vpiConstType " << constType;
  }
}

// ---------------------------------------------------------------------------
// Nothing else
// ---------------------------------------------------------------------------

// The file defines module test and nothing else: no interface, program,
// package, class, UDP, configuration, checker or typedef.
TEST_F(TimescaleModuleTest, NothingElseIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(fromSourceFile(m_design->getAllInterfaces()).size(), 0u);
  EXPECT_EQ(fromSourceFile(m_design->getAllPrograms()).size(), 0u);
  EXPECT_EQ(fromSourceFile(m_design->getAllPackages()).size(), 0u);
  EXPECT_EQ(fromSourceFile(m_design->getAllClasses()).size(), 0u);
  EXPECT_EQ(fromSourceFile(m_design->getAllUdps()).size(), 0u);
  EXPECT_EQ(fromSourceFile(m_design->getConfigs()).size(), 0u);
  EXPECT_EQ(fromSourceFile(m_design->getCheckerDecls()).size(), 0u);
  EXPECT_EQ(fromSourceFile(m_design->getTypedefs()).size(), 0u);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
