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

// Test for tests/Google/generic/class/class_test_49.sv
//
// THE SOURCE
//
// Lines 1-7 are one-line comments with the license, lines 8-9 are blank and
// lines 10-14 are one block comment carrying the test metadata (tags 6.15 and
// 8.3). Line 15 is blank. The SystemVerilog text is:
//
//   16: package glb;
//   17:   localparam int arr[3] = '{1,2,3};
//   18: endpackage
//   19:
//   20: function int f(int a);
//   21:   return a;
//   22: endfunction
//   23:
//   24: class params_as_class_item;
//   25:   parameter N = 2;
//   26:   parameter reg P = '1;
//   27:   localparam M = f(glb::arr[N]) + 1;
//   28: endclass
//   29:
//   30: module test;
//   31: endmodule
//
// Line 31 ends with a line ending. The repository stores the file with LF
// line endings; a Windows checkout (core.autocrlf) turns them into CR LF. 5.3
// lists "newlines" as white space but does not name the carriage return, so
// this test treats CR LF as one line ending and asserts nothing about the CR
// itself.
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Lines 16-18 are a package_declaration (26.2, A.1.2) of glb whose one item
//   is the local parameter arr: an unpacked array of three int (7.4) with the
//   assignment pattern '{1,2,3} as its value (10.9.1).
//
// Lines 20-22 are a function_declaration (13.4) of f in the compilation-unit
//   scope, which "contains all declarations that lie outside any other scope"
//   (3.12.1). f has one input argument a of type int and returns it. It meets
//   every constraint 13.4.3 sets for a constant function: no output, inout or
//   ref argument, no event, fork, hierarchical reference or function call, and
//   no identifier other than its own argument.
//
// Lines 24-28 are a class_declaration (8.3, A.1.2) of params_as_class_item
//   with three parameter items and no other item:
//   - "All param_assignments appearing within a class body shall become
//     localparam declarations regardless of the presence or absence of a
//     parameter_port_list" (6.20.1). So N and P are local parameters like M.
//   - N has no type or range; its value 2 is an unsized decimal, so N takes
//     "the type and range of the final value assigned to the parameter"
//     (6.20.2), at least 32 bits (an "implementation-dependent value of at
//     least 31" for the MSB).
//   - "A parameter with a type specification, but with no range
//     specification, shall be of the type specified" (6.20.2): P is a reg,
//     and "logic and reg denote the same type" (6.11.2), a "4-state data type,
//     user-defined vector size, unsigned" (Table 6-8). "A data object declared
//     as reg, logic, or bit ... without a range specification shall be
//     considered 1-bit wide" (6.9). '1 "sets all bits to specified value"
//     (5.7.1), so P is 1'b1.
//   - M's value "f(glb::arr[N]) + 1" is a constant expression: glb::arr[N] is
//     "ps_parameter_identifier constant_select" (constant_primary, A.8.4), and
//     f(glb::arr[N]) is a constant function call, a call "of a constant
//     function that is evaluated at elaboration time, wherein the constant
//     function's declaration is local to the calling design element or is in a
//     package or $unit, and where the arguments to the function, if any, are
//     all constant expressions" (13.4.3). N is declared on line 25, before
//     line 27, and glb on lines 16-18, where "the compilation of a package is
//     required to precede references to it" (3.12).
//
// Lines 30-31 are one module_declaration (A.1.2). test is never instantiated,
//   so it is a top-level module: "A design shall contain at least one
//   top-level module. A top-level module is implicitly instantiated once, and
//   its instance name is the same as the module name" (23.3.1).
//
// No line holds a compiler directive, so lines are numbered from 1 at the
//   start of the file (22.12).
//
// HOW THE TEST OBSERVES IT
//
// The class defn (37.31) has vpiName, vpiVirtual, vpiExtends, vpiParameter
// ("both the parameters declared in the parameter port list of the class
// declaration and the parameters declared within the body of the class
// declaration as class items. The property vpiLocalParam (see 37.28) shall
// return TRUE for parameters declared within the body", detail 7) and its
// member relations. A parameter (37.28) has vpiName, vpiLocalParam, vpiSigned,
// vpiSize, vpiExpr ("The vpiExpr relation of a value parameter shall return
// the default expr", detail 3) and vpiLeftRange and vpiRightRange ("If a value
// parameter does not have an explicitly defined range, vpiLeftRange and
// vpiRightRange shall return a NULL handle", detail 5). vpiDecompile "shall
// return a string with a functionally equivalent expression to the original
// expression within the source code. Parentheses shall be added only to
// preserve precedence. Each operand and operator shall be separated by a
// single space character" (37.59 detail 2); for each default expr this test
// takes that string to be the expression as written with that spacing, an LRM
// interpretation rather than a quoted rule. A package is an instance (37.10)
// with vpiName and a parameter relation; the object model also represents a
// compilation unit as a package, with vpiUnit TRUE (37.10), although "the
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
//   Lines20To22FunctionIsAccepted             none on lines 20-22
//   Lines24To28ClassIsAccepted                none on lines 24-28
//   Lines30To31ModuleIsAccepted               none on lines 30-31
//   ReportsNoMisreadingDiagnostics            no syntax, keyword, identifier
//                                             length or non-ASCII diagnostic
//                                             at any severity
//   ReportsNoPackageDiagnostics               no duplicate or undefined
//                                             package
//   ReportsNoClassDiagnostics                 no duplicate class, missing base
//                                             class or label mismatch
//   ReportsNoParameterDiagnostics             no duplicate parameter,
//                                             "localparam without value",
//                                             unbound name or non-constant
//                                             expression
//   ReportsNoDesignHierarchyDiagnostics       no duplicate design unit,
//                                             missing definition or missing
//                                             top-level module
//   SourceFileHasNoIncludesOrMacros           one SourceFile, no includes,
//                                             no macro definitions or usages
//   PackageGlbIsDeclared                      exactly one declared package
//                                             from this file, glb, on lines
//                                             16-18
//   PackageGlbHoldsLocalparamArr              glb holds one parameter, arr, a
//                                             local parameter with a default
//                                             expr, on line 17
//   ClassIsDeclared                           exactly one class from this
//                                             file, a vpiClassDefn named
//                                             params_as_class_item
//   ClassIsOnLines24To28                      it starts on line 24 and ends on
//                                             line 28
//   ClassIsNotVirtualAndExtendsNothing        vpiVirtual is false; no extends
//                                             and no derived class
//   ClassHasThreeParameters                   vpiParameter holds exactly
//                                             three parameters, N, P and M
//   ParameterNIsLocalWithDefault2             N: local, default expr 2, at
//                                             least 32 bits, no range, line 25
//   ParameterPIsALocalOneBitReg               P: local, unsigned, 1 bit,
//                                             default expr '1, line 26
//   ParameterMIsLocalWithFunctionCall         M: local, default expr
//                                             f(glb::arr[N]) + 1, no range,
//                                             line 27
//   ClassHasNoMembers                         no variable, method, constraint,
//                                             typedef, named event (array) or
//                                             internal scope
//   ModuleTestIsDeclared                      exactly one module from this
//                                             file, named test
//   ModuleTestIsOnLines30To31                 it starts on line 30 and ends on
//                                             line 31
//   ModuleTestIsEmpty                         no variable, net, parameter,
//                                             typedef or class defn in it
//   NoOtherDesignObjectsDeclared              no interface, program, UDP,
//                                             config, checker or
//                                             compilation-unit variable comes
//                                             from this file
//   TestIsTheOnlyTopLevelInstance             elaborated runs only: one
//                                             top-level instance, test, of the
//                                             definition test at line 30, with
//                                             no class defn in it
//
// Exactly one check is gated on Design::getElaborated():
// TestIsTheOnlyTopLevelInstance. The top-level instance is created by
// elaboration (23.3.1), so it genuinely exists only after it. It has no else
// branch: before elaboration there is no instance, and the LRM defines no
// value to assert for one. Everything the parser establishes (the package and
// arr, the class, its parameters as written, the module, the names and the
// lines) is asserted unconditionally.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The values of arr, N, P and M, among them M's constant function call
//     result. The LRM defines them at elaboration, but a parameter's value is
//     read with vpi_get_value(), "the value that the parameter has at the end
//     of elaboration" (37.28 detail 1), which the vpi_get()/vpi_get_str()
//     interface this suite uses does not give. Nearest assertions:
//     ParameterNIsLocalWithDefault2, ParameterPIsALocalOneBitReg and
//     ParameterMIsLocalWithFunctionCall (the default exprs as written).
//   - The names M's expression binds to (the N of line 25, the function f,
//     glb's arr). Clause 37 does not fix the object form of a call, a
//     package-scoped select or their references. Nearest assertions:
//     ReportsNoParameterDiagnostics (the names bind and the expression is
//     constant) and ParameterMIsLocalWithFunctionCall.
//   - The function f of lines 20-22 as an object. It lies in the
//     compilation-unit scope, and this suite reaches compilation-unit objects
//     only through Design lists it has confirmed to build, none of which holds
//     functions. Nearest assertions: Lines20To22FunctionIsAccepted and
//     ReportsNoParameterDiagnostics.
//   - The typespecs of arr, N and M, and arr's default expr text. 6.20.2 makes
//     N's and M's types those of their final values, and Clause 37 does not
//     fix the decompiled form of an assignment pattern. Nearest assertions:
//     PackageGlbHoldsLocalparamArr, ParameterNIsLocalWithDefault2 and
//     ParameterMIsLocalWithFunctionCall.
//   - The Design's parameter and parameter assignment lists. Where HLDB lists
//     package and class parameters outside the object model is a tool
//     convention. Nearest assertions: PackageGlbHoldsLocalparamArr and
//     ClassHasThreeParameters.
//   - The order of the vpiParameter iteration. 37.31 does not fix it, so N, P
//     and M are looked up by name. Nearest assertion: ClassHasThreeParameters.
//   - A package object for the compilation unit of this file. The object
//     model represents a compilation unit as a package with vpiUnit (37.10);
//     whether HLDB lists one for this file is a tool convention, so only
//     packages with vpiUnit not TRUE are counted. Nearest assertion:
//     PackageGlbIsDeclared.
//   - vpiFullName of any object. The full name is a computed property that
//     this suite does not assert. Nearest assertions: the ...IsDeclared tests
//     and TestIsTheOnlyTopLevelInstance (vpiName and vpiDefName).
//   - Run-time behavior. The file calls f only in a constant expression and
//     has no new (8.4), so nothing in it exists only while simulation time
//     advances; this is permanently out of scope. Nearest assertion:
//     ClassHasNoMembers.
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
constexpr std::string_view kSourceFileName = "class_test_49.sv";

// vpiVariables, the VPI relation from a scope to its variables (100 in IEEE
// 1800-2023 Annex K, vpi_user.h). HLDB's vpi_user.h names the same relation
// vpiVariable.
constexpr int32_t kVpiVariables = vpiVariable;

// The lines of the declarations.
constexpr uint32_t kPackageFirstLine = 16;
constexpr uint32_t kArrLine = 17;
constexpr uint32_t kPackageLastLine = 18;
constexpr uint32_t kFunctionFirstLine = 20;
constexpr uint32_t kFunctionLastLine = 22;
constexpr uint32_t kClassFirstLine = 24;
constexpr uint32_t kParameterNLine = 25;
constexpr uint32_t kParameterPLine = 26;
constexpr uint32_t kParameterMLine = 27;
constexpr uint32_t kClassLastLine = 28;
constexpr uint32_t kModuleFirstLine = 30;
constexpr uint32_t kModuleLastLine = 31;

// The least width of a parameter whose unsized value gives it "an MSB equal to
// an implementation-dependent value of at least 31" (6.20.2).
constexpr int64_t kUnsizedParameterMinBits = 32;

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

class ClassTest49Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_test_49.hlc"}); }
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

  // The package glb declared in the file under test, or nullptr.
  static const hldb::Package *findPackage() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllPackages(), "glb");
  }

  // The class declared in the file under test, or nullptr.
  static const hldb::ClassDefn *findClass() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllClasses(), "params_as_class_item");
  }

  // The parameter named 'name' of the class, or nullptr.
  static const hldb::Any *findClassParameter(std::string_view name) {
    const hldb::ClassDefn *const classDefn = findClass();
    return (classDefn == nullptr) ? nullptr : findInRelation(classDefn, vpiParameter, name);
  }

  // The module declared in the file under test, or nullptr.
  static const hldb::Module *findModule() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllModules());
  }

  // Checks the class parameter 'name' declared on 'line': a value parameter
  // that is local, being a class item (6.20.1, 37.31 detail 7), whose default
  // expr decompiles to 'defaultText' (37.28 detail 3, 37.59 detail 2, as
  // interpreted in the header) and which has no explicit range (37.28 detail
  // 5).
  static void expectLocalParameter(std::string_view name, uint32_t line, std::string_view defaultText) {
    const hldb::Any *const parameter = findClassParameter(name);
    ASSERT_NE(parameter, nullptr) << "no parameter " << name << " in the class";

    EXPECT_EQ(parameter->getVpiType(), static_cast<uint32_t>(vpiParameter));
    EXPECT_EQ(stringProperty(parameter, vpiName), std::string(name));
    EXPECT_EQ(integerProperty(parameter, vpiLocalParam), 1);

    const hldb::Any *const defaultExpr = relationObject(parameter, vpiExpr);
    ASSERT_NE(defaultExpr, nullptr) << "parameter " << name << " has no default expr";
    EXPECT_EQ(stringProperty(defaultExpr, vpiDecompile), std::string(defaultText));
    EXPECT_EQ(relationSize(parameter, vpiLeftRange), 0u);
    EXPECT_EQ(relationSize(parameter, vpiRightRange), 0u);

    EXPECT_TRUE(endsWith(parameter->getFile(), kSourceFileName)) << parameter->getFile();
    EXPECT_EQ(parameter->getStartLine(), line);
  }
};

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Every line of the file is legal, and the design has its top-level module,
// so nothing is fatal, a syntax error or an error, in any category.
TEST_F(ClassTest49Test, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

// Lines 16-18: a package_declaration with one local parameter (26.2, 6.20.4).
TEST_F(ClassTest49Test, Lines16To18PackageIsAccepted) {
  EXPECT_EQ(errorsOnLines(kPackageFirstLine, kPackageLastLine), "");
}

// Lines 20-22: a function_declaration in the compilation-unit scope (13.4,
// 3.12.1).
TEST_F(ClassTest49Test, Lines20To22FunctionIsAccepted) {
  EXPECT_EQ(errorsOnLines(kFunctionFirstLine, kFunctionLastLine), "");
}

// Lines 24-28: a class_declaration with three parameter items, the last of
// which calls the constant function f (8.3, 6.20.1, 13.4.3).
TEST_F(ClassTest49Test, Lines24To28ClassIsAccepted) { EXPECT_EQ(errorsOnLines(kClassFirstLine, kClassLastLine), ""); }

// Lines 30-31: an empty module_declaration (A.1.2).
TEST_F(ClassTest49Test, Lines30To31ModuleIsAccepted) {
  EXPECT_EQ(errorsOnLines(kModuleFirstLine, kModuleLastLine), "");
}

// Each of these diagnostics, at any severity and on any line, would mean HLC
// misread the text of the file.
TEST_F(ClassTest49Test, ReportsNoMisreadingDiagnostics) {
  // Every declaration matches the grammar (A.1.2, A.1.9, A.2.6, A.8.4), and
  // the file uses no preprocessor directive.
  EXPECT_EQ(findEvent(EventType::PpSyntaxError), nullptr);
  EXPECT_EQ(findEvent(EventType::PaSyntaxError), nullptr);

  // glb, arr, f, a, params_as_class_item, N, P, M and test are no keywords
  // (Table B.1). The longest, params_as_class_item, has 20 characters, within
  // any limit an implementation may set, which "shall be at least 1024
  // characters" (5.6).
  EXPECT_EQ(findEvent(EventType::PaReservedKeyword), nullptr);
  EXPECT_EQ(findEvent(EventType::PaMaxLengthIdentifier), nullptr);

  // Every character of the file is ASCII.
  EXPECT_EQ(findEvent(EventType::PpNonAsciiContent), nullptr);
}

// glb is declared once, on lines 16-18, before the reference glb::arr on line
// 27 (3.12, 26.3), so none of these diagnostics applies, at any severity.
TEST_F(ClassTest49Test, ReportsNoPackageDiagnostics) {
  EXPECT_EQ(findEvent(EventType::CompMultiplyDefinedPackage), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUndefinedPackage), nullptr);
  EXPECT_EQ(findEvent(EventType::ElabUndefinedPackage), nullptr);
}

// The class is declared once, extends nothing, and endclass has no label, so
// none of these diagnostics applies, at any severity.
TEST_F(ClassTest49Test, ReportsNoClassDiagnostics) {
  EXPECT_EQ(findEvent(EventType::CompMultiplyDefinedClass), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUndefinedBaseClass), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUnmatchedLabel), nullptr);
}

// The parameter names are distinct in each scope, every local parameter has
// a value, N, f and glb::arr resolve to the declarations of lines 25, 20 and
// 17, and "f(glb::arr[N]) + 1" is a constant expression, f(glb::arr[N]) being
// a constant function call (13.4.3, A.8.4). So none of these diagnostics
// applies, at any severity.
TEST_F(ClassTest49Test, ReportsNoParameterDiagnostics) {
  EXPECT_EQ(findEvent(EventType::CompMultiplyDefinedParameter), nullptr);
  EXPECT_EQ(findEvent(EventType::CompLocalparamNoValue), nullptr);
  EXPECT_EQ(findEvent(EventType::CompFailedToBind), nullptr);
  EXPECT_EQ(findEvent(EventType::CompNotAConstantExpression), nullptr);
}

// The package, the function, the class and the module are each declared once,
// nothing is instantiated, and test is a top-level module (23.3.1), so none of
// these diagnostics applies, at any severity.
TEST_F(ClassTest49Test, ReportsNoDesignHierarchyDiagnostics) {
  EXPECT_EQ(findEvent(EventType::CompMultiplyDefinedDesignUnit), nullptr);
  EXPECT_EQ(findEvent(EventType::ElabMultiplyDefinedModule), nullptr);
  EXPECT_EQ(findEvent(EventType::ElabNoModuleDefinition), nullptr);
  EXPECT_EQ(findEvent(EventType::ElabNoTopLevelModule), nullptr);
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once (run configuration). It has no `include (22.4), no
// `define (22.5.1) and no macro usage; the apostrophes of '1 and '{1,2,3} are
// no grave accents.
TEST_F(ClassTest49Test, SourceFileHasNoIncludesOrMacros) {
  const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
  ASSERT_EQ(sourceFiles.size(), 1u);
  const hldb::SourceFile *const sourceFile = sourceFiles.front();

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// ---------------------------------------------------------------------------
// Lines 16-18: package glb
// ---------------------------------------------------------------------------

// The file declares one package, glb (26.2), from "package glb;" on line 16
// to "endpackage" on line 18. A compilation-unit package object (vpiUnit
// TRUE, 37.10) is not a declared package and is not counted.
TEST_F(ClassTest49Test, PackageGlbIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countDeclaredPackagesFromSourceFile(m_design->getAllPackages()), 1u);

  const hldb::Package *const package = findPackage();
  ASSERT_NE(package, nullptr);
  EXPECT_EQ(package->getVpiType(), static_cast<uint32_t>(vpiPackage));
  EXPECT_EQ(stringProperty(package, vpiName), "glb");
  EXPECT_NE(integerProperty(package, vpiUnit), 1);
  EXPECT_EQ(package->getStartLine(), kPackageFirstLine);
  EXPECT_EQ(package->getEndLine(), kPackageLastLine);
}

// glb's one item is "localparam int arr[3] = '{1,2,3};": one parameter of the
// package (37.10), local (6.20.4), with the assignment pattern as its default
// expr (37.28 detail 3), declared on line 17.
TEST_F(ClassTest49Test, PackageGlbHoldsLocalparamArr) {
  const hldb::Package *const package = findPackage();
  ASSERT_NE(package, nullptr);
  EXPECT_EQ(relationSize(package, vpiParameter), 1u);
  const hldb::Any *const arr = findInRelation(package, vpiParameter, "arr");
  ASSERT_NE(arr, nullptr) << "no parameter arr in glb";

  EXPECT_EQ(arr->getVpiType(), static_cast<uint32_t>(vpiParameter));
  EXPECT_EQ(stringProperty(arr, vpiName), "arr");
  EXPECT_EQ(integerProperty(arr, vpiLocalParam), 1);
  EXPECT_EQ(relationSize(arr, vpiExpr), 1u);
  EXPECT_EQ(arr->getStartLine(), kArrLine);
}

// ---------------------------------------------------------------------------
// Lines 24-28: class params_as_class_item
// ---------------------------------------------------------------------------

// The file declares one class, params_as_class_item (8.3), in the
// compilation-unit scope (3.12.1). Its object is a class defn (37.31), whose
// vpiName is the class_identifier.
TEST_F(ClassTest49Test, ClassIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 1u);

  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);
  EXPECT_EQ(classDefn->getVpiType(), static_cast<uint32_t>(vpiClassDefn));
  EXPECT_EQ(stringProperty(classDefn, vpiName), "params_as_class_item");
}

// "class params_as_class_item;" is on line 24 and "endclass" on line 28.
TEST_F(ClassTest49Test, ClassIsOnLines24To28) {
  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);

  EXPECT_TRUE(endsWith(classDefn->getFile(), kSourceFileName)) << classDefn->getFile();
  EXPECT_EQ(classDefn->getStartLine(), kClassFirstLine);
  EXPECT_EQ(classDefn->getEndLine(), kClassLastLine);
}

// No virtual, so vpiVirtual is false; no extends, so there is no extends
// object (37.31 detail 6); no other class extends it (37.31 detail 5).
TEST_F(ClassTest49Test, ClassIsNotVirtualAndExtendsNothing) {
  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);

  EXPECT_EQ(integerProperty(classDefn, vpiVirtual), 0);
  EXPECT_EQ(relationSize(classDefn, vpiExtends), 0u);
  EXPECT_EQ(relationSize(classDefn, vpiDerivedClasses), 0u);
}

// "The vpiParameter iteration shall return both the parameters declared in the
// parameter port list of the class declaration and the parameters declared
// within the body of the class declaration as class items" (37.31 detail 7):
// N, P and M from the body, and none from a port list, which the class lacks.
TEST_F(ClassTest49Test, ClassHasThreeParameters) {
  const hldb::ClassDefn *const classDefn = findClass();
  ASSERT_NE(classDefn, nullptr);

  EXPECT_EQ(relationSize(classDefn, vpiParameter), 3u);
  EXPECT_NE(findClassParameter("N"), nullptr);
  EXPECT_NE(findClassParameter("P"), nullptr);
  EXPECT_NE(findClassParameter("M"), nullptr);
}

// "parameter N = 2;": local in a class body (6.20.1), default expr 2, and
// with no type or range, at least 32 bits for the unsized value (6.20.2).
TEST_F(ClassTest49Test, ParameterNIsLocalWithDefault2) {
  expectLocalParameter("N", kParameterNLine, "2");

  const hldb::Any *const parameter = findClassParameter("N");
  ASSERT_NE(parameter, nullptr);
  EXPECT_GE(integerProperty(parameter, vpiSize), kUnsizedParameterMinBits);
}

// "parameter reg P = '1;": local in a class body (6.20.1), of type reg with no
// range, so 1 bit wide (6.9) and unsigned (Table 6-8), default expr '1.
TEST_F(ClassTest49Test, ParameterPIsALocalOneBitReg) {
  expectLocalParameter("P", kParameterPLine, "'1");

  const hldb::Any *const parameter = findClassParameter("P");
  ASSERT_NE(parameter, nullptr);
  EXPECT_EQ(integerProperty(parameter, vpiSize), 1);
  EXPECT_EQ(integerProperty(parameter, vpiSigned), 0);
}

// "localparam M = f(glb::arr[N]) + 1;": local, default expr the sum of the
// constant function call and 1, decompiled with one space around the + (37.59
// detail 2, as interpreted in the header).
TEST_F(ClassTest49Test, ParameterMIsLocalWithFunctionCall) {
  expectLocalParameter("M", kParameterMLine, "f(glb::arr[N]) + 1");
}

// The three class items are parameters, which are no members (8.2), so the
// class has nothing else that 37.31 lets a class defn hold. The implicit
// constructor does not count (37.31 detail 1).
TEST_F(ClassTest49Test, ClassHasNoMembers) {
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
// Lines 30-31: module test
// ---------------------------------------------------------------------------

// The file declares one module, test (A.1.2).
TEST_F(ClassTest49Test, ModuleTestIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);

  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "test");
}

// "module test;" is on line 30 and "endmodule" on line 31.
TEST_F(ClassTest49Test, ModuleTestIsOnLines30To31) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);

  EXPECT_TRUE(endsWith(module->getFile(), kSourceFileName)) << module->getFile();
  EXPECT_EQ(module->getStartLine(), kModuleFirstLine);
  EXPECT_EQ(module->getEndLine(), kModuleLastLine);
}

// test has no ports, parameters or items. The package, the function and the
// class are declared outside it (3.12.1).
TEST_F(ClassTest49Test, ModuleTestIsEmpty) {
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

// The file declares one package, one function, one class and one module, and
// no variable in the compilation-unit scope, so none of these design objects
// comes from it.
TEST_F(ClassTest49Test, NoOtherDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
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
// 30 of this file (vpiDefName, vpiDefFile, vpiDefLineNo, 37.10), and the class
// is not in it (3.12.1). The instance only exists after elaboration.
TEST_F(ClassTest49Test, TestIsTheOnlyTopLevelInstance) {
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
