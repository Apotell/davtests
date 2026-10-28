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

// Test for tests/Google/generic/class/class_test_12.sv
//
// THE SOURCE
//
// Lines 1-7 are one-line comments with the license, lines 8-9 are blank and
// lines 10-14 are one block comment carrying the test metadata (tags 6.15 and
// 8.3). The SystemVerilog text is:
//
//   15: class Bar #(int X=0, int Y=1, int Z=2); endclass
//   16: localparam x=3, y=4, z=5;
//   17:
//   18: class Foo #(int N=1, int P=2) extends Bar #(x,y,z);
//   19: endclass
//   20:
//   21: module test;
//   22: endmodule
//
// The repository stores the file with LF line endings; a Windows checkout
// (core.autocrlf) turns them into CR LF. 5.3 lists "newlines" as white space
// but does not name the carriage return, so this test treats CR LF as one
// line ending and asserts nothing about the CR itself.
//
// WHAT THE LRM SAYS ABOUT IT
//
// Line 15 is a class_declaration (8.3, A.1.2) of the class Bar with a
//   parameter_port_list (A.1.3) of three parameter_port_declarations of the
//   form data_type list_of_param_assignments: X = 0, Y = 1 and Z = 2, each of
//   data_type int. "A parameter with a type specification, but with no range
//   specification, shall be of the type specified" (6.20.2), and int is a
//   "2-state data type, 32-bit signed integer" (6.11, Table 6-8). They are
//   parameters, not local parameters: only "param_assignments appearing within
//   a class body shall become localparam declarations" (6.20.1). Bar has no
//   class_item.
//
// Line 16 declares three local parameters x = 3, y = 4 and z = 5 outside any
//   design element, in the compilation-unit scope, which "contains all
//   declarations that lie outside any other scope" and "can contain any item
//   that can be defined within a package" (3.12.1). They have neither a type
//   nor a range, so each "shall default to the type and range of the final
//   value assigned", and as that value is unsized each has "an implied range
//   with an LSB equal to 0 and an MSB equal to an implementation-dependent
//   value of at least 31" (6.20.2): at least 32 bits. Identifiers are case
//   sensitive (5.6), so x, y and z are not Bar's X, Y and Z.
//
// Lines 18-19 are a class_declaration of the class Foo with the parameters
//   N = 1 and P = 2, both int, and "extends Bar #(x,y,z)": Foo derives from a
//   specialization of Bar whose parameters are overridden by an ordered list
//   (8.25 uses "the same parameter override rules (see 23.10)"). "The order of
//   the assignments in the ... parameter assignment by ordered list shall
//   follow the order of declaration of the parameters" (23.10.2.1), so x
//   overrides X, y overrides Y and z overrides Z. There is no argument list
//   after the class type, so no constructor arguments. Foo has no class_item.
//   The names x, y and z in the override resolve to the local parameters of
//   line 16, the only declarations of those names.
//
// Neither class is virtual, and nothing else derives from either. With no
//   class_item, neither class has a member: "A class's data are referred to as
//   class properties, and its subroutines are called methods; both are members
//   of the class" (8.2).
//
// Lines 21-22 are one module_declaration (A.1.2) with the module_identifier
//   test and no ports, parameters or items. test is never instantiated, so it
//   is a top-level module: "A design shall contain at least one top-level
//   module. A top-level module is implicitly instantiated once, and its
//   instance name is the same as the module name" (23.3.1). With test, the
//   design meets that rule, so even an elaborating run has nothing to report
//   as an error. Every line of the file is legal.
//
// No line holds a compiler directive, so lines are numbered from 1 at the
//   start of the file (22.12).
//
// HOW THE TEST OBSERVES IT
//
// The LRM's object models are queried the way PLI would see them. A class
// defn (37.31) has vpiName, vpiVirtual, vpiParameter (the port-list and body
// parameters, detail 7), vpiExtends ("The relation to vpiExtends exists
// whenever one class is derived from another class ... The relation from
// extends to class typespec provides the base class. The vpiArgument iterator
// from extends shall provide the arguments used in constructor chaining",
// detail 6), vpiDerivedClasses ("all the class defns derived from the given
// class defn", detail 5) and its member relations. A class typespec obtained
// from the extends object represents a lexical construct (37.32 detail 1); its
// param assigns carry, as vpiRhs, "the object for that parameter" when the
// class type "includes an explicit parameter expression" (37.32 detail 3), and
// "vpiLhs from a param assign object shall return a handle to the overridden
// value parameter" (37.28 detail 4). A parameter (37.28) has vpiName,
// vpiLocalParam, vpiSigned, vpiSize, a typespec, vpiExpr ("the default expr",
// detail 3) and vpiLeftRange and vpiRightRange ("a NULL handle" without an
// explicit range, detail 5). vpiDecompile "shall return a string with a
// functionally equivalent expression to the original expression within the
// source code" (37.59 detail 2); for a default expr that is a lone literal
// with no operator, this test takes that string to be the literal as written,
// an LRM interpretation rather than a quoted rule. A module instance (37.10)
// has vpiName,
// vpiDefName, vpiDefFile and vpiDefLineNo. HLDB answers vpi_get() and
// vpi_iterate() through getVpiPropertyValue() and getByVpiType(), so the test
// asks those questions directly with the LRM's vpiXXX codes. The local
// parameters of the compilation-unit scope are read from the Design's
// parameter list.
//
// Every expected value comes from the source text and the LRM, so a red test
// means HLC deviates from the LRM.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors                     no fatal, syntax or error
//                                             diagnostic of any category
//   Line15ClassBarIsAccepted                  none on line 15
//   Line16LocalparamsAreAccepted              none on line 16
//   Lines18To19ClassFooIsAccepted             none on lines 18-19
//   Lines21To22ModuleIsAccepted               none on lines 21-22
//   ReportsNoMisreadingDiagnostics            no syntax, keyword, identifier
//                                             length or non-ASCII diagnostic
//   ReportsNoClassDiagnostics                 no duplicate or undefined class,
//                                             missing base class or label
//                                             mismatch
//   ReportsNoParameterDiagnostics             no duplicate parameter, unbound
//                                             name, bad override or
//                                             non-constant value
//   ReportsNoDesignHierarchyDiagnostics       no duplicate design unit,
//                                             missing definition or missing
//                                             top-level module
//   SourceFileHasNoIncludesOrMacros           one SourceFile, no includes,
//                                             no macro definitions or usages
//   ClassesBarAndFooAreDeclared               exactly two classes from this
//                                             file, Bar and Foo
//   ClassBarIsOnLine15                        Bar lies on line 15 only
//   ClassBarHasThreeParameters                Bar's vpiParameter: 3
//   BarParameterXIsIntWithDefault0            X: int, not local, default 0
//   BarParameterYIsIntWithDefault1            Y: int, not local, default 1
//   BarParameterZIsIntWithDefault2            Z: int, not local, default 2
//   ClassBarIsNotVirtualAndExtendsNothing     vpiVirtual false, no extends
//   ClassBarIsTheBaseOfFoo                    Bar's only derived class is Foo
//   ClassBarHasNoMembers                      no member relation holds
//                                             anything
//   LocalparamsXYZAreDeclared                 three compilation-unit local
//                                             parameters from this file
//   LocalparamXIs3                            x: local, default 3, at least
//                                             32 bits, line 16
//   LocalparamYIs4                            y: the same with 4
//   LocalparamZIs5                            z: the same with 5
//   ClassFooIsOnLines18To19                   Foo starts on 18, ends on 19
//   ClassFooHasTwoParameters                  Foo's vpiParameter: 2
//   FooParameterNIsIntWithDefault1            N: int, not local, default 1
//   FooParameterPIsIntWithDefault2            P: int, not local, default 2
//   ClassFooIsNotVirtualAndHasNoDerivedClass  vpiVirtual false, no derived
//                                             class
//   ClassFooExtendsBar                        one extends, no constructor
//                                             argument, a class typespec of
//                                             the class defn Bar
//   FooOverridesXWithLocalparamX              X <- x, bound to line 16
//   FooOverridesYWithLocalparamY              Y <- y, bound to line 16
//   FooOverridesZWithLocalparamZ              Z <- z, bound to line 16
//   ClassFooHasNoMembers                      no member relation holds
//                                             anything
//   ModuleTestIsDeclared                      exactly one module, test
//   ModuleTestIsOnLines21To22                 it starts on 21, ends on 22
//   ModuleTestIsEmpty                         no variable, net, parameter,
//                                             typedef or class defn in it
//   NoOtherDesignObjectsDeclared              no other design element or
//                                             variable comes from this file
//   TestIsTheOnlyTopLevelInstance             elaborated runs only: one
//                                             top-level instance, test, of the
//                                             definition at line 21
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
//   - The specialization Bar #(3,4,5) and its resolved parameter values. Foo
//     is a generic class that is never specialized, "A generic class is not a
//     type; only a concrete specialization represents a type" (8.25), and the
//     LRM does not say whether the base of an unused generic class is
//     specialized at all, so the vpiClassTypespec iteration of Bar (37.32
//     detail 10) is not asserted. Nearest assertions: the three
//     FooOverrides... tests (the lexical overrides).
//   - Parameter values as returned by vpi_get_value(). They are values "at the
//     end of elaboration" (37.28 detail 1), and this suite reads defaults
//     through vpiExpr instead. Nearest assertions: the parameter tests
//     (vpiDecompile of each default expr).
//   - The sign of x, y and z. 6.20.2 gives them the type of their value but
//     also calls such a parameter "a logic vector"; only the width is asserted.
//     Nearest assertions: LocalparamXIs3, LocalparamYIs4 and LocalparamZIs5.
//   - The order of vpiParameter and vpiParamAssign iterations. The LRM does
//     not fix it, so items are looked up by name. Nearest assertions:
//     ClassBarHasThreeParameters, ClassFooHasTwoParameters and the three
//     FooOverrides... tests.
//   - The Design's parameter assignment list. How HLDB records the
//     assignments of the local parameters is a tool convention. Nearest
//     assertions: LocalparamXIs3, LocalparamYIs4 and LocalparamZIs5.
//   - The implicit constructors. "the iteration over vpiMethods shall not
//     include built-in methods for which there is no explicit declaration"
//     (37.31 detail 1). Nearest assertions: ClassBarHasNoMembers and
//     ClassFooHasNoMembers.
//   - vpiFullName of any object, and the compilation-unit object (vpiUnit,
//     37.10). The full name is a computed property this suite does not assert,
//     and how HLDB groups $unit items is a tool convention. Nearest assertions:
//     ClassesBarAndFooAreDeclared and LocalparamsXYZAreDeclared.
//   - Objects of the classes (6.15). Objects only exist while a simulation
//     runs; this is permanently out of scope. Nearest assertions:
//     ClassBarHasNoMembers and ClassFooHasNoMembers.
//   - The location of the instance itself (vpiFile, vpiLineNo). A top-level
//     instance has no instantiation statement. Nearest assertion:
//     TestIsTheOnlyTopLevelInstance (vpiDefFile and vpiDefLineNo).
//   - The object HLDB returns for a typespec relation: a parameter's typespec
//     (37.28) and the class typespec of Foo's extends object (37.31 detail
//     6).
//     The LRM draws these as direct relations to a typespec; HLDB may instead
//     return an object of its own type that refers to the typespec. That
//     indirection is an HLDB model convention the LRM does not define, so its
//     type is not asserted; the test follows it through vpiActual to the
//     typespec it refers to. Nearest assertion: ClassFooExtendsBar
//     (vpiClassTypespec on that typespec).
//   - Warnings and informational diagnostics. The LRM neither requires nor
//     forbids them. Nearest assertions: CompilesWithoutErrors and the four
//     ReportsNo... tests.
//   - The comments on lines 1-14 (5.4) and columns. The LRM gives comments no
//     meaning. "The compiler shall maintain the current line number" (22.12);
//     the LRM keeps no column for a comment. Nearest
//     assertions: the line checks, which hold only if lines 1-14 were read as
//     whole lines.

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
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "class_test_12.sv";

// vpiVariables, the VPI relation from a scope to its variables (100 in IEEE
// 1800-2023 Annex K, vpi_user.h). HLDB's vpi_user.h names the same relation
// vpiVariable.
constexpr int32_t kVpiVariables = vpiVariable;

// The lines of the declarations.
constexpr uint32_t kBarLine = 15;
constexpr uint32_t kLocalparamLine = 16;
constexpr uint32_t kFooFirstLine = 18;
constexpr uint32_t kFooLastLine = 19;
constexpr uint32_t kModuleFirstLine = 21;
constexpr uint32_t kModuleLastLine = 22;

// The width of int, a "2-state data type, 32-bit signed integer" (Table 6-8).
constexpr int64_t kIntBits = 32;

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

// The parameter an expression names: the expression itself when it is the
// parameter's handle, otherwise the vpiActual of the reference.
const hldb::Any *boundParameter(const hldb::Any *expression) {
  if (expression == nullptr) return nullptr;
  if (expression->getVpiType() == static_cast<uint32_t>(vpiParameter)) return expression;
  return relationObject(expression, vpiActual);
}

// The line a diagnostic points at, or 0 when it carries no location.
uint32_t lineOf(const Error &error) { return error.getLocations().empty() ? 0 : error.getLocations().front().m_line; }

// True when HLC's error table gives 'error' a fatal, syntax or error
// severity. Every category counts: the design has a top-level module, so not
// even elaboration has an error to report (23.3.1).
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

class ClassTest12Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_test_12.hlc"}); }
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

  // Every error-class diagnostic, described; empty when there is none.
  static std::string allErrors() {
    if ((m_session == nullptr) || (m_session->getErrorContainer() == nullptr)) return "no error container";
    std::string description;
    for (const Error &error : m_session->getErrorContainer()->getErrors()) {
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

  // The class named 'name' declared in the file under test, or nullptr.
  static const hldb::ClassDefn *findClass(std::string_view name) {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllClasses(), name);
  }

  // The compilation-unit parameter named 'name' declared in the file under
  // test, or nullptr.
  static const hldb::Any *findUnitParameter(std::string_view name) {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getParameters(), name);
  }

  // The module declared in the file under test, or nullptr.
  static const hldb::Module *findModule() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllModules());
  }

  // The class typespec Foo's extends object leads to: the base class as written
  // in the source, a lexical construct (37.31 detail 6, 37.32 detail 1).
  static const hldb::Any *findFooBaseTypespec() {
    const hldb::ClassDefn *const foo = findClass("Foo");
    if (foo == nullptr) return nullptr;
    const hldb::Any *const extendsObject = relationObject(foo, vpiExtends);
    return (extendsObject == nullptr) ? nullptr : typespecOf(extendsObject, vpiClassTypespec);
  }

  // Checks the parameter 'name' of the class 'owner', declared on 'line' as
  // "int <name>=<defaultText>" in the parameter port list.
  static void expectIntParameter(std::string_view owner, std::string_view name, uint32_t line,
                                 std::string_view defaultText) {
    const hldb::ClassDefn *const classDefn = findClass(owner);
    ASSERT_NE(classDefn, nullptr) << "no class " << owner;
    const hldb::Any *const parameter = findInRelation(classDefn, vpiParameter, name);
    ASSERT_NE(parameter, nullptr) << "no parameter " << name << " in " << owner;

    // A value parameter of the port list, so not a local parameter (6.20.1,
    // 37.31 detail 7).
    EXPECT_EQ(parameter->getVpiType(), static_cast<uint32_t>(vpiParameter));
    EXPECT_EQ(stringProperty(parameter, vpiName), std::string(name));
    EXPECT_EQ(integerProperty(parameter, vpiLocalParam), 0);

    // Of the type specified, int (6.20.2): a 32-bit signed integer (Table 6-8).
    const hldb::Any *const typespec = typespecOf(parameter, vpiTypespec);
    ASSERT_NE(typespec, nullptr) << "parameter " << name << " has no typespec";
    EXPECT_EQ(typespec->getVpiType(), static_cast<uint32_t>(vpiIntTypespec));
    EXPECT_EQ(integerProperty(parameter, vpiSigned), 1);
    EXPECT_EQ(integerProperty(parameter, vpiSize), kIntBits);

    // The default expr (37.28 detail 3) is a lone literal, so this test takes
    // its vpiDecompile to be the literal as written: an interpretation of
    // 37.59 detail 2, which asks only for "a functionally equivalent
    // expression". There is no range (37.28 detail 5).
    const hldb::Any *const defaultExpr = relationObject(parameter, vpiExpr);
    ASSERT_NE(defaultExpr, nullptr) << "parameter " << name << " has no default expr";
    EXPECT_EQ(stringProperty(defaultExpr, vpiDecompile), std::string(defaultText));
    EXPECT_EQ(relationSize(parameter, vpiLeftRange), 0u);
    EXPECT_EQ(relationSize(parameter, vpiRightRange), 0u);

    EXPECT_TRUE(endsWith(parameter->getFile(), kSourceFileName)) << parameter->getFile();
    EXPECT_EQ(parameter->getStartLine(), line);
  }

  // Checks the compilation-unit local parameter 'name' of line 16, declared
  // as "<name>=<defaultText>" with neither type nor range.
  static void expectUnitLocalparam(std::string_view name, std::string_view defaultText) {
    const hldb::Any *const parameter = findUnitParameter(name);
    ASSERT_NE(parameter, nullptr) << "no local parameter " << name;

    EXPECT_EQ(parameter->getVpiType(), static_cast<uint32_t>(vpiParameter));
    EXPECT_EQ(stringProperty(parameter, vpiName), std::string(name));
    EXPECT_EQ(integerProperty(parameter, vpiLocalParam), 1);

    // The default expr (37.28 detail 3) is a lone literal, so this test takes
    // its vpiDecompile to be the literal as written: an interpretation of
    // 37.59 detail 2, which asks only for "a functionally equivalent
    // expression". The unsized value gives at least 32 bits, and there is no
    // explicit range (6.20.2, 37.28 detail 5).
    const hldb::Any *const defaultExpr = relationObject(parameter, vpiExpr);
    ASSERT_NE(defaultExpr, nullptr) << "local parameter " << name << " has no default expr";
    EXPECT_EQ(stringProperty(defaultExpr, vpiDecompile), std::string(defaultText));
    EXPECT_GE(integerProperty(parameter, vpiSize), kUnsizedParameterMinBits);
    EXPECT_EQ(relationSize(parameter, vpiLeftRange), 0u);
    EXPECT_EQ(relationSize(parameter, vpiRightRange), 0u);

    EXPECT_TRUE(endsWith(parameter->getFile(), kSourceFileName)) << parameter->getFile();
    EXPECT_EQ(parameter->getStartLine(), kLocalparamLine);
  }

  // Checks that Foo's base class type overrides Bar's parameter 'overridden'
  // (declared on line 15) with the name 'local', which binds to the
  // compilation-unit local parameter of line 16 (37.32 detail 3, 37.28 detail
  // 4, 23.10.2.1).
  static void expectOverride(std::string_view overridden, std::string_view local) {
    const hldb::Any *const typespec = findFooBaseTypespec();
    ASSERT_NE(typespec, nullptr) << "Foo has no base class typespec";

    const hldb::Any *paramAssign = nullptr;
    if (const std::vector<const hldb::Any *> *const paramAssigns = relationObjects(typespec, vpiParamAssign)) {
      for (const hldb::Any *candidate : *paramAssigns) {
        const hldb::Any *const lhs = (candidate == nullptr) ? nullptr : relationObject(candidate, vpiLhs);
        if ((lhs != nullptr) && (lhs->getName() == overridden)) paramAssign = candidate;
      }
    }
    ASSERT_NE(paramAssign, nullptr) << "no override of " << overridden;

    // vpiLhs is Bar's parameter as declared on line 15.
    const hldb::Any *const lhs = relationObject(paramAssign, vpiLhs);
    EXPECT_EQ(lhs->getVpiType(), static_cast<uint32_t>(vpiParameter));
    EXPECT_EQ(lhs->getStartLine(), kBarLine);

    // vpiRhs is the name as written, bound to the local parameter of line 16,
    // not to Bar's parameter of the same letter in the other case (5.6).
    const hldb::Any *const rhs = relationObject(paramAssign, vpiRhs);
    ASSERT_NE(rhs, nullptr) << "the override of " << overridden << " has no vpiRhs";
    const hldb::Any *const target = boundParameter(rhs);
    ASSERT_NE(target, nullptr) << "the override of " << overridden << " is not bound";
    EXPECT_EQ(target->getName(), local);
    EXPECT_EQ(integerProperty(target, vpiLocalParam), 1);
    EXPECT_TRUE(endsWith(target->getFile(), kSourceFileName)) << target->getFile();
    EXPECT_EQ(target->getStartLine(), kLocalparamLine);
  }

  // Checks that the class 'name' has no member and nothing else 37.31 lets a
  // class defn hold besides parameters (8.2; 37.31 detail 1 leaves out the
  // implicit constructor).
  static void expectNoMembers(std::string_view name) {
    const hldb::ClassDefn *const classDefn = findClass(name);
    ASSERT_NE(classDefn, nullptr) << "no class " << name;

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
TEST_F(ClassTest12Test, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

// Line 15: the class Bar with three int parameters (8.3, A.1.3).
TEST_F(ClassTest12Test, Line15ClassBarIsAccepted) { EXPECT_EQ(errorsOnLines(kBarLine, kBarLine), ""); }

// Line 16: local parameters in the compilation-unit scope (3.12.1, 6.20.1).
TEST_F(ClassTest12Test, Line16LocalparamsAreAccepted) {
  EXPECT_EQ(errorsOnLines(kLocalparamLine, kLocalparamLine), "");
}

// Lines 18-19: the class Foo, derived from a specialization of Bar (8.25).
TEST_F(ClassTest12Test, Lines18To19ClassFooIsAccepted) { EXPECT_EQ(errorsOnLines(kFooFirstLine, kFooLastLine), ""); }

// Lines 21-22: an empty module_declaration (A.1.2).
TEST_F(ClassTest12Test, Lines21To22ModuleIsAccepted) {
  EXPECT_EQ(errorsOnLines(kModuleFirstLine, kModuleLastLine), "");
}

// Each of these diagnostics, at any severity and on any line, would mean HLC
// misread the text of the file.
TEST_F(ClassTest12Test, ReportsNoMisreadingDiagnostics) {
  // Every declaration matches the grammar (A.1.2, A.1.3, A.2.1.1), and the
  // file uses no preprocessor directive.
  EXPECT_EQ(findError(ErrorDefinition::PP_SYNTAX_ERROR), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_SYNTAX_ERROR), nullptr);

  // Bar, X, Y, Z, x, y, z, Foo, N, P and test are no keywords (5.6.2, Table
  // B.1), and none is longer than 4 characters, within any limit an
  // implementation may set, which "shall be at least 1024 characters" (5.6).
  EXPECT_EQ(findError(ErrorDefinition::PA_RESERVED_KEYWORD), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_MAX_LENGTH_IDENTIFIER), nullptr);

  // Every character of the file is ASCII.
  EXPECT_EQ(findError(ErrorDefinition::PP_NON_ASCII_CONTENT), nullptr);
}

// Bar and Foo are each declared once, Foo's base class Bar is declared before
// it, and neither endclass has a label, so none of these diagnostics applies,
// at any severity.
TEST_F(ClassTest12Test, ReportsNoClassDiagnostics) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_MULTIPLY_DEFINED_CLASS), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_CLASS), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_BASE_CLASS), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_UNMATCHED_LABEL), nullptr);
}

// The parameter names are distinct in each scope, every parameter has a
// value, x, y and z resolve to the local parameters of line 16, the override
// list has one constant value for each of Bar's three parameters (23.10.2.1),
// so none of these diagnostics applies, at any severity.
TEST_F(ClassTest12Test, ReportsNoParameterDiagnostics) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_MULTIPLY_DEFINED_PARAMETER), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_LOCALPARAM_NO_VALUE), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_NOT_A_CONSTANT_EXPRESSION), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::ELAB_UNKNOWN_PARAMETER_OVERRIDE), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::ELAB_MISSING_PARAMETER_OVERRIDE), nullptr);
}

// The classes and the module are each declared once, nothing is
// instantiated, and test is a top-level module (23.3.1), so none of these
// diagnostics applies, at any severity.
TEST_F(ClassTest12Test, ReportsNoDesignHierarchyDiagnostics) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_MULTIPLY_DEFINED_DESIGN_UNIT), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::ELAB_MULTIPLY_DEFINED_MODULE), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::ELAB_NO_MODULE_DEFINITION), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::ELAB_NO_TOP_LEVEL_MODULE), nullptr);
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once. It has no `include (22.4), no `define (22.5.1) and
// no macro usage.
TEST_F(ClassTest12Test, SourceFileHasNoIncludesOrMacros) {
  const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
  ASSERT_EQ(sourceFiles.size(), 1u);
  const hldb::SourceFile *const sourceFile = sourceFiles.front();

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// ---------------------------------------------------------------------------
// The classes
// ---------------------------------------------------------------------------

// The file declares two classes, Bar and Foo (8.3), each a class defn (37.31)
// whose vpiName is its class_identifier.
TEST_F(ClassTest12Test, ClassesBarAndFooAreDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 2u);

  const hldb::ClassDefn *const bar = findClass("Bar");
  ASSERT_NE(bar, nullptr);
  EXPECT_EQ(bar->getVpiType(), static_cast<uint32_t>(vpiClassDefn));
  EXPECT_EQ(stringProperty(bar, vpiName), "Bar");

  const hldb::ClassDefn *const foo = findClass("Foo");
  ASSERT_NE(foo, nullptr);
  EXPECT_EQ(foo->getVpiType(), static_cast<uint32_t>(vpiClassDefn));
  EXPECT_EQ(stringProperty(foo, vpiName), "Foo");
}

// ---------------------------------------------------------------------------
// Line 15: class Bar #(int X=0, int Y=1, int Z=2)
// ---------------------------------------------------------------------------

// The whole declaration of Bar, endclass included, is on line 15.
TEST_F(ClassTest12Test, ClassBarIsOnLine15) {
  const hldb::ClassDefn *const bar = findClass("Bar");
  ASSERT_NE(bar, nullptr);

  EXPECT_EQ(bar->getStartLine(), kBarLine);
  EXPECT_EQ(bar->getEndLine(), kBarLine);
}

// Bar declares X, Y and Z in its parameter port list and none in its empty
// body (37.31 detail 7).
TEST_F(ClassTest12Test, ClassBarHasThreeParameters) {
  const hldb::ClassDefn *const bar = findClass("Bar");
  ASSERT_NE(bar, nullptr);
  EXPECT_EQ(relationSize(bar, vpiParameter), 3u);
}

// "int X=0" (6.20.1, 6.20.2).
TEST_F(ClassTest12Test, BarParameterXIsIntWithDefault0) { expectIntParameter("Bar", "X", kBarLine, "0"); }

// "int Y=1" (6.20.1, 6.20.2).
TEST_F(ClassTest12Test, BarParameterYIsIntWithDefault1) { expectIntParameter("Bar", "Y", kBarLine, "1"); }

// "int Z=2" (6.20.1, 6.20.2).
TEST_F(ClassTest12Test, BarParameterZIsIntWithDefault2) { expectIntParameter("Bar", "Z", kBarLine, "2"); }

// Bar's header has no virtual and no extends (37.31 detail 6).
TEST_F(ClassTest12Test, ClassBarIsNotVirtualAndExtendsNothing) {
  const hldb::ClassDefn *const bar = findClass("Bar");
  ASSERT_NE(bar, nullptr);

  EXPECT_EQ(integerProperty(bar, vpiVirtual), 0);
  EXPECT_EQ(relationSize(bar, vpiExtends), 0u);
}

// "The vpiDerivedClasses iterator shall return all the class defns derived
// from the given class defn" (37.31 detail 5): Foo, and only Foo.
TEST_F(ClassTest12Test, ClassBarIsTheBaseOfFoo) {
  const hldb::ClassDefn *const bar = findClass("Bar");
  ASSERT_NE(bar, nullptr);

  EXPECT_EQ(relationSize(bar, vpiDerivedClasses), 1u);
  EXPECT_NE(findInRelation(bar, vpiDerivedClasses, "Foo"), nullptr);
}

// Bar has no class_item, so it has no member (8.2).
TEST_F(ClassTest12Test, ClassBarHasNoMembers) { expectNoMembers("Bar"); }

// ---------------------------------------------------------------------------
// Line 16: localparam x=3, y=4, z=5
// ---------------------------------------------------------------------------

// Line 16 declares three local parameters in the compilation-unit scope
// (3.12.1), and the file declares no other parameter there.
TEST_F(ClassTest12Test, LocalparamsXYZAreDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 3u);
  EXPECT_NE(findUnitParameter("x"), nullptr);
  EXPECT_NE(findUnitParameter("y"), nullptr);
  EXPECT_NE(findUnitParameter("z"), nullptr);
}

// "x=3" (6.20.1, 6.20.2).
TEST_F(ClassTest12Test, LocalparamXIs3) { expectUnitLocalparam("x", "3"); }

// "y=4" (6.20.1, 6.20.2).
TEST_F(ClassTest12Test, LocalparamYIs4) { expectUnitLocalparam("y", "4"); }

// "z=5" (6.20.1, 6.20.2).
TEST_F(ClassTest12Test, LocalparamZIs5) { expectUnitLocalparam("z", "5"); }

// ---------------------------------------------------------------------------
// Lines 18-19: class Foo #(int N=1, int P=2) extends Bar #(x,y,z)
// ---------------------------------------------------------------------------

// "class Foo ... ;" is on line 18 and "endclass" on line 19.
TEST_F(ClassTest12Test, ClassFooIsOnLines18To19) {
  const hldb::ClassDefn *const foo = findClass("Foo");
  ASSERT_NE(foo, nullptr);

  EXPECT_EQ(foo->getStartLine(), kFooFirstLine);
  EXPECT_EQ(foo->getEndLine(), kFooLastLine);
}

// Foo declares N and P in its parameter port list and none in its empty body
// (37.31 detail 7); the parameters of its base class are Bar's, not Foo's.
TEST_F(ClassTest12Test, ClassFooHasTwoParameters) {
  const hldb::ClassDefn *const foo = findClass("Foo");
  ASSERT_NE(foo, nullptr);
  EXPECT_EQ(relationSize(foo, vpiParameter), 2u);
}

// "int N=1" (6.20.1, 6.20.2).
TEST_F(ClassTest12Test, FooParameterNIsIntWithDefault1) { expectIntParameter("Foo", "N", kFooFirstLine, "1"); }

// "int P=2" (6.20.1, 6.20.2).
TEST_F(ClassTest12Test, FooParameterPIsIntWithDefault2) { expectIntParameter("Foo", "P", kFooFirstLine, "2"); }

// Foo's header has no virtual, and no class derives from Foo (37.31 detail 5).
TEST_F(ClassTest12Test, ClassFooIsNotVirtualAndHasNoDerivedClass) {
  const hldb::ClassDefn *const foo = findClass("Foo");
  ASSERT_NE(foo, nullptr);

  EXPECT_EQ(integerProperty(foo, vpiVirtual), 0);
  EXPECT_EQ(relationSize(foo, vpiDerivedClasses), 0u);
}

// Foo derives from Bar, so it has an extends object (37.31 detail 6). No
// argument list follows the base class type, so the extends object has no
// constructor argument, and its class typespec is of the class defn Bar.
TEST_F(ClassTest12Test, ClassFooExtendsBar) {
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
  EXPECT_EQ(base->getName(), "Bar");
  EXPECT_EQ(base->getStartLine(), kBarLine);
}

// "Bar #(x,y,z)": by ordered list, x overrides X (23.10.2.1).
TEST_F(ClassTest12Test, FooOverridesXWithLocalparamX) { expectOverride("X", "x"); }

// "Bar #(x,y,z)": by ordered list, y overrides Y (23.10.2.1).
TEST_F(ClassTest12Test, FooOverridesYWithLocalparamY) { expectOverride("Y", "y"); }

// "Bar #(x,y,z)": by ordered list, z overrides Z (23.10.2.1).
TEST_F(ClassTest12Test, FooOverridesZWithLocalparamZ) { expectOverride("Z", "z"); }

// Foo has no class_item, so it has no member (8.2).
TEST_F(ClassTest12Test, ClassFooHasNoMembers) { expectNoMembers("Foo"); }

// ---------------------------------------------------------------------------
// Lines 21-22: module test
// ---------------------------------------------------------------------------

// The file declares one module, test (A.1.2).
TEST_F(ClassTest12Test, ModuleTestIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);

  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "test");
}

// "module test;" is on line 21 and "endmodule" on line 22.
TEST_F(ClassTest12Test, ModuleTestIsOnLines21To22) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);

  EXPECT_TRUE(endsWith(module->getFile(), kSourceFileName)) << module->getFile();
  EXPECT_EQ(module->getStartLine(), kModuleFirstLine);
  EXPECT_EQ(module->getEndLine(), kModuleLastLine);
}

// test has no ports, parameters or items. The classes and the local
// parameters are declared outside it, in the compilation-unit scope (3.12.1).
TEST_F(ClassTest12Test, ModuleTestIsEmpty) {
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

// The file declares two classes, three local parameters and one module, so no
// other design element or variable comes from it.
TEST_F(ClassTest12Test, NoOtherDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
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
// 21 of this file (vpiDefName, vpiDefFile, vpiDefLineNo, 37.10), and no class
// is in it (3.12.1). The instance only exists after elaboration.
TEST_F(ClassTest12Test, TestIsTheOnlyTopLevelInstance) {
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
