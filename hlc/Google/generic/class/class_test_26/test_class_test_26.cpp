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

// Test for tests/Google/generic/class/class_test_26.sv
//
// THE SOURCE
//
// Lines 1-7 are one-line comments with the license, lines 8-9 are blank and
// lines 10-14 are one block comment carrying the test metadata (tags 6.15 and
// 8.3). The SystemVerilog text is:
//
//   15: interface class Bar #(parameter N); endclass
//   16:
//   17: parameter int N = 1;
//   18: class Foo implements Bar#(N); endclass
//   19:
//   20: module test;
//   21: endmodule
//
// Line 21 ends with a line ending. The repository stores the file with LF
// line endings; a Windows checkout (core.autocrlf) turns them into CR LF. 5.3
// lists "newlines" as white space but does not name the carriage return, so
// this test treats CR LF as one line ending and asserts nothing about the CR
// itself.
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Line 15 is an interface_class_declaration (8.26.1, A.1.2) of Bar with a
//   parameter_port_list:
//
//     interface_class_declaration ::= interface class class_identifier
//         [ parameter_port_list ] [ extends interface_class_type ... ] ;
//         { interface_class_item } endclass [ : class_identifier ]
//
//   - "parameter N" is a parameter_declaration in the port list that declares
//     one value parameter, N, with no data type, range or value (6.20.1,
//     A.1.3). "It shall be legal to omit the constant_param_expression from a
//     param_assignment or the data_type from a type_assignment only within a
//     parameter_port_list" (Annex A footnote 22).
//   - "A parameter declaration with no type or range specification shall
//     default to the type and range of the final value assigned to the
//     parameter, after any value overrides have been applied" (6.20.2).
//
// Line 17 declares a parameter of type int named N in the compilation-unit
//   scope, which "contains all declarations that lie outside any other scope"
//   (3.12.1). "All param_assignments appearing within a generate block,
//   package, or compilation-unit scope shall become localparam declarations"
//   (6.20.1), so this N is a local parameter. int is a "2-state data type,
//   32-bit signed integer" (Table 6-8), and its value is 1.
//
// Line 18 is a class_declaration (8.3, A.1.2) of Foo with the clause
//   "implements Bar#(N)" ("..." marks shortened parts):
//
//     class_declaration ::= [ virtual ] class [ final_specifier ] class_identifier
//         [ parameter_port_list ] [ extends class_type ... ]
//         [ implements interface_class_type { , interface_class_type } ] ;
//         { class_item } endclass [ : class_identifier ]
//     interface_class_type ::= ps_class_identifier [ parameter_value_assignment ]
//
//   (interface_class_type: A.2.2.1.)
//   - A class "may implement, but not extend, one or more interface classes"
//     (8.26.2), and "An interface class shall be declared before it is
//     implemented or extended" (8.26.4): Bar is declared on line 15.
//   - The N inside "#(N)" is written in the compilation-unit scope, where
//     Bar's own parameter N is not visible: it names the local parameter of
//     line 17, declared before it (3.12.1). By ordered list it gives Bar's N
//     the value 1 (23.10.2.1).
//   - Bar declares no method, so Foo has nothing to implement.
//   - Implementing is not extending (8.26.2), so Foo has no base class (37.31
//     detail 6).
//
// Lines 20-21 are one module_declaration (A.1.2). test is never instantiated,
//   so it is a top-level module: "A design shall contain at least one
//   top-level module. A top-level module is implicitly instantiated once, and
//   its instance name is the same as the module name" (23.3.1).
//
// No line holds a compiler directive, so lines are numbered from 1 at the
//   start of the file (22.12).
//
// HOW THE TEST OBSERVES IT
//
// The object model has one object for a class declaration, the class defn
// (37.31), with vpiName, vpiVirtual, vpiExtends (detail 6), vpiDerivedClasses
// ("all the class defns derived from the given class defn", detail 5),
// vpiParameter ("both the parameters declared in the parameter port list of
// the class declaration and the parameters declared within the body of the
// class declaration as class items", detail 7) and its member relations. It
// has no separate object for an interface class, so this test reads Bar as a
// class defn too: an interpretation, since 37.31 does not name interface
// classes. A parameter (37.28) has vpiName, vpiLocalParam, vpiSigned,
// vpiSize, a typespec, vpiExpr ("The vpiExpr relation of a value parameter
// shall return the default expr", detail 3) and vpiLeftRange and
// vpiRightRange ("If a value parameter does not have an explicitly defined
// range, vpiLeftRange and vpiRightRange shall return a NULL handle", detail
// 5). vpiDecompile "shall return a string with a functionally equivalent
// expression to the original expression within the source code" (37.59
// detail 2); for a default expr that is a lone literal with no operator, this
// test takes that string to be the literal as written, an LRM interpretation
// rather than a quoted rule. A module instance (37.10) has vpiName,
// vpiDefName, vpiDefFile and vpiDefLineNo. HLDB answers vpi_get() and
// vpi_iterate() through getVpiPropertyValue() and getByVpiType(), so the test
// asks those questions directly with the LRM's vpiXXX codes. The
// compilation-unit parameter is read from the Design's parameter list.
//
// Every expected value comes from the source text and the LRM, so a red test
// means HLC deviates from the LRM.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors                     no fatal, syntax or error
//                                             diagnostic of any category
//   Line15InterfaceClassBarIsAccepted         none on line 15
//   Line17ParameterIsAccepted                 none on line 17
//   Line18ClassFooIsAccepted                  none on line 18
//   Lines20To21ModuleIsAccepted               none on lines 20-21
//   ReportsNoMisreadingDiagnostics            no syntax, keyword, identifier
//                                             length or non-ASCII diagnostic
//                                             at any severity
//   ReportsNoClassDiagnostics                 no duplicate or undefined class,
//                                             missing base class or label
//                                             mismatch
//   ReportsNoParameterDiagnostics             no duplicate parameter,
//                                             "localparam without value",
//                                             unbound name, non-constant value
//                                             or bad or missing override
//   ReportsNoDesignHierarchyDiagnostics       no duplicate design unit,
//                                             missing definition or missing
//                                             top-level module
//   SourceFileHasNoIncludesOrMacros           one SourceFile, no includes,
//                                             no macro definitions or usages
//   InterfaceClassBarIsDeclared               a class defn named Bar from this
//                                             file, on line 15
//   InterfaceClassBarHasOneParameter          vpiParameter holds exactly one
//                                             parameter, N
//   BarParameterNHasNoDefault                 Bar's N: a port-list value
//                                             parameter with no default and
//                                             no range, on line 15
//   InterfaceClassBarHasNoBaseOrDerivedClass  no extends; vpiDerivedClasses is
//                                             empty: Foo implements Bar, it
//                                             does not derive from it
//   InterfaceClassBarHasNoMembers             no variable, method, constraint,
//                                             typedef, named event (array) or
//                                             internal scope
//   UnitParameterNIsDeclared                  exactly one compilation-unit
//                                             parameter from this file, N, on
//                                             line 17
//   UnitParameterNIsLocalIntOne               it is a local parameter of type
//                                             int (signed, 32 bits) with the
//                                             default expr 1 and no range
//   ClassFooIsDeclared                        a class defn named Foo from this
//                                             file
//   ClassFooIsOnLine18                        it lies on line 18 only
//   ClassFooExtendsNothing                    vpiVirtual is false; no
//                                             extends, derived class or
//                                             parameter
//   ClassFooHasNoMembers                      as for Bar
//   ModuleTestIsDeclared                      exactly one module from this
//                                             file, named test
//   ModuleTestIsOnLines20To21                 it starts on line 20 and ends on
//                                             line 21
//   ModuleTestIsEmpty                         no variable, net, parameter,
//                                             typedef or class defn in it
//   NoOtherDesignObjectsDeclared              no interface, program, declared
//                                             package, UDP, config, checker
//                                             or variable comes from this file
//   TestIsTheOnlyTopLevelInstance             elaborated runs only: one
//                                             top-level instance, test, of the
//                                             definition test at line 20, with
//                                             no class defn in it
//
// Exactly one check is gated on Design::getElaborated():
// TestIsTheOnlyTopLevelInstance. The top-level instance is created by
// elaboration (23.3.1), so it genuinely exists only after it. It has no else
// branch: before elaboration there is no instance, and the LRM defines no
// value to assert for one. Everything the parser establishes (the classes,
// both parameters N, the module, the names and the lines) is asserted
// unconditionally.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Which interface classes Foo implements, which N "Bar#(N)" names and the
//     value it gives Bar's N. 37.31 gives a class defn vpiExtends for its base
//     class but no relation for the interface classes it implements, so the
//     implements clause and its specialization have no object to read; this
//     test does not invent one. Nearest assertions: ClassFooExtendsNothing
//     (implements is not read as extends) and ReportsNoParameterDiagnostics
//     (the name binds and the override is accepted).
//   - The type and size of Bar's N. With no type or range, a parameter takes
//     "the type and range of the final value assigned to the parameter, after
//     any value overrides have been applied" (6.20.2); Bar declares no value,
//     and its only specialization is the one the object model does not show.
//     Nearest assertion: BarParameterNHasNoDefault (no default, no range).
//   - The object HLDB returns for the typespec relation of the local
//     parameter N (37.28). The LRM draws it as a direct relation to a
//     typespec; HLDB may instead return an object of its own type that refers
//     to the typespec. That indirection is an HLDB model convention the LRM
//     does not define, so its type is not asserted; the test follows it
//     through vpiActual to the typespec it refers to. Nearest assertion:
//     UnitParameterNIsLocalIntOne (vpiIntTypespec on that typespec).
//   - The value of the local parameter N as vpi_get_value() returns it, "the
//     value that the parameter has at the end of elaboration" (37.28 detail
//     1). Nearest assertion: UnitParameterNIsLocalIntOne (the default expr).
//   - The Design's parameter assignment list. How HLDB records the override
//     list of a specialization outside the object model is a tool convention.
//     Nearest assertion: ReportsNoParameterDiagnostics.
//   - Whether Bar is marked as an interface class. 37.31 defines no property
//     for it. Nearest assertion: InterfaceClassBarIsDeclared.
//   - A package object for the compilation unit of this file. The object
//     model represents a compilation unit as a package with vpiUnit (37.10),
//     although "the compilation-unit scope is not a package" (3.12.1);
//     whether HLDB lists one for this file is a tool convention, so only
//     packages with vpiUnit not TRUE are counted. Nearest assertion:
//     NoOtherDesignObjectsDeclared.
//   - vpiFullName of any object. The full name is a computed property that
//     this suite does not assert. Nearest assertions:
//     InterfaceClassBarIsDeclared, ClassFooIsDeclared and
//     TestIsTheOnlyTopLevelInstance (vpiName and vpiDefName).
//   - Objects of the classes (6.15). The file has no new (8.4), so it creates
//     no object, and objects only exist while a simulation runs; this is
//     permanently out of scope. Nearest assertion: ClassFooHasNoMembers.
//   - The location of the instance itself (vpiFile, vpiLineNo). A top-level
//     instance has no instantiation statement, so the LRM fixes only its
//     definition location. Nearest assertion: TestIsTheOnlyTopLevelInstance
//     (vpiDefFile and vpiDefLineNo).
//   - Warnings and informational diagnostics. The LRM neither requires nor
//     forbids them for this file. Nearest assertions: CompilesWithoutErrors
//     and the four ReportsNo... tests.
//   - The comments on lines 1-14 (5.4) and columns. The LRM gives comments no
//     meaning. "The compiler shall maintain the current line number" (22.12);
//     the LRM keeps no column for a comment. Nearest assertions: the line
//     checks, which hold only if lines 1-14 were read as whole lines.

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
#include <hldb/sv_vpi_user.h>
#include <hldb/udp_defn.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "class_test_26.sv";

// vpiVariables, the VPI relation from a scope to its variables (100 in IEEE
// 1800-2023 Annex K, vpi_user.h). HLDB's vpi_user.h names the same relation
// vpiVariable.
constexpr int32_t kVpiVariables = vpiVariable;

// The lines of the declarations.
constexpr uint32_t kBarLine = 15;
constexpr uint32_t kUnitParameterLine = 17;
constexpr uint32_t kFooLine = 18;
constexpr uint32_t kModuleFirstLine = 20;
constexpr uint32_t kModuleLastLine = 21;

// The width of int, a "2-state data type, 32-bit signed integer" (Table 6-8).
constexpr int64_t kIntBits = 32;

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

// The object in 'collection' from the file under test named 'name', or
// nullptr; with an empty 'name', the first object from the file.
template <typename T>
const T *findFromSourceFile(const std::vector<T *> *collection, std::string_view name = "") {
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (endsWith(object->getFile(), kSourceFileName) && (name.empty() || (object->getName() == name))) {
        return object;
      }
    }
  }
  return nullptr;
}

// The objects 'object' has in the one-to-many VPI relation 'relation', as
// vpi_iterate() would return them (37.3), or nullptr when there are none.
const std::vector<const hldb::Any *> *relationObjects(const hldb::Any *object, int32_t relation) {
  return std::get<2>(object->getByVpiType(relation));
}

// The object 'object' has in the one-to-one VPI relation 'relation', as
// vpi_handle() would return it (37.3), or nullptr.
const hldb::Any *relationObject(const hldb::Any *object, int32_t relation) {
  return std::get<1>(object->getByVpiType(relation));
}

// The typespec 'object' has in the VPI relation 'relation', or nullptr. HLDB
// may return an object of its own type that refers to the typespec through
// vpiActual; that reference is followed. A typespec itself has no vpiActual
// relation, so a direct typespec is returned as is.
const hldb::Any *typespecOf(const hldb::Any *object, int32_t relation) {
  const hldb::Any *const typespec = relationObject(object, relation);
  if (typespec == nullptr) return nullptr;
  const hldb::Any *const referenced = relationObject(typespec, vpiActual);
  return (referenced != nullptr) ? referenced : typespec;
}

// Number of objects 'object' has in the VPI relation 'relation'; 0 when the
// relation is empty.
size_t relationSize(const hldb::Any *object, int32_t relation) {
  if (const std::vector<const hldb::Any *> *const objects = relationObjects(object, relation)) return objects->size();
  return (relationObject(object, relation) != nullptr) ? 1 : 0;
}

// The object named 'name' in the VPI relation 'relation' of 'object', or
// nullptr.
const hldb::Any *findInRelation(const hldb::Any *object, int32_t relation, std::string_view name) {
  if (const std::vector<const hldb::Any *> *const objects = relationObjects(object, relation)) {
    for (const hldb::Any *candidate : *objects) {
      if ((candidate != nullptr) && (candidate->getName() == name)) return candidate;
    }
  }
  return nullptr;
}

// The string VPI property 'property' of 'object', as vpi_get_str() would
// return it, or a marker when the property holds no string.
std::string stringProperty(const hldb::Any *object, int32_t property) {
  const hldb::Any::vpi_property_value_t value = object->getVpiPropertyValue(property);
  if (const char *const *const text = std::get_if<const char *>(&value)) {
    return (*text == nullptr) ? std::string("<null>") : std::string(*text);
  }
  return "<not a string>";
}

// The integer VPI property 'property' of 'object', as vpi_get() would return
// it, or vpiUndefined when the property holds no integer.
int64_t integerProperty(const hldb::Any *object, int32_t property) {
  const hldb::Any::vpi_property_value_t value = object->getVpiPropertyValue(property);
  if (const int64_t *const number = std::get_if<int64_t>(&value)) return *number;
  return vpiUndefined;
}

// Number of packages from the file under test that are declared packages,
// not the package object the model may use for a compilation unit (vpiUnit
// TRUE, 37.10).
template <typename T>
size_t countDeclaredPackagesFromSourceFile(const std::vector<T *> *packages) {
  size_t count = 0;
  if (packages != nullptr) {
    for (const T *package : *packages) {
      if (endsWith(package->getFile(), kSourceFileName) && (integerProperty(package, vpiUnit) != 1)) ++count;
    }
  }
  return count;
}

// The line a diagnostic points at, or 0 when it carries no location.
uint32_t lineOf(const Event &error) { return error.getArguments().empty() ? 0 : error.getArguments().front().m_line; }

// True when HLC's error table gives 'error' a fatal, syntax or error
// severity. Every category counts: the design has a top-level module, so not
// even elaboration has an error to report (23.3.1).
bool isErrorClass(const Event &error) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
  if (errorInfo == nullptr) return false;
  const EventSeverity severity = errorInfo->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

// One diagnostic as " [<code> line <n>: <message template>]", so that a red
// run shows what was reported and where.
std::string describeError(const Event &error) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
  std::string description = " [" + std::to_string(static_cast<int32_t>(error.getType()));
  description += " line " + std::to_string(lineOf(error));
  if (errorInfo != nullptr) description += ": " + std::string(errorInfo->m_lines[0].m_text);
  return description + "]";
}
}  // namespace

class ClassTest26Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_test_26.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Every error-class diagnostic on a line from 'firstLine' to 'lastLine',
  // described; empty when there is none. A run without an error container is
  // reported, not passed.
  static std::string errorsOnLines(uint32_t firstLine, uint32_t lastLine) {
    if ((m_session == nullptr) || (m_session->getEventListener() == nullptr)) return "no error container";
    std::string description;
    for (const Event &error : m_session->getEventListener()->getEvents()) {
      const uint32_t line = lineOf(error);
      if (isErrorClass(error) && (line >= firstLine) && (line <= lastLine)) description += describeError(error);
    }
    return description;
  }

  // Every error-class diagnostic, described; empty when there is none.
  static std::string allErrors() {
    if ((m_session == nullptr) || (m_session->getEventListener() == nullptr)) return "no error container";
    std::string description;
    for (const Event &error : m_session->getEventListener()->getEvents()) {
      if (isErrorClass(error)) description += describeError(error);
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

  // The compilation-unit class named 'name' declared in the file under test,
  // or nullptr.
  static const hldb::ClassDefn *findClass(std::string_view name) {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllClasses(), name);
  }

  // The compilation-unit parameter N of line 17, or nullptr. Bar has a
  // parameter of the same name, so the line is part of the match.
  static const hldb::Any *findUnitParameterN() {
    if ((m_design == nullptr) || (m_design->getParameters() == nullptr)) return nullptr;
    for (const hldb::Any *parameter : *m_design->getParameters()) {
      if (endsWith(parameter->getFile(), kSourceFileName) && (parameter->getName() == "N") &&
          (parameter->getStartLine() == kUnitParameterLine)) {
        return parameter;
      }
    }
    return nullptr;
  }

  // The module declared in the file under test, or nullptr.
  static const hldb::Module *findModule() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllModules());
  }

  // Checks that 'classDefn' has no member and nothing else 37.31 lets a class
  // defn hold besides parameters (8.2; 37.31 detail 1 leaves out the implicit
  // constructor).
  static void expectNoMembers(const hldb::Any *classDefn) {
    ASSERT_NE(classDefn, nullptr);

    EXPECT_EQ(relationSize(classDefn, kVpiVariables), 0u);
    EXPECT_EQ(relationSize(classDefn, vpiMethods), 0u);
    EXPECT_EQ(relationSize(classDefn, vpiConstraint), 0u);
    EXPECT_EQ(relationSize(classDefn, vpiTypedef), 0u);
    EXPECT_EQ(relationSize(classDefn, vpiNamedEvent), 0u);
    EXPECT_EQ(relationSize(classDefn, vpiNamedEventArray), 0u);
    EXPECT_EQ(relationSize(classDefn, vpiInternalScope), 0u);
  }
};

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Every line of the file is legal, and the design has its top-level module,
// so nothing is fatal, a syntax error or an error, in any category.
TEST_F(ClassTest26Test, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

// Line 15: an interface_class_declaration whose parameter has no value, as a
// parameter_port_list allows (8.26.1, Annex A footnote 22).
TEST_F(ClassTest26Test, Line15InterfaceClassBarIsAccepted) { EXPECT_EQ(errorsOnLines(kBarLine, kBarLine), ""); }

// Line 17: a parameter declaration in the compilation-unit scope (6.20.1,
// 3.12.1).
TEST_F(ClassTest26Test, Line17ParameterIsAccepted) {
  EXPECT_EQ(errorsOnLines(kUnitParameterLine, kUnitParameterLine), "");
}

// Line 18: a class_declaration that implements a specialization of an
// interface class declared earlier (8.26, 8.26.4, A.2.2.1).
TEST_F(ClassTest26Test, Line18ClassFooIsAccepted) { EXPECT_EQ(errorsOnLines(kFooLine, kFooLine), ""); }

// Lines 20-21: an empty module_declaration (A.1.2).
TEST_F(ClassTest26Test, Lines20To21ModuleIsAccepted) {
  EXPECT_EQ(errorsOnLines(kModuleFirstLine, kModuleLastLine), "");
}

// Each of these diagnostics, at any severity and on any line, would mean HLC
// misread the text of the file.
TEST_F(ClassTest26Test, ReportsNoMisreadingDiagnostics) {
  // Every declaration matches the grammar (A.1.2, A.1.3, A.2.2.1), and the
  // file uses no preprocessor directive.
  EXPECT_EQ(findEvent(EventType::PpSyntaxError), nullptr);
  EXPECT_EQ(findEvent(EventType::PaSyntaxError), nullptr);

  // Bar, N, Foo and test are no keywords (5.6.2, Table B.1), and the longest,
  // test, has 4 characters, within any limit an implementation may set, which
  // "shall be at least 1024 characters" (5.6).
  EXPECT_EQ(findEvent(EventType::PaReservedKeyword), nullptr);
  EXPECT_EQ(findEvent(EventType::PaMaxLengthIdentifier), nullptr);

  // Every character of the file is ASCII.
  EXPECT_EQ(findEvent(EventType::PpNonAsciiContent), nullptr);
}

// Bar and Foo are each declared once, Bar is declared before Foo implements
// it (8.26.4), Foo has no base class, and neither endclass has a label, so
// none of these diagnostics applies, at any severity.
TEST_F(ClassTest26Test, ReportsNoClassDiagnostics) {
  EXPECT_EQ(findEvent(EventType::CompMultiplyDefinedClass), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUndefinedClass), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUndefinedBaseClass), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUnmatchedLabel), nullptr);
}

// The two parameters named N are in different scopes, Bar's and the
// compilation unit's (3.13); Bar's N is a parameter of a port list, which may
// have no value (Annex A footnote 22); the local parameter N has the value 1;
// the N in "#(N)" binds to it, declared on line 17, before line 18; and
// "#(N)" gives Bar's one parameter one constant (23.10.2.1). So none of these
// diagnostics applies, at any severity.
TEST_F(ClassTest26Test, ReportsNoParameterDiagnostics) {
  EXPECT_EQ(findEvent(EventType::CompMultiplyDefinedParameter), nullptr);
  EXPECT_EQ(findEvent(EventType::CompLocalparamNoValue), nullptr);
  EXPECT_EQ(findEvent(EventType::CompFailedToBind), nullptr);
  EXPECT_EQ(findEvent(EventType::CompNotAConstantExpression), nullptr);
  EXPECT_EQ(findEvent(EventType::ElabUnknownParameterOverride), nullptr);
  EXPECT_EQ(findEvent(EventType::ElabMissingParameterOverride), nullptr);
}

// The classes and the module are each declared once, nothing is
// instantiated, and test is a top-level module (23.3.1), so none of these
// diagnostics applies, at any severity.
TEST_F(ClassTest26Test, ReportsNoDesignHierarchyDiagnostics) {
  EXPECT_EQ(findEvent(EventType::CompMultiplyDefinedDesignUnit), nullptr);
  EXPECT_EQ(findEvent(EventType::ElabMultiplyDefinedModule), nullptr);
  EXPECT_EQ(findEvent(EventType::ElabNoModuleDefinition), nullptr);
  EXPECT_EQ(findEvent(EventType::ElabNoTopLevelModule), nullptr);
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once (run configuration). It has no `include (22.4), no
// `define (22.5.1) and no macro usage.
TEST_F(ClassTest26Test, SourceFileHasNoIncludesOrMacros) {
  const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
  ASSERT_EQ(sourceFiles.size(), 1u);
  const hldb::SourceFile *const sourceFile = sourceFiles.front();

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// ---------------------------------------------------------------------------
// Line 15: interface class Bar #(parameter N)
// ---------------------------------------------------------------------------

// Bar is declared in the compilation-unit scope (3.12.1), as a class defn
// (37.31), and its whole declaration is on line 15.
TEST_F(ClassTest26Test, InterfaceClassBarIsDeclared) {
  const hldb::ClassDefn *const bar = findClass("Bar");
  ASSERT_NE(bar, nullptr);

  EXPECT_EQ(bar->getVpiType(), static_cast<uint32_t>(vpiClassDefn));
  EXPECT_EQ(stringProperty(bar, vpiName), "Bar");
  EXPECT_EQ(bar->getStartLine(), kBarLine);
  EXPECT_EQ(bar->getEndLine(), kBarLine);
}

// "The vpiParameter iteration shall return both the parameters declared in the
// parameter port list of the class declaration and the parameters declared
// within the body of the class declaration as class items" (37.31 detail 7):
// N from the port list, and none from the empty body.
TEST_F(ClassTest26Test, InterfaceClassBarHasOneParameter) {
  const hldb::ClassDefn *const bar = findClass("Bar");
  ASSERT_NE(bar, nullptr);

  EXPECT_EQ(relationSize(bar, vpiParameter), 1u);
  EXPECT_NE(findInRelation(bar, vpiParameter, "N"), nullptr);
}

// "parameter N": a value parameter of the port list, so not a local
// parameter (6.20.1, 37.31 detail 7), declared on line 15 with no value
// (Annex A footnote 22), so no default expr (37.28 detail 3), and no range, so
// no range bounds (37.28 detail 5).
TEST_F(ClassTest26Test, BarParameterNHasNoDefault) {
  const hldb::ClassDefn *const bar = findClass("Bar");
  ASSERT_NE(bar, nullptr);
  const hldb::Any *const parameter = findInRelation(bar, vpiParameter, "N");
  ASSERT_NE(parameter, nullptr) << "no parameter N in Bar";

  EXPECT_EQ(parameter->getVpiType(), static_cast<uint32_t>(vpiParameter));
  EXPECT_EQ(stringProperty(parameter, vpiName), "N");
  EXPECT_EQ(integerProperty(parameter, vpiLocalParam), 0);
  EXPECT_EQ(relationSize(parameter, vpiExpr), 0u);
  EXPECT_EQ(relationSize(parameter, vpiLeftRange), 0u);
  EXPECT_EQ(relationSize(parameter, vpiRightRange), 0u);
  EXPECT_EQ(parameter->getStartLine(), kBarLine);
}

// Bar's header has no extends clause, so there is no extends object (37.31
// detail 6). "The vpiDerivedClasses iterator shall return all the class defns
// derived from the given class defn" (37.31 detail 5); Foo implements Bar,
// which a class "may implement, but not extend" (8.26.2), so no class defn
// derives from Bar.
TEST_F(ClassTest26Test, InterfaceClassBarHasNoBaseOrDerivedClass) {
  const hldb::ClassDefn *const bar = findClass("Bar");
  ASSERT_NE(bar, nullptr);

  EXPECT_EQ(relationSize(bar, vpiExtends), 0u);
  EXPECT_EQ(relationSize(bar, vpiDerivedClasses), 0u);
}

// Bar has no interface_class_item, so it has no member (8.2).
TEST_F(ClassTest26Test, InterfaceClassBarHasNoMembers) { expectNoMembers(findClass("Bar")); }

// ---------------------------------------------------------------------------
// Line 17: parameter int N = 1 in the compilation-unit scope
// ---------------------------------------------------------------------------

// Line 17 declares one parameter in the compilation-unit scope (3.12.1);
// Bar's N belongs to Bar, so the file declares no other parameter there.
TEST_F(ClassTest26Test, UnitParameterNIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 1u);

  const hldb::Any *const parameter = findUnitParameterN();
  ASSERT_NE(parameter, nullptr) << "no parameter N on line 17";
  EXPECT_EQ(parameter->getVpiType(), static_cast<uint32_t>(vpiParameter));
  EXPECT_EQ(stringProperty(parameter, vpiName), "N");
}

// "All param_assignments appearing within a generate block, package, or
// compilation-unit scope shall become localparam declarations" (6.20.1), so N
// is a local parameter. Its type is int (6.20.2): signed, 32 bits (Table
// 6-8). Its default expr (37.28 detail 3) is a lone literal, so this test
// takes its vpiDecompile to be the literal as written: an interpretation of
// 37.59 detail 2, which asks only for "a functionally equivalent expression".
// It has no range (37.28 detail 5).
TEST_F(ClassTest26Test, UnitParameterNIsLocalIntOne) {
  const hldb::Any *const parameter = findUnitParameterN();
  ASSERT_NE(parameter, nullptr) << "no parameter N on line 17";

  EXPECT_EQ(integerProperty(parameter, vpiLocalParam), 1);

  const hldb::Any *const typespec = typespecOf(parameter, vpiTypespec);
  ASSERT_NE(typespec, nullptr) << "the local parameter N has no typespec";
  EXPECT_EQ(typespec->getVpiType(), static_cast<uint32_t>(vpiIntTypespec));
  EXPECT_EQ(integerProperty(parameter, vpiSigned), 1);
  EXPECT_EQ(integerProperty(parameter, vpiSize), kIntBits);

  const hldb::Any *const defaultExpr = relationObject(parameter, vpiExpr);
  ASSERT_NE(defaultExpr, nullptr) << "the local parameter N has no default expr";
  EXPECT_EQ(stringProperty(defaultExpr, vpiDecompile), "1");
  EXPECT_EQ(relationSize(parameter, vpiLeftRange), 0u);
  EXPECT_EQ(relationSize(parameter, vpiRightRange), 0u);
}

// ---------------------------------------------------------------------------
// Line 18: class Foo implements Bar#(N)
// ---------------------------------------------------------------------------

// Foo is declared in the compilation-unit scope (3.12.1), as a class defn
// (37.31) whose vpiName is its class_identifier.
TEST_F(ClassTest26Test, ClassFooIsDeclared) {
  const hldb::ClassDefn *const foo = findClass("Foo");
  ASSERT_NE(foo, nullptr);
  EXPECT_EQ(foo->getVpiType(), static_cast<uint32_t>(vpiClassDefn));
  EXPECT_EQ(stringProperty(foo, vpiName), "Foo");
}

// The whole declaration of Foo, endclass included, is on line 18.
TEST_F(ClassTest26Test, ClassFooIsOnLine18) {
  const hldb::ClassDefn *const foo = findClass("Foo");
  ASSERT_NE(foo, nullptr);

  EXPECT_TRUE(endsWith(foo->getFile(), kSourceFileName)) << foo->getFile();
  EXPECT_EQ(foo->getStartLine(), kFooLine);
  EXPECT_EQ(foo->getEndLine(), kFooLine);
}

// Foo's header has no virtual and no extends clause; implementing Bar is not
// extending it (8.26.2), so there is no extends object (37.31 detail 6). No
// class derives from Foo (37.31 detail 5). Foo has no parameter_port_list and
// an empty body; the parameter that "#(N)" overrides is Bar's (37.31 detail
// 7).
TEST_F(ClassTest26Test, ClassFooExtendsNothing) {
  const hldb::ClassDefn *const foo = findClass("Foo");
  ASSERT_NE(foo, nullptr);

  EXPECT_EQ(integerProperty(foo, vpiVirtual), 0);
  EXPECT_EQ(relationSize(foo, vpiExtends), 0u);
  EXPECT_EQ(relationSize(foo, vpiDerivedClasses), 0u);
  EXPECT_EQ(relationSize(foo, vpiParameter), 0u);
}

// Foo has no class_item, so it has no member (8.2); Bar has no method for it
// to implement.
TEST_F(ClassTest26Test, ClassFooHasNoMembers) { expectNoMembers(findClass("Foo")); }

// ---------------------------------------------------------------------------
// Lines 20-21: module test
// ---------------------------------------------------------------------------

// The file declares one module, test (A.1.2).
TEST_F(ClassTest26Test, ModuleTestIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);

  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "test");
}

// "module test;" is on line 20 and "endmodule" on line 21.
TEST_F(ClassTest26Test, ModuleTestIsOnLines20To21) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);

  EXPECT_TRUE(endsWith(module->getFile(), kSourceFileName)) << module->getFile();
  EXPECT_EQ(module->getStartLine(), kModuleFirstLine);
  EXPECT_EQ(module->getEndLine(), kModuleLastLine);
}

// test has no ports, parameters or items. The classes and the local
// parameter N are declared outside it (3.12.1).
TEST_F(ClassTest26Test, ModuleTestIsEmpty) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);

  EXPECT_EQ(relationSize(module, kVpiVariables), 0u);
  EXPECT_EQ(relationSize(module, vpiNet), 0u);
  EXPECT_EQ(relationSize(module, vpiParameter), 0u);
  EXPECT_EQ(relationSize(module, vpiTypedef), 0u);
  EXPECT_EQ(relationSize(module, vpiClassDefn), 0u);
}

// ---------------------------------------------------------------------------
// Nothing else is declared
// ---------------------------------------------------------------------------

// The file declares two classes, one parameter and one module. An interface
// class is not an interface: interface_class_declaration and
// interface_declaration are different productions (A.1.2). The file declares
// no package (a compilation-unit package object, vpiUnit TRUE, is not
// counted) and no variable.
TEST_F(ClassTest26Test, NoOtherDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countDeclaredPackagesFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
}

// ---------------------------------------------------------------------------
// Elaboration: the top-level instance
// ---------------------------------------------------------------------------

// test is never instantiated, so it is the design's top-level module: it "is
// implicitly instantiated once, and its instance name is the same as the
// module name" (23.3.1). The instance's definition is test, declared on line
// 20 of this file (vpiDefName, vpiDefFile, vpiDefLineNo, 37.10), and no class
// is in it (3.12.1). The instance only exists after elaboration.
TEST_F(ClassTest26Test, TestIsTheOnlyTopLevelInstance) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    ASSERT_EQ(countFromSourceFile(m_design->getTopModules()), 1u);
    const hldb::Module *const instance = findFromSourceFile(m_design->getTopModules());
    ASSERT_NE(instance, nullptr);

    EXPECT_EQ(instance->getName(), "test");
    EXPECT_EQ(stringProperty(instance, vpiDefName), "test");
    EXPECT_TRUE(endsWith(stringProperty(instance, vpiDefFile), kSourceFileName))
        << stringProperty(instance, vpiDefFile);
    EXPECT_EQ(integerProperty(instance, vpiDefLineNo), static_cast<int64_t>(kModuleFirstLine));
    EXPECT_EQ(relationSize(instance, vpiClassDefn), 0u);
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
