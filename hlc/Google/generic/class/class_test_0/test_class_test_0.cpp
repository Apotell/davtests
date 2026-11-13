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

// Test for tests/Google/generic/class/class_test_0.sv
//
// THE SOURCE
//
// Lines 1-7 are one-line comments with the license, lines 8-9 are blank and
// lines 10-14 are one block comment carrying the test metadata (tags 6.15 and
// 8.3). The SystemVerilog text is:
//
//   15: class semicolon_classy; ; ;;; ; ; ;endclass
//   16:
//   17: module test;
//   18: endmodule
//
// The repository stores the file with LF line endings; a Windows checkout
// (core.autocrlf) turns them into CR LF. 5.3 lists "newlines" as white space
// but does not name the carriage return, so this test treats CR LF as one
// line ending and asserts nothing about the CR itself.
//
// WHAT THE LRM SAYS ABOUT IT
//
// Line 15 is one class_declaration (8.3, A.1.2):
//
//     class_declaration ::= [ virtual ] class [ final_specifier ] class_identifier
//         [ parameter_port_list ] [ extends class_type ... ] [ implements ... ] ;
//         { class_item } endclass [ : class_identifier ]
//     class_item ::= ... | local_parameter_declaration ; | parameter_declaration ; | ;
//
//   - The class_identifier is semicolon_classy, a simple_identifier (A.9.3)
//     that is not a keyword (Table B.1). There is no virtual, no
//     final_specifier, no parameter_port_list, no extends, no implements and
//     no label after endclass.
//   - The semicolon after semicolon_classy ends the class header. The seven
//     semicolons that follow it (";", ";;;", ";", ";", ";") are seven class
//     items of the last form, ";": empty items. The white space between them
//     only separates tokens (5.3), and endclass may follow the last ";"
//     directly.
//   - "A class's data are referred to as class properties, and its
//     subroutines are called methods; both are members of the class" (8.2).
//     Empty class items declare nothing, so the class has no property, no
//     method, no constraint, no parameter, no typedef, no event and no nested
//     class.
//
// The class lies outside any module, so it belongs to the compilation-unit
//   scope, which "contains all declarations that lie outside any other scope"
//   (3.12.1); it is not a member of module test. "The items in a
//   compilation-unit scope can be accessed using the PLI" (3.12.1).
//
// Lines 17-18 are one module_declaration (A.1.2) with the module_identifier
//   test, not a keyword (Table B.1), and no ports, parameters or items. test is
//   never instantiated, so it is a top-level module: "A design shall contain
//   at least one top-level module. A top-level module is implicitly
//   instantiated once, and its instance name is the same as the module name"
//   (23.3.1). With test, the design meets that rule, so even an elaborating
//   run has nothing to report as an error.
//
// No line holds a compiler directive, so lines are numbered from 1 at the
//   start of the file (22.12): the class is on line 15 and the module spans
//   lines 17-18.
//
// HOW THE TEST OBSERVES IT
//
// The LRM's object model for a class definition (37.31) gives it the name
// vpiName, the property vpiVirtual, and the relations vpiExtends,
// vpiDerivedClasses, vpiParameter, vpiVariables, vpiMethods, vpiConstraint,
// vpiTypedef, vpiNamedEvent, vpiNamedEventArray and vpiInternalScope; the
// object model for an instance (37.10) gives a module instance vpiName,
// vpiDefName, vpiDefFile, vpiDefLineNo and a class defn relation. HLDB keeps
// each of these as a typed member of the object, so the test reads them
// through HLDB's typed getters: getName(), getVirtual(), getExtends(),
// getDerivedClasses(), getParameters(), getVariables(), getMethods(),
// getConstraints(), getTypedefs(), getNamedEvents(),
// getNamedEventArrays() and getInternalScopes() on the class defn, and
// getDefName(), getDefFile(), getDefLineNo(), getNets() and getClassDefns()
// on the module. The location checks use getFile() and getStartLine(),
// HLDB's vpiFile and vpiLineNo, and getEndLine().
//
// Every expected value comes from the source text and the LRM, so a red test
// means HLC deviates from the LRM.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors                     no fatal, syntax or error
//                                             diagnostic of any category
//   Line15ClassIsAccepted                     no such diagnostic on line 15
//   Lines17To18ModuleIsAccepted               none on lines 17-18
//   SourceFileHasNoIncludesOrMacros           one SourceFile, no includes,
//                                             no macro definitions or usages
//   ClassSemicolonClassyIsDeclared            exactly one class from this
//                                             file, a vpiClassDefn named
//                                             semicolon_classy
//   ClassIsOnLine15                           it lies on line 15 only
//   ClassIsNotVirtualAndExtendsNothing        vpiVirtual is false; no
//                                             extends, derived class or
//                                             parameter
//   ClassHasNoMembers                         no variable, method, constraint,
//                                             typedef, named event (array) or
//                                             internal scope
//   ModuleTestIsDeclared                      exactly one module from this
//                                             file, named test
//   ModuleTestIsOnLines17To18                 it starts on line 17 and ends on
//                                             line 18
//   ModuleTestIsEmpty                         no variable, net, parameter,
//                                             typedef or class defn in it
//   NoOtherDesignObjectsDeclared              nothing else in the design
//                                             comes from this file
//   TestIsTheOnlyTopLevelInstance             elaborated runs only: one
//                                             top-level instance, test, of the
//                                             definition test at line 17, with
//                                             no class defn in it
//
// Exactly one check is gated on Design::getElaborated():
// TestIsTheOnlyTopLevelInstance. The top-level instance is created by
// elaboration (23.3.1), so it genuinely exists only after it. It has no else
// branch: before elaboration there is no instance, and the LRM defines no
// value to assert for one. Everything the parser establishes (the class, its
// members, the module definition, the names and the lines) is asserted
// unconditionally.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The implicit constructor. A class without an explicit new gets one, but
//     "the iteration over vpiMethods shall not include built-in methods for
//     which there is no explicit declaration" (37.31), so it is not looked
//     for. Nearest assertion: ClassHasNoMembers (zero vpiMethods).
//   - vpiFullName of the class ("$unit::" first, 37.10) and of the instance.
//     The full name is a computed property that this suite does not assert.
//     Nearest assertions: ClassSemicolonClassyIsDeclared and
//     TestIsTheOnlyTopLevelInstance (vpiName and vpiDefName).
//   - The compilation-unit object that holds the class (vpiUnit, 37.10) and
//     the Design's typedef list. How HLDB groups $unit items is a tool
//     convention, and a class declaration declares no typedef. Nearest
//     assertions: ClassSemicolonClassyIsDeclared and ModuleTestIsEmpty (the
//     class is not inside test).
//   - Objects of the class and their handles (6.15). The file creates no
//     object, and objects only exist while a simulation runs; this is
//     permanently out of scope. Nearest assertion: ClassHasNoMembers.
//   - The location of the instance itself (vpiFile, vpiLineNo). A top-level
//     instance has no instantiation statement, so the LRM fixes only its
//     definition location. Nearest assertion: TestIsTheOnlyTopLevelInstance
//     (vpiDefFile and vpiDefLineNo).
//   - Warnings and informational diagnostics. The LRM neither requires nor
//     forbids them for this file. Nearest assertions: CompilesWithoutErrors
//     and the three ReportsNo... tests.
//   - The comments on lines 1-14 (5.4) and columns. The LRM gives comments no
//     meaning. "The compiler shall maintain the current line number" (22.12);
//     the LRM keeps no column for a comment. Nearest
//     assertions: ClassIsOnLine15 and ModuleTestIsOnLines17To18, which hold
//     only if lines 1-14 were read as whole lines.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/checker_decl.h>
#include <hldb/class_defn.h>
#include <hldb/config_decl.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/program.h>
#include <hldb/source_file.h>
#include <hldb/udp_defn.h>
#include <hldb/unit.h>
#include <hldb/variable.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "class_test_0.sv";

// The line of the class declaration, and the first and last line of the
// module declaration.
constexpr uint32_t kClassLine = 15;
constexpr uint32_t kModuleFirstLine = 17;
constexpr uint32_t kModuleLastLine = 18;

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

// Size of an HLDB collection; a collection that was never created is empty.
template <typename T>
size_t sizeOf(const std::vector<T *> *collection) {
  return (collection == nullptr) ? 0 : collection->size();
}

// Number of objects in 'collection' whose source location is the file under test.
template <typename T>
size_t countFromSourceFile(const std::vector<T *> *collection) {
  size_t count = 0;
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (endsWith(object->getFile(), kSourceFileName)) ++count;
    }
  }
  return count;
}

// The first object in 'collection' whose source location is the file under
// test, or nullptr.
template <typename T>
const T *findFromSourceFile(const std::vector<T *> *collection) {
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (endsWith(object->getFile(), kSourceFileName)) return object;
    }
  }
  return nullptr;
}

// The line a diagnostic points at, or 0 when it carries no location.
uint32_t lineOf(const Event &event) {
  for (const EventArg &argument : event.getArguments()) {
    if (argument.m_line != 0) return argument.m_line;
  }
  return 0;
}

// True for an error-class event: its definition gives it a fatal, syntax or
// error severity. Every category counts: the design has a top-level module, so
// not even elaboration has an error to report (23.3.1).
bool isErrorClass(const Event &event) {
  const EventDefinition *const definition = event.getDefinition();
  if (definition == nullptr) return false;
  const EventSeverity severity = definition->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

// One event as hldb::renderEvent() prints it, one line per output line, so that a
// failing test shows what was reported and where.
std::string describeEvent(const Event &event) {
  std::ostringstream oss;
  const std::vector<std::string> lines = hldb::renderEvent(event);
  std::copy(lines.cbegin(), lines.cend(), std::ostream_iterator<std::string>(oss, "\n"));
  return oss.str();
}
}  // namespace

class ClassTest0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_test_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Every error-class diagnostic on a line from 'firstLine' to 'lastLine',
  // described; empty when there is none.
  static std::string eventsOnLines(uint32_t firstLine, uint32_t lastLine) {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const Event &event : listener->getEvents()) {
      const uint32_t line = lineOf(event);
      if (isErrorClass(event) && (line >= firstLine) && (line <= lastLine)) oss << describeEvent(event);
    }
    return oss.str();
  }

  // Every error-class diagnostic, described; empty when there is none.
  static std::string allEvents() {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const Event &event : listener->getEvents()) {
      if (isErrorClass(event)) oss << describeEvent(event);
    }
    return oss.str();
  }

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

  // The class declared in the file under test, or nullptr.
  static const hldb::ClassDefn *findClass() {
    return (m_unit == nullptr) ? nullptr : findFromSourceFile(m_unit->getClassDefns());
  }

  // The module declared in the file under test, or nullptr.
  static const hldb::Module *findModule() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllModules());
  }
};

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Every line of the file is legal, and the design has its top-level module,
// so nothing is fatal, a syntax error or an error, in any category.
TEST_F(ClassTest0Test, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

// Line 15: a class_declaration whose seven extra semicolons are empty
// class_items, the ";" form of class_item (8.3, A.1.9).
TEST_F(ClassTest0Test, Line15ClassIsAccepted) { EXPECT_EQ(eventsOnLines(kClassLine, kClassLine), ""); }

// Lines 17-18: an empty module_declaration (A.1.2).
TEST_F(ClassTest0Test, Lines17To18ModuleIsAccepted) { EXPECT_EQ(eventsOnLines(kModuleFirstLine, kModuleLastLine), ""); }

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once. It has no `include (22.4), no `define (22.5.1) and
// no macro usage.
TEST_F(ClassTest0Test, SourceFileHasNoIncludesOrMacros) {
  const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
  ASSERT_EQ(sourceFiles.size(), 1u);
  const hldb::SourceFile *const sourceFile = sourceFiles.front();

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// ---------------------------------------------------------------------------
// Line 15: class semicolon_classy
// ---------------------------------------------------------------------------

// The file declares one class, semicolon_classy (8.3). Its object is a class
// defn (37.31), whose vpiName is the class_identifier.
TEST_F(ClassTest0Test, ClassSemicolonClassyIsDeclared) {
  ASSERT_NE(m_unit, nullptr);
  EXPECT_EQ(countFromSourceFile(m_unit->getClassDefns()), 1u);

  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);
  EXPECT_EQ(classDefn->getAnyType(), hldb::AnyType::ClassDefn);
  EXPECT_EQ(classDefn->getDefName(), std::string_view("semicolon_classy"));
}

// The whole declaration, endclass included, is on line 15.
TEST_F(ClassTest0Test, ClassIsOnLine15) {
  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);

  EXPECT_TRUE(endsWith(classDefn->getFile(), kSourceFileName)) << classDefn->getFile();
  EXPECT_EQ(classDefn->getStartLine(), kClassLine);
  EXPECT_EQ(classDefn->getEndLine(), kClassLine);
}

// The header is "class semicolon_classy;": no virtual, so vpiVirtual is
// false; no extends, so there is no extends object (37.31 detail 6); no
// parameter_port_list, and no parameter among the items, so vpiParameter is
// empty (37.31 detail 7). No other class extends it, so vpiDerivedClasses is
// empty (37.31 detail 5).
TEST_F(ClassTest0Test, ClassIsNotVirtualAndExtendsNothing) {
  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);

  EXPECT_FALSE(classDefn->getVirtual());
  EXPECT_EQ(classDefn->getExtends(), nullptr);
  EXPECT_EQ(sizeOf(classDefn->getDerivedClasses()), 0u);
  EXPECT_EQ(sizeOf(classDefn->getParameters()), 0u);
}

// The seven class items are all ";", which declares nothing, so the class has
// no members (8.2) and nothing else that 37.31 lets a class defn hold. The
// implicit constructor does not count: vpiMethods "shall not include built-in
// methods for which there is no explicit declaration" (37.31 detail 1).
TEST_F(ClassTest0Test, ClassHasNoMembers) {
  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);

  EXPECT_EQ(sizeOf(classDefn->getVariables()), 0u);
  EXPECT_EQ(sizeOf(classDefn->getMethods()), 0u);
  EXPECT_EQ(sizeOf(classDefn->getConstraints()), 0u);
  EXPECT_EQ(sizeOf(classDefn->getTypedefs()), 0u);
  EXPECT_EQ(sizeOf(classDefn->getNamedEvents()), 0u);
  EXPECT_EQ(sizeOf(classDefn->getNamedEventArrays()), 0u);
  EXPECT_EQ(sizeOf(classDefn->getInternalScopes()), 0u);
}

// ---------------------------------------------------------------------------
// Lines 17-18: module test
// ---------------------------------------------------------------------------

// The file declares one module, test (A.1.2).
TEST_F(ClassTest0Test, ModuleTestIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);

  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getDefName(), std::string_view("test"));
}

// "module test;" is on line 17 and "endmodule" on line 18.
TEST_F(ClassTest0Test, ModuleTestIsOnLines17To18) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);

  EXPECT_TRUE(endsWith(module->getFile(), kSourceFileName)) << module->getFile();
  EXPECT_EQ(module->getStartLine(), kModuleFirstLine);
  EXPECT_EQ(module->getEndLine(), kModuleLastLine);
}

// test has no ports, parameters or items. The class is declared outside it,
// in the compilation-unit scope (3.12.1), so test holds no class defn either.
TEST_F(ClassTest0Test, ModuleTestIsEmpty) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);

  EXPECT_EQ(sizeOf(module->getVariables()), 0u);
  EXPECT_EQ(sizeOf(module->getNets()), 0u);
  EXPECT_EQ(sizeOf(module->getParameters()), 0u);
  EXPECT_EQ(sizeOf(module->getTypedefs()), 0u);
  EXPECT_EQ(sizeOf(module->getClassDefns()), 0u);
}

// ---------------------------------------------------------------------------
// Nothing else is declared
// ---------------------------------------------------------------------------

// The file declares one class and one module, and the empty class items
// declare nothing (8.3). So no other design object comes from this file.
TEST_F(ClassTest0Test, NoOtherDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);

  ASSERT_NE(m_unit, nullptr);
  EXPECT_EQ(countFromSourceFile(m_unit->getCheckerDecls()), 0u);
  EXPECT_EQ(countFromSourceFile(m_unit->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_unit->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_unit->getVariables()), 0u);
}

// ---------------------------------------------------------------------------
// Elaboration: the top-level instance
// ---------------------------------------------------------------------------

// test is never instantiated, so it is the design's top-level module: it "is
// implicitly instantiated once, and its instance name is the same as the
// module name" (23.3.1). The instance's definition is test, declared on line
// 17 of this file (vpiDefName, vpiDefFile, vpiDefLineNo, 37.10), and the class
// is not in it (3.12.1). The instance only exists after elaboration.
TEST_F(ClassTest0Test, TestIsTheOnlyTopLevelInstance) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    ASSERT_EQ(countFromSourceFile(m_design->getTopModules()), 1u);
    const hldb::Module *const instance = findFromSourceFile(m_design->getTopModules());
    ASSERT_NE(instance, nullptr);

    EXPECT_EQ(instance->getName(), "test");
    EXPECT_EQ(instance->getDefName(), "test");
    EXPECT_TRUE(endsWith(instance->getDefFile(), kSourceFileName)) << instance->getDefFile();
    EXPECT_EQ(instance->getDefLineNo(), static_cast<int32_t>(kModuleFirstLine));
    EXPECT_EQ(sizeOf(instance->getClassDefns()), 0u);
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
