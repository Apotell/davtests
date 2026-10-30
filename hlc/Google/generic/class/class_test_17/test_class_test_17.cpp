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

// Test for tests/Google/generic/class/class_test_17.sv
//
// THE SOURCE
//
// Lines 1-7 are one-line comments with the license, lines 8-9 are blank and
// lines 10-14 are one block comment carrying the test metadata (tags 6.15 and
// 8.3). The SystemVerilog text is:
//
//   15: class Foo #(type IFType=virtual x_if);
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
// Lines 15-16 are a class_declaration (8.3, A.1.2) of the class Foo with a
//   parameter_port_list (A.1.3) of one parameter_port_declaration, a
//   type_parameter_declaration (A.2.1.1):
//
//     type_parameter_declaration ::= type [ forward_type ] list_of_type_assignments
//     type_assignment ::= type_identifier [ = data_type_or_incomplete_class_scoped_type ]
//     data_type ::= ... | virtual [ interface ] interface_identifier [ parameter_value_assignment ]
//                         [ . modport_identifier ]
//
//   - IFType is a type parameter: it is declared with the type keyword. "A
//     data type parameter (parameter type) can only be set to a data type"
//     (6.20.3), and its default, "virtual x_if", is the virtual interface form
//     of data_type (A.2.2.1, 25.9) with the interface_identifier x_if.
//   - It is a parameter, not a local parameter: only "param_assignments
//     appearing within a class body shall become localparam declarations"
//     (6.20.1).
//   - Foo, IFType and x_if are simple_identifiers (A.9.3) and no keywords
//     (5.6.2, Table B.1). Foo has no virtual, no extends, no implements, no
//     label after endclass and no class_item, so it has no member (8.2).
//
// No interface named x_if is declared in the file. Foo is a generic class
//   that is never specialized: "A generic class is not a type; only a
//   concrete specialization represents a type" (8.25). The LRM says nothing
//   about an interface name in the default of a type parameter that no
//   specialization ever uses. So the LRM neither requires nor forbids a
//   diagnostic for x_if on line 15; this test
//   leaves that line's diagnostics, and diagnostics about a missing interface
//   definition, out.
//
// Lines 18-19 are one module_declaration (A.1.2) with the module_identifier
//   test and no ports, parameters or items. test is never instantiated, so it
//   is a top-level module: "A design shall contain at least one top-level
//   module. A top-level module is implicitly instantiated once, and its
//   instance name is the same as the module name" (23.3.1).
//
// No line holds a compiler directive, so lines are numbered from 1 at the
//   start of the file (22.12).
//
// HOW THE TEST OBSERVES IT
//
// The LRM's object models are queried the way PLI would see them. A class
// defn (37.31) has vpiName, vpiVirtual, vpiParameter ("both the parameters
// declared in the parameter port list of the class declaration and the
// parameters declared within the body of the class declaration as class
// items", detail 7), vpiExtends, vpiDerivedClasses and its member relations.
// The parameters of 37.28 include type parameters, which have vpiName and
// vpiLocalParam. A module instance
// (37.10) has vpiName, vpiDefName, vpiDefFile and vpiDefLineNo. HLDB answers
// vpi_get() and vpi_iterate() through getVpiPropertyValue() and getByVpiType(),
// so the test asks those questions directly with the LRM's vpiXXX codes.
//
// Every expected value comes from the source text and the LRM, so a red test
// means HLC deviates from the LRM.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   NoErrorsOutsideLine15                     no fatal, syntax or error
//                                             diagnostic of any category on
//                                             any line but 15
//   Lines18To19ModuleIsAccepted               none on lines 18-19
//   ReportsNoMisreadingDiagnostics            no syntax, keyword, identifier
//                                             length or non-ASCII diagnostic
//   ReportsNoClassDiagnostics                 no duplicate class, missing base
//                                             class or label mismatch
//   ReportsNoParameterDiagnostics             no duplicate parameter, no
//                                             "localparam without value" and
//                                             no missing override
//   ReportsNoDesignHierarchyDiagnostics       no duplicate design unit or
//                                             missing top-level module
//   SourceFileHasNoIncludesOrMacros           one SourceFile, no includes,
//                                             no macro definitions or usages
//   ClassFooIsDeclared                        exactly one class from this
//                                             file, a vpiClassDefn named Foo
//   ClassIsOnLines15To16                      it starts on line 15 and ends on
//                                             line 16
//   ClassIsNotVirtualAndExtendsNothing        vpiVirtual is false; no
//                                             extends and no derived class
//   ClassHasOneParameter                      vpiParameter holds exactly one
//                                             parameter, IFType
//   TypeParameterIFTypeIsDeclared             IFType: a type parameter of the
//                                             port list, not local, line 15
//   ClassHasNoMembers                         no variable, method, constraint,
//                                             typedef, named event (array) or
//                                             internal scope
//   ModuleTestIsDeclared                      exactly one module from this
//                                             file, named test
//   ModuleTestIsOnLines18To19                 it starts on line 18 and ends on
//                                             line 19
//   ModuleTestIsEmpty                         no variable, net, parameter,
//                                             typedef or class defn in it
//   NoOtherDesignObjectsDeclared              no interface (x_if is only
//                                             named) and nothing else comes
//                                             from this file
//   TestIsTheOnlyTopLevelInstance             elaborated runs only: one
//                                             top-level instance, test, of the
//                                             definition test at line 18
//
// Exactly one check is gated on Design::getElaborated():
// TestIsTheOnlyTopLevelInstance. The top-level instance is created by
// elaboration (23.3.1), so it genuinely exists only after it. It has no else
// branch: before elaboration there is no instance, and the LRM defines no
// value to assert for one. Everything the parser establishes is asserted
// unconditionally.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Any diagnostic on line 15, and diagnostics about a missing interface or
//     module definition. Whether the undeclared x_if in an unused default
//     must, may or must not draw one is not settled by the LRM (8.25 only
//     demands that a base class name resolve). Nearest assertions:
//     NoErrorsOutsideLine15 and ReportsNoMisreadingDiagnostics (no syntax
//     error: "virtual x_if" is a data_type).
//   - The default typespec of IFType (vpiExpr, 37.28 detail 3). Its type is a
//     virtual interface of x_if, which names no declared interface, and the
//     LRM's object model has no interface typespec to compare it with.
//     Nearest assertion: TypeParameterIFTypeIsDeclared.
//   - The typespec of IFType "at the end of elaboration" (37.28 detail 2).
//     Foo is never specialized. Nearest assertion:
//     TypeParameterIFTypeIsDeclared.
//   - The implicit constructor. "the iteration over vpiMethods shall not
//     include built-in methods for which there is no explicit declaration"
//     (37.31 detail 1). Nearest assertion: ClassHasNoMembers.
//   - vpiFullName of any object, and the compilation-unit object (vpiUnit,
//     37.10). The full name is a computed property this suite does not assert,
//     and how HLDB groups $unit items is a tool convention. Nearest assertion:
//     ClassFooIsDeclared.
//   - Objects of the class and virtual interface variables (6.15, 25.9). They
//     only exist while a simulation runs; this is permanently out of scope.
//     Nearest assertion: ClassHasNoMembers.
//   - The location of the instance itself (vpiFile, vpiLineNo). A top-level
//     instance has no instantiation statement. Nearest assertion:
//     TestIsTheOnlyTopLevelInstance (vpiDefFile and vpiDefLineNo).
//   - Warnings and informational diagnostics. The LRM neither requires nor
//     forbids them. Nearest assertions: NoErrorsOutsideLine15 and the four
//     ReportsNo... tests.
//   - The comments on lines 1-14 (5.4) and columns. The LRM gives comments no
//     meaning. "The compiler shall maintain the current line number" (22.12);
//     the LRM keeps no column for a comment. Nearest
//     assertions: ClassIsOnLines15To16 and ModuleTestIsOnLines18To19, which
//     hold only if lines 1-14 were read as whole lines.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
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
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "class_test_17.sv";

// vpiVariables, the VPI relation from a scope to its variables (100 in IEEE
// 1800-2023 Annex K, vpi_user.h). HLDB's vpi_user.h names the same relation
// vpiVariable.
constexpr int32_t kVpiVariables = vpiVariable;

// The first and last line of the class declaration (the first is also the
// line of the type parameter and its default), and of the module declaration.
constexpr uint32_t kClassFirstLine = 15;
constexpr uint32_t kClassLastLine = 16;
constexpr uint32_t kModuleFirstLine = 18;
constexpr uint32_t kModuleLastLine = 19;

// Upper bound for "any later line".
constexpr uint32_t kAnyLaterLine = std::numeric_limits<uint32_t>::max();

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

// The line a diagnostic points at, or 0 when it carries no location.
uint32_t lineOf(const Error &error) { return error.getLocations().empty() ? 0 : error.getLocations().front().m_line; }

// True when HLC's error table gives 'error' a fatal, syntax or error
// severity. Every category counts: the design has a top-level module (23.3.1).
bool isErrorClass(const Error &error) {
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  const ErrorDefinition::ErrorMap::const_iterator errorInfo = errorInfoMap.find(error.getType());
  if (errorInfo == errorInfoMap.end()) return false;
  const ErrorDefinition::ErrorSeverity severity = errorInfo->second.m_severity;
  return (severity == ErrorDefinition::FATAL) || (severity == ErrorDefinition::SYNTAX) ||
         (severity == ErrorDefinition::ERROR);
}

// One diagnostic as " [<code> line <n>: <message template>]", so that a red
// run shows what was reported and where.
std::string describeError(const Error &error) {
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  const ErrorDefinition::ErrorMap::const_iterator errorInfo = errorInfoMap.find(error.getType());
  std::string description = " [" + std::to_string(static_cast<int32_t>(error.getType()));
  description += " line " + std::to_string(lineOf(error));
  if (errorInfo != errorInfoMap.end()) description += ": " + std::string(errorInfo->second.m_errorText);
  return description + "]";
}
}  // namespace

class ClassTest17Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_test_17.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Every error-class diagnostic on a line from 'firstLine' to 'lastLine',
  // described; empty when there is none. A run without an error container is
  // reported, not passed.
  static std::string errorsOnLines(uint32_t firstLine, uint32_t lastLine) {
    if ((m_session == nullptr) || (m_session->getErrorContainer() == nullptr)) return "no error container";
    std::string description;
    for (const Error &error : m_session->getErrorContainer()->getErrors()) {
      const uint32_t line = lineOf(error);
      if (isErrorClass(error) && (line >= firstLine) && (line <= lastLine)) description += describeError(error);
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
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllClasses());
  }

  // The module declared in the file under test, or nullptr.
  static const hldb::Module *findModule() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllModules());
  }
};

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Every line but 15 is legal beyond doubt, and the design has its top-level
// module, so no line but 15 draws a fatal, syntax or error diagnostic, in any
// category. Line 15 is left out: see WHAT IS NOT CHECKED.
TEST_F(ClassTest17Test, NoErrorsOutsideLine15) {
  EXPECT_EQ(errorsOnLines(1, kClassFirstLine - 1), "");
  EXPECT_EQ(errorsOnLines(kClassFirstLine + 1, kAnyLaterLine), "");
}

// Lines 18-19: an empty module_declaration (A.1.2).
TEST_F(ClassTest17Test, Lines18To19ModuleIsAccepted) {
  EXPECT_EQ(errorsOnLines(kModuleFirstLine, kModuleLastLine), "");
}

// Each of these diagnostics, at any severity and on any line, would mean HLC
// misread the text of the file.
TEST_F(ClassTest17Test, ReportsNoMisreadingDiagnostics) {
  // Every declaration matches the grammar (A.1.2, A.1.3, A.2.1.1, A.2.2.1):
  // "virtual x_if" is a data_type whatever x_if names, and the file uses no
  // preprocessor directive.
  EXPECT_EQ(findError(ErrorDefinition::PP_SYNTAX_ERROR), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_SYNTAX_ERROR), nullptr);

  // Foo, IFType, x_if and test are no keywords (5.6.2, Table B.1), and the
  // longest, IFType, has 6 characters, within any limit an implementation may
  // set, which "shall be at least 1024 characters" (5.6).
  EXPECT_EQ(findError(ErrorDefinition::PA_RESERVED_KEYWORD), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_MAX_LENGTH_IDENTIFIER), nullptr);

  // Every character of the file is ASCII.
  EXPECT_EQ(findError(ErrorDefinition::PP_NON_ASCII_CONTENT), nullptr);
}

// The class is declared once, extends nothing, and endclass has no label, so
// none of these diagnostics applies, at any severity.
TEST_F(ClassTest17Test, ReportsNoClassDiagnostics) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_MULTIPLY_DEFINED_CLASS), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_BASE_CLASS), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_UNMATCHED_LABEL), nullptr);
}

// IFType is the only parameter, it is not a localparam, and it has a default,
// so no specialization could lack an override (6.20.1). None of these
// diagnostics applies, at any severity.
TEST_F(ClassTest17Test, ReportsNoParameterDiagnostics) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_MULTIPLY_DEFINED_PARAMETER), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_LOCALPARAM_NO_VALUE), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::ELAB_MISSING_PARAMETER_OVERRIDE), nullptr);
}

// The class and the module are each declared once and test is a top-level
// module (23.3.1), so none of these diagnostics applies, at any severity.
TEST_F(ClassTest17Test, ReportsNoDesignHierarchyDiagnostics) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_MULTIPLY_DEFINED_DESIGN_UNIT), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::ELAB_MULTIPLY_DEFINED_MODULE), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::ELAB_NO_TOP_LEVEL_MODULE), nullptr);
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once. It has no `include (22.4), no `define (22.5.1) and
// no macro usage.
TEST_F(ClassTest17Test, SourceFileHasNoIncludesOrMacros) {
  const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
  ASSERT_EQ(sourceFiles.size(), 1u);
  const hldb::SourceFile *const sourceFile = sourceFiles.front();

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// ---------------------------------------------------------------------------
// Lines 15-16: class Foo #(type IFType=virtual x_if)
// ---------------------------------------------------------------------------

// The file declares one class, Foo (8.3), a class defn (37.31) whose vpiName is
// its class_identifier.
TEST_F(ClassTest17Test, ClassFooIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 1u);

  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);
  EXPECT_EQ(classDefn->getVpiType(), static_cast<uint32_t>(vpiClassDefn));
  EXPECT_EQ(classDefn->getName(), "Foo");
  EXPECT_EQ(stringProperty(classDefn, vpiName), "Foo");
}

// "class Foo #(type IFType=virtual x_if);" is on line 15 and "endclass" on
// line 16.
TEST_F(ClassTest17Test, ClassIsOnLines15To16) {
  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);

  EXPECT_TRUE(endsWith(classDefn->getFile(), kSourceFileName)) << classDefn->getFile();
  EXPECT_EQ(classDefn->getStartLine(), kClassFirstLine);
  EXPECT_EQ(classDefn->getEndLine(), kClassLastLine);
}

// The header has no virtual, so vpiVirtual is false, and no extends, so there
// is no extends object (37.31 detail 6); no class derives from Foo (37.31
// detail 5). A virtual interface type in a parameter default does not make the
// class virtual.
TEST_F(ClassTest17Test, ClassIsNotVirtualAndExtendsNothing) {
  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);

  EXPECT_EQ(integerProperty(classDefn, vpiVirtual), 0);
  EXPECT_EQ(relationSize(classDefn, vpiExtends), 0u);
  EXPECT_EQ(relationSize(classDefn, vpiDerivedClasses), 0u);
}

// The parameter port list declares IFType, and the empty body declares
// nothing (37.31 detail 7); a type parameter is one of the parameters (37.28).
TEST_F(ClassTest17Test, ClassHasOneParameter) {
  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);
  EXPECT_EQ(relationSize(classDefn, vpiParameter), 1u);
}

// "type IFType=...": a type parameter (6.20.3, A.2.1.1) of the port list, so
// not a local parameter (6.20.1, 37.31 detail 7), declared on line 15.
TEST_F(ClassTest17Test, TypeParameterIFTypeIsDeclared) {
  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);
  const std::vector<const hldb::Any *> *const parameters = relationObjects(classDefn, vpiParameter);
  ASSERT_EQ(sizeOf(parameters), 1u);
  const hldb::Any *const parameter = parameters->front();
  ASSERT_NE(parameter, nullptr);

  EXPECT_EQ(parameter->getVpiType(), static_cast<uint32_t>(vpiTypeParameter));
  EXPECT_EQ(parameter->getName(), "IFType");
  EXPECT_EQ(stringProperty(parameter, vpiName), "IFType");
  EXPECT_EQ(integerProperty(parameter, vpiLocalParam), 0);
  EXPECT_TRUE(endsWith(parameter->getFile(), kSourceFileName)) << parameter->getFile();
  EXPECT_EQ(parameter->getStartLine(), kClassFirstLine);
}

// The class has no class_item, so it has no members (8.2) and nothing else
// that 37.31 lets a class defn hold besides its parameter. The implicit
// constructor does not count (37.31 detail 1).
TEST_F(ClassTest17Test, ClassHasNoMembers) {
  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);

  EXPECT_EQ(relationSize(classDefn, kVpiVariables), 0u);
  EXPECT_EQ(relationSize(classDefn, vpiMethods), 0u);
  EXPECT_EQ(relationSize(classDefn, vpiConstraint), 0u);
  EXPECT_EQ(relationSize(classDefn, vpiTypedef), 0u);
  EXPECT_EQ(relationSize(classDefn, vpiNamedEvent), 0u);
  EXPECT_EQ(relationSize(classDefn, vpiNamedEventArray), 0u);
  EXPECT_EQ(relationSize(classDefn, vpiInternalScope), 0u);
}

// ---------------------------------------------------------------------------
// Lines 18-19: module test
// ---------------------------------------------------------------------------

// The file declares one module, test (A.1.2).
TEST_F(ClassTest17Test, ModuleTestIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);

  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "test");
}

// "module test;" is on line 18 and "endmodule" on line 19.
TEST_F(ClassTest17Test, ModuleTestIsOnLines18To19) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);

  EXPECT_TRUE(endsWith(module->getFile(), kSourceFileName)) << module->getFile();
  EXPECT_EQ(module->getStartLine(), kModuleFirstLine);
  EXPECT_EQ(module->getEndLine(), kModuleLastLine);
}

// test has no ports, parameters or items. The class is declared outside it,
// in the compilation-unit scope (3.12.1).
TEST_F(ClassTest17Test, ModuleTestIsEmpty) {
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

// The file declares one class and one module. x_if is only named in a data
// type; no interface_declaration (25.3) is in the file, so no interface comes
// from it, and nothing else does either.
TEST_F(ClassTest17Test, NoOtherDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
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
// 18 of this file (vpiDefName, vpiDefFile, vpiDefLineNo, 37.10), and the class
// is not in it (3.12.1). The instance only exists after elaboration.
TEST_F(ClassTest17Test, TestIsTheOnlyTopLevelInstance) {
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
