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

// Test for tests/Google/generic/class/class_test_21.sv
//
// THE SOURCE
//
// Lines 1-7 are one-line comments with the license, lines 8-9 are blank and
// lines 10-14 are one block comment carrying the test metadata (tags 6.15 and
// 8.3). The SystemVerilog text is:
//
//   15: package Package;
//   16: class Bar #(int X=0, int Y=1, int Z=2); endclass
//   17: endpackage
//   18: localparam x=3, y=4, z=5;
//   19:
//   20: class Foo extends Package::Bar #(x,y,z); endclass
//   21:
//   22: module test;
//   23: endmodule
//
// The repository stores the file with LF line endings; a Windows checkout
// (core.autocrlf) turns them into CR LF. 5.3 lists "newlines" as white space
// but does not name the carriage return, so this test treats CR LF as one
// line ending and asserts nothing about the CR itself.
//
// WHAT THE LRM SAYS ABOUT IT
//
// Lines 15-17 are a package_declaration (26.2, A.1.2) of the package Package.
//   Identifiers are case sensitive (5.6) and "All keywords are defined in
//   lowercase only" (5.6.2), so the name Package is not the keyword package.
//   Its one package item, on line 16, is a class_declaration, which
//   package_or_generate_item_declaration allows (A.1.11): the class Bar with a
//   parameter_port_list (A.1.3) of three parameters X = 0, Y = 1 and Z = 2,
//   each of data_type int. "A parameter with a type specification, but with no
//   range specification, shall be of the type specified" (6.20.2), and int is
//   a "2-state data type, 32-bit signed integer" (6.11, Table 6-8). They are
//   parameters, not local parameters: only "param_assignments appearing within
//   a class body shall become localparam declarations" (6.20.1). Bar has no
//   class_item.
//
// Line 18 declares three local parameters x = 3, y = 4 and z = 5 after the
//   package, in the compilation-unit scope, which "contains all declarations
//   that lie outside any other scope" and "can contain any item that can be
//   defined within a package" (3.12.1). They have neither a type nor a range,
//   so each "shall default to the type and range of the final value assigned",
//   and as that value is unsized each has "an implied range with an LSB equal
//   to 0 and an MSB equal to an implementation-dependent value of at least 31"
//   (6.20.2): at least 32 bits. x, y and z are not Bar's X, Y and Z (5.6).
//
// Line 20 is a class_declaration of the class Foo, in the compilation-unit
//   scope, with no parameter_port_list and "extends Package::Bar #(x,y,z)".
//   The base class is a class_type whose ps_class_identifier carries the
//   package_scope "Package ::" (A.2.2.1, A.9.3), so it names the class Bar of
//   the package Package. Foo derives from a specialization of that Bar whose
//   parameters are overridden by an ordered list (8.25 uses "the same parameter
//   override rules (see 23.10)"): "The order of the assignments in the ...
//   parameter assignment by ordered list shall follow the order of declaration
//   of the parameters" (23.10.2.1), so x overrides X, y overrides Y and z
//   overrides Z. No argument list follows the class type, so there are no
//   constructor arguments. The names x, y and z resolve to the local
//   parameters of line 18, the only declarations of those names. Foo has no
//   class_item and no parameter of its own.
//
// Neither class is virtual, and nothing else derives from either. With no
//   class_item, neither class has a member: "A class's data are referred to as
//   class properties, and its subroutines are called methods; both are members
//   of the class" (8.2).
//
// Lines 22-23 are one module_declaration (A.1.2) with the module_identifier
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
// The LRM's object models are queried the way PLI would see them. A package
// is an instance (37.10) and holds its classes through the class defn
// relation. A class defn (37.31) has vpiName, vpiVirtual, vpiParameter (the
// port-list and body parameters, detail 7), vpiExtends ("The relation to
// vpiExtends exists whenever one class is derived from another class ... The
// relation from extends to class typespec provides the base class. The
// vpiArgument iterator from extends shall provide the arguments used in
// constructor chaining", detail 6), vpiDerivedClasses ("all the class defns
// derived from the given class defn", detail 5) and its member relations. A
// class typespec obtained from the extends object represents a lexical
// construct (37.32 detail 1); its param assigns carry, as vpiRhs, "the object
// for that parameter" when the class type "includes an explicit parameter
// expression" (37.32 detail 3), and "vpiLhs from a param assign object shall
// return a handle to the overridden value parameter" (37.28 detail 4). A
// parameter (37.28) has vpiName, vpiLocalParam, vpiSigned, vpiSize, a
// typespec, vpiExpr ("the default expr", detail 3) and vpiLeftRange and
// vpiRightRange ("a NULL handle" without an explicit range, detail 5).
// vpiDecompile "shall return a string with a functionally equivalent
// expression to the original expression within the source code" (37.59
// detail 2); for a default expr that is a lone literal with no operator, this
// test takes that string to be the literal as written, an LRM interpretation
// rather than a quoted rule.
// A module instance (37.10) has vpiName, vpiDefName, vpiDefFile and
// vpiDefLineNo. HLDB keeps each of these as a typed member of the object, so
// the test reads them through HLDB's typed getters: getClassDefns() on the
// package; getName(), getVirtual(), getParameters(), getExtends(),
// getDerivedClasses() and the member getters on a class defn;
// getClassTypespecs() and getArguments() on the extends object;
// getClassDefn() and getParamAssigns() on the class typespec; getLhs() and
// getRhs() on a param assign; getLocalParam(), getTypespec() and getRanges()
// on a parameter; getDecompile() and getSize() on a literal; and getDefName(),
// getDefFile() and getDefLineNo() on the instance. The default expr of a
// parameter (vpiExpr, 37.28 detail 3) is read with getExpr(). HLDB has no
// typed getter for a parameter's vpiSize or vpiSigned, so the test reads them
// from where the LRM puts them: the int typespec of a typed parameter (the
// type fixes 32 bits, Table 6-8, and getSigned() gives the sign), and the
// default literal of an untyped one, which takes "the type and range of the
// final value assigned" (6.20.2). Packages, compilation-unit classes and
// compilation-unit local parameters are read from the Design's lists.
//
// Every expected value comes from the source text and the LRM, so a red test
// means HLC deviates from the LRM.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors                     no fatal, syntax or error
//                                             diagnostic of any category
//   Lines15To17PackageIsAccepted              none on lines 15-17
//   Line18LocalparamsAreAccepted              none on line 18
//   Line20ClassFooIsAccepted                  none on line 20
//   Lines22To23ModuleIsAccepted               none on lines 22-23
//   SourceFileHasNoIncludesOrMacros           one SourceFile, no includes,
//                                             no macro definitions or usages
//   PackagePackageIsDeclared                  exactly one package from this
//                                             file, Package, on lines 15-17
//   PackageHoldsClassBar                      its one class defn is Bar
//   ClassBarIsOnLine16                        Bar lies on line 16 only
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
//                                             32 bits, line 18
//   LocalparamYIs4                            y: the same with 4
//   LocalparamZIs5                            z: the same with 5
//   ClassFooIsDeclared                        Foo is a compilation-unit class
//                                             defn from this file
//   ClassFooIsOnLine20                        Foo lies on line 20 only
//   ClassFooHasNoParameters                   Foo's vpiParameter is empty
//   ClassFooIsNotVirtualAndHasNoDerivedClass  vpiVirtual false, no derived
//                                             class
//   ClassFooExtendsPackageBar                 one extends, no constructor
//                                             argument, a class typespec of
//                                             the package's class defn Bar
//   FooOverridesXWithLocalparamX              X <- x, bound to line 18
//   FooOverridesYWithLocalparamY              Y <- y, bound to line 18
//   FooOverridesZWithLocalparamZ              Z <- z, bound to line 18
//   ClassFooHasNoMembers                      no member relation holds
//                                             anything
//   ModuleTestIsDeclared                      exactly one module, test
//   ModuleTestIsOnLines22To23                 it starts on 22, ends on 23
//   ModuleTestIsEmpty                         no variable, net, parameter,
//                                             typedef or class defn in it
//   NoOtherDesignObjectsDeclared              no other design element or
//                                             variable comes from this file
//   TestIsTheOnlyTopLevelInstance             elaborated runs only: one
//                                             top-level instance, test, of the
//                                             definition at line 22
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
//   - The specialization Package::Bar #(3,4,5) and its resolved parameter
//     values. The LRM requires the base class type to resolve but does not say
//     whether the vpiClassTypespec iteration of Bar (37.32 detail 10) lists it,
//     and its values are values "at the end of elaboration" (37.28 detail 1).
//     Nearest assertions: the three FooOverrides... tests (the lexical
//     overrides).
//   - Parameter values as returned by vpi_get_value(). This suite reads the
//     default exprs instead. Nearest assertions: the parameter tests
//     (vpiDecompile of each default expr).
//   - The sign of x, y and z. 6.20.2 gives them the type of their value but
//     also calls such a parameter "a logic vector"; only the width is asserted.
//     Nearest assertions: LocalparamXIs3, LocalparamYIs4 and LocalparamZIs5.
//   - Whether the Design's list of all classes also lists Bar. How HLDB lists
//     package classes is a tool convention; Bar is reached through its package.
//     Nearest assertions: PackageHoldsClassBar and ClassFooIsDeclared.
//   - The order of vpiParameter and vpiParamAssign iterations. The LRM does
//     not fix it, so items are looked up by name. Nearest assertions:
//     ClassBarHasThreeParameters and the three FooOverrides... tests.
//   - The rest of the Design's parameter assignment list. The test takes only
//     the default exprs of x, y and z from it; what else HLDB records there is
//     a tool convention. Nearest assertions: LocalparamXIs3, LocalparamYIs4
//     and LocalparamZIs5.
//   - The implicit constructors. "the iteration over vpiMethods shall not
//     include built-in methods for which there is no explicit declaration"
//     (37.31 detail 1). Nearest assertions: ClassBarHasNoMembers and
//     ClassFooHasNoMembers.
//   - vpiFullName of any object, and the compilation-unit object (vpiUnit,
//     37.10). The full name is a computed property this suite does not assert,
//     and how HLDB groups $unit items is a tool convention. Nearest assertions:
//     ClassFooIsDeclared and LocalparamsXYZAreDeclared.
//   - Objects of the classes (6.15). Objects only exist while a simulation
//     runs; this is permanently out of scope. Nearest assertions:
//     ClassBarHasNoMembers and ClassFooHasNoMembers.
//   - The location of the instance itself (vpiFile, vpiLineNo). A top-level
//     instance has no instantiation statement. Nearest assertion:
//     TestIsTheOnlyTopLevelInstance (vpiDefFile and vpiDefLineNo).
//   - The object HLDB returns for a typespec relation, a parameter's typespec
//     (37.28) and the class typespec of Foo's extends object (37.31 detail
//     6), and for the vpiLhs of a param assign (37.28 detail 4).
//     The LRM draws these as direct relations to the typespec or parameter;
//     HLDB may instead return an object of its own type that refers to it.
//     That indirection is an HLDB model convention the LRM does not define, so
//     its type is not asserted; the test follows it through getActual() to the
//     object it refers to. Nearest assertions: ClassFooExtendsPackageBar (a
//     class typespec) and the three FooOverrides... tests (Bar's parameter).
//   - Warnings and informational diagnostics. The LRM neither requires nor
//     forbids them. Nearest assertions: CompilesWithoutErrors and the five
//     ReportsNo... tests.
//   - The comments on lines 1-14 (5.4) and columns. The LRM gives comments no
//     meaning. "The compiler shall maintain the current line number" (22.12);
//     the LRM keeps no column for a comment. Nearest
//     assertions: the line checks, which hold only if lines 1-14 were read as
//     whole lines.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/checker_decl.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/config_decl.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/extends.h>
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
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "class_test_21.sv";

// The lines of the declarations.
constexpr uint32_t kPackageFirstLine = 15;
constexpr uint32_t kBarLine = 16;
constexpr uint32_t kPackageLastLine = 17;
constexpr uint32_t kLocalparamLine = 18;
constexpr uint32_t kFooLine = 20;
constexpr uint32_t kModuleFirstLine = 22;
constexpr uint32_t kModuleLastLine = 23;

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
      if (endsWith(object->getFile(), kSourceFileName) &&
          (name.empty() || (object->getName() == name) || (hldb::getDefName(object) == name))) {
        return object;
      }
    }
  }
  return nullptr;
}

// The base class typespec of 'extendsObject' (37.31 detail 6), or nullptr.
const hldb::Typespec *baseTypespecOf(const hldb::Extends *extendsObject) {
  const hldb::RefTypespecCollection *const typespecs = extendsObject->getClassTypespecs();
  return ((typespecs == nullptr) || typespecs->empty()) ? nullptr
                                                        : hldb::getActual<hldb::Typespec>(typespecs->front());
}

// The parameter an expression names: the expression itself when it is the
// parameter, otherwise the parameter the reference is bound to.
const hldb::Parameter *boundParameter(const hldb::Any *expression) {
  if (const hldb::Parameter *const parameter = any_cast<hldb::Parameter>(expression)) return parameter;
  const hldb::RefObj *const reference = any_cast<hldb::RefObj>(expression);
  return (reference == nullptr) ? nullptr : any_cast<hldb::Parameter>(reference->getActual());
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

class ClassTest21Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_test_21.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Every error-class diagnostic on a line from 'firstLine' to 'lastLine',
  // described; empty when there is none. A run without an event listener is
  // reported, not passed.
  static std::string eventsOnLines(uint32_t firstLine, uint32_t lastLine) {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const Event &event : listener->getEvents()) {
      const uint32_t line = lineOf(event);
      if (isErrorClass(event) && (line >= firstLine) && (line <= lastLine)) {
        std::vector<std::string> msgs = hldb::renderEvent(event);
        std::copy(msgs.cbegin(), msgs.cend(), std::ostream_iterator<std::string>(oss));
      }
    }
    return oss.str();
  }

  // Every error-class diagnostic, described; empty when there is none.
  static std::string allEvents() {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const hldb::Event &event : listener->getEvents()) {
      if (isErrorClass(event)) {
        std::vector<std::string> msgs = hldb::renderEvent(event);
        std::copy(msgs.cbegin(), msgs.cend(), std::ostream_iterator<std::string>(oss));
      }
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

  // The package Package declared in the file under test, or nullptr.
  static const hldb::Package *findPackage() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllPackages(), "Package");
  }

  // The class Bar of the package Package, or nullptr.
  static const hldb::ClassDefn *findBar() {
    const hldb::Package *const package = findPackage();
    return (package == nullptr) ? nullptr : hldb::findByDefName<hldb::ClassDefn>("Bar", package->getClassDefns());
  }

  // The compilation-unit class Foo declared in the file under test, or
  // nullptr.
  static const hldb::ClassDefn *findFoo() {
    return (m_unit == nullptr) ? nullptr : findFromSourceFile(m_unit->getClassDefns(), "Foo");
  }

  // The compilation-unit parameter named 'name' declared in the file under
  // test, or nullptr.
  static const hldb::Any *findUnitParameter(std::string_view name) {
    return (m_unit == nullptr) ? nullptr : findFromSourceFile(m_unit->getParameters(), name);
  }

  // The module declared in the file under test, or nullptr.
  static const hldb::Module *findModule() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllModules());
  }

  // The class typespec Foo's extends object leads to: the base class as written
  // in the source, a lexical construct (37.31 detail 6, 37.32 detail 1).
  static const hldb::ClassTypespec *findFooBaseTypespec() {
    const hldb::ClassDefn *const foo = findFoo();
    const hldb::Extends *const extendsObject = (foo == nullptr) ? nullptr : foo->getExtends();
    return (extendsObject == nullptr) ? nullptr : any_cast<hldb::ClassTypespec>(baseTypespecOf(extendsObject));
  }

  // Checks the parameter 'name' of Bar, declared on line 16 as
  // "int <name>=<defaultText>" in the parameter port list.
  static void expectBarParameter(std::string_view name, std::string_view defaultText) {
    const hldb::ClassDefn *const bar = findBar();
    ASSERT_NE(bar, nullptr) << "no class Bar in Package";
    const hldb::Parameter *const parameter = hldb::findByName<hldb::Parameter>(name, bar->getParameters());
    ASSERT_NE(parameter, nullptr) << "no parameter " << name << " in Bar";

    // A value parameter of the port list, so not a local parameter (6.20.1,
    // 37.31 detail 7).
    EXPECT_EQ(parameter->getName(), name);
    EXPECT_FALSE(parameter->getLocalParam());

    // Of the type specified, int (6.20.2): a 32-bit signed integer (Table 6-8).
    const hldb::Typespec *const typespec = hldb::getActual<hldb::Typespec>(parameter->getTypespec());
    ASSERT_NE(typespec, nullptr) << "parameter " << name << " has no typespec";
    EXPECT_EQ(typespec->getAnyType(), hldb::AnyType::IntTypespec);
    const hldb::IntTypespec *const intTypespec = any_cast<hldb::IntTypespec>(typespec);
    ASSERT_NE(intTypespec, nullptr) << "parameter " << name << " is not an int";
    EXPECT_TRUE(intTypespec->getSigned());

    // The default expr (37.28 detail 3) is a lone literal, so this test takes
    // its vpiDecompile to be the literal as written: an interpretation of
    // 37.59 detail 2, which asks only for "a functionally equivalent
    // expression". There is no range (37.28 detail 5).
    const hldb::Any *const defaultExpr = parameter->getExpr();
    ASSERT_NE(defaultExpr, nullptr) << "parameter " << name << " has no default expr";
    const hldb::Constant *const literal = any_cast<hldb::Constant>(defaultExpr);
    ASSERT_NE(literal, nullptr) << "the default expr of " << name << " is not a literal";
    EXPECT_EQ(literal->getDecompile(), defaultText);

    EXPECT_TRUE(endsWith(parameter->getFile(), kSourceFileName)) << parameter->getFile();
    EXPECT_EQ(parameter->getStartLine(), kBarLine);
  }

  // Checks the compilation-unit local parameter 'name' of line 18, declared
  // as "<name>=<defaultText>" with neither type nor range.
  static void expectUnitLocalparam(std::string_view name, std::string_view defaultText) {
    const hldb::Any *const object = findUnitParameter(name);
    ASSERT_NE(object, nullptr) << "no local parameter " << name;

    EXPECT_EQ(object->getAnyType(), hldb::AnyType::Parameter);
    const hldb::Parameter *const parameter = any_cast<hldb::Parameter>(object);
    ASSERT_NE(parameter, nullptr) << name << " is not a value parameter";
    EXPECT_EQ(parameter->getName(), name);
    EXPECT_TRUE(parameter->getLocalParam());

    // The default expr (37.28 detail 3) is a lone literal, so this test takes
    // its vpiDecompile to be the literal as written: an interpretation of
    // 37.59 detail 2, which asks only for "a functionally equivalent
    // expression". The parameter takes the range of that unsized value, at
    // least 32 bits, and there is no explicit range (6.20.2, 37.28 detail 5).
    const hldb::Any *const defaultExpr = parameter->getExpr();
    ASSERT_NE(defaultExpr, nullptr) << "local parameter " << name << " has no default expr";
    const hldb::Constant *const literal = any_cast<hldb::Constant>(defaultExpr);
    ASSERT_NE(literal, nullptr) << "the default expr of " << name << " is not a literal";
    EXPECT_EQ(literal->getDecompile(), defaultText);
    EXPECT_GE(literal->getSize(), kUnsizedParameterMinBits);

    EXPECT_TRUE(endsWith(parameter->getFile(), kSourceFileName)) << parameter->getFile();
    EXPECT_EQ(parameter->getStartLine(), kLocalparamLine);
  }

  // Checks that Foo's base class type overrides Bar's parameter 'overridden'
  // (declared on line 16) with the name 'local', which binds to the
  // compilation-unit local parameter of line 18 (37.32 detail 3, 37.28 detail
  // 4, 23.10.2.1).
  static void expectOverride(std::string_view overridden, std::string_view local) {
    const hldb::ClassTypespec *const typespec = findFooBaseTypespec();
    ASSERT_NE(typespec, nullptr) << "Foo has no base class typespec";

    const hldb::ParamAssign *const paramAssign =
        hldb::findByName<hldb::ParamAssign>(overridden, typespec->getParamAssigns());
    ASSERT_NE(paramAssign, nullptr) << "no override of " << overridden;

    // vpiLhs is Bar's parameter as declared on line 16.
    const hldb::Parameter *const lhs = boundParameter(paramAssign->getLhs());
    ASSERT_NE(lhs, nullptr) << "the override of " << overridden << " names no parameter";
    EXPECT_EQ(lhs->getStartLine(), kBarLine);

    // vpiRhs is the name as written, bound to the local parameter of line 18,
    // not to Bar's parameter of the same letter in the other case (5.6).
    const hldb::Any *const rhs = paramAssign->getRhs();
    ASSERT_NE(rhs, nullptr) << "the override of " << overridden << " has no vpiRhs";
    const hldb::Parameter *const target = boundParameter(rhs);
    ASSERT_NE(target, nullptr) << "the override of " << overridden << " is not bound";
    EXPECT_EQ(target->getName(), local);
    EXPECT_TRUE(target->getLocalParam());
    EXPECT_TRUE(endsWith(target->getFile(), kSourceFileName)) << target->getFile();
    EXPECT_EQ(target->getStartLine(), kLocalparamLine);
  }

  // Checks that 'classDefn' has no member and nothing else 37.31 lets a class
  // defn hold besides parameters (8.2; 37.31 detail 1 leaves out the implicit
  // constructor).
  static void expectNoMembers(const hldb::ClassDefn *classDefn) {
    ASSERT_NE(classDefn, nullptr);

    EXPECT_EQ(sizeOf(classDefn->getVariables()), 0u);
    EXPECT_EQ(sizeOf(classDefn->getMethods()), 0u);
    EXPECT_EQ(sizeOf(classDefn->getConstraints()), 0u);
    EXPECT_EQ(sizeOf(classDefn->getTypedefs()), 0u);
    EXPECT_EQ(sizeOf(classDefn->getNamedEvents()), 0u);
    EXPECT_EQ(sizeOf(classDefn->getNamedEventArrays()), 0u);
    EXPECT_EQ(sizeOf(classDefn->getInternalScopes()), 0u);
  }
};

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Every line of the file is legal, and the design has its top-level module,
// so nothing is fatal, a syntax error or an error, in any category.
TEST_F(ClassTest21Test, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

// Lines 15-17: the package Package holding the class Bar (26.2, 8.3).
TEST_F(ClassTest21Test, Lines15To17PackageIsAccepted) {
  EXPECT_EQ(eventsOnLines(kPackageFirstLine, kPackageLastLine), "");
}

// Line 18: local parameters in the compilation-unit scope (3.12.1, 6.20.1).
TEST_F(ClassTest21Test, Line18LocalparamsAreAccepted) {
  EXPECT_EQ(eventsOnLines(kLocalparamLine, kLocalparamLine), "");
}

// Line 20: the class Foo, derived from a specialization of Package::Bar
// (8.25, A.2.2.1).
TEST_F(ClassTest21Test, Line20ClassFooIsAccepted) { EXPECT_EQ(eventsOnLines(kFooLine, kFooLine), ""); }

// Lines 22-23: an empty module_declaration (A.1.2).
TEST_F(ClassTest21Test, Lines22To23ModuleIsAccepted) {
  EXPECT_EQ(eventsOnLines(kModuleFirstLine, kModuleLastLine), "");
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once. It has no `include (22.4), no `define (22.5.1) and
// no macro usage.
TEST_F(ClassTest21Test, SourceFileHasNoIncludesOrMacros) {
  const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
  ASSERT_EQ(sourceFiles.size(), 1u);
  const hldb::SourceFile *const sourceFile = sourceFiles.front();

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// ---------------------------------------------------------------------------
// Lines 15-17: package Package
// ---------------------------------------------------------------------------

// The file declares one package, Package (26.2), from "package Package;" on
// line 15 to "endpackage" on line 17.
TEST_F(ClassTest21Test, PackagePackageIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 1u);

  const hldb::Package *const package = findPackage();
  ASSERT_NE(package, nullptr);
  EXPECT_EQ(package->getAnyType(), hldb::AnyType::Package);
  EXPECT_EQ(package->getName(), "Package");
  EXPECT_EQ(package->getStartLine(), kPackageFirstLine);
  EXPECT_EQ(package->getEndLine(), kPackageLastLine);
}

// The package's one item is the class Bar, which it holds as a class defn
// (37.10).
TEST_F(ClassTest21Test, PackageHoldsClassBar) {
  const hldb::Package *const package = findPackage();
  ASSERT_NE(package, nullptr);

  EXPECT_EQ(sizeOf(package->getClassDefns()), 1u);
  const hldb::ClassDefn *const bar = findBar();
  ASSERT_NE(bar, nullptr);
  EXPECT_EQ(bar->getAnyType(), hldb::AnyType::ClassDefn);
  EXPECT_EQ(bar->getDefName(), "Bar");
}

// ---------------------------------------------------------------------------
// Line 16: class Bar #(int X=0, int Y=1, int Z=2)
// ---------------------------------------------------------------------------

// The whole declaration of Bar, endclass included, is on line 16.
TEST_F(ClassTest21Test, ClassBarIsOnLine16) {
  const hldb::ClassDefn *const bar = findBar();
  ASSERT_NE(bar, nullptr);

  EXPECT_TRUE(endsWith(bar->getFile(), kSourceFileName)) << bar->getFile();
  EXPECT_EQ(bar->getStartLine(), kBarLine);
  EXPECT_EQ(bar->getEndLine(), kBarLine);
}

// Bar declares X, Y and Z in its parameter port list and none in its empty
// body (37.31 detail 7).
TEST_F(ClassTest21Test, ClassBarHasThreeParameters) {
  const hldb::ClassDefn *const bar = findBar();
  ASSERT_NE(bar, nullptr);
  EXPECT_EQ(sizeOf(bar->getParameters()), 3u);
}

// "int X=0" (6.20.1, 6.20.2).
TEST_F(ClassTest21Test, BarParameterXIsIntWithDefault0) { expectBarParameter("X", "0"); }

// "int Y=1" (6.20.1, 6.20.2).
TEST_F(ClassTest21Test, BarParameterYIsIntWithDefault1) { expectBarParameter("Y", "1"); }

// "int Z=2" (6.20.1, 6.20.2).
TEST_F(ClassTest21Test, BarParameterZIsIntWithDefault2) { expectBarParameter("Z", "2"); }

// Bar's header has no virtual and no extends (37.31 detail 6).
TEST_F(ClassTest21Test, ClassBarIsNotVirtualAndExtendsNothing) {
  const hldb::ClassDefn *const bar = findBar();
  ASSERT_NE(bar, nullptr);

  EXPECT_FALSE(bar->getVirtual());
  EXPECT_EQ(bar->getExtends(), nullptr);
}

// "The vpiDerivedClasses iterator shall return all the class defns derived
// from the given class defn" (37.31 detail 5): Foo, and only Foo.
TEST_F(ClassTest21Test, ClassBarIsTheBaseOfFoo) {
  const hldb::ClassDefn *const bar = findBar();
  ASSERT_NE(bar, nullptr);

  EXPECT_EQ(sizeOf(bar->getDerivedClasses()), 1u);
  EXPECT_NE(hldb::findByDefName<hldb::ClassDefn>("Foo", bar->getDerivedClasses()), nullptr);
}

// Bar has no class_item, so it has no member (8.2).
TEST_F(ClassTest21Test, ClassBarHasNoMembers) { expectNoMembers(findBar()); }

// ---------------------------------------------------------------------------
// Line 18: localparam x=3, y=4, z=5
// ---------------------------------------------------------------------------

// Line 18 declares three local parameters in the compilation-unit scope
// (3.12.1); Bar's parameters belong to Bar, so the file declares no other
// parameter there.
TEST_F(ClassTest21Test, LocalparamsXYZAreDeclared) {
  ASSERT_NE(m_unit, nullptr);
  EXPECT_EQ(countFromSourceFile(m_unit->getParameters()), 3u);
  EXPECT_NE(findUnitParameter("x"), nullptr);
  EXPECT_NE(findUnitParameter("y"), nullptr);
  EXPECT_NE(findUnitParameter("z"), nullptr);
}

// "x=3" (6.20.1, 6.20.2).
TEST_F(ClassTest21Test, LocalparamXIs3) { expectUnitLocalparam("x", "3"); }

// "y=4" (6.20.1, 6.20.2).
TEST_F(ClassTest21Test, LocalparamYIs4) { expectUnitLocalparam("y", "4"); }

// "z=5" (6.20.1, 6.20.2).
TEST_F(ClassTest21Test, LocalparamZIs5) { expectUnitLocalparam("z", "5"); }

// ---------------------------------------------------------------------------
// Line 20: class Foo extends Package::Bar #(x,y,z)
// ---------------------------------------------------------------------------

// Foo is declared in the compilation-unit scope (3.12.1), as a class defn
// (37.31) whose vpiName is its class_identifier.
TEST_F(ClassTest21Test, ClassFooIsDeclared) {
  const hldb::ClassDefn *const foo = findFoo();
  ASSERT_NE(foo, nullptr);
  EXPECT_EQ(foo->getAnyType(), hldb::AnyType::ClassDefn);
  EXPECT_EQ(foo->getDefName(), std::string_view("Foo"));
}

// The whole declaration of Foo, endclass included, is on line 20.
TEST_F(ClassTest21Test, ClassFooIsOnLine20) {
  const hldb::ClassDefn *const foo = findFoo();
  ASSERT_NE(foo, nullptr);

  EXPECT_EQ(foo->getStartLine(), kFooLine);
  EXPECT_EQ(foo->getEndLine(), kFooLine);
}

// Foo has no parameter_port_list and an empty body (37.31 detail 7); the
// parameters of its base class are Bar's, not Foo's.
TEST_F(ClassTest21Test, ClassFooHasNoParameters) {
  const hldb::ClassDefn *const foo = findFoo();
  ASSERT_NE(foo, nullptr);
  EXPECT_EQ(sizeOf(foo->getParameters()), 0u);
}

// Foo's header has no virtual, and no class derives from Foo (37.31 detail 5).
TEST_F(ClassTest21Test, ClassFooIsNotVirtualAndHasNoDerivedClass) {
  const hldb::ClassDefn *const foo = findFoo();
  ASSERT_NE(foo, nullptr);

  EXPECT_FALSE(foo->getVirtual());
  EXPECT_EQ(sizeOf(foo->getDerivedClasses()), 0u);
}

// Foo derives from Package::Bar, so it has an extends object (37.31 detail 6).
// No argument list follows the base class type, so the extends object has no
// constructor argument, and its class typespec is of the class defn Bar that
// the package holds.
TEST_F(ClassTest21Test, ClassFooExtendsPackageBar) {
  const hldb::ClassDefn *const foo = findFoo();
  ASSERT_NE(foo, nullptr);
  const hldb::Extends *const extendsObject = foo->getExtends();
  ASSERT_NE(extendsObject, nullptr);
  EXPECT_EQ(sizeOf(extendsObject->getArguments()), 0u);

  const hldb::Typespec *const typespec = baseTypespecOf(extendsObject);
  ASSERT_NE(typespec, nullptr);
  EXPECT_EQ(typespec->getAnyType(), hldb::AnyType::ClassTypespec);
  const hldb::ClassTypespec *const classTypespec = any_cast<hldb::ClassTypespec>(typespec);
  ASSERT_NE(classTypespec, nullptr);
  const hldb::ClassDefn *const base = classTypespec->getClassDefn();
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->getDefName(), "Bar");
  EXPECT_EQ(base->getStartLine(), kBarLine);
}

// "Package::Bar #(x,y,z)": by ordered list, x overrides X (23.10.2.1).
TEST_F(ClassTest21Test, FooOverridesXWithLocalparamX) { expectOverride("X", "x"); }

// "Package::Bar #(x,y,z)": by ordered list, y overrides Y (23.10.2.1).
TEST_F(ClassTest21Test, FooOverridesYWithLocalparamY) { expectOverride("Y", "y"); }

// "Package::Bar #(x,y,z)": by ordered list, z overrides Z (23.10.2.1).
TEST_F(ClassTest21Test, FooOverridesZWithLocalparamZ) { expectOverride("Z", "z"); }

// Foo has no class_item, so it has no member (8.2).
TEST_F(ClassTest21Test, ClassFooHasNoMembers) { expectNoMembers(findFoo()); }

// ---------------------------------------------------------------------------
// Lines 22-23: module test
// ---------------------------------------------------------------------------

// The file declares one module, test (A.1.2).
TEST_F(ClassTest21Test, ModuleTestIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);

  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getDefName(), std::string_view("test"));
}

// "module test;" is on line 22 and "endmodule" on line 23.
TEST_F(ClassTest21Test, ModuleTestIsOnLines22To23) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);

  EXPECT_TRUE(endsWith(module->getFile(), kSourceFileName)) << module->getFile();
  EXPECT_EQ(module->getStartLine(), kModuleFirstLine);
  EXPECT_EQ(module->getEndLine(), kModuleLastLine);
}

// test has no ports, parameters or items. The package, the classes and the
// local parameters are declared outside it (3.12.1).
TEST_F(ClassTest21Test, ModuleTestIsEmpty) {
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

// The file declares one package, two classes, three local parameters and one
// module, so no other design element or variable comes from it.
TEST_F(ClassTest21Test, NoOtherDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);

  ASSERT_NE(m_unit, nullptr);
  EXPECT_EQ(countFromSourceFile(m_unit->getCheckerDecls()), 0u);
  EXPECT_EQ(countFromSourceFile(m_unit->getVariables()), 0u);
}

// ---------------------------------------------------------------------------
// Elaboration: the top-level instance
// ---------------------------------------------------------------------------

// test is never instantiated, so it is the design's top-level module: it "is
// implicitly instantiated once, and its instance name is the same as the
// module name" (23.3.1). The instance's definition is test, declared on line
// 22 of this file (vpiDefName, vpiDefFile, vpiDefLineNo, 37.10), and no class
// is in it (3.12.1). The instance only exists after elaboration.
TEST_F(ClassTest21Test, TestIsTheOnlyTopLevelInstance) {
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
