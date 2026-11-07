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

// Test for tests/Google/generic/class/class_test_29.sv
//
// THE SOURCE
//
// Lines 1-7 are one-line comments with the license, lines 8-9 are blank and
// lines 10-14 are one block comment carrying the test metadata (tags 6.15 and
// 8.3). Line 15 is blank. The SystemVerilog text is:
//
//   16: package Pkg;
//   17:   interface class Bar; endclass
//   18: endpackage
//   19:
//   20: class Base; endclass
//   21: interface class Baz; endclass
//   22:
//   23: class Foo extends Base implements Pkg::Bar, Baz; endclass
//   24:
//   25: module test;
//   26: endmodule
//
// Line 26 ends with a line ending. The repository stores the file with LF
// line endings; a Windows checkout (core.autocrlf) turns them into CR LF. 5.3
// lists "newlines" as white space but does not name the carriage return, so
// this test treats CR LF as one line ending and asserts nothing about the CR
// itself.
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Lines 16-18 are one package_declaration (26.2, A.1.2) named Pkg, whose one
//   item is the interface_class_declaration (8.26.1, A.1.2) of Bar on line 17.
//
// Line 20 is a class_declaration (8.3, A.1.2) of Base with no parameter,
//   extends, implements or item. Line 21 is an interface_class_declaration of
//   Baz with no parameter, extends or item.
//
// Line 23 is a class_declaration of Foo with both clauses ("..." marks
//   shortened parts):
//
//     class_declaration ::= [ virtual ] class [ final_specifier ] class_identifier
//         [ parameter_port_list ] [ extends class_type ... ]
//         [ implements interface_class_type { , interface_class_type } ] ;
//         { class_item } endclass [ : class_identifier ]
//     interface_class_type ::= ps_class_identifier [ parameter_value_assignment ]
//     ps_class_identifier ::= [ package_scope ] class_identifier
//
//   (interface_class_type: A.2.2.1; ps_class_identifier: A.9.3.)
//   - A class or virtual class "may extend at most one other class or virtual
//     class" and "may simultaneously extend a class and implement interface
//     classes" (8.26.2): Base is a class; Pkg::Bar and Baz are interface
//     classes, the second named through the "{ , interface_class_type }"
//     repetition.
//   - "An interface class shall be declared before it is implemented or
//     extended" (8.26.4): Bar is declared on line 17 and Baz on line 21.
//     "the compilation of a package is required to precede references to it"
//     (3.12): Pkg ends on line 18. Base is declared on line 20, and "The
//     declaration of a user-defined data type shall precede any reference to
//     its type_identifier" (6.18).
//   - Bar and Baz declare no method, so Foo has nothing to implement.
//   - Foo derives from Base (8.13). Implementing Bar and Baz is not extending
//     them: a class "may implement, but not extend, one or more interface
//     classes" (8.26.2).
//
// Base, Baz, Foo and the module lie outside any other scope, so they belong
//   to the compilation-unit scope, which "contains all declarations that lie
//   outside any other scope" (3.12.1).
//
// Lines 25-26 are one module_declaration (A.1.2). test is never instantiated,
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
// (37.31), with vpiName, vpiVirtual, vpiParameter, vpiExtends ("The relation
// to vpiExtends exists whenever one class is derived from another class
// (refer to 8.13). The relation from extends to class typespec provides the
// base class. The vpiArgument iterator from extends shall provide the
// arguments used in constructor chaining", detail 6), vpiDerivedClasses ("The
// vpiDerivedClasses iterator shall return all the class defns derived from
// the given class defn", detail 5) and its member relations. A class typespec
// obtained from the extends object leads to its class defn (37.32). The
// object model has no separate object for an interface class, so this test
// reads Bar and Baz as class defns too: an interpretation, since 37.31 does
// not name interface classes. A package is an instance (37.10) with vpiName
// and a class defn relation; the object model also represents a compilation
// unit as a package, with vpiUnit TRUE (37.10), although "the
// compilation-unit scope is not a package" (3.12.1), so only packages whose
// vpiUnit is not TRUE are counted. A module instance (37.10) has vpiName,
// vpiDefName, vpiDefFile and vpiDefLineNo. HLDB answers vpi_get() and
// vpi_iterate() through getVpiPropertyValue() and getByVpiType(), so the test
// asks those questions directly with the LRM's vpiXXX codes.
//
// Every expected value comes from the source text and the LRM, so a red test
// means HLC deviates from the LRM.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors                     no fatal, syntax or error
//                                             diagnostic of any category
//   Lines16To18PackageIsAccepted              none on lines 16-18
//   Line20ClassBaseIsAccepted                 none on line 20
//   Line21InterfaceClassBazIsAccepted         none on line 21
//   Line23ClassFooIsAccepted                  none on line 23
//   Lines25To26ModuleIsAccepted               none on lines 25-26
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
//   PackagePkgIsDeclared                      exactly one declared package
//                                             from this file, Pkg, on lines
//                                             16-18
//   PackageHoldsInterfaceClassBar             Pkg holds one class defn, Bar,
//                                             on line 17
//   InterfaceClassBarHasNoParametersOrBase    no parameter, no extends
//   InterfaceClassBarHasNoDerivedClass        vpiDerivedClasses is empty
//   InterfaceClassBarHasNoMembers             no variable, method, constraint,
//                                             typedef, named event (array) or
//                                             internal scope
//   ClassBaseIsDeclared                       a class defn named Base from
//                                             this file, on line 20
//   ClassBaseHasNoParametersOrBase            vpiVirtual is false; no
//                                             parameter, no extends
//   ClassBaseIsTheBaseOfFoo                   vpiDerivedClasses holds Foo,
//                                             and only Foo
//   ClassBaseHasNoMembers                     as for Bar
//   InterfaceClassBazIsDeclared               a class defn named Baz from this
//                                             file, on line 21
//   InterfaceClassBazHasNoParametersOrBase    no parameter, no extends
//   InterfaceClassBazHasNoDerivedClass        vpiDerivedClasses is empty
//   InterfaceClassBazHasNoMembers             as for Bar
//   ClassFooIsDeclared                        a class defn named Foo from this
//                                             file
//   ClassFooIsOnLine23                        it lies on line 23 only
//   ClassFooExtendsBase                       one extends object with no
//                                             constructor argument; its class
//                                             typespec is of the class defn
//                                             Base of line 20
//   ClassFooIsNotVirtualAndHasNoDerivedClass  vpiVirtual is false; no derived
//                                             class, no parameter
//   ClassFooHasNoMembers                      as for Bar
//   ModuleTestIsDeclared                      exactly one module from this
//                                             file, named test
//   ModuleTestIsOnLines25To26                 it starts on line 25 and ends on
//                                             line 26
//   ModuleTestIsEmpty                         no variable, net, parameter,
//                                             typedef or class defn in it
//   NoOtherDesignObjectsDeclared              no interface, program, UDP,
//                                             config, checker, parameter or
//                                             variable comes from this file
//   TestIsTheOnlyTopLevelInstance             elaborated runs only: one
//                                             top-level instance, test, of the
//                                             definition test at line 25, with
//                                             no class defn in it
//
// Exactly one check is gated on Design::getElaborated():
// TestIsTheOnlyTopLevelInstance. The top-level instance is created by
// elaboration (23.3.1), so it genuinely exists only after it. It has no else
// branch: before elaboration there is no instance, and the LRM defines no
// value to assert for one. Everything the parser establishes (the package,
// the four classes, Foo's base class, the module, the names and the lines) is
// asserted unconditionally.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Which interface classes Foo implements, as the object model sees it.
//     37.31 gives a class defn vpiExtends for its base class but no relation
//     for the interface classes it implements, so the implements clause has no
//     object to read; this test does not invent one. Nearest assertions:
//     ClassFooExtendsBase (exactly one extends object, for Base),
//     InterfaceClassBarHasNoDerivedClass and
//     InterfaceClassBazHasNoDerivedClass.
//   - Whether Bar and Baz are marked as interface classes. 37.31 defines no
//     property for it. Nearest assertions: PackageHoldsInterfaceClassBar and
//     InterfaceClassBazIsDeclared.
//   - The object HLDB returns for the class typespec relation of the extends
//     object (37.31 detail 6). The LRM draws it as a direct relation to a
//     typespec; HLDB may instead return an object of its own type that refers
//     to the typespec. That indirection is an HLDB model convention the LRM
//     does not define, so its type is not asserted; the test follows it
//     through vpiActual to the typespec it refers to. Nearest assertion:
//     ClassFooExtendsBase (vpiClassTypespec on that typespec).
//   - The implicit constructors. A class without an explicit new gets one, but
//     "the iteration over vpiMethods shall not include built-in methods for
//     which there is no explicit declaration" (37.31 detail 1). Nearest
//     assertion: ClassFooHasNoMembers (zero vpiMethods).
//   - A package object for the compilation unit of this file. The object
//     model represents a compilation unit as a package with vpiUnit (37.10);
//     whether HLDB lists one for this file is a tool convention, so only
//     packages with vpiUnit not TRUE are counted. Nearest assertion:
//     PackagePkgIsDeclared.
//   - vpiFullName of any object. The full name is a computed property that
//     this suite does not assert. Nearest assertions: the ...IsDeclared tests
//     and TestIsTheOnlyTopLevelInstance (vpiName and vpiDefName).
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
constexpr std::string_view kSourceFileName = "class_test_29.sv";

// vpiVariables, the VPI relation from a scope to its variables (100 in IEEE
// 1800-2023 Annex K, vpi_user.h). HLDB's vpi_user.h names the same relation
// vpiVariable.
constexpr int32_t kVpiVariables = vpiVariable;

// The lines of the declarations.
constexpr uint32_t kPackageFirstLine = 16;
constexpr uint32_t kBarLine = 17;
constexpr uint32_t kPackageLastLine = 18;
constexpr uint32_t kBaseLine = 20;
constexpr uint32_t kBazLine = 21;
constexpr uint32_t kFooLine = 23;
constexpr uint32_t kModuleFirstLine = 25;
constexpr uint32_t kModuleLastLine = 26;

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

class ClassTest29Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_test_29.hlc"}); }
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

  // The package Pkg declared in the file under test, or nullptr.
  static const hldb::Package *findPackage() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllPackages(), "Pkg");
  }

  // The interface class Bar of the package Pkg, or nullptr.
  static const hldb::Any *findBar() {
    const hldb::Package *const package = findPackage();
    return (package == nullptr) ? nullptr : findInRelation(package, vpiClassDefn, "Bar");
  }

  // The compilation-unit class named 'name' declared in the file under test,
  // or nullptr.
  static const hldb::ClassDefn *findClass(std::string_view name) {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllClasses(), name);
  }

  // The module declared in the file under test, or nullptr.
  static const hldb::Module *findModule() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllModules());
  }

  // Checks that 'classDefn' is the class defn named 'name' (37.31) and that
  // its whole declaration is on 'line'.
  static void expectClassOnLine(const hldb::Any *classDefn, std::string_view name, uint32_t line) {
    ASSERT_NE(classDefn, nullptr) << "no class " << name;

    EXPECT_EQ(classDefn->getVpiType(), static_cast<uint32_t>(vpiClassDefn));
    EXPECT_EQ(stringProperty(classDefn, vpiName), std::string(name));
    EXPECT_TRUE(endsWith(classDefn->getFile(), kSourceFileName)) << classDefn->getFile();
    EXPECT_EQ(classDefn->getStartLine(), line);
    EXPECT_EQ(classDefn->getEndLine(), line);
  }

  // Checks that 'classDefn' has no parameter_port_list, no parameter item and
  // no extends clause (37.31 details 6 and 7).
  static void expectNoParametersOrBase(const hldb::Any *classDefn) {
    ASSERT_NE(classDefn, nullptr);

    EXPECT_EQ(relationSize(classDefn, vpiParameter), 0u);
    EXPECT_EQ(relationSize(classDefn, vpiExtends), 0u);
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
TEST_F(ClassTest29Test, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

// Lines 16-18: a package_declaration holding one interface_class_declaration
// (26.2, 8.26.1, A.1.2).
TEST_F(ClassTest29Test, Lines16To18PackageIsAccepted) {
  EXPECT_EQ(errorsOnLines(kPackageFirstLine, kPackageLastLine), "");
}

// Line 20: an empty class_declaration (8.3, A.1.2).
TEST_F(ClassTest29Test, Line20ClassBaseIsAccepted) { EXPECT_EQ(errorsOnLines(kBaseLine, kBaseLine), ""); }

// Line 21: an empty interface_class_declaration (8.26.1, A.1.2).
TEST_F(ClassTest29Test, Line21InterfaceClassBazIsAccepted) { EXPECT_EQ(errorsOnLines(kBazLine, kBazLine), ""); }

// Line 23: a class_declaration that extends a class and implements two
// interface classes at once, as 8.26.2 allows.
TEST_F(ClassTest29Test, Line23ClassFooIsAccepted) { EXPECT_EQ(errorsOnLines(kFooLine, kFooLine), ""); }

// Lines 25-26: an empty module_declaration (A.1.2).
TEST_F(ClassTest29Test, Lines25To26ModuleIsAccepted) {
  EXPECT_EQ(errorsOnLines(kModuleFirstLine, kModuleLastLine), "");
}

// Each of these diagnostics, at any severity and on any line, would mean HLC
// misread the text of the file.
TEST_F(ClassTest29Test, ReportsNoMisreadingDiagnostics) {
  // Every declaration matches the grammar (A.1.2, A.2.2.1, A.9.3), and the
  // file uses no preprocessor directive.
  EXPECT_EQ(findEvent(EventType::PpSyntaxError), nullptr);
  EXPECT_EQ(findEvent(EventType::PaSyntaxError), nullptr);

  // Pkg, Bar, Base, Baz, Foo and test are no keywords (5.6.2, Table B.1), and
  // the longest have 4 characters, within any limit an implementation may
  // set, which "shall be at least 1024 characters" (5.6).
  EXPECT_EQ(findEvent(EventType::PaReservedKeyword), nullptr);
  EXPECT_EQ(findEvent(EventType::PaMaxLengthIdentifier), nullptr);

  // Every character of the file is ASCII.
  EXPECT_EQ(findEvent(EventType::PpNonAsciiContent), nullptr);
}

// Pkg is declared once, on lines 16-18, before the class that names it on
// line 23 (3.12, 26.3), so none of these diagnostics applies, at any
// severity.
TEST_F(ClassTest29Test, ReportsNoPackageDiagnostics) {
  EXPECT_EQ(findEvent(EventType::CompMultiplyDefinedPackage), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUndefinedPackage), nullptr);
  EXPECT_EQ(findEvent(EventType::ElabUndefinedPackage), nullptr);
}

// The four classes are each declared once, Base, Pkg::Bar and Baz are
// declared before Foo names them (6.18, 8.26.4), and no endclass has a label,
// so none of these diagnostics applies, at any severity.
TEST_F(ClassTest29Test, ReportsNoClassDiagnostics) {
  EXPECT_EQ(findEvent(EventType::CompMultiplyDefinedClass), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUndefinedClass), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUndefinedBaseClass), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUnmatchedLabel), nullptr);
}

// The package, the classes and the module are each declared once, nothing is
// instantiated, and test is a top-level module (23.3.1), so none of these
// diagnostics applies, at any severity.
TEST_F(ClassTest29Test, ReportsNoDesignHierarchyDiagnostics) {
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
TEST_F(ClassTest29Test, SourceFileHasNoIncludesOrMacros) {
  const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
  ASSERT_EQ(sourceFiles.size(), 1u);
  const hldb::SourceFile *const sourceFile = sourceFiles.front();

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// ---------------------------------------------------------------------------
// Lines 16-18: package Pkg with the interface class Bar
// ---------------------------------------------------------------------------

// The file declares one package, Pkg (26.2), from "package Pkg;" on line 16
// to "endpackage" on line 18. A compilation-unit package object (vpiUnit
// TRUE, 37.10) is not a declared package and is not counted.
TEST_F(ClassTest29Test, PackagePkgIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countDeclaredPackagesFromSourceFile(m_design->getAllPackages()), 1u);

  const hldb::Package *const package = findPackage();
  ASSERT_NE(package, nullptr);
  EXPECT_EQ(package->getVpiType(), static_cast<uint32_t>(vpiPackage));
  EXPECT_EQ(stringProperty(package, vpiName), "Pkg");
  EXPECT_NE(integerProperty(package, vpiUnit), 1);
  EXPECT_EQ(package->getStartLine(), kPackageFirstLine);
  EXPECT_EQ(package->getEndLine(), kPackageLastLine);
}

// The package's one item is the interface class Bar, which it holds as a
// class defn (37.10, 37.31); its whole declaration is on line 17.
TEST_F(ClassTest29Test, PackageHoldsInterfaceClassBar) {
  const hldb::Package *const package = findPackage();
  ASSERT_NE(package, nullptr);
  EXPECT_EQ(relationSize(package, vpiClassDefn), 1u);

  expectClassOnLine(findBar(), "Bar", kBarLine);
}

// "interface class Bar;": no parameter_port_list and no item, no extends
// clause.
TEST_F(ClassTest29Test, InterfaceClassBarHasNoParametersOrBase) { expectNoParametersOrBase(findBar()); }

// A class is derived through extends (37.31 detail 6, 8.13). Foo implements
// Bar, which a class "may implement, but not extend" (8.26.2), so no class
// defn derives from Bar (37.31 detail 5).
TEST_F(ClassTest29Test, InterfaceClassBarHasNoDerivedClass) {
  const hldb::Any *const bar = findBar();
  ASSERT_NE(bar, nullptr);
  EXPECT_EQ(relationSize(bar, vpiDerivedClasses), 0u);
}

// Bar has no interface_class_item, so it has no member (8.2).
TEST_F(ClassTest29Test, InterfaceClassBarHasNoMembers) { expectNoMembers(findBar()); }

// ---------------------------------------------------------------------------
// Line 20: class Base
// ---------------------------------------------------------------------------

// Base is declared in the compilation-unit scope (3.12.1), as a class defn
// (37.31), and its whole declaration is on line 20.
TEST_F(ClassTest29Test, ClassBaseIsDeclared) { expectClassOnLine(findClass("Base"), "Base", kBaseLine); }

// "class Base;": no virtual, no parameter_port_list and no item, no extends
// clause.
TEST_F(ClassTest29Test, ClassBaseHasNoParametersOrBase) {
  const hldb::ClassDefn *const base = findClass("Base");
  ASSERT_NE(base, nullptr);

  EXPECT_EQ(integerProperty(base, vpiVirtual), 0);
  expectNoParametersOrBase(base);
}

// "The vpiDerivedClasses iterator shall return all the class defns derived
// from the given class defn" (37.31 detail 5): Foo, and only Foo.
TEST_F(ClassTest29Test, ClassBaseIsTheBaseOfFoo) {
  const hldb::ClassDefn *const base = findClass("Base");
  ASSERT_NE(base, nullptr);

  EXPECT_EQ(relationSize(base, vpiDerivedClasses), 1u);
  EXPECT_NE(findInRelation(base, vpiDerivedClasses, "Foo"), nullptr);
}

// Base has no class_item, so it has no member (8.2).
TEST_F(ClassTest29Test, ClassBaseHasNoMembers) { expectNoMembers(findClass("Base")); }

// ---------------------------------------------------------------------------
// Line 21: interface class Baz
// ---------------------------------------------------------------------------

// Baz is declared in the compilation-unit scope (3.12.1), read as a class
// defn (37.31), and its whole declaration is on line 21.
TEST_F(ClassTest29Test, InterfaceClassBazIsDeclared) { expectClassOnLine(findClass("Baz"), "Baz", kBazLine); }

// "interface class Baz;": no parameter_port_list and no item, no extends
// clause.
TEST_F(ClassTest29Test, InterfaceClassBazHasNoParametersOrBase) { expectNoParametersOrBase(findClass("Baz")); }

// Foo implements Baz, which a class "may implement, but not extend" (8.26.2),
// so no class defn derives from Baz (37.31 details 5 and 6).
TEST_F(ClassTest29Test, InterfaceClassBazHasNoDerivedClass) {
  const hldb::ClassDefn *const baz = findClass("Baz");
  ASSERT_NE(baz, nullptr);
  EXPECT_EQ(relationSize(baz, vpiDerivedClasses), 0u);
}

// Baz has no interface_class_item, so it has no member (8.2).
TEST_F(ClassTest29Test, InterfaceClassBazHasNoMembers) { expectNoMembers(findClass("Baz")); }

// ---------------------------------------------------------------------------
// Line 23: class Foo extends Base implements Pkg::Bar, Baz
// ---------------------------------------------------------------------------

// Foo is declared in the compilation-unit scope (3.12.1), as a class defn
// (37.31) whose vpiName is its class_identifier.
TEST_F(ClassTest29Test, ClassFooIsDeclared) {
  const hldb::ClassDefn *const foo = findClass("Foo");
  ASSERT_NE(foo, nullptr);
  EXPECT_EQ(foo->getVpiType(), static_cast<uint32_t>(vpiClassDefn));
  EXPECT_EQ(stringProperty(foo, vpiName), "Foo");
}

// The whole declaration of Foo, endclass included, is on line 23.
TEST_F(ClassTest29Test, ClassFooIsOnLine23) {
  const hldb::ClassDefn *const foo = findClass("Foo");
  ASSERT_NE(foo, nullptr);

  EXPECT_TRUE(endsWith(foo->getFile(), kSourceFileName)) << foo->getFile();
  EXPECT_EQ(foo->getStartLine(), kFooLine);
  EXPECT_EQ(foo->getEndLine(), kFooLine);
}

// Foo derives from Base, so it has one extends object (37.31 detail 6); Bar
// and Baz are implemented, not extended (8.26.2), so they add none. No
// argument list follows the base class type, so the extends object has no
// constructor argument, and its class typespec is of the class defn Base of
// line 20.
TEST_F(ClassTest29Test, ClassFooExtendsBase) {
  const hldb::ClassDefn *const foo = findClass("Foo");
  ASSERT_NE(foo, nullptr);
  EXPECT_EQ(relationSize(foo, vpiExtends), 1u);
  const hldb::Any *const extendsObject = relationObject(foo, vpiExtends);
  ASSERT_NE(extendsObject, nullptr);
  EXPECT_EQ(relationSize(extendsObject, vpiArgument), 0u);

  const hldb::Any *const typespec = typespecOf(extendsObject, vpiClassTypespec);
  ASSERT_NE(typespec, nullptr);
  EXPECT_EQ(typespec->getVpiType(), static_cast<uint32_t>(vpiClassTypespec));
  const hldb::Any *const base = relationObject(typespec, vpiClassDefn);
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->getName(), "Base");
  EXPECT_EQ(base->getStartLine(), kBaseLine);
}

// Foo's header has no virtual, no class derives from Foo (37.31 detail 5),
// and Foo has no parameter_port_list and an empty body (37.31 detail 7).
TEST_F(ClassTest29Test, ClassFooIsNotVirtualAndHasNoDerivedClass) {
  const hldb::ClassDefn *const foo = findClass("Foo");
  ASSERT_NE(foo, nullptr);

  EXPECT_EQ(integerProperty(foo, vpiVirtual), 0);
  EXPECT_EQ(relationSize(foo, vpiDerivedClasses), 0u);
  EXPECT_EQ(relationSize(foo, vpiParameter), 0u);
}

// Foo has no class_item, so it has no member (8.2); Bar and Baz have no
// method for it to implement.
TEST_F(ClassTest29Test, ClassFooHasNoMembers) { expectNoMembers(findClass("Foo")); }

// ---------------------------------------------------------------------------
// Lines 25-26: module test
// ---------------------------------------------------------------------------

// The file declares one module, test (A.1.2).
TEST_F(ClassTest29Test, ModuleTestIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);

  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "test");
}

// "module test;" is on line 25 and "endmodule" on line 26.
TEST_F(ClassTest29Test, ModuleTestIsOnLines25To26) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);

  EXPECT_TRUE(endsWith(module->getFile(), kSourceFileName)) << module->getFile();
  EXPECT_EQ(module->getStartLine(), kModuleFirstLine);
  EXPECT_EQ(module->getEndLine(), kModuleLastLine);
}

// test has no ports, parameters or items. The package and the classes are
// declared outside it (3.12.1).
TEST_F(ClassTest29Test, ModuleTestIsEmpty) {
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

// The file declares one package, four classes and one module. An interface
// class is not an interface: interface_class_declaration and
// interface_declaration are different productions (A.1.2). The file declares
// no parameter and no variable.
TEST_F(ClassTest29Test, NoOtherDesignObjectsDeclared) {
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
// 25 of this file (vpiDefName, vpiDefFile, vpiDefLineNo, 37.10), and no class
// is in it (3.12.1). The instance only exists after elaboration.
TEST_F(ClassTest29Test, TestIsTheOnlyTopLevelInstance) {
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
