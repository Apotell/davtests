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

// Test for tests/Google/generic/class/class_test_35.sv
//
// THE SOURCE
//
// Lines 1-5 are one block comment carrying the test metadata (tags 6.15 and
// 8.3); this file has no license comment. The SystemVerilog text is:
//
//   6: class zzxx;
//   7: extern function  void set_port(int ap);
//   8: endclass
//
// Line 8 ends with a line ending. The repository stores the file with LF line
// endings; a Windows checkout (core.autocrlf) turns them into CR LF. 5.3 lists
// "newlines" as white space but does not name the carriage return, so this
// test treats CR LF as one line ending and asserts nothing about the CR
// itself. The two spaces between function and void are white space that only
// separates tokens (5.3).
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Lines 6-8 are one class_declaration (8.3, A.1.2) of zzxx whose one
//   class_item is a method prototype ("..." marks shortened parts):
//
//     class_method ::= ... | extern { method_qualifier } method_prototype ;
//     method_prototype ::= task_prototype | function_prototype
//     function_prototype ::= function [ dynamic_override_specifiers ]
//         data_type_or_void function_identifier [ ( [ tf_port_list ] ) ]
//
//   (class_method: A.1.9; function_prototype: A.2.6.)
//   - set_port is a function returning void with one argument, ap, of type
//     int, a "2-state data type, 32-bit signed integer" (Table 6-8). ap has no
//     direction, and "Function declarations default to the formal direction
//     input if no direction has been specified" (13.4).
//   - "The extern qualifier indicates that the body of the method (its
//     implementation) is to be found outside the declaration" (8.24). The file
//     has no out-of-block declaration for set_port. 8.24 sets rules for one
//     that is given ("The out-of-block method declaration shall match the
//     prototype declaration exactly", "It shall be an error if more than one
//     out-of-block declaration is provided for a particular extern method"),
//     but no clause of the LRM says that a prototype without one is an error,
//     and nothing in the file calls set_port. With no "shall" broken, this
//     test treats the file as legal. That reading is this test's, and it is
//     recorded under WHAT IS NOT CHECKED.
//   - "The lifetime of methods declared as part of a class type shall be
//     automatic" (8.6), and set_port is a member of zzxx (8.2).
//
// The class lies outside any other scope, so it belongs to the
//   compilation-unit scope, which "contains all declarations that lie outside
//   any other scope" (3.12.1).
//
// The file has no module, and "A design shall contain at least one top-level
//   module" (23.3.1). An elaborating run may report that; this test accepts
//   that one diagnostic and no other error.
//
// No line holds a compiler directive, so lines are numbered from 1 at the
//   start of the file (22.12).
//
// HOW THE TEST OBSERVES IT
//
// The class defn (37.31) has vpiName, vpiVirtual, vpiExtends, vpiParameter,
// vpiMethods and its other member relations; "the iteration over vpiMethods
// shall not include built-in methods for which there is no explicit
// declaration" (37.31 detail 1), and set_port is declared explicitly by its
// prototype. A method is a task func (37.41): here a function, with vpiMethod,
// vpiAutomatic ("default lifetime") and vpiSize ("For a void function,
// vpiSize shall return 0", detail 12); its arguments are io decls (37.13) with
// vpiName, vpiDirection and a typespec. "If the number of statements is zero,
// the vpiStmt relation shall return NULL" (37.12 detail 5): a prototype has no
// statement. HLDB answers vpi_get() and vpi_iterate() through
// getVpiPropertyValue() and getByVpiType(), so the test asks those questions
// directly with the LRM's vpiXXX codes.
//
// Every expected value comes from the source text and the LRM, so a red test
// means HLC deviates from the LRM, or reads the missing body differently (see
// WHAT IS NOT CHECKED).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors                     no fatal, syntax or error
//                                             diagnostic of any category
//                                             except the 23.3.1 one
//   Lines6To8ClassIsAccepted                  none on lines 6-8
//   ReportsNoMisreadingDiagnostics            no syntax, keyword, identifier
//                                             length or non-ASCII diagnostic
//                                             at any severity
//   ReportsNoClassDiagnostics                 no duplicate class, missing base
//                                             class or label mismatch
//   ReportsNoDesignUnitDiagnostics            no duplicate design unit or
//                                             missing definition
//   SourceFileHasNoIncludesOrMacros           one SourceFile, no includes,
//                                             no macro definitions or usages
//   ClassZzxxIsDeclared                       a class defn named zzxx from
//                                             this file
//   ClassZzxxIsOnLines6To8                    it starts on line 6 and ends on
//                                             line 8
//   ClassZzxxIsNotVirtualAndExtendsNothing    vpiVirtual is false; no
//                                             extends, derived class or
//                                             parameter
//   ClassZzxxHasOneMethod                     vpiMethods holds exactly one
//                                             method, set_port
//   ClassZzxxHasNoOtherMembers                no variable, constraint,
//                                             typedef, named event (array) or
//                                             internal scope
//   MethodSetPortIsAVoidFunction              a function, a method,
//                                             automatic, of size 0, on line 7
//   MethodSetPortHasOneInputArgumentAp        one io decl, ap, input, of type
//                                             int, on line 7
//   MethodSetPortHasNoStatement               vpiStmt is NULL
//   NoOtherDesignObjectsDeclared              no module, interface, program,
//                                             declared package, UDP, config,
//                                             checker, parameter or variable
//                                             comes from this file
//
// No check is gated on Design::getElaborated(): the file has nothing that
// elaborates or reduces. The 23.3.1 error an elaborating run may report is
// left out, not asserted.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Whether a tool may report the missing out-of-block declaration of
//     set_port. The LRM states no rule for an extern prototype without one
//     (8.24 rules only on a given declaration and on more than one), so this
//     test treats the file as legal; an HLC error for it would be a reading
//     the LRM does not support, to report rather than to accept silently, and
//     a warning is allowed. Nearest assertions: CompilesWithoutErrors and
//     MethodSetPortHasNoStatement.
//   - Whether set_port is marked extern. 37.41 defines no property for the
//     extern qualifier. Nearest assertion: MethodSetPortHasNoStatement.
//   - The return variable of set_port (37.41 details 1 and 3). A void
//     function returns no value (13.4.1). Nearest assertion:
//     MethodSetPortIsAVoidFunction (vpiSize 0).
//   - The object HLDB returns for the typespec relation of the io decl ap
//     (37.13). The LRM draws it as a direct relation to a typespec; HLDB may
//     instead return an object of its own type that refers to the typespec.
//     That indirection is an HLDB model convention the LRM does not define, so
//     its type is not asserted; the test follows it through vpiActual to the
//     typespec it refers to. Nearest assertion:
//     MethodSetPortHasOneInputArgumentAp (vpiIntTypespec on that typespec).
//   - vpiVisibility of set_port and vpiFullName of any object. They are
//     computed or defaulted properties that this suite does not assert.
//     Nearest assertions: ClassZzxxIsDeclared and MethodSetPortIsAVoidFunction.
//   - A package object for the compilation unit of this file. The object
//     model represents a compilation unit as a package with vpiUnit (37.10),
//     although "the compilation-unit scope is not a package" (3.12.1);
//     whether HLDB lists one for this file is a tool convention, so only
//     packages with vpiUnit not TRUE are counted. Nearest assertion:
//     NoOtherDesignObjectsDeclared.
//   - Run-time behavior. The file has no procedural code and no new (8.4), so
//     it creates no object and nothing in it exists only while simulation
//     time advances; this is permanently out of scope. Nearest assertion:
//     MethodSetPortHasNoStatement.
//   - The 23.3.1 diagnostic for a design without a top-level module, and
//     warnings and informational diagnostics. The LRM requires the first for
//     a design and neither requires nor forbids the others. Nearest
//     assertion: CompilesWithoutErrors.

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
constexpr std::string_view kSourceFileName = "class_test_35.sv";

// vpiVariables, the VPI relation from a scope to its variables (100 in IEEE
// 1800-2023 Annex K, vpi_user.h). HLDB's vpi_user.h names the same relation
// vpiVariable.
constexpr int32_t kVpiVariables = vpiVariable;

// The first and last line of the class declaration, and the line of the
// method prototype.
constexpr uint32_t kClassFirstLine = 6;
constexpr uint32_t kPrototypeLine = 7;
constexpr uint32_t kClassLastLine = 8;

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
// nullptr.
template <typename T>
const T *findFromSourceFile(const std::vector<T *> *collection, std::string_view name) {
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (endsWith(object->getFile(), kSourceFileName) && (object->getName() == name)) return object;
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

// True when HLC's error table gives 'error' a fatal, syntax or error severity
// and 'error' is not the one diagnostic the file may draw legitimately: the
// one for a design without a top-level module (23.3.1).
bool isUnexpectedError(const Event &error) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
  if (errorInfo == nullptr) return false;
  const EventSeverity severity = errorInfo->m_severity;
  const bool isErrorClass =
      (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) || (severity == EventSeverity::Error);
  return isErrorClass && (error.getType() != EventType::ElabNoTopLevelModule);
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

class ClassTest35Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_test_35.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Every unexpected error (see isUnexpectedError) on a line from 'firstLine'
  // to 'lastLine', described; empty when there is none. A run without an
  // error container is reported, not passed.
  static std::string errorsOnLines(uint32_t firstLine, uint32_t lastLine) {
    if ((m_session == nullptr) || (m_session->getEventListener() == nullptr)) return "no error container";
    std::string description;
    for (const Event &error : m_session->getEventListener()->getEvents()) {
      const uint32_t line = lineOf(error);
      if (isUnexpectedError(error) && (line >= firstLine) && (line <= lastLine)) description += describeError(error);
    }
    return description;
  }

  // Every unexpected error, described; empty when there is none.
  static std::string allErrors() {
    if ((m_session == nullptr) || (m_session->getEventListener() == nullptr)) return "no error container";
    std::string description;
    for (const Event &error : m_session->getEventListener()->getEvents()) {
      if (isUnexpectedError(error)) description += describeError(error);
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

  // The class zzxx declared in the file under test, or nullptr.
  static const hldb::ClassDefn *findClass() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllClasses(), "zzxx");
  }

  // The method set_port of zzxx, or nullptr.
  static const hldb::Any *findSetPort() {
    const hldb::ClassDefn *const classDefn = findClass();
    return (classDefn == nullptr) ? nullptr : findInRelation(classDefn, vpiMethods, "set_port");
  }
};

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Every line of the file is legal (see WHAT THE LRM SAYS ABOUT IT). The only
// error the file may draw is the 23.3.1 one for a design without a top-level
// module, so nothing else is fatal, a syntax error or an error, in any
// category.
TEST_F(ClassTest35Test, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

// Lines 6-8: a class_declaration whose one item is an extern method prototype
// (8.24, A.1.9, A.2.6).
TEST_F(ClassTest35Test, Lines6To8ClassIsAccepted) { EXPECT_EQ(errorsOnLines(kClassFirstLine, kClassLastLine), ""); }

// Each of these diagnostics, at any severity and on any line, would mean HLC
// misread the text of the file.
TEST_F(ClassTest35Test, ReportsNoMisreadingDiagnostics) {
  // The declaration matches the grammar (A.1.2, A.1.9, A.2.6), and the file
  // uses no preprocessor directive.
  EXPECT_EQ(findEvent(EventType::PpSyntaxError), nullptr);
  EXPECT_EQ(findEvent(EventType::PaSyntaxError), nullptr);

  // zzxx, set_port and ap are no keywords (Table B.1); extern, function, void
  // and int are, each where the grammar puts it. The longest identifier,
  // set_port, has 8 characters, within any limit an implementation may set,
  // which "shall be at least 1024 characters" (5.6).
  EXPECT_EQ(findEvent(EventType::PaReservedKeyword), nullptr);
  EXPECT_EQ(findEvent(EventType::PaMaxLengthIdentifier), nullptr);

  // Every character of the file is ASCII.
  EXPECT_EQ(findEvent(EventType::PpNonAsciiContent), nullptr);
}

// The class is declared once, extends nothing, and endclass has no label, so
// none of these diagnostics applies, at any severity.
TEST_F(ClassTest35Test, ReportsNoClassDiagnostics) {
  EXPECT_EQ(findEvent(EventType::CompMultiplyDefinedClass), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUndefinedBaseClass), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUnmatchedLabel), nullptr);
}

// The class is the only design unit, declared once, and nothing is
// instantiated, so none of these diagnostics applies, at any severity. The
// 23.3.1 diagnostic for a design without a top-level module is legitimate
// here and is not checked.
TEST_F(ClassTest35Test, ReportsNoDesignUnitDiagnostics) {
  EXPECT_EQ(findEvent(EventType::CompMultiplyDefinedDesignUnit), nullptr);
  EXPECT_EQ(findEvent(EventType::ElabMultiplyDefinedModule), nullptr);
  EXPECT_EQ(findEvent(EventType::ElabNoModuleDefinition), nullptr);
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once (run configuration). It has no `include (22.4), no
// `define (22.5.1) and no macro usage.
TEST_F(ClassTest35Test, SourceFileHasNoIncludesOrMacros) {
  const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
  ASSERT_EQ(sourceFiles.size(), 1u);
  const hldb::SourceFile *const sourceFile = sourceFiles.front();

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// ---------------------------------------------------------------------------
// Lines 6-8: class zzxx
// ---------------------------------------------------------------------------

// zzxx is declared in the compilation-unit scope (3.12.1), as a class defn
// (37.31) whose vpiName is its class_identifier.
TEST_F(ClassTest35Test, ClassZzxxIsDeclared) {
  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);
  EXPECT_EQ(classDefn->getVpiType(), static_cast<uint32_t>(vpiClassDefn));
  EXPECT_EQ(stringProperty(classDefn, vpiName), "zzxx");
}

// "class zzxx;" is on line 6 and "endclass" on line 8.
TEST_F(ClassTest35Test, ClassZzxxIsOnLines6To8) {
  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);

  EXPECT_TRUE(endsWith(classDefn->getFile(), kSourceFileName)) << classDefn->getFile();
  EXPECT_EQ(classDefn->getStartLine(), kClassFirstLine);
  EXPECT_EQ(classDefn->getEndLine(), kClassLastLine);
}

// "class zzxx;": no virtual, no extends clause, so no extends object (37.31
// detail 6); no class derives from zzxx (37.31 detail 5); no
// parameter_port_list and no parameter item (37.31 detail 7).
TEST_F(ClassTest35Test, ClassZzxxIsNotVirtualAndExtendsNothing) {
  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);

  EXPECT_EQ(integerProperty(classDefn, vpiVirtual), 0);
  EXPECT_EQ(relationSize(classDefn, vpiExtends), 0u);
  EXPECT_EQ(relationSize(classDefn, vpiDerivedClasses), 0u);
  EXPECT_EQ(relationSize(classDefn, vpiParameter), 0u);
}

// The prototype of line 7 declares the method set_port explicitly, so
// vpiMethods holds it (37.31 detail 1), and nothing else.
TEST_F(ClassTest35Test, ClassZzxxHasOneMethod) {
  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);

  EXPECT_EQ(relationSize(classDefn, vpiMethods), 1u);
  EXPECT_NE(findSetPort(), nullptr);
}

// The one class_item is the prototype, so zzxx has no property (8.2) and
// nothing else that 37.31 lets a class defn hold.
TEST_F(ClassTest35Test, ClassZzxxHasNoOtherMembers) {
  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);

  EXPECT_EQ(relationSize(classDefn, kVpiVariables), 0u);
  EXPECT_EQ(relationSize(classDefn, vpiConstraint), 0u);
  EXPECT_EQ(relationSize(classDefn, vpiTypedef), 0u);
  EXPECT_EQ(relationSize(classDefn, vpiNamedEvent), 0u);
  EXPECT_EQ(relationSize(classDefn, vpiNamedEventArray), 0u);
  EXPECT_EQ(relationSize(classDefn, vpiInternalScope), 0u);
}

// ---------------------------------------------------------------------------
// Line 7: extern function void set_port(int ap)
// ---------------------------------------------------------------------------

// "function void set_port": a function (A.2.6) and a member of zzxx, so a
// method (37.41); automatic (8.6); "For a void function, vpiSize shall return
// 0" (37.41 detail 12). The whole prototype is on line 7.
TEST_F(ClassTest35Test, MethodSetPortIsAVoidFunction) {
  const hldb::Any *const setPort = findSetPort();
  ASSERT_NE(setPort, nullptr);

  EXPECT_EQ(setPort->getVpiType(), static_cast<uint32_t>(vpiFunction));
  EXPECT_EQ(integerProperty(setPort, vpiMethod), 1);
  EXPECT_EQ(integerProperty(setPort, vpiAutomatic), 1);
  EXPECT_EQ(integerProperty(setPort, vpiSize), 0);
  EXPECT_TRUE(endsWith(setPort->getFile(), kSourceFileName)) << setPort->getFile();
  EXPECT_EQ(setPort->getStartLine(), kPrototypeLine);
}

// "(int ap)": one argument, an io decl (37.13) named ap, of type int (Table
// 6-8); "Function declarations default to the formal direction input if no
// direction has been specified" (13.4).
TEST_F(ClassTest35Test, MethodSetPortHasOneInputArgumentAp) {
  const hldb::Any *const setPort = findSetPort();
  ASSERT_NE(setPort, nullptr);
  EXPECT_EQ(relationSize(setPort, vpiIODecl), 1u);
  const hldb::Any *const argument = findInRelation(setPort, vpiIODecl, "ap");
  ASSERT_NE(argument, nullptr) << "set_port has no argument ap";

  EXPECT_EQ(argument->getVpiType(), static_cast<uint32_t>(vpiIODecl));
  EXPECT_EQ(integerProperty(argument, vpiDirection), static_cast<int64_t>(vpiInput));
  const hldb::Any *const typespec = typespecOf(argument, vpiTypespec);
  ASSERT_NE(typespec, nullptr) << "the argument ap has no typespec";
  EXPECT_EQ(typespec->getVpiType(), static_cast<uint32_t>(vpiIntTypespec));
  EXPECT_EQ(argument->getStartLine(), kPrototypeLine);
}

// A prototype has no body: the body "is to be found outside the declaration"
// (8.24), and the file gives none. "If the number of statements is zero, the
// vpiStmt relation shall return NULL" (37.12 detail 5).
TEST_F(ClassTest35Test, MethodSetPortHasNoStatement) {
  const hldb::Any *const setPort = findSetPort();
  ASSERT_NE(setPort, nullptr);
  EXPECT_EQ(relationSize(setPort, vpiStmt), 0u);
}

// ---------------------------------------------------------------------------
// Nothing else is declared
// ---------------------------------------------------------------------------

// Each of these design elements contains its own keyword (module or
// macromodule, interface, program, package, primitive, config or checker;
// A.1.2, A.1.5, A.5.1), and none of these keywords appears in the file; the
// class declares no parameter, and no variable lies outside it. A
// compilation-unit package object (vpiUnit TRUE) is not counted as a declared
// package.
TEST_F(ClassTest35Test, NoOtherDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTopModules()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countDeclaredPackagesFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);

  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
