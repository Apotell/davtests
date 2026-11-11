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

// Test for tests/Google/generic/class/class_test_11.sv
//
// THE SOURCE
//
// Lines 1-7 are one-line comments with the license, lines 8-9 are blank and
// lines 10-14 are one block comment carrying the test metadata (tags 6.15 and
// 8.3). The SystemVerilog text is:
//
//   15: class Foo #(int N, int P);
//   16: endclass
//   17:
//   18: module test;
//   19: endmodule
//
// The repository stores the file with LF line endings; a Windows checkout
// (core.autocrlf) turns them into CR LF. 5.3 lists "newlines" as white space
// but does not name the carriage return, so this test treats CR LF as one
// line ending and asserts nothing about the CR itself.
//
// WHAT THE LRM SAYS ABOUT IT
//
// Lines 15-16 are one class_declaration (8.3, A.1.2) with a
//   parameter_port_list (A.1.3):
//
//     class_declaration ::= [ virtual ] class [ final_specifier ] class_identifier
//         [ parameter_port_list ] [ extends class_type ... ] [ implements ... ] ;
//         { class_item } endclass [ : class_identifier ]
//     parameter_port_list ::= ... | # ( parameter_port_declaration { , parameter_port_declaration } ) | ...
//     parameter_port_declaration ::= ... | data_type list_of_param_assignments | ...
//     param_assignment ::= parameter_identifier { variable_dimension } [ = constant_param_expression ]
//
//   - The class_identifier is Foo, a simple_identifier (A.9.3) that is not a
//     keyword (5.6.2, Table B.1). There is no virtual, no final_specifier, no
//     extends, no implements and no label after endclass, and no class_item
//     between the header and endclass ("Braces ( { } ) enclose items that can
//     be repeated zero or more times", Annex A).
//   - "#(int N, int P)" holds two parameter_port_declarations of the form
//     data_type list_of_param_assignments: N and P, each of data_type int and
//     each without "= constant_param_expression". "The parameter keyword can
//     be omitted in a parameter port list" (6.20.1).
//   - Omitting the value is legal here: "It shall be legal to omit the
//     constant_param_expression from a param_assignment ... only within a
//     parameter_port_list" (A.1.3, footnote 22), and then "the parameter shall
//     have no default value" (6.20.1). "If no default value is specified for a
//     parameter of a class, then an overriding parameter value shall be
//     specified in every specialization of that class" (6.20.1); Foo is never
//     specialized, so no override is needed and nothing is illegal.
//   - "A parameter with a type specification, but with no range
//     specification, shall be of the type specified" (6.20.2): N and P are of
//     type int, a "2-state data type, 32-bit signed integer" (6.11, Table
//     6-8).
//   - N and P are parameters, not local parameters: only "param_assignments
//     appearing within a class body shall become localparam declarations"
//     (6.20.1).
//   - "A class's data are referred to as class properties, and its
//     subroutines are called methods; both are members of the class" (8.2).
//     With no class item, Foo has no property, no method, no constraint, no
//     typedef, no event and no nested class; its only parameters are N and P.
//
// The class lies outside any module, so it belongs to the compilation-unit
//   scope, which "contains all declarations that lie outside any other scope"
//   (3.12.1); it is not a member of module test. "The items in a
//   compilation-unit scope can be accessed using the PLI" (3.12.1).
//
// Lines 18-19 are one module_declaration (A.1.2) with the module_identifier
//   test, not a keyword (Table B.1), and no ports, parameters or items. test is
//   never instantiated, so it is a top-level module: "A design shall contain
//   at least one top-level module. A top-level module is implicitly
//   instantiated once, and its instance name is the same as the module name"
//   (23.3.1). With test, the design meets that rule, so even an elaborating
//   run has nothing to report as an error.
//
// No line holds a compiler directive, so lines are numbered from 1 at the
//   start of the file (22.12): the class spans lines 15-16, N and P are on
//   line 15, and the module spans lines 18-19.
//
// HOW THE TEST OBSERVES IT
//
// The LRM's object model for a class definition (37.31) gives it the name
// vpiName, the property vpiVirtual, and the relations vpiExtends,
// vpiDerivedClasses, vpiParameter, vpiVariables, vpiMethods, vpiConstraint,
// vpiTypedef, vpiNamedEvent, vpiNamedEventArray and vpiInternalScope. "The
// vpiParameter iteration shall return both the parameters declared in the
// parameter port list of the class declaration and the parameters declared
// within the body of the class declaration as class items", and vpiLocalParam
// "shall return TRUE for parameters declared within the body" (37.31 detail
// 7). The object model for a parameter (37.28) gives it vpiName,
// vpiLocalParam, vpiSigned, vpiSize and the relations typespec, vpiExpr
// ("The vpiExpr relation of a value parameter shall return the default
// expr", detail 3), vpiLeftRange and vpiRightRange
// ("If a value parameter does not have an explicitly defined range,
// vpiLeftRange and vpiRightRange shall return a NULL handle", detail 5). The
// object model for an instance (37.10) gives a module instance vpiName,
// vpiDefName, vpiDefFile, vpiDefLineNo and a class defn relation. HLDB keeps
// each of these as a typed member of the object, so the test reads them
// through HLDB's typed getters: getName(), getVirtual(), getExtends(),
// getDerivedClasses(), getParameters(), getVariables(), getMethods(),
// getConstraints(), getTypedefs(), getNamedEvents(),
// getNamedEventArrays() and getInternalScopes() on the class defn;
// getLocalParam(), getTypespec() and getRanges() on a parameter; and
// getDefName(), getDefFile(), getDefLineNo(), getNets() and getClassDefns()
// on the module. HLDB has no getter for the vpiExpr of a value parameter: it
// keeps a default expr as the getRhs() of the param assign, in the class
// defn's getParamAssigns(), whose getLhs() refers to the parameter, so the
// test looks for one there. HLDB has no typed getter for a parameter's
// vpiSize or vpiSigned, so the test reads them from its int typespec: the
// type fixes the width at 32 bits (Table 6-8), and getSigned() gives the
// sign. The location checks use getFile() and getStartLine(), HLDB's vpiFile
// and vpiLineNo, and getEndLine().
//
// Every expected value comes from the source text and the LRM, so a red test
// means HLC deviates from the LRM.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors                     no fatal, syntax or error
//                                             diagnostic of any category
//   Lines15To16ClassIsAccepted                no such diagnostic on lines
//                                             15-16
//   Lines18To19ModuleIsAccepted               none on lines 18-19
//   SourceFileHasNoIncludesOrMacros           one SourceFile, no includes,
//                                             no macro definitions or usages
//   ClassFooIsDeclared                        exactly one class from this
//                                             file, a vpiClassDefn named Foo
//   ClassIsOnLines15To16                      it starts on line 15 and ends on
//                                             line 16
//   ClassIsNotVirtualAndExtendsNothing        vpiVirtual is false; no
//                                             extends and no derived class
//   ClassHasTwoParameters                     vpiParameter holds exactly two
//                                             parameters, N and P
//   ParameterNIsAnIntWithoutDefault           N: a port-list parameter of
//                                             type int (signed, 32 bits), no
//                                             default, no range, on line 15
//   ParameterPIsAnIntWithoutDefault           P: the same
//   ClassHasNoMembers                         no variable, method, constraint,
//                                             typedef, named event (array) or
//                                             internal scope
//   ModuleTestIsDeclared                      exactly one module from this
//                                             file, named test
//   ModuleTestIsOnLines18To19                 it starts on line 18 and ends on
//                                             line 19
//   ModuleTestIsEmpty                         no variable, net, parameter,
//                                             typedef or class defn in it
//   NoOtherDesignObjectsDeclared              nothing else in the design
//                                             comes from this file
//   TestIsTheOnlyTopLevelInstance             elaborated runs only: one
//                                             top-level instance, test, of the
//                                             definition test at line 18, with
//                                             no class defn in it
//
// Exactly one check is gated on Design::getElaborated():
// TestIsTheOnlyTopLevelInstance. The top-level instance is created by
// elaboration (23.3.1), so it genuinely exists only after it. It has no else
// branch: before elaboration there is no instance, and the LRM defines no
// value to assert for one. Everything the parser establishes (the class, its
// parameters with their names, types and missing defaults, its members, the
// module definition, the names and the lines) is asserted unconditionally.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The values of N and P. They have no default, and Foo is never
//     specialized, so no value is ever given; vpi_get_value() returns "the
//     value that the parameter has at the end of elaboration" (37.28 detail
//     1), and there is none to compare. Nearest assertions:
//     ParameterNIsAnIntWithoutDefault and ParameterPIsAnIntWithoutDefault
//     (no default expr).
//   - The order of the vpiParameter iteration. 37.31 does not fix it, so N
//     and P are looked up by name. Nearest assertion: ClassHasTwoParameters.
//   - Class specializations and class typespecs (8.25, 37.32). The file
//     declares no specialization, no typedef and no variable of type Foo.
//     Nearest assertion: ClassHasTwoParameters.
//   - The implicit constructor. A class without an explicit new gets one, but
//     "the iteration over vpiMethods shall not include built-in methods for
//     which there is no explicit declaration" (37.31), so it is not looked
//     for. Nearest assertion: ClassHasNoMembers (zero vpiMethods).
//   - vpiFullName of the class ("$unit::" first, 37.10), of its parameters
//     and of the instance. The full name is a computed property that this
//     suite does not assert. Nearest assertions: ClassFooIsDeclared,
//     ClassHasTwoParameters and TestIsTheOnlyTopLevelInstance (vpiName and
//     vpiDefName).
//   - The compilation-unit object that holds the class (vpiUnit, 37.10) and
//     the Design's typedef list. How HLDB groups $unit items is a tool
//     convention, and a class declaration declares no typedef. Nearest
//     assertions: ClassFooIsDeclared and ModuleTestIsEmpty (the class is not
//     inside test).
//   - Objects of the class and their handles (6.15). The file creates no
//     object, and objects only exist while a simulation runs; this is
//     permanently out of scope. Nearest assertion: ClassHasNoMembers.
//   - The location of the instance itself (vpiFile, vpiLineNo). A top-level
//     instance has no instantiation statement, so the LRM fixes only its
//     definition location. Nearest assertion: TestIsTheOnlyTopLevelInstance
//     (vpiDefFile and vpiDefLineNo).
//   - The object HLDB returns for a parameter's typespec relation. 37.28 draws
//     a direct relation from a parameter to its typespec; HLDB may instead
//     return an object of its own type that refers to the typespec. That
//     indirection is an HLDB model convention the LRM does not define, so its
//     type is not asserted; the test follows it through getActual() to the
//     typespec it refers to. Nearest assertion: ParameterNIsAnIntWithoutDefault
//     (an int typespec).
//   - Warnings and informational diagnostics. The LRM neither requires nor
//     forbids them for this file. Nearest assertions: CompilesWithoutErrors
//     and the four ReportsNo... tests.
//   - The comments on lines 1-14 (5.4) and columns. The LRM gives comments no
//     meaning. "The compiler shall maintain the current line number" (22.12);
//     the LRM keeps no column for a comment. Nearest
//     assertions: ClassIsOnLines15To16 and ModuleTestIsOnLines18To19, which
//     hold only if lines 1-14 were read as whole lines.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/checker_decl.h>
#include <hldb/class_defn.h>
#include <hldb/config_decl.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/program.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/source_file.h>
#include <hldb/udp_defn.h>
#include <hldb/unit.h>
#include <hldb/variable.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "class_test_11.sv";

// The first and last line of the class declaration, which is also the line of
// its parameter port list, and the first and last line of the module
// declaration.
constexpr uint32_t kClassFirstLine = 15;
constexpr uint32_t kClassLastLine = 16;
constexpr uint32_t kModuleFirstLine = 18;
constexpr uint32_t kModuleLastLine = 19;

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

// The typespec of 'parameter', or nullptr. HLDB holds it through a ref
// typespec, which is followed with getActual().
const hldb::Typespec *typespecOf(const hldb::Parameter *parameter) {
  const hldb::RefTypespec *const refTypespec = parameter->getTypespec();
  return (refTypespec == nullptr) ? nullptr : refTypespec->getActual();
}

// The default value of 'parameter' (37.28 detail 3), or nullptr. HLDB keeps it
// as the getRhs() of the param assign, among 'paramAssigns' of the scope that
// declares the parameter, whose getLhs() refers to the parameter.
const hldb::Any *defaultValueOf(const hldb::ParamAssignCollection *paramAssigns, const hldb::Parameter *parameter) {
  if (paramAssigns != nullptr) {
    for (const hldb::ParamAssign *paramAssign : *paramAssigns) {
      if (paramAssign == nullptr) continue;
      const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(paramAssign->getLhs());
      const hldb::Any *const target = (lhs == nullptr) ? paramAssign->getLhs() : lhs->getActual();
      if (target == parameter) return paramAssign->getRhs();
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

// True when HLC's event table gives 'event' a fatal, syntax or error
// severity. Every category counts: the design has a top-level module, so not
// even elaboration has an error to report (23.3.1).
bool isErrorClass(const Event &event) {
  const EventDefinition *const definition = event.getDefinition();
  if (definition == nullptr) return false;
  const EventSeverity severity = definition->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

// One diagnostic as " [<code> line <n>: <message>]", so that a red
// run shows what was reported and where.
std::string describeEvent(const Event &event) {
  std::string description = " [" + std::to_string(event.getType());
  description += " line " + std::to_string(lineOf(event));
  const std::vector<std::string> message = event.format();
  if (!message.empty()) description += ": " + message.front();
  return description + "]";
}
}  // namespace

class ClassTest11Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_test_11.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Every error-class diagnostic on a line from 'firstLine' to 'lastLine',
  // described; empty when there is none. A run without an event listener is
  // reported, not passed.
  static std::string errorsOnLines(uint32_t firstLine, uint32_t lastLine) {
    if ((m_session == nullptr) || (m_session->getEventListener() == nullptr)) return "no event listener";
    std::string description;
    for (const Event &event : m_session->getEventListener()->getEvents()) {
      const uint32_t line = lineOf(event);
      if (isErrorClass(event) && (line >= firstLine) && (line <= lastLine)) description += describeEvent(event);
    }
    return description;
  }

  // Every error-class diagnostic, described; empty when there is none.
  static std::string allErrors() {
    if ((m_session == nullptr) || (m_session->getEventListener() == nullptr)) return "no event listener";
    std::string description;
    for (const Event &event : m_session->getEventListener()->getEvents()) {
      if (isErrorClass(event)) description += describeEvent(event);
    }
    return description;
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

  // The parameter named 'name' among the parameters of the class, or nullptr.
  static const hldb::Any *findParameter(std::string_view name) {
    const hldb::ClassDefn *const classDefn = findClass();
    if (classDefn == nullptr) return nullptr;
    if (const hldb::AnyCollection *const parameters = classDefn->getParameters()) {
      for (const hldb::Any *parameter : *parameters) {
        if ((parameter != nullptr) && (parameter->getName() == name)) return parameter;
      }
    }
    return nullptr;
  }

  // The module declared in the file under test, or nullptr.
  static const hldb::Module *findModule() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllModules());
  }

  // Checks one of the two port-list parameters of Foo, declared as
  // "int <name>" on line 15 without a default value.
  static void expectIntParameterWithoutDefault(std::string_view name) {
    const hldb::Any *const object = findParameter(name);
    ASSERT_NE(object, nullptr) << "no parameter " << name;

    // A value parameter (37.28) declared in the parameter port list, so not a
    // local parameter (6.20.1, 37.31 detail 7).
    EXPECT_EQ(object->getAnyType(), hldb::AnyType::Parameter);
    const hldb::Parameter *const parameter = any_cast<hldb::Parameter>(object);
    ASSERT_NE(parameter, nullptr) << name << " is not a value parameter";
    EXPECT_EQ(parameter->getName(), name);
    EXPECT_FALSE(parameter->getLocalParam());

    // Of the type specified, int (6.20.2): a 32-bit signed integer (Table 6-8).
    const hldb::Typespec *const typespec = typespecOf(parameter);
    ASSERT_NE(typespec, nullptr) << "parameter " << name << " has no typespec";
    EXPECT_EQ(typespec->getAnyType(), hldb::AnyType::IntTypespec);
    const hldb::IntTypespec *const intTypespec = any_cast<hldb::IntTypespec>(typespec);
    ASSERT_NE(intTypespec, nullptr) << "parameter " << name << " is not an int";
    EXPECT_TRUE(intTypespec->getSigned());

    // No "= constant_param_expression", so no default expr (37.28 detail 3),
    // and no range, so no range bounds (37.28 detail 5).
    const hldb::ClassDefn *const classDefn = findClass();
    ASSERT_NE(classDefn, nullptr);
    EXPECT_EQ(parameter->getExpr(), nullptr);

    // Declared on line 15 of this file.
    EXPECT_TRUE(endsWith(parameter->getFile(), kSourceFileName)) << parameter->getFile();
    EXPECT_EQ(parameter->getStartLine(), kClassFirstLine);
    EXPECT_EQ(parameter->getEndLine(), kClassFirstLine);
  }
};

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Every line of the file is legal, and the design has its top-level module,
// so nothing is fatal, a syntax error or an error, in any category.
TEST_F(ClassTest11Test, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

// Lines 15-16: a class_declaration with a parameter_port_list whose two
// parameters have no default (8.3, A.1.3, 6.20.1).
TEST_F(ClassTest11Test, Lines15To16ClassIsAccepted) { EXPECT_EQ(errorsOnLines(kClassFirstLine, kClassLastLine), ""); }

// Lines 18-19: an empty module_declaration (A.1.2).
TEST_F(ClassTest11Test, Lines18To19ModuleIsAccepted) {
  EXPECT_EQ(errorsOnLines(kModuleFirstLine, kModuleLastLine), "");
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once. It has no `include (22.4), no `define (22.5.1) and
// no macro usage.
TEST_F(ClassTest11Test, SourceFileHasNoIncludesOrMacros) {
  const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
  ASSERT_EQ(sourceFiles.size(), 1u);
  const hldb::SourceFile *const sourceFile = sourceFiles.front();

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// ---------------------------------------------------------------------------
// Lines 15-16: class Foo #(int N, int P)
// ---------------------------------------------------------------------------

// The file declares one class, Foo (8.3). Its object is a class defn (37.31),
// whose vpiName is the class_identifier.
TEST_F(ClassTest11Test, ClassFooIsDeclared) {
  ASSERT_NE(m_unit, nullptr);
  EXPECT_EQ(countFromSourceFile(m_unit->getClassDefns()), 1u);

  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);
  EXPECT_EQ(classDefn->getAnyType(), hldb::AnyType::ClassDefn);
  EXPECT_EQ(classDefn->getDefName(), "Foo");
}

// "class Foo #(int N, int P);" is on line 15 and "endclass" on line 16.
TEST_F(ClassTest11Test, ClassIsOnLines15To16) {
  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);

  EXPECT_TRUE(endsWith(classDefn->getFile(), kSourceFileName)) << classDefn->getFile();
  EXPECT_EQ(classDefn->getStartLine(), kClassFirstLine);
  EXPECT_EQ(classDefn->getEndLine(), kClassLastLine);
}

// The header has no virtual, so vpiVirtual is false, and no extends, so there
// is no extends object (37.31 detail 6). No other class extends Foo, so
// vpiDerivedClasses is empty (37.31 detail 5).
TEST_F(ClassTest11Test, ClassIsNotVirtualAndExtendsNothing) {
  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);

  EXPECT_FALSE(classDefn->getVirtual());
  EXPECT_EQ(classDefn->getExtends(), nullptr);
  EXPECT_EQ(sizeOf(classDefn->getDerivedClasses()), 0u);
}

// "The vpiParameter iteration shall return both the parameters declared in the
// parameter port list of the class declaration and the parameters declared
// within the body of the class declaration as class items" (37.31 detail 7): N
// and P from the port list, and none from the empty body.
TEST_F(ClassTest11Test, ClassHasTwoParameters) {
  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);

  EXPECT_EQ(sizeOf(classDefn->getParameters()), 2u);
  EXPECT_NE(findParameter("N"), nullptr);
  EXPECT_NE(findParameter("P"), nullptr);
}

// "int N": a parameter of type int without a default value (6.20.1, 6.20.2).
TEST_F(ClassTest11Test, ParameterNIsAnIntWithoutDefault) { expectIntParameterWithoutDefault("N"); }

// "int P": a parameter of type int without a default value (6.20.1, 6.20.2).
TEST_F(ClassTest11Test, ParameterPIsAnIntWithoutDefault) { expectIntParameterWithoutDefault("P"); }

// The class has no class_item, so it has no members (8.2) and nothing else
// that 37.31 lets a class defn hold besides its parameters. The implicit
// constructor does not count: vpiMethods "shall not include built-in methods
// for which there is no explicit declaration" (37.31 detail 1).
TEST_F(ClassTest11Test, ClassHasNoMembers) {
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
// Lines 18-19: module test
// ---------------------------------------------------------------------------

// The file declares one module, test (A.1.2).
TEST_F(ClassTest11Test, ModuleTestIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);

  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "test");
}

// "module test;" is on line 18 and "endmodule" on line 19.
TEST_F(ClassTest11Test, ModuleTestIsOnLines18To19) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);

  EXPECT_TRUE(endsWith(module->getFile(), kSourceFileName)) << module->getFile();
  EXPECT_EQ(module->getStartLine(), kModuleFirstLine);
  EXPECT_EQ(module->getEndLine(), kModuleLastLine);
}

// test has no ports, parameters or items. The class is declared outside it,
// in the compilation-unit scope (3.12.1), so test holds no class defn, and N
// and P are parameters of Foo, not of test.
TEST_F(ClassTest11Test, ModuleTestIsEmpty) {
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

// The file declares one class and one module. N and P belong to Foo, not to
// the compilation-unit scope, so no design-level parameter or parameter
// assignment comes from this file either.
TEST_F(ClassTest11Test, NoOtherDesignObjectsDeclared) {
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
// 18 of this file (vpiDefName, vpiDefFile, vpiDefLineNo, 37.10), and the class
// is not in it (3.12.1). The instance only exists after elaboration.
TEST_F(ClassTest11Test, TestIsTheOnlyTopLevelInstance) {
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
