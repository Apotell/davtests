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

// Test for tests/Google/generic/class/class_test_25.sv
//
// THE SOURCE
//
// Lines 1-7 are one-line comments with the license, lines 8-9 are blank and
// lines 10-14 are one block comment carrying the test metadata (tags 6.15 and
// 8.3). The SystemVerilog text is:
//
//   15: package Package;
//   16: <tab>interface class Bar; endclass
//   17: endpackage
//   18:
//   19: class Foo implements Package::Bar; endclass
//   20:
//   21: module test;
//   22: endmodule
//
// Line 22 ends with a line ending. The repository stores the file with LF
// line endings; a Windows checkout (core.autocrlf) turns them into CR LF. 5.3
// lists "newlines" as white space but does not name the carriage return, so
// this test treats CR LF as one line ending and asserts nothing about the CR
// itself. The tab that indents line 16 is white space too (5.3).
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Lines 15-17 are one package_declaration (26.2, A.1.2) named Package. "All
// keywords are defined in lowercase only" (5.6.2) and "Identifiers shall be
//   case sensitive" (5.6), so Package is an identifier, not the keyword
//   package.
//
// Line 16 is the package's one item, an interface_class_declaration (8.26.1,
//   A.1.2):
//
//     interface_class_declaration ::= interface class class_identifier
//         [ parameter_port_list ] [ extends interface_class_type ... ] ;
//         { interface_class_item } endclass [ : class_identifier ]
//
//   Bar has no parameter_port_list, extends nothing and has no item.
//
// Line 19 is a class_declaration (8.3, A.1.2) of Foo with the clause
//   "implements Package::Bar" ("..." marks shortened parts):
//
//     class_declaration ::= [ virtual ] class [ final_specifier ] class_identifier
//         [ parameter_port_list ] [ extends class_type ... ]
//         [ implements interface_class_type { , interface_class_type } ] ;
//         { class_item } endclass [ : class_identifier ]
//     interface_class_type ::= ps_class_identifier [ parameter_value_assignment ]
//     ps_class_identifier ::= [ package_scope ] class_identifier
//
//   (interface_class_type: A.2.2.1; ps_class_identifier: A.9.3.)
//   - "A non-interface class can be declared as implementing one or more
//     interface classes" (8.26). A class "may implement, but not extend, one
//     or more interface classes" and "may not implement a class or virtual
//     class" (8.26.2); Package::Bar names the interface class Bar of
//     Package.
//   - "An interface class shall be declared before it is implemented or
//     extended" (8.26.4): Bar is declared on line 16, before line 19. Package
//     items may be referenced "using the class scope resolution operator" or
//     an import (26.3), and "the compilation of a package is required to
//     precede references to it" (3.12): Package ends on line 17.
//   - Bar declares no method, so Foo has nothing to implement, and its empty
//     body is enough.
//   - Implementing is not extending (8.26.2), so Foo has no base class: "The
//     relation to vpiExtends exists whenever one class is derived from another
//     class (refer to 8.13)" (37.31 detail 6), and Foo derives from no class.
//
// Foo and the module lie outside any other scope, so they belong to the
//   compilation-unit scope, which "contains all declarations that lie outside
//   any other scope" (3.12.1).
//
// Lines 21-22 are one module_declaration (A.1.2) with no ports, parameters or
//   items. test is never instantiated, so it is a top-level module: "A design
//   shall contain at least one top-level module. A top-level module is
//   implicitly instantiated once, and its instance name is the same as the
//   module name" (23.3.1). So even an elaborating run has nothing to report as
//   an error.
//
// No line holds a compiler directive, so lines are numbered from 1 at the
//   start of the file (22.12).
//
// HOW THE TEST OBSERVES IT
//
// The object model has one object for a class declaration, the class defn
// (37.31), with vpiName, vpiVirtual, vpiExtends ("The relation to vpiExtends
// exists whenever one class is derived from another class", detail 6),
// vpiDerivedClasses ("all the class defns derived from the given class defn",
// detail 5), vpiParameter and its member relations. It has no separate object
// for an interface class, so this test reads Bar as a class defn too: an
// interpretation, since 37.31 does not name interface classes. A package is
// an instance (37.10) with vpiName and a class defn relation; the object model
// also represents a compilation unit as a package, with vpiUnit TRUE (37.10),
// although "the compilation-unit scope is not a package" (3.12.1), so only
// packages whose vpiUnit is not TRUE are counted as declared packages. A
// module instance (37.10) has vpiName, vpiDefName, vpiDefFile and
// vpiDefLineNo. HLDB answers vpi_get() and vpi_iterate() through
// getVpiPropertyValue() and getByVpiType(), so the test asks those questions
// directly with the LRM's vpiXXX codes. The location checks use getFile(),
// getStartLine() and getEndLine().
//
// Every expected value comes from the source text and the LRM, so a red test
// means HLC deviates from the LRM.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors                     no fatal, syntax or error
//                                             diagnostic of any category
//   Lines15To17PackageIsAccepted              none on lines 15-17
//   Line19ClassFooIsAccepted                  none on line 19
//   Lines21To22ModuleIsAccepted               none on lines 21-22
//   ReportsNoMisreadingDiagnostics            no syntax, keyword, identifier
//                                             length or non-ASCII diagnostic
//                                             at any severity
//   ReportsNoPackageDiagnostics               no duplicate or undefined
//                                             package
//   ReportsNoClassDiagnostics                 no duplicate or undefined class,
//                                             missing base class or label
//                                             mismatch
//   ReportsNoDesignHierarchyDiagnostics       no duplicate design unit,
//                                             missing definition or missing
//                                             top-level module
//   SourceFileHasNoIncludesOrMacros           one SourceFile, no includes,
//                                             no macro definitions or usages
//   PackagePackageIsDeclared                  exactly one declared package
//                                             from this file, Package, on
//                                             lines 15-17
//   PackageHoldsInterfaceClassBar             Package holds one class defn,
//                                             Bar, on line 16
//   InterfaceClassBarHasNoParametersOrBase    no parameter, no extends
//   InterfaceClassBarHasNoDerivedClass        vpiDerivedClasses is empty:
//                                             Foo implements Bar, it does not
//                                             derive from it
//   InterfaceClassBarHasNoMembers             no variable, method, constraint,
//                                             typedef, named event (array) or
//                                             internal scope
//   ClassFooIsDeclared                        a class defn named Foo from this
//                                             file
//   ClassFooIsOnLine19                        it lies on line 19 only
//   ClassFooExtendsNothing                    vpiVirtual is false; no
//                                             extends, derived class or
//                                             parameter
//   ClassFooHasNoMembers                      as for Bar
//   ModuleTestIsDeclared                      exactly one module from this
//                                             file, named test
//   ModuleTestIsOnLines21To22                 it starts on line 21 and ends on
//                                             line 22
//   ModuleTestIsEmpty                         no variable, net, parameter,
//                                             typedef or class defn in it
//   NoOtherDesignObjectsDeclared              no interface, program, UDP,
//                                             config, checker, parameter or
//                                             variable comes from this file
//   TestIsTheOnlyTopLevelInstance             elaborated runs only: one
//                                             top-level instance, test, of the
//                                             definition test at line 21, with
//                                             no class defn in it
//
// Exactly one check is gated on Design::getElaborated():
// TestIsTheOnlyTopLevelInstance. The top-level instance is created by
// elaboration (23.3.1), so it genuinely exists only after it. It has no else
// branch: before elaboration there is no instance, and the LRM defines no
// value to assert for one. Everything the parser establishes (the package,
// the two classes, the module, the names and the lines) is asserted
// unconditionally.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Which interface classes Foo implements, as the object model sees it.
//     37.31 gives a class defn vpiExtends for its base class but no relation
//     for the interface classes it implements, so the implements clause has no
//     object to read; this test does not invent one. Nearest assertions:
//     ClassFooExtendsNothing (implements is not read as extends) and
//     ReportsNoClassDiagnostics (Package::Bar resolves).
//   - Whether Bar is marked as an interface class. 37.31 defines no property
//     for it. Nearest assertion: PackageHoldsInterfaceClassBar.
//   - A package object for the compilation unit of this file. The object
//     model represents a compilation unit as a package with vpiUnit (37.10);
//     whether HLDB lists one for this file is a tool convention, so only
//     packages with vpiUnit not TRUE are counted. Nearest assertion:
//     PackagePackageIsDeclared.
//   - vpiFullName of the classes ("Package::" or "$unit::" first, 37.10) and
//     of the instance. The full name is a computed property that this suite
//     does not assert. Nearest assertions: PackageHoldsInterfaceClassBar,
//     ClassFooIsDeclared and TestIsTheOnlyTopLevelInstance (vpiName and
//     vpiDefName).
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
constexpr std::string_view kSourceFileName = "class_test_25.sv";

// vpiVariables, the VPI relation from a scope to its variables (100 in IEEE
// 1800-2023 Annex K, vpi_user.h). HLDB's vpi_user.h names the same relation
// vpiVariable.
constexpr int32_t kVpiVariables = vpiVariable;

// The lines of the declarations.
constexpr uint32_t kPackageFirstLine = 15;
constexpr uint32_t kBarLine = 16;
constexpr uint32_t kPackageLastLine = 17;
constexpr uint32_t kFooLine = 19;
constexpr uint32_t kModuleFirstLine = 21;
constexpr uint32_t kModuleLastLine = 22;

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

class ClassTest25Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_test_25.hlc"}); }
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
TEST_F(ClassTest25Test, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

// Lines 15-17: a package_declaration holding one interface_class_declaration
// (26.2, 8.26.1, A.1.2).
TEST_F(ClassTest25Test, Lines15To17PackageIsAccepted) {
  EXPECT_EQ(errorsOnLines(kPackageFirstLine, kPackageLastLine), "");
}

// Line 19: a class_declaration whose implements clause names an interface
// class declared earlier in a package (8.26, 8.26.4, A.2.2.1, A.9.3).
TEST_F(ClassTest25Test, Line19ClassFooIsAccepted) { EXPECT_EQ(errorsOnLines(kFooLine, kFooLine), ""); }

// Lines 21-22: an empty module_declaration (A.1.2).
TEST_F(ClassTest25Test, Lines21To22ModuleIsAccepted) {
  EXPECT_EQ(errorsOnLines(kModuleFirstLine, kModuleLastLine), "");
}

// Each of these diagnostics, at any severity and on any line, would mean HLC
// misread the text of the file.
TEST_F(ClassTest25Test, ReportsNoMisreadingDiagnostics) {
  // Every declaration matches the grammar (A.1.2, A.2.2.1, A.9.3), and the
  // file uses no preprocessor directive.
  EXPECT_EQ(findEvent(EventType::PpSyntaxError), nullptr);
  EXPECT_EQ(findEvent(EventType::PaSyntaxError), nullptr);

  // Package, Bar, Foo and test are no keywords: "All keywords are defined in
  // lowercase only" (5.6.2), so Package is not the keyword package. The
  // longest, Package, has 7 characters, within any limit an implementation
  // may set, which "shall be at least 1024 characters" (5.6).
  EXPECT_EQ(findEvent(EventType::PaReservedKeyword), nullptr);
  EXPECT_EQ(findEvent(EventType::PaMaxLengthIdentifier), nullptr);

  // Every character of the file is ASCII.
  EXPECT_EQ(findEvent(EventType::PpNonAsciiContent), nullptr);
}

// Package is declared once, on lines 15-17, before the class that names it
// on line 19 (3.12, 26.3), so none of these diagnostics applies, at any
// severity.
TEST_F(ClassTest25Test, ReportsNoPackageDiagnostics) {
  EXPECT_EQ(findEvent(EventType::CompMultiplyDefinedPackage), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUndefinedPackage), nullptr);
  EXPECT_EQ(findEvent(EventType::ElabUndefinedPackage), nullptr);
}

// Bar and Foo are each declared once, Package::Bar is declared before Foo
// implements it (8.26.4), Foo has no base class, and neither endclass has a
// label, so none of these diagnostics applies, at any severity.
TEST_F(ClassTest25Test, ReportsNoClassDiagnostics) {
  EXPECT_EQ(findEvent(EventType::CompMultiplyDefinedClass), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUndefinedClass), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUndefinedBaseClass), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUnmatchedLabel), nullptr);
}

// The package, the classes and the module are each declared once, nothing is
// instantiated, and test is a top-level module (23.3.1), so none of these
// diagnostics applies, at any severity.
TEST_F(ClassTest25Test, ReportsNoDesignHierarchyDiagnostics) {
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
TEST_F(ClassTest25Test, SourceFileHasNoIncludesOrMacros) {
  const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
  ASSERT_EQ(sourceFiles.size(), 1u);
  const hldb::SourceFile *const sourceFile = sourceFiles.front();

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// ---------------------------------------------------------------------------
// Lines 15-17: package Package with the interface class Bar
// ---------------------------------------------------------------------------

// The file declares one package, Package (26.2), from "package Package;" on
// line 15 to "endpackage" on line 17. A compilation-unit package object
// (vpiUnit TRUE, 37.10) is not a declared package and is not counted.
TEST_F(ClassTest25Test, PackagePackageIsDeclared) {
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
// class defn (37.10, 37.31); its whole declaration is on line 16.
TEST_F(ClassTest25Test, PackageHoldsInterfaceClassBar) {
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

// "interface class Bar;": no parameter_port_list and no item, so vpiParameter
// is empty (37.31 detail 7); no extends clause, so there is no extends object
// (37.31 detail 6).
TEST_F(ClassTest25Test, InterfaceClassBarHasNoParametersOrBase) {
  const hldb::Any *const bar = findBar();
  ASSERT_NE(bar, nullptr);

  EXPECT_EQ(relationSize(bar, vpiParameter), 0u);
  EXPECT_EQ(relationSize(bar, vpiExtends), 0u);
}

// "The vpiDerivedClasses iterator shall return all the class defns derived
// from the given class defn" (37.31 detail 5), and a class is derived through
// extends (37.31 detail 6, 8.13). Foo implements Bar, which a class "may
// implement, but not extend" (8.26.2), so no class defn derives from Bar.
TEST_F(ClassTest25Test, InterfaceClassBarHasNoDerivedClass) {
  const hldb::Any *const bar = findBar();
  ASSERT_NE(bar, nullptr);
  EXPECT_EQ(relationSize(bar, vpiDerivedClasses), 0u);
}

// Bar has no interface_class_item, so it has no member (8.2).
TEST_F(ClassTest25Test, InterfaceClassBarHasNoMembers) { expectNoMembers(findBar()); }

// ---------------------------------------------------------------------------
// Line 19: class Foo implements Package::Bar
// ---------------------------------------------------------------------------

// Foo is declared in the compilation-unit scope (3.12.1), as a class defn
// (37.31) whose vpiName is its class_identifier.
TEST_F(ClassTest25Test, ClassFooIsDeclared) {
  const hldb::ClassDefn *const foo = findFoo();
  ASSERT_NE(foo, nullptr);
  EXPECT_EQ(foo->getVpiType(), static_cast<uint32_t>(vpiClassDefn));
  EXPECT_EQ(stringProperty(foo, vpiName), "Foo");
}

// The whole declaration of Foo, endclass included, is on line 19.
TEST_F(ClassTest25Test, ClassFooIsOnLine19) {
  const hldb::ClassDefn *const foo = findFoo();
  ASSERT_NE(foo, nullptr);

  EXPECT_TRUE(endsWith(foo->getFile(), kSourceFileName)) << foo->getFile();
  EXPECT_EQ(foo->getStartLine(), kFooLine);
  EXPECT_EQ(foo->getEndLine(), kFooLine);
}

// Foo's header has no virtual and no extends clause; implementing Bar is not
// extending it (8.26.2), so there is no extends object (37.31 detail 6). No
// class derives from Foo (37.31 detail 5), and Foo has no parameter_port_list
// and an empty body (37.31 detail 7).
TEST_F(ClassTest25Test, ClassFooExtendsNothing) {
  const hldb::ClassDefn *const foo = findFoo();
  ASSERT_NE(foo, nullptr);

  EXPECT_EQ(integerProperty(foo, vpiVirtual), 0);
  EXPECT_EQ(relationSize(foo, vpiExtends), 0u);
  EXPECT_EQ(relationSize(foo, vpiDerivedClasses), 0u);
  EXPECT_EQ(relationSize(foo, vpiParameter), 0u);
}

// Foo has no class_item, so it has no member (8.2); Bar has no method for it
// to implement.
TEST_F(ClassTest25Test, ClassFooHasNoMembers) { expectNoMembers(findFoo()); }

// ---------------------------------------------------------------------------
// Lines 21-22: module test
// ---------------------------------------------------------------------------

// The file declares one module, test (A.1.2).
TEST_F(ClassTest25Test, ModuleTestIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);

  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "test");
}

// "module test;" is on line 21 and "endmodule" on line 22.
TEST_F(ClassTest25Test, ModuleTestIsOnLines21To22) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);

  EXPECT_TRUE(endsWith(module->getFile(), kSourceFileName)) << module->getFile();
  EXPECT_EQ(module->getStartLine(), kModuleFirstLine);
  EXPECT_EQ(module->getEndLine(), kModuleLastLine);
}

// test has no ports, parameters or items. The package and the classes are
// declared outside it (3.12.1).
TEST_F(ClassTest25Test, ModuleTestIsEmpty) {
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
// class is a class, not an interface: interface_class_declaration and
// interface_declaration are different productions (A.1.2), so no interface
// comes from this file, and neither does any other design element, parameter
// or variable.
TEST_F(ClassTest25Test, NoOtherDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);

  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParamAssigns()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
}

// ---------------------------------------------------------------------------
// Elaboration: the top-level instance
// ---------------------------------------------------------------------------

// test is never instantiated, so it is the design's top-level module: it "is
// implicitly instantiated once, and its instance name is the same as the
// module name" (23.3.1). The instance's definition is test, declared on line
// 21 of this file (vpiDefName, vpiDefFile, vpiDefLineNo, 37.10), and no class
// is in it (3.12.1). The instance only exists after elaboration.
TEST_F(ClassTest25Test, TestIsTheOnlyTopLevelInstance) {
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
