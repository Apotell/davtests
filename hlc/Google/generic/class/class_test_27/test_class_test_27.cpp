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

// Test for tests/Google/generic/class/class_test_27.sv
//
// THE SOURCE
//
// Lines 1-7 are one-line comments with the license, lines 8-9 are blank and
// lines 10-14 are one block comment carrying the test metadata (tags 6.15 and
// 8.3). Line 15 is blank. The SystemVerilog text is:
//
//   16: package Package;
//   17:   interface class Bar #(parameter A, B); endclass
//   18: endpackage
//   19:
//   20: class Foo implements Package::Bar#(1, 2); endclass
//   21:
//   22: module test;
//   23: endmodule
//
// Line 23 ends with a line ending. The repository stores the file with LF
// line endings; a Windows checkout (core.autocrlf) turns them into CR LF. 5.3
// lists "newlines" as white space but does not name the carriage return, so
// this test treats CR LF as one line ending and asserts nothing about the CR
// itself.
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Lines 16-18 are one package_declaration (26.2, A.1.2) named Package. "All
// keywords are defined in lowercase only" (5.6.2) and "Identifiers shall be
//   case sensitive" (5.6), so Package is an identifier, not the keyword
//   package.
//
// Line 17 is the package's one item, an interface_class_declaration (8.26.1,
//   A.1.2) of Bar with a parameter_port_list:
//
//     interface_class_declaration ::= interface class class_identifier
//         [ parameter_port_list ] [ extends interface_class_type ... ] ;
//         { interface_class_item } endclass [ : class_identifier ]
//     parameter_port_declaration ::= parameter_declaration | ...
//     parameter_declaration ::= parameter data_type_or_implicit
//         list_of_param_assignments
//
//   - "parameter A, B" is one parameter_declaration whose
//     list_of_param_assignments declares two value parameters, A and B,
//     neither with a data type, a range or a value (6.20.1, A.1.3, A.2.1.1).
//   - "It shall be legal to omit the constant_param_expression from a
//     param_assignment or the data_type from a type_assignment only within a
//     parameter_port_list" (Annex A footnote 22): A and B are in one.
//   - "A parameter declaration with no type or range specification shall
//     default to the type and range of the final value assigned to the
//     parameter, after any value overrides have been applied" (6.20.2).
//   - Bar has a parameter port list, so it is a generic class: "A generic
//     class is not a type; only a concrete specialization represents a type"
//     (8.25).
//
// Line 20 is a class_declaration (8.3, A.1.2) of Foo with the clause
//   "implements Package::Bar#(1, 2)" ("..." marks shortened parts):
//
//     class_declaration ::= [ virtual ] class [ final_specifier ] class_identifier
//         [ parameter_port_list ] [ extends class_type ... ]
//         [ implements interface_class_type { , interface_class_type } ] ;
//         { class_item } endclass [ : class_identifier ]
//     interface_class_type ::= ps_class_identifier [ parameter_value_assignment ]
//     ps_class_identifier ::= [ package_scope ] class_identifier
//
//   (interface_class_type: A.2.2.1; ps_class_identifier: A.9.3.)
//   - A class "may implement, but not extend, one or more interface classes"
//     (8.26.2), and "An interface class shall be declared before it is
//     implemented or extended" (8.26.4): Bar is declared on line 17. "the
//     compilation of a package is required to precede references to it"
//     (3.12): Package ends on line 18.
//   - "#(1, 2)" is a parameter_value_assignment by ordered list (23.10.2.1):
//     1 for A and 2 for B, one constant for each of Bar's two parameters.
//   - Bar declares no method, so Foo has nothing to implement.
//   - Implementing is not extending (8.26.2), so Foo has no base class (37.31
//     detail 6).
//
// Lines 22-23 are one module_declaration (A.1.2). test is never instantiated,
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
// classes. A parameter (37.28) has vpiName and vpiLocalParam; "The vpiExpr
// relation of a value parameter shall return the default expr" (detail 3),
// and "If a value parameter does not have an explicitly defined range,
// vpiLeftRange and vpiRightRange shall return a NULL handle" (detail 5). A
// package is an instance (37.10) with vpiName and a class defn relation; the
// object model also represents a compilation unit as a package, with vpiUnit
// TRUE (37.10), although "the compilation-unit scope is not a package"
// (3.12.1), so only packages whose vpiUnit is not TRUE are counted. A module
// instance (37.10) has vpiName, vpiDefName, vpiDefFile and vpiDefLineNo. HLDB
// answers vpi_get() and vpi_iterate() through getVpiPropertyValue() and
// getByVpiType(), so the test asks those questions directly with the LRM's
// vpiXXX codes.
//
// Every expected value comes from the source text and the LRM, so a red test
// means HLC deviates from the LRM.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors                     no fatal, syntax or error
//                                             diagnostic of any category
//   Lines16To18PackageIsAccepted              none on lines 16-18
//   Line20ClassFooIsAccepted                  none on line 20
//   Lines22To23ModuleIsAccepted               none on lines 22-23
//   ReportsNoMisreadingDiagnostics            no syntax, keyword, identifier
//                                             length or non-ASCII diagnostic
//                                             at any severity
//   ReportsNoPackageDiagnostics               no duplicate or undefined
//                                             package
//   ReportsNoClassDiagnostics                 no duplicate or undefined class,
//                                             missing base class or label
//                                             mismatch
//   ReportsNoParameterDiagnostics             no duplicate parameter,
//                                             "localparam without value",
//                                             non-constant value or bad or
//                                             missing override
//   ReportsNoDesignHierarchyDiagnostics       no duplicate design unit,
//                                             missing definition or missing
//                                             top-level module
//   SourceFileHasNoIncludesOrMacros           one SourceFile, no includes,
//                                             no macro definitions or usages
//   PackagePackageIsDeclared                  exactly one declared package
//                                             from this file, Package, on
//                                             lines 16-18
//   PackageHoldsInterfaceClassBar             Package holds one class defn,
//                                             Bar, on line 17
//   InterfaceClassBarHasTwoParameters         vpiParameter holds exactly two
//                                             parameters, A and B
//   BarParameterAHasNoDefault                 A: a port-list value parameter
//                                             with no default and no range,
//                                             on line 17
//   BarParameterBHasNoDefault                 B: the same
//   InterfaceClassBarHasNoBase                no extends
//   InterfaceClassBarHasNoDerivedClass        vpiDerivedClasses is empty:
//                                             Foo implements Bar, it does not
//                                             derive from it
//   InterfaceClassBarHasNoMembers             no variable, method, constraint,
//                                             typedef, named event (array) or
//                                             internal scope
//   ClassFooIsDeclared                        a class defn named Foo from this
//                                             file
//   ClassFooIsOnLine20                        it lies on line 20 only
//   ClassFooExtendsNothing                    vpiVirtual is false; no
//                                             extends, derived class or
//                                             parameter
//   ClassFooHasNoMembers                      as for Bar
//   ModuleTestIsDeclared                      exactly one module from this
//                                             file, named test
//   ModuleTestIsOnLines22To23                 it starts on line 22 and ends on
//                                             line 23
//   ModuleTestIsEmpty                         no variable, net, parameter,
//                                             typedef or class defn in it
//   NoOtherDesignObjectsDeclared              no interface, program, UDP,
//                                             config, checker, compilation-
//                                             unit parameter or variable
//                                             comes from this file
//   TestIsTheOnlyTopLevelInstance             elaborated runs only: one
//                                             top-level instance, test, of the
//                                             definition test at line 22, with
//                                             no class defn in it
//
// Exactly one check is gated on Design::getElaborated():
// TestIsTheOnlyTopLevelInstance. The top-level instance is created by
// elaboration (23.3.1), so it genuinely exists only after it. It has no else
// branch: before elaboration there is no instance, and the LRM defines no
// value to assert for one. Everything the parser establishes (the package,
// the classes, Bar's parameters, the module, the names and the lines) is
// asserted unconditionally.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Which interface classes Foo implements, and the values 1 and 2 that
//     "#(1, 2)" gives A and B. 37.31 gives a class defn vpiExtends for its
//     base class but no relation for the interface classes it implements, so
//     the implements clause and its specialization have no object to read;
//     this test does not invent one. Nearest assertions:
//     ClassFooExtendsNothing (implements is not read as extends) and
//     ReportsNoParameterDiagnostics (the overrides are accepted).
//   - The type and size of A and B. With no type or range, a parameter takes
//     "the type and range of the final value assigned to the parameter, after
//     any value overrides have been applied" (6.20.2); Bar declares no value,
//     and its only specialization is the one the object model does not show.
//     Nearest assertions: BarParameterAHasNoDefault and
//     BarParameterBHasNoDefault (no default, no range).
//   - The order of the vpiParameter iteration. 37.31 does not fix it, so A
//     and B are looked up by name. Nearest assertion:
//     InterfaceClassBarHasTwoParameters.
//   - The Design's parameter assignment list. How HLDB records the override
//     list of a specialization outside the object model is a tool convention.
//     Nearest assertion: ReportsNoParameterDiagnostics.
//   - Whether Bar is marked as an interface class. 37.31 defines no property
//     for it. Nearest assertion: PackageHoldsInterfaceClassBar.
//   - A package object for the compilation unit of this file. The object
//     model represents a compilation unit as a package with vpiUnit (37.10);
//     whether HLDB lists one for this file is a tool convention, so only
//     packages with vpiUnit not TRUE are counted. Nearest assertion:
//     PackagePackageIsDeclared.
//   - vpiFullName of any object. The full name is a computed property that
//     this suite does not assert. Nearest assertions:
//     PackageHoldsInterfaceClassBar, ClassFooIsDeclared and
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
//     and the five ReportsNo... tests.
//   - The comments on lines 1-14 (5.4) and columns. The LRM gives comments no
//     meaning. "The compiler shall maintain the current line number" (22.12);
//     the LRM keeps no column for a comment. Nearest assertions: the line
//     checks, which hold only if lines 1-15 were read as whole lines.

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
constexpr std::string_view kSourceFileName = "class_test_27.sv";

// vpiVariables, the VPI relation from a scope to its variables (100 in IEEE
// 1800-2023 Annex K, vpi_user.h). HLDB's vpi_user.h names the same relation
// vpiVariable.
constexpr int32_t kVpiVariables = vpiVariable;

// The lines of the declarations.
constexpr uint32_t kPackageFirstLine = 16;
constexpr uint32_t kBarLine = 17;
constexpr uint32_t kPackageLastLine = 18;
constexpr uint32_t kFooLine = 20;
constexpr uint32_t kModuleFirstLine = 22;
constexpr uint32_t kModuleLastLine = 23;

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

class ClassTest27Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_test_27.hlc"}); }
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

  // The package Package declared in the file under test, or nullptr.
  static const hldb::Package *findPackage() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllPackages(), "Package");
  }

  // The interface class Bar of the package Package, or nullptr.
  static const hldb::Any *findBar() {
    const hldb::Package *const package = findPackage();
    return (package == nullptr) ? nullptr : findInRelation(package, vpiClassDefn, "Bar");
  }

  // The compilation-unit class Foo declared in the file under test, or
  // nullptr.
  static const hldb::ClassDefn *findFoo() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllClasses(), "Foo");
  }

  // The module declared in the file under test, or nullptr.
  static const hldb::Module *findModule() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllModules());
  }

  // Checks the parameter 'name' of Bar, declared on line 17 in "parameter A,
  // B" with neither a type, a range nor a value.
  static void expectBarParameterWithoutDefault(std::string_view name) {
    const hldb::Any *const bar = findBar();
    ASSERT_NE(bar, nullptr) << "no interface class Bar in Package";
    const hldb::Any *const parameter = findInRelation(bar, vpiParameter, name);
    ASSERT_NE(parameter, nullptr) << "no parameter " << name << " in Bar";

    // A value parameter (37.28) declared in the parameter port list with the
    // keyword parameter, so not a local parameter (6.20.1, 37.31 detail 7).
    EXPECT_EQ(parameter->getVpiType(), static_cast<uint32_t>(vpiParameter));
    EXPECT_EQ(stringProperty(parameter, vpiName), std::string(name));
    EXPECT_EQ(integerProperty(parameter, vpiLocalParam), 0);

    // No "= constant_param_expression" (Annex A footnote 22), so no default
    // expr (37.28 detail 3); no range, so no range bounds (37.28 detail 5).
    EXPECT_EQ(relationSize(parameter, vpiExpr), 0u);
    EXPECT_EQ(relationSize(parameter, vpiLeftRange), 0u);
    EXPECT_EQ(relationSize(parameter, vpiRightRange), 0u);

    EXPECT_TRUE(endsWith(parameter->getFile(), kSourceFileName)) << parameter->getFile();
    EXPECT_EQ(parameter->getStartLine(), kBarLine);
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
TEST_F(ClassTest27Test, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

// Lines 16-18: a package_declaration holding one interface_class_declaration
// whose parameters have no value, as a parameter_port_list allows (26.2,
// 8.26.1, Annex A footnote 22).
TEST_F(ClassTest27Test, Lines16To18PackageIsAccepted) {
  EXPECT_EQ(errorsOnLines(kPackageFirstLine, kPackageLastLine), "");
}

// Line 20: a class_declaration that implements a specialization of an
// interface class declared earlier in a package (8.26, 8.26.4, A.2.2.1).
TEST_F(ClassTest27Test, Line20ClassFooIsAccepted) { EXPECT_EQ(errorsOnLines(kFooLine, kFooLine), ""); }

// Lines 22-23: an empty module_declaration (A.1.2).
TEST_F(ClassTest27Test, Lines22To23ModuleIsAccepted) {
  EXPECT_EQ(errorsOnLines(kModuleFirstLine, kModuleLastLine), "");
}

// Each of these diagnostics, at any severity and on any line, would mean HLC
// misread the text of the file.
TEST_F(ClassTest27Test, ReportsNoMisreadingDiagnostics) {
  // Every declaration matches the grammar (A.1.2, A.1.3, A.2.2.1, A.9.3), and
  // the file uses no preprocessor directive.
  EXPECT_EQ(findEvent(EventType::PpSyntaxError), nullptr);
  EXPECT_EQ(findEvent(EventType::PaSyntaxError), nullptr);

  // Package, Bar, A, B, Foo and test are no keywords: "All keywords are
  // defined in lowercase only" (5.6.2). The longest, Package, has 7
  // characters, within any limit an implementation may set, which "shall be at
  // least 1024 characters" (5.6).
  EXPECT_EQ(findEvent(EventType::PaReservedKeyword), nullptr);
  EXPECT_EQ(findEvent(EventType::PaMaxLengthIdentifier), nullptr);

  // Every character of the file is ASCII.
  EXPECT_EQ(findEvent(EventType::PpNonAsciiContent), nullptr);
}

// Package is declared once, on lines 16-18, before the class that names it
// on line 20 (3.12, 26.3), so none of these diagnostics applies, at any
// severity.
TEST_F(ClassTest27Test, ReportsNoPackageDiagnostics) {
  EXPECT_EQ(findEvent(EventType::CompMultiplyDefinedPackage), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUndefinedPackage), nullptr);
  EXPECT_EQ(findEvent(EventType::ElabUndefinedPackage), nullptr);
}

// Bar and Foo are each declared once, Package::Bar is declared before Foo
// implements it (8.26.4), Foo has no base class, and neither endclass has a
// label, so none of these diagnostics applies, at any severity.
TEST_F(ClassTest27Test, ReportsNoClassDiagnostics) {
  EXPECT_EQ(findEvent(EventType::CompMultiplyDefinedClass), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUndefinedClass), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUndefinedBaseClass), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUnmatchedLabel), nullptr);
}

// A and B are distinct names; they are parameters, not local parameters, so
// they may have no value in a parameter_port_list (Annex A footnote 22); and
// "#(1, 2)" gives each of Bar's two parameters one constant by ordered list
// (23.10.2.1). So none of these diagnostics applies, at any severity.
TEST_F(ClassTest27Test, ReportsNoParameterDiagnostics) {
  EXPECT_EQ(findEvent(EventType::CompMultiplyDefinedParameter), nullptr);
  EXPECT_EQ(findEvent(EventType::CompLocalparamNoValue), nullptr);
  EXPECT_EQ(findEvent(EventType::CompNotAConstantExpression), nullptr);
  EXPECT_EQ(findEvent(EventType::ElabUnknownParameterOverride), nullptr);
  EXPECT_EQ(findEvent(EventType::ElabMissingParameterOverride), nullptr);
}

// The package, the classes and the module are each declared once, nothing is
// instantiated, and test is a top-level module (23.3.1), so none of these
// diagnostics applies, at any severity.
TEST_F(ClassTest27Test, ReportsNoDesignHierarchyDiagnostics) {
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
TEST_F(ClassTest27Test, SourceFileHasNoIncludesOrMacros) {
  const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
  ASSERT_EQ(sourceFiles.size(), 1u);
  const hldb::SourceFile *const sourceFile = sourceFiles.front();

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// ---------------------------------------------------------------------------
// Lines 16-18: package Package with the interface class Bar
// ---------------------------------------------------------------------------

// The file declares one package, Package (26.2), from "package Package;" on
// line 16 to "endpackage" on line 18. A compilation-unit package object
// (vpiUnit TRUE, 37.10) is not a declared package and is not counted.
TEST_F(ClassTest27Test, PackagePackageIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countDeclaredPackagesFromSourceFile(m_design->getAllPackages()), 1u);

  const hldb::Package *const package = findPackage();
  ASSERT_NE(package, nullptr);
  EXPECT_EQ(package->getVpiType(), static_cast<uint32_t>(vpiPackage));
  EXPECT_EQ(stringProperty(package, vpiName), "Package");
  EXPECT_NE(integerProperty(package, vpiUnit), 1);
  EXPECT_EQ(package->getStartLine(), kPackageFirstLine);
  EXPECT_EQ(package->getEndLine(), kPackageLastLine);
}

// The package's one item is the interface class Bar, which it holds as a
// class defn (37.10, 37.31); its whole declaration is on line 17.
TEST_F(ClassTest27Test, PackageHoldsInterfaceClassBar) {
  const hldb::Package *const package = findPackage();
  ASSERT_NE(package, nullptr);
  EXPECT_EQ(relationSize(package, vpiClassDefn), 1u);

  const hldb::Any *const bar = findBar();
  ASSERT_NE(bar, nullptr);
  EXPECT_EQ(bar->getVpiType(), static_cast<uint32_t>(vpiClassDefn));
  EXPECT_EQ(stringProperty(bar, vpiName), "Bar");
  EXPECT_TRUE(endsWith(bar->getFile(), kSourceFileName)) << bar->getFile();
  EXPECT_EQ(bar->getStartLine(), kBarLine);
  EXPECT_EQ(bar->getEndLine(), kBarLine);
}

// "The vpiParameter iteration shall return both the parameters declared in the
// parameter port list of the class declaration and the parameters declared
// within the body of the class declaration as class items" (37.31 detail 7):
// A and B from the port list, and none from the empty body.
TEST_F(ClassTest27Test, InterfaceClassBarHasTwoParameters) {
  const hldb::Any *const bar = findBar();
  ASSERT_NE(bar, nullptr);

  EXPECT_EQ(relationSize(bar, vpiParameter), 2u);
  EXPECT_NE(findInRelation(bar, vpiParameter, "A"), nullptr);
  EXPECT_NE(findInRelation(bar, vpiParameter, "B"), nullptr);
}

// "parameter A": a value parameter of the port list without a value (6.20.1,
// Annex A footnote 22).
TEST_F(ClassTest27Test, BarParameterAHasNoDefault) { expectBarParameterWithoutDefault("A"); }

// "B" in "parameter A, B": the same declaration, so the same kind (6.20.1).
TEST_F(ClassTest27Test, BarParameterBHasNoDefault) { expectBarParameterWithoutDefault("B"); }

// "interface class Bar #(...);" has no extends clause, so there is no extends
// object (37.31 detail 6).
TEST_F(ClassTest27Test, InterfaceClassBarHasNoBase) {
  const hldb::Any *const bar = findBar();
  ASSERT_NE(bar, nullptr);
  EXPECT_EQ(relationSize(bar, vpiExtends), 0u);
}

// "The vpiDerivedClasses iterator shall return all the class defns derived
// from the given class defn" (37.31 detail 5), and a class is derived through
// extends (37.31 detail 6, 8.13). Foo implements Bar, which a class "may
// implement, but not extend" (8.26.2), so no class defn derives from Bar.
TEST_F(ClassTest27Test, InterfaceClassBarHasNoDerivedClass) {
  const hldb::Any *const bar = findBar();
  ASSERT_NE(bar, nullptr);
  EXPECT_EQ(relationSize(bar, vpiDerivedClasses), 0u);
}

// Bar has no interface_class_item, so it has no member (8.2).
TEST_F(ClassTest27Test, InterfaceClassBarHasNoMembers) { expectNoMembers(findBar()); }

// ---------------------------------------------------------------------------
// Line 20: class Foo implements Package::Bar#(1, 2)
// ---------------------------------------------------------------------------

// Foo is declared in the compilation-unit scope (3.12.1), as a class defn
// (37.31) whose vpiName is its class_identifier.
TEST_F(ClassTest27Test, ClassFooIsDeclared) {
  const hldb::ClassDefn *const foo = findFoo();
  ASSERT_NE(foo, nullptr);
  EXPECT_EQ(foo->getVpiType(), static_cast<uint32_t>(vpiClassDefn));
  EXPECT_EQ(stringProperty(foo, vpiName), "Foo");
}

// The whole declaration of Foo, endclass included, is on line 20.
TEST_F(ClassTest27Test, ClassFooIsOnLine20) {
  const hldb::ClassDefn *const foo = findFoo();
  ASSERT_NE(foo, nullptr);

  EXPECT_TRUE(endsWith(foo->getFile(), kSourceFileName)) << foo->getFile();
  EXPECT_EQ(foo->getStartLine(), kFooLine);
  EXPECT_EQ(foo->getEndLine(), kFooLine);
}

// Foo's header has no virtual and no extends clause; implementing Bar is not
// extending it (8.26.2), so there is no extends object (37.31 detail 6). No
// class derives from Foo (37.31 detail 5). Foo has no parameter_port_list and
// an empty body; the parameters that "#(1, 2)" overrides are Bar's, not Foo's
// (37.31 detail 7).
TEST_F(ClassTest27Test, ClassFooExtendsNothing) {
  const hldb::ClassDefn *const foo = findFoo();
  ASSERT_NE(foo, nullptr);

  EXPECT_EQ(integerProperty(foo, vpiVirtual), 0);
  EXPECT_EQ(relationSize(foo, vpiExtends), 0u);
  EXPECT_EQ(relationSize(foo, vpiDerivedClasses), 0u);
  EXPECT_EQ(relationSize(foo, vpiParameter), 0u);
}

// Foo has no class_item, so it has no member (8.2); Bar has no method for it
// to implement.
TEST_F(ClassTest27Test, ClassFooHasNoMembers) { expectNoMembers(findFoo()); }

// ---------------------------------------------------------------------------
// Lines 22-23: module test
// ---------------------------------------------------------------------------

// The file declares one module, test (A.1.2).
TEST_F(ClassTest27Test, ModuleTestIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);

  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "test");
}

// "module test;" is on line 22 and "endmodule" on line 23.
TEST_F(ClassTest27Test, ModuleTestIsOnLines22To23) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);

  EXPECT_TRUE(endsWith(module->getFile(), kSourceFileName)) << module->getFile();
  EXPECT_EQ(module->getStartLine(), kModuleFirstLine);
  EXPECT_EQ(module->getEndLine(), kModuleLastLine);
}

// test has no ports, parameters or items. The package and the classes are
// declared outside it (3.12.1).
TEST_F(ClassTest27Test, ModuleTestIsEmpty) {
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

// The file declares one package, two classes and one module. An interface
// class is not an interface: interface_class_declaration and
// interface_declaration are different productions (A.1.2). A and B belong to
// Bar, so no parameter is declared in the compilation-unit scope, and no
// variable is declared anywhere.
TEST_F(ClassTest27Test, NoOtherDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);

  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
}

// ---------------------------------------------------------------------------
// Elaboration: the top-level instance
// ---------------------------------------------------------------------------

// test is never instantiated, so it is the design's top-level module: it "is
// implicitly instantiated once, and its instance name is the same as the
// module name" (23.3.1). The instance's definition is test, declared on line
// 22 of this file (vpiDefName, vpiDefFile, vpiDefLineNo, 37.10), and no class
// is in it (3.12.1). The instance only exists after elaboration.
TEST_F(ClassTest27Test, TestIsTheOnlyTopLevelInstance) {
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
