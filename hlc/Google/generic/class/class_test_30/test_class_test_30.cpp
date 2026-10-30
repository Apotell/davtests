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

// Test for tests/Google/generic/class/class_test_30.sv
//
// THE SOURCE
//
// Lines 1-7 are one-line comments with the license, lines 8-9 are blank and
// lines 10-14 are one block comment carrying the test metadata (tags 6.15 and
// 8.3). The SystemVerilog text is:
//
//   15: class Foo;
//   16: integer size;
//   17: function new (integer size);
//   18:   begin
//   19:     this.size = size;
//   20:   end
//   21: endfunction
//   22: task print();
//   23:   begin
//   24:     $write("Hello, world!");
//   25:   end
//   26: endtask
//   27: endclass
//   28:
//   29: module test;
//   30: endmodule
//
// Line 30 ends with a line ending. The repository stores the file with LF
// line endings; a Windows checkout (core.autocrlf) turns them into CR LF. 5.3
// lists "newlines" as white space but does not name the carriage return, so
// this test treats CR LF as one line ending and asserts nothing about the CR
// itself.
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Lines 15-27 are one class_declaration (8.3, A.1.2) of Foo with three
// class_items (A.1.9):
//
//   - Line 16 is a class property: "A class's data are referred to as class
//     properties, and its subroutines are called methods; both are members of
//     the class" (8.2). size is of type integer, a "4-state data type, 32-bit
//     signed integer" (Table 6-8).
//   - Lines 17-21 are the constructor, a class_constructor_declaration
//     (8.7, A.1.9):
//
//       class_constructor_declaration ::= function [ class_scope ] new
//           [ ( [ class_constructor_arg_list ] ) ] ; { block_item_declaration }
//           [ super . new ... ; ] { function_statement_or_null }
//           endfunction [ : new ]
//       class_constructor_arg ::= tf_port_item | default
//
//     Its one argument, "integer size", has no direction, and "Function
//     declarations default to the formal direction input if no direction has
//     been specified" (13.4). Its one statement is a begin-end block (9.3.1)
//     holding one blocking assignment (10.4.1), "this.size = size;". 8.11
//     shows this very form: "The x is now both a property of the class and an
//     argument to the function new. In the function new, an unqualified
//     reference to x shall be resolved by looking at the innermost scope, in
//     this case, the subroutine argument declaration. To access the instance
//     class property, it is qualified with the this keyword, to refer to the
//     current instance." So the right-hand size is the argument and the
//     left-hand this.size is the property of line 16.
//   - Lines 22-26 are a task, print, with an empty port list (13.3). Its one
//     statement is a begin-end block holding one system task call, the
//     display task $write (21.2.1), with one argument, the string literal
//     "Hello, world!" (5.9).
//   - "The lifetime of methods declared as part of a class type shall be
//     automatic" (8.6), and both methods are members of Foo (8.2).
//
// Foo and the module lie outside any other scope, so they belong to the
//   compilation-unit scope, which "contains all declarations that lie outside
//   any other scope" (3.12.1).
//
// Lines 29-30 are one module_declaration (A.1.2). test is never instantiated,
//   so it is a top-level module: "A design shall contain at least one
//   top-level module. A top-level module is implicitly instantiated once, and
//   its instance name is the same as the module name" (23.3.1).
//
// No line holds a compiler directive, so lines are numbered from 1 at the
//   start of the file (22.12).
//
// HOW THE TEST OBSERVES IT
//
// The class defn (37.31) has vpiName, vpiVirtual, vpiExtends, vpiParameter,
// vpiVariables, vpiMethods and its other member relations; "the iteration
// over vpiMethods shall not include built-in methods for which there is no
// explicit declaration" (37.31 detail 1), and new is declared explicitly. A
// method is a task func (37.41): a function or a task, with vpiMethod and
// vpiAutomatic ("default lifetime"), and its arguments are io decls (37.13)
// with vpiName, vpiDirection and a typespec. "A task func can have zero or
// more statements (see 13.3, 13.4). If the number of statements is greater
// than 1, the vpiStmt relation shall return an unnamed begin that contains the
// statements of the task or function. If the number of statements is zero,
// the vpiStmt relation shall return NULL" (37.12 detail 5); here each method
// has one statement, its begin-end block, so vpiStmt returns that block. A
// variable (37.17) has vpiName, vpiSigned and vpiSize. A module instance
// (37.10) has vpiName, vpiDefName, vpiDefFile and vpiDefLineNo. HLDB answers
// vpi_get() and vpi_iterate() through getVpiPropertyValue() and
// getByVpiType(), so the test asks those questions directly with the LRM's
// vpiXXX codes. vpiDecompile "shall return a string with a functionally
// equivalent expression to the original expression within the source code"
// (37.59 detail 2); for the string literal argument of $write this test takes
// that string to be the literal as written, quotes included, an LRM
// interpretation rather than a quoted rule.
//
// Every expected value comes from the source text and the LRM, so a red test
// means HLC deviates from the LRM.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors                     no fatal, syntax or error
//                                             diagnostic of any category
//   Lines15To27ClassFooIsAccepted             none on lines 15-27
//   Lines29To30ModuleIsAccepted               none on lines 29-30
//   ReportsNoMisreadingDiagnostics            no syntax, keyword, identifier
//                                             length or non-ASCII diagnostic
//                                             at any severity
//   ReportsNoClassDiagnostics                 no duplicate class, missing base
//                                             class or label mismatch
//   ReportsNoDesignHierarchyDiagnostics       no duplicate design unit,
//                                             missing definition or missing
//                                             top-level module
//   SourceFileHasNoIncludesOrMacros           one SourceFile, no includes,
//                                             no macro definitions or usages
//   ClassFooIsDeclared                        a class defn named Foo from this
//                                             file
//   ClassFooIsOnLines15To27                   it starts on line 15 and ends on
//                                             line 27
//   ClassFooIsNotVirtualAndExtendsNothing     vpiVirtual is false; no
//                                             extends, derived class or
//                                             parameter
//   ClassFooHasOneProperty                    vpiVariables holds exactly one
//                                             variable, size
//   PropertySizeIsAnInteger                   an integer variable, signed, 32
//                                             bits, on line 16
//   ClassFooHasTwoMethods                     vpiMethods holds exactly two
//                                             methods, new and print
//   ClassFooHasNoOtherMembers                 no constraint, typedef, named
//                                             event (array) or internal scope
//   ConstructorNewIsAFunction                 new: a function, a method,
//                                             automatic, on lines 17-21
//   ConstructorNewHasOneInputArgumentSize     one io decl, size, input, of
//                                             type integer, on line 17
//   ConstructorNewAssignsTheArgument          its statement is a begin block
//                                             holding one assignment, on line
//                                             19, whose right side is size
//   TaskPrintIsATask                          print: a task, a method,
//                                             automatic, without io decl, on
//                                             lines 22-26
//   TaskPrintCallsWrite                       its statement is a begin block
//                                             holding one system task call,
//                                             $write, on line 24, whose one
//                                             argument is the string literal
//   ModuleTestIsDeclared                      exactly one module from this
//                                             file, named test
//   ModuleTestIsOnLines29To30                 it starts on line 29 and ends on
//                                             line 30
//   ModuleTestIsEmpty                         no variable, net, parameter,
//                                             typedef or class defn in it
//   NoOtherDesignObjectsDeclared              no interface, program, declared
//                                             package, UDP, config, checker,
//                                             parameter or compilation-unit
//                                             variable comes from this file
//   TestIsTheOnlyTopLevelInstance             elaborated runs only: one
//                                             top-level instance, test, of the
//                                             definition test at line 29, with
//                                             no class defn in it
//
// Exactly one check is gated on Design::getElaborated():
// TestIsTheOnlyTopLevelInstance. The top-level instance is created by
// elaboration (23.3.1), so it genuinely exists only after it. It has no else
// branch: before elaboration there is no instance, and the LRM defines no
// value to assert for one. Everything the parser establishes (the class, its
// property, its methods, their arguments and statements, the module, the
// names and the lines) is asserted unconditionally.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Run-time behavior: the text $write prints, the value size holds after
//     new runs, and any object of Foo. The file has no new expression (8.4)
//     and calls no method, so nothing in it runs at all, and even a call
//     would only act while simulation time advances; this is permanently out
//     of scope. Nearest assertions: TaskPrintCallsWrite and
//     ConstructorNewAssignsTheArgument (the statements as written).
//   - The form of "this.size" in the object model, and the object the
//     right-hand size's vpiActual returns. 8.11 fixes what both names mean,
//     the property and the argument, but Clause 37 does not fix which objects
//     represent a this-qualified property or a reference to a subroutine
//     argument. Nearest assertions: ConstructorNewAssignsTheArgument (the
//     right side is named size) and ConstructorNewHasOneInputArgumentSize.
//   - The return variable of new (37.41 details 1 and 3). The constructor's
//     result is the new object handle (8.7), which no statement here names.
//     Nearest assertion: ConstructorNewIsAFunction.
//   - The object HLDB returns for the typespec relation of new's io decl
//     (37.13). The LRM draws it as a direct relation to a typespec; HLDB may
//     instead return an object of its own type that refers to the typespec.
//     That indirection is an HLDB model convention the LRM does not define, so
//     its type is not asserted; the test follows it through vpiActual to the
//     typespec it refers to. Nearest assertion:
//     ConstructorNewHasOneInputArgumentSize (vpiIntegerTypespec on that
//     typespec).
//   - vpiVisibility of the methods and vpiFullName of any object. They are
//     computed or defaulted properties that this suite does not assert.
//     Nearest assertions: ConstructorNewIsAFunction and TaskPrintIsATask.
//   - A package object for the compilation unit of this file. The object
//     model represents a compilation unit as a package with vpiUnit (37.10),
//     although "the compilation-unit scope is not a package" (3.12.1);
//     whether HLDB lists one for this file is a tool convention, so only
//     packages with vpiUnit not TRUE are counted. Nearest assertion:
//     NoOtherDesignObjectsDeclared.
//   - The location of the instance itself (vpiFile, vpiLineNo). A top-level
//     instance has no instantiation statement, so the LRM fixes only its
//     definition location. Nearest assertion: TestIsTheOnlyTopLevelInstance
//     (vpiDefFile and vpiDefLineNo).
//   - Warnings and informational diagnostics. The LRM neither requires nor
//     forbids them for this file. Nearest assertions: CompilesWithoutErrors
//     and the three ReportsNo... tests.
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
constexpr std::string_view kSourceFileName = "class_test_30.sv";

// vpiVariables, the VPI relation from a scope to its variables (100 in IEEE
// 1800-2023 Annex K, vpi_user.h). HLDB's vpi_user.h names the same relation
// vpiVariable.
constexpr int32_t kVpiVariables = vpiVariable;

// The lines of the declarations and statements.
constexpr uint32_t kClassFirstLine = 15;
constexpr uint32_t kSizeLine = 16;
constexpr uint32_t kNewFirstLine = 17;
constexpr uint32_t kAssignmentLine = 19;
constexpr uint32_t kNewLastLine = 21;
constexpr uint32_t kPrintFirstLine = 22;
constexpr uint32_t kWriteLine = 24;
constexpr uint32_t kPrintLastLine = 26;
constexpr uint32_t kClassLastLine = 27;
constexpr uint32_t kModuleFirstLine = 29;
constexpr uint32_t kModuleLastLine = 30;

// The width of integer, a "4-state data type, 32-bit signed integer" (Table
// 6-8).
constexpr int64_t kIntegerBits = 32;

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

// The only object 'object' has in the VPI relation 'relation', whether the
// relation is one-to-one or one-to-many, or nullptr when it has none or more
// than one.
const hldb::Any *onlyInRelation(const hldb::Any *object, int32_t relation) {
  if (const std::vector<const hldb::Any *> *const objects = relationObjects(object, relation)) {
    return (objects->size() == 1) ? objects->front() : nullptr;
  }
  return relationObject(object, relation);
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

class ClassTest30Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_test_30.hlc"}); }
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

  // The class Foo declared in the file under test, or nullptr.
  static const hldb::ClassDefn *findFoo() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllClasses(), "Foo");
  }

  // The method of Foo named 'name', or nullptr.
  static const hldb::Any *findMethod(std::string_view name) {
    const hldb::ClassDefn *const foo = findFoo();
    return (foo == nullptr) ? nullptr : findInRelation(foo, vpiMethods, name);
  }

  // The module declared in the file under test, or nullptr.
  static const hldb::Module *findModule() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllModules());
  }

  // Checks that 'method' is a method of kind 'kind' (vpiFunction or vpiTask)
  // that is automatic and spans lines 'firstLine' to 'lastLine' (37.41, 8.6).
  static void expectMethod(const hldb::Any *method, int32_t kind, uint32_t firstLine, uint32_t lastLine) {
    ASSERT_NE(method, nullptr);

    EXPECT_EQ(method->getVpiType(), static_cast<uint32_t>(kind));
    EXPECT_EQ(integerProperty(method, vpiMethod), 1);
    EXPECT_EQ(integerProperty(method, vpiAutomatic), 1);
    EXPECT_TRUE(endsWith(method->getFile(), kSourceFileName)) << method->getFile();
    EXPECT_EQ(method->getStartLine(), firstLine);
    EXPECT_EQ(method->getEndLine(), lastLine);
  }

  // The one statement of the begin-end block that is the one statement of
  // 'method' (37.12 detail 5), or nullptr. Checks the block on the way.
  static const hldb::Any *onlyStatementOfBlock(const hldb::Any *method) {
    if (method == nullptr) return nullptr;
    const hldb::Any *const block = relationObject(method, vpiStmt);
    if (block == nullptr) return nullptr;
    EXPECT_EQ(block->getVpiType(), static_cast<uint32_t>(vpiBegin));
    EXPECT_EQ(relationSize(block, vpiStmt), 1u);
    return onlyInRelation(block, vpiStmt);
  }
};

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Every line of the file is legal, and the design has its top-level module,
// so nothing is fatal, a syntax error or an error, in any category.
TEST_F(ClassTest30Test, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

// Lines 15-27: a class_declaration with a property, a constructor and a task
// (8.3, 8.7, A.1.9).
TEST_F(ClassTest30Test, Lines15To27ClassFooIsAccepted) {
  EXPECT_EQ(errorsOnLines(kClassFirstLine, kClassLastLine), "");
}

// Lines 29-30: an empty module_declaration (A.1.2).
TEST_F(ClassTest30Test, Lines29To30ModuleIsAccepted) {
  EXPECT_EQ(errorsOnLines(kModuleFirstLine, kModuleLastLine), "");
}

// Each of these diagnostics, at any severity and on any line, would mean HLC
// misread the text of the file.
TEST_F(ClassTest30Test, ReportsNoMisreadingDiagnostics) {
  // Every declaration and statement matches the grammar (A.1.2, A.1.9, A.6),
  // and the file uses no preprocessor directive.
  EXPECT_EQ(findEvent(EventType::PpSyntaxError), nullptr);
  EXPECT_EQ(findEvent(EventType::PaSyntaxError), nullptr);

  // Foo, size, print and test are no keywords (Table B.1); new and this are,
  // and each stands where the grammar puts it (A.1.9, 8.11). The longest
  // identifier, print, has 5 characters, within any limit an implementation
  // may set, which "shall be at least 1024 characters" (5.6).
  EXPECT_EQ(findEvent(EventType::PaReservedKeyword), nullptr);
  EXPECT_EQ(findEvent(EventType::PaMaxLengthIdentifier), nullptr);

  // Every character of the file is ASCII.
  EXPECT_EQ(findEvent(EventType::PpNonAsciiContent), nullptr);
}

// The class is declared once, extends nothing, and endclass has no label, so
// none of these diagnostics applies, at any severity.
TEST_F(ClassTest30Test, ReportsNoClassDiagnostics) {
  EXPECT_EQ(findEvent(EventType::CompMultiplyDefinedClass), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUndefinedBaseClass), nullptr);
  EXPECT_EQ(findEvent(EventType::CompUnmatchedLabel), nullptr);
}

// The class and the module are each declared once, nothing is instantiated,
// and test is a top-level module (23.3.1), so none of these diagnostics
// applies, at any severity.
TEST_F(ClassTest30Test, ReportsNoDesignHierarchyDiagnostics) {
  EXPECT_EQ(findEvent(EventType::CompMultiplyDefinedDesignUnit), nullptr);
  EXPECT_EQ(findEvent(EventType::ElabMultiplyDefinedModule), nullptr);
  EXPECT_EQ(findEvent(EventType::ElabNoModuleDefinition), nullptr);
  EXPECT_EQ(findEvent(EventType::ElabNoTopLevelModule), nullptr);
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once (run configuration). It has no `include (22.4), no
// `define (22.5.1) and no macro usage; $write is a system task (21.2.1), not a
// macro.
TEST_F(ClassTest30Test, SourceFileHasNoIncludesOrMacros) {
  const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
  ASSERT_EQ(sourceFiles.size(), 1u);
  const hldb::SourceFile *const sourceFile = sourceFiles.front();

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// ---------------------------------------------------------------------------
// Lines 15-27: class Foo
// ---------------------------------------------------------------------------

// Foo is declared in the compilation-unit scope (3.12.1), as a class defn
// (37.31) whose vpiName is its class_identifier.
TEST_F(ClassTest30Test, ClassFooIsDeclared) {
  const hldb::ClassDefn *const foo = findFoo();
  ASSERT_NE(foo, nullptr);
  EXPECT_EQ(foo->getVpiType(), static_cast<uint32_t>(vpiClassDefn));
  EXPECT_EQ(stringProperty(foo, vpiName), "Foo");
}

// "class Foo;" is on line 15 and "endclass" on line 27.
TEST_F(ClassTest30Test, ClassFooIsOnLines15To27) {
  const hldb::ClassDefn *const foo = findFoo();
  ASSERT_NE(foo, nullptr);

  EXPECT_TRUE(endsWith(foo->getFile(), kSourceFileName)) << foo->getFile();
  EXPECT_EQ(foo->getStartLine(), kClassFirstLine);
  EXPECT_EQ(foo->getEndLine(), kClassLastLine);
}

// "class Foo;": no virtual, no extends clause, so no extends object (37.31
// detail 6); no class derives from Foo (37.31 detail 5); no
// parameter_port_list and no parameter item (37.31 detail 7).
TEST_F(ClassTest30Test, ClassFooIsNotVirtualAndExtendsNothing) {
  const hldb::ClassDefn *const foo = findFoo();
  ASSERT_NE(foo, nullptr);

  EXPECT_EQ(integerProperty(foo, vpiVirtual), 0);
  EXPECT_EQ(relationSize(foo, vpiExtends), 0u);
  EXPECT_EQ(relationSize(foo, vpiDerivedClasses), 0u);
  EXPECT_EQ(relationSize(foo, vpiParameter), 0u);
}

// Line 16 declares Foo's one class property, size (8.2); the size of line 17
// is an argument of new, not a property.
TEST_F(ClassTest30Test, ClassFooHasOneProperty) {
  const hldb::ClassDefn *const foo = findFoo();
  ASSERT_NE(foo, nullptr);

  EXPECT_EQ(relationSize(foo, kVpiVariables), 1u);
  EXPECT_NE(findInRelation(foo, kVpiVariables, "size"), nullptr);
}

// "integer size;": an integer variable (37.17), a "4-state data type, 32-bit
// signed integer" (Table 6-8), declared on line 16.
TEST_F(ClassTest30Test, PropertySizeIsAnInteger) {
  const hldb::ClassDefn *const foo = findFoo();
  ASSERT_NE(foo, nullptr);
  const hldb::Any *const size = findInRelation(foo, kVpiVariables, "size");
  ASSERT_NE(size, nullptr) << "no property size in Foo";

  EXPECT_EQ(size->getVpiType(), static_cast<uint32_t>(vpiIntegerVar));
  EXPECT_EQ(stringProperty(size, vpiName), "size");
  EXPECT_EQ(integerProperty(size, vpiSigned), 1);
  EXPECT_EQ(integerProperty(size, vpiSize), kIntegerBits);
  EXPECT_EQ(size->getStartLine(), kSizeLine);
}

// Foo declares two methods, new and print (8.2), both explicitly, so both are
// in vpiMethods (37.31 detail 1), and nothing else is.
TEST_F(ClassTest30Test, ClassFooHasTwoMethods) {
  const hldb::ClassDefn *const foo = findFoo();
  ASSERT_NE(foo, nullptr);

  EXPECT_EQ(relationSize(foo, vpiMethods), 2u);
  EXPECT_NE(findMethod("new"), nullptr);
  EXPECT_NE(findMethod("print"), nullptr);
}

// Foo's three class items are a property and two methods, so it has nothing
// else that 37.31 lets a class defn hold.
TEST_F(ClassTest30Test, ClassFooHasNoOtherMembers) {
  const hldb::ClassDefn *const foo = findFoo();
  ASSERT_NE(foo, nullptr);

  EXPECT_EQ(relationSize(foo, vpiConstraint), 0u);
  EXPECT_EQ(relationSize(foo, vpiTypedef), 0u);
  EXPECT_EQ(relationSize(foo, vpiNamedEvent), 0u);
  EXPECT_EQ(relationSize(foo, vpiNamedEventArray), 0u);
  EXPECT_EQ(relationSize(foo, vpiInternalScope), 0u);
}

// ---------------------------------------------------------------------------
// Lines 17-21: function new
// ---------------------------------------------------------------------------

// The constructor is a function (8.7, A.1.9) and a member of Foo, so a method
// (37.41), and "The lifetime of methods declared as part of a class type shall
// be automatic" (8.6). It spans lines 17-21.
TEST_F(ClassTest30Test, ConstructorNewIsAFunction) {
  expectMethod(findMethod("new"), vpiFunction, kNewFirstLine, kNewLastLine);
}

// "(integer size)": one argument, an io decl (37.13) named size, of type
// integer; "Function declarations default to the formal direction input if no
// direction has been specified" (13.4).
TEST_F(ClassTest30Test, ConstructorNewHasOneInputArgumentSize) {
  const hldb::Any *const constructor = findMethod("new");
  ASSERT_NE(constructor, nullptr);
  EXPECT_EQ(relationSize(constructor, vpiIODecl), 1u);
  const hldb::Any *const argument = findInRelation(constructor, vpiIODecl, "size");
  ASSERT_NE(argument, nullptr) << "new has no argument size";

  EXPECT_EQ(argument->getVpiType(), static_cast<uint32_t>(vpiIODecl));
  EXPECT_EQ(integerProperty(argument, vpiDirection), static_cast<int64_t>(vpiInput));
  const hldb::Any *const typespec = typespecOf(argument, vpiTypespec);
  ASSERT_NE(typespec, nullptr) << "the argument size has no typespec";
  EXPECT_EQ(typespec->getVpiType(), static_cast<uint32_t>(vpiIntegerTypespec));
  EXPECT_EQ(argument->getStartLine(), kNewFirstLine);
}

// new has one statement, the begin-end block of lines 18-20, so vpiStmt
// returns it (37.12 detail 5). The block holds one statement, the blocking
// assignment of line 19 (10.4.1), whose right side is the unqualified size,
// which "shall be resolved by looking at the innermost scope, in this case,
// the subroutine argument declaration" (8.11).
TEST_F(ClassTest30Test, ConstructorNewAssignsTheArgument) {
  const hldb::Any *const statement = onlyStatementOfBlock(findMethod("new"));
  ASSERT_NE(statement, nullptr) << "new has no begin block with one statement";

  EXPECT_EQ(statement->getVpiType(), static_cast<uint32_t>(vpiAssignment));
  EXPECT_EQ(statement->getStartLine(), kAssignmentLine);
  const hldb::Any *const rhs = relationObject(statement, vpiRhs);
  ASSERT_NE(rhs, nullptr) << "the assignment has no right side";
  EXPECT_EQ(stringProperty(rhs, vpiName), "size");
}

// ---------------------------------------------------------------------------
// Lines 22-26: task print
// ---------------------------------------------------------------------------

// print is a task (13.3) and a member of Foo, so a method (37.41); it is
// automatic (8.6), has an empty port list, so no io decl (37.13), and spans
// lines 22-26.
TEST_F(ClassTest30Test, TaskPrintIsATask) {
  const hldb::Any *const print = findMethod("print");
  expectMethod(print, vpiTask, kPrintFirstLine, kPrintLastLine);
  ASSERT_NE(print, nullptr);
  EXPECT_EQ(relationSize(print, vpiIODecl), 0u);
}

// print has one statement, the begin-end block of lines 23-25, so vpiStmt
// returns it (37.12 detail 5). The block holds one statement, the call of the
// system task $write on line 24 (21.2.1), with one argument, the string
// literal "Hello, world!" (5.9), a constant of type vpiStringConst. Its
// vpiDecompile is taken to be the literal as written, quotes included: an
// interpretation of 37.59 detail 2, which asks only for "a functionally
// equivalent expression".
TEST_F(ClassTest30Test, TaskPrintCallsWrite) {
  const hldb::Any *const statement = onlyStatementOfBlock(findMethod("print"));
  ASSERT_NE(statement, nullptr) << "print has no begin block with one statement";

  EXPECT_EQ(statement->getVpiType(), static_cast<uint32_t>(vpiSysTaskCall));
  EXPECT_EQ(stringProperty(statement, vpiName), "$write");
  EXPECT_EQ(statement->getStartLine(), kWriteLine);

  EXPECT_EQ(relationSize(statement, vpiArgument), 1u);
  const hldb::Any *const argument = onlyInRelation(statement, vpiArgument);
  ASSERT_NE(argument, nullptr) << "$write has no single argument";
  EXPECT_EQ(argument->getVpiType(), static_cast<uint32_t>(vpiConstant));
  EXPECT_EQ(integerProperty(argument, vpiConstType), static_cast<int64_t>(vpiStringConst));
  EXPECT_EQ(stringProperty(argument, vpiDecompile), "\"Hello, world!\"");
}

// ---------------------------------------------------------------------------
// Lines 29-30: module test
// ---------------------------------------------------------------------------

// The file declares one module, test (A.1.2).
TEST_F(ClassTest30Test, ModuleTestIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);

  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "test");
}

// "module test;" is on line 29 and "endmodule" on line 30.
TEST_F(ClassTest30Test, ModuleTestIsOnLines29To30) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);

  EXPECT_TRUE(endsWith(module->getFile(), kSourceFileName)) << module->getFile();
  EXPECT_EQ(module->getStartLine(), kModuleFirstLine);
  EXPECT_EQ(module->getEndLine(), kModuleLastLine);
}

// test has no ports, parameters or items. The class is declared outside it,
// in the compilation-unit scope (3.12.1).
TEST_F(ClassTest30Test, ModuleTestIsEmpty) {
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

// The file declares one class and one module. The property size belongs to
// Foo, so no variable is declared in the compilation-unit scope, and the file
// declares no package (a compilation-unit package object, vpiUnit TRUE, is
// not counted) and no parameter.
TEST_F(ClassTest30Test, NoOtherDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countDeclaredPackagesFromSourceFile(m_design->getAllPackages()), 0u);
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
// 29 of this file (vpiDefName, vpiDefFile, vpiDefLineNo, 37.10), and the class
// is not in it (3.12.1). The instance only exists after elaboration.
TEST_F(ClassTest30Test, TestIsTheOnlyTopLevelInstance) {
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
