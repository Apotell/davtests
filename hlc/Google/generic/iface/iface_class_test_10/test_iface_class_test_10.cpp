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

// Validates the HLDB model built for
// tests/Google/generic/iface/iface_class_test_10.sv. Lines 1-7 are one-line
// "//" license comments, lines 8-9 are blank, lines 10-14 are the block
// comment carrying the suite's :name:/:description:/:tags: metadata, and
// the rest is:
//
//   15  interface class base_ic;
//   16  pure virtual function void pure_task1;
//   17  pure virtual function string concatenator(string arg);
//   18  endclass
//   19
//   20  module test;
//   21  endmodule
//
// The fixture is tagged 8.3 (class declarations) and 8.26 (interface
// classes).
//
// interface class base_ic; ... endclass (8.26, A.1.9)
//   interface_class_method ::= pure virtual method_prototype ;
//   - An interface class named base_ic, declared once, as a
//     compilation-unit item (3.12.1); user-defined (vpiClassType
//     vpiUserDefinedClass, 37.29) and an interface class (8.26).
//   - No parameter port list, no 'extends', no 'implements' (8.26.2), no
//     property (8.26), no end label.
//   - Exactly two methods, both functions declared 'pure virtual' (8.21,
//     8.26: "all methods of an interface class are pure virtual"), in this
//     order:
//       16  pure_task1    a void function (13.4.1); no argument list, so no
//                         arguments. (Despite its name it is a function.)
//       17  concatenator  returns string (6.16); one argument, arg, of type
//                         string. No direction is written, so it is an
//                         input (13.4: "the default direction of a function
//                         argument is input")
//   - A pure virtual method has no body (8.21: only "the prototype ...
//     without an implementation").
//
// module test; endmodule (23.2)
//   - A module named test, declared once, with no port list and no module
//     items.
//   - test is never instantiated, so it is a top-level module, implicitly
//     instantiated once under its own name (23.3.1). The instance tree and
//     the top-module flag only exist after elaboration.
//
// The file is legal, so it compiles without errors.
//
// The .hlc also puts UVM 1800.2-2017 on the include path ('-I'), but adds no
// UVM source file, and passes -nobuiltin, so nothing else is compiled.
//
// Model note: a pure virtual method is a prototype without an
// implementation (8.21). HLDB records method prototypes as TaskFuncDecl in
// ClassDefn::getTaskFuncDecls(); getMethods() holds methods with a body,
// so for base_ic it is empty (see iface_class_test_9).
//
// KNOWN FAILURE (pure virtual prototypes not flagged as methods), not a
// defect in this test: as in iface_class_test_9 (task prototypes), HLC 577
// records pure_task1 and concatenator as FunctionDecl prototypes of
// base_ic, virtual and pure, with the right return types and arguments,
// but getMethod() (vpiMethod) is false on both, although they are declared
// in the body of class base_ic (8.6; this suite asserts getMethod() for
// class methods, e.g. 18.13.2--urandom_range_1). The getMethod() checks of
// BaseIcHasTwoPureVirtualFunctions assert the LRM behavior and are
// expected to fail until HLC is fixed; they are intentionally not skipped
// or relaxed.
//
// What is checked
//   - No fatal, syntax or error diagnostic.
//   - Exactly one SourceFile for the .sv, with no includes, no generic
//     directives, no macro definitions and no macro instances.
//   - Exactly one class named base_ic: user-defined, an interface class,
//     with no parameters, base classes, implemented classes, properties,
//     constraints, typedefs, nested classes or end label.
//   - base_ic has exactly two method prototypes (TaskFuncDecl), in order
//     pure_task1 and concatenator, and no method with a body; each is a
//     function, a class method, virtual and pure, bound to no
//     implementation.
//   - pure_task1 returns void and has no arguments.
//   - concatenator returns string and has exactly one argument, arg: an
//     input of type string.
//   - Exactly one module named test, with no port, process, net, variable,
//     continuous assignment, primitive, module instance or class, and no
//     end label.
//   - No other design element and no compilation-unit declaration.
//   - Top-level status of test: after elaboration test is a top-level
//     module with one top instance named test; before elaboration no module
//     is flagged top (maintainer's convention for non-elaborated models).
//
// What is NOT checked, and why
//   - Method lifetimes (automatic/static): 8.6 makes class methods
//     automatic, but the suite tracks HLC's handling of class method
//     lifetimes elsewhere (18.13.2/18.13.3 tests), and a pure virtual
//     prototype has no body whose variables a lifetime could govern.
//   - The function kind code (getFuncType): its encoding of the return type
//     is a model convention; the return typespec is asserted instead.
//   - The methods' access (local/protected): none is written, so both are
//     public (8.18); the access-type encoding is a model convention.
//   - ClassDefn::getVirtual() for base_ic: the 'virtual' keyword is not
//     written before 'interface class'; whether the flag reflects
//     abstract-ness is a model convention.
//   - The license and metadata comments (5.4): comments carry no meaning.
//   - The SourceFile time unit and precision: there is no `timescale and the
//     default is implementation-specific (3.14.2.3).
//   - Source line numbers: a start line encodes nothing about
//     SystemVerilog semantics.
//   - The warning count. The LRM neither requires nor forbids warnings here.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/checker_decl.h>
#include <hldb/class_defn.h>
#include <hldb/config_decl.h>
#include <hldb/design.h>
#include <hldb/directive.h>
#include <hldb/function_decl.h>
#include <hldb/include_stmt.h>
#include <hldb/interface.h>
#include <hldb/io_decl.h>
#include <hldb/let_decl.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/program.h>
#include <hldb/ref_typespec.h>
#include <hldb/source_file.h>
#include <hldb/string_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task_func.h>
#include <hldb/task_func_decl.h>
#include <hldb/typedef.h>
#include <hldb/udp_defn.h>
#include <hldb/variable.h>
#include <hldb/void_typespec.h>
#include <hldb/vpi_user.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "iface_class_test_10.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

// Size of an HLDB collection; a collection that was never created is empty.
template <typename T>
size_t sizeOf(const std::vector<T *> *collection) {
  if (collection == nullptr) {
    return 0;
  }
  return collection->size();
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

// Number of objects in 'collection' named 'name'.
template <typename T>
size_t countNamed(const std::vector<T *> *collection, std::string_view name) {
  size_t count = 0;
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (object->getName() == name) ++count;
    }
  }
  return count;
}

// The line a diagnostic points at, or 0 when it carries no location.
uint32_t lineOf(const Error &error) {
  if (error.getLocations().empty()) {
    return 0;
  }
  return error.getLocations().front().m_line;
}

// Every diagnostic of 'severity' as " [<code> line <n>: <text>]", for failure
// messages, so a red run shows what was reported and where. <text> is HLC's
// message template, so its placeholders are not filled in.
std::string describeErrors(const ErrorContainer *errorContainer, ErrorDefinition::ErrorSeverity severity) {
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  std::string description;
  for (const Error &error : errorContainer->getErrors()) {
    const ErrorDefinition::ErrorMap::const_iterator errorInfo = errorInfoMap.find(error.getType());
    if ((errorInfo == errorInfoMap.end()) || (errorInfo->second.m_severity != severity)) continue;
    description += " [" + std::to_string(static_cast<int32_t>(error.getType()));
    description += " line " + std::to_string(lineOf(error));
    description += ": " + std::string(errorInfo->second.m_errorText) + "]";
  }
  return description;
}
}  // namespace

class IfaceClassTest10Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "iface_class_test_10.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // All SourceFile nodes recorded for the file under test.
  static std::vector<const hldb::SourceFile *> findSourceFiles() {
    std::vector<const hldb::SourceFile *> sourceFiles;
    if (m_design == nullptr) {
      return sourceFiles;
    }
    const hldb::SourceFileCollection *const collection = m_design->getSourceFiles();
    if (collection == nullptr) {
      return sourceFiles;
    }
    for (const hldb::SourceFile *sourceFile : *collection) {
      if (endsWith(sourceFile->getName(), kSourceFileName)) sourceFiles.emplace_back(sourceFile);
    }
    return sourceFiles;
  }

  // The SourceFile node for the file under test, or nullptr unless there is exactly one.
  static const hldb::SourceFile *getSourceFile() {
    const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
    if (sourceFiles.size() != 1) {
      return nullptr;
    }
    return sourceFiles.front();
  }

  // The interface class base_ic, or nullptr.
  static const hldb::ClassDefn *getBaseIc() {
    if (m_design == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::ClassDefn>("base_ic", m_design->getAllClasses());
  }

  // The function prototype 'name' of base_ic, or nullptr. A pure virtual
  // method is a prototype without an implementation (8.21), which HLDB
  // records as a TaskFuncDecl; getMethods() holds methods with a body.
  static const hldb::FunctionDecl *getFunctionDecl(std::string_view name) {
    const hldb::ClassDefn *const baseIc = getBaseIc();
    if (baseIc == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::FunctionDecl>(name, baseIc->getTaskFuncDecls());
  }

  // The module definition test, or nullptr.
  static const hldb::Module *getTestModule() {
    if (m_design == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  }
};

// Lines 15-21 are a legal interface class (8.26) and module (23.2), so
// nothing is fatal, a syntax error or an error.
TEST_F(IfaceClassTest10Test, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);

  const ErrorContainer::Stats stats = errorContainer->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0) << describeErrors(errorContainer, ErrorDefinition::FATAL);
  EXPECT_EQ(stats.nbSyntax, 0) << describeErrors(errorContainer, ErrorDefinition::SYNTAX);
  EXPECT_EQ(stats.nbError, 0) << describeErrors(errorContainer, ErrorDefinition::ERROR);
}

TEST_F(IfaceClassTest10Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

// The file holds no compiler directive, no `define and no macro usage.
TEST_F(IfaceClassTest10Test, SourceFileHasNoIncludesDirectivesOrMacros) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getIncludeStmts()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// Line 15: 'interface class base_ic;' declares the class once. It is written
// in the source, so it is user-defined (37.29 vpiClassType), and the
// 'interface' keyword makes it an interface class (8.26).
TEST_F(IfaceClassTest10Test, BaseIcIsUserDefinedInterfaceClass) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countNamed(m_design->getAllClasses(), "base_ic"), 1u) << "base_ic is declared exactly once";

  const hldb::ClassDefn *const baseIc = getBaseIc();
  ASSERT_NE(baseIc, nullptr);
  EXPECT_EQ(baseIc->getClassType(), vpiUserDefinedClass) << "8.3: base_ic is declared in the source text";
  EXPECT_TRUE(baseIc->getIsInterfaceClass()) << "8.26: base_ic is declared 'interface class'";
}

// Line 15 has no parameter port list (8.25), no 'extends' and no
// 'implements' (8.26.2); lines 16-17 are method prototypes only, so base_ic
// has no property (8.26), constraint, typedef or nested class; line 18 is
// 'endclass' without ': base_ic'.
TEST_F(IfaceClassTest10Test, BaseIcHasOnlyMethodPrototypes) {
  const hldb::ClassDefn *const baseIc = getBaseIc();
  ASSERT_NE(baseIc, nullptr);

  EXPECT_EQ(sizeOf(baseIc->getParameters()), 0u) << "8.25: no parameter port list is written";
  EXPECT_EQ(baseIc->getExtends(), nullptr) << "8.26.3: no 'extends' list is written";
  EXPECT_EQ(baseIc->getImplements(), nullptr) << "8.26.2: an interface class implements nothing";
  EXPECT_EQ(sizeOf(baseIc->getVariables()), 0u) << "8.26: an interface class has no properties";
  EXPECT_EQ(sizeOf(baseIc->getConstraints()), 0u) << "no constraint is declared";
  EXPECT_EQ(sizeOf(baseIc->getTypedefs()), 0u) << "no type declaration is written";
  EXPECT_EQ(sizeOf(baseIc->getClassDefns()), 0u) << "no nested class is declared";
  EXPECT_EQ(baseIc->getEndLabelObj(), nullptr) << "'endclass' is written without ': base_ic'";
}

// Lines 16-17: exactly two methods, pure_task1 then concatenator, both
// function prototypes declared 'pure virtual' (8.21, 8.26). A pure virtual
// method has no implementation (8.21), so base_ic has no method with a body
// and neither prototype is bound to one.
TEST_F(IfaceClassTest10Test, BaseIcHasTwoPureVirtualFunctions) {
  const hldb::ClassDefn *const baseIc = getBaseIc();
  ASSERT_NE(baseIc, nullptr);
  EXPECT_EQ(sizeOf(baseIc->getMethods()), 0u) << "8.21: a pure virtual method has no implementation";
  const hldb::TaskFuncDeclCollection *const prototypes = baseIc->getTaskFuncDecls();
  ASSERT_EQ(sizeOf(prototypes), 2u) << "lines 16-17 declare two method prototypes";

  const std::vector<std::string_view> expectedNames = {"pure_task1", "concatenator"};
  for (size_t index = 0; index < expectedNames.size(); ++index) {
    const hldb::TaskFuncDecl *const prototype = prototypes->at(index);
    EXPECT_EQ(prototype->getName(), expectedNames[index]) << "method " << (index + 1) << " in source order";
    EXPECT_NE(::any_cast<hldb::FunctionDecl>(prototype), nullptr) << expectedNames[index] << " is declared 'function'";
    EXPECT_TRUE(prototype->getMethod()) << expectedNames[index] << " is a class method";
    EXPECT_TRUE(prototype->getVirtual()) << "8.26: " << expectedNames[index] << " is declared 'virtual'";
    EXPECT_TRUE(prototype->getPure()) << "8.21: " << expectedNames[index] << " is declared 'pure'";
    EXPECT_EQ(prototype->getTaskFunc(), nullptr) << "8.21: " << expectedNames[index] << " has no implementation";
  }
}

// Line 16: 'pure virtual function void pure_task1;' returns void (13.4.1)
// and has no argument list, so no arguments.
TEST_F(IfaceClassTest10Test, PureTask1IsVoidWithNoArguments) {
  const hldb::FunctionDecl *const function = getFunctionDecl("pure_task1");
  ASSERT_NE(function, nullptr);
  const hldb::RefTypespec *const returnType = function->getReturn();
  ASSERT_NE(returnType, nullptr) << "'function void' writes an explicit return type";
  EXPECT_NE(returnType->getActual<hldb::VoidTypespec>(), nullptr) << "13.4.1: pure_task1 returns void";
  EXPECT_EQ(sizeOf(function->getIODecls()), 0u) << "no argument list is written";
}

// Line 17: 'function string concatenator(string arg)' returns string (6.16)
// and has one argument, arg, of type string. No direction is written, so it
// is an input (13.4).
TEST_F(IfaceClassTest10Test, ConcatenatorReturnsStringFromStringInput) {
  const hldb::FunctionDecl *const function = getFunctionDecl("concatenator");
  ASSERT_NE(function, nullptr);
  const hldb::RefTypespec *const returnType = function->getReturn();
  ASSERT_NE(returnType, nullptr) << "'function string' writes an explicit return type";
  EXPECT_NE(returnType->getActual<hldb::StringTypespec>(), nullptr) << "6.16: concatenator returns string";

  const hldb::IODeclCollection *const arguments = function->getIODecls();
  ASSERT_EQ(sizeOf(arguments), 1u) << "concatenator declares one argument";
  const hldb::IODecl *const arg = arguments->at(0);
  EXPECT_EQ(arg->getName(), "arg");
  EXPECT_EQ(arg->getDirection(), vpiInput) << "13.4: the default direction of a function argument is input";
  const hldb::RefTypespec *const type = arg->getTypespec();
  ASSERT_NE(type, nullptr) << "'string arg' has an explicit data type";
  EXPECT_NE(type->getActual<hldb::StringTypespec>(), nullptr) << "6.16: arg is declared 'string'";
}

// Lines 20-21: 'module test; endmodule' has no port list and no module
// items (23.2), and 'endmodule' carries no ': test' end label.
TEST_F(IfaceClassTest10Test, TestModuleIsEmpty) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u) << "the file declares exactly one module";

  const hldb::Module *const test = getTestModule();
  ASSERT_NE(test, nullptr);
  EXPECT_EQ(test->getEndLabelObj(), nullptr) << "'endmodule' is written without ': test'";
  EXPECT_EQ(sizeOf(test->getPorts()), 0u) << "'module test;' has no port list";
  EXPECT_EQ(sizeOf(test->getProcesses()), 0u);
  EXPECT_EQ(sizeOf(test->getNets()), 0u);
  EXPECT_EQ(sizeOf(test->getVariables()), 0u);
  EXPECT_EQ(sizeOf(test->getContAssigns()), 0u);
  EXPECT_EQ(sizeOf(test->getPrimitives()), 0u);
  EXPECT_EQ(sizeOf(test->getModules()), 0u);
  EXPECT_EQ(sizeOf(test->getClassDefns()), 0u) << "base_ic is declared outside module test";
}

// Besides base_ic and test the file declares no design element
// (3.2-3.12): base_ic is the only class, and it is in no package.
TEST_F(IfaceClassTest10Test, NoOtherDesignElementsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 1u) << "base_ic is the only class";
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);
}

// No compilation-unit item (3.12.1) other than base_ic is declared: the
// functions are methods of base_ic, and the file declares no let,
// parameter, variable or typedef.
TEST_F(IfaceClassTest10Test, NoCompilationUnitDeclarations) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getTaskFuncs()), 0u) << "the functions are methods of base_ic";
  EXPECT_EQ(countFromSourceFile(m_design->getLetDecls()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParamAssigns()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTypedefs()), 0u);
}

// 23.3.1: test is never instantiated, so it is a top-level module,
// implicitly instantiated once under its own name. The instance tree and
// the top-module flag only exist after elaboration; in a non-elaborated
// model no module is flagged top.
TEST_F(IfaceClassTest10Test, TestIsTheTopLevelModule) {
  const hldb::Module *const test = getTestModule();
  ASSERT_NE(test, nullptr);

  if (m_design->getElaborated()) {
    ASSERT_EQ(countFromSourceFile(m_design->getTopModules()), 1u) << "23.3.1: test is the only top-level module";
    const hldb::Module *const topInstance = hldb::findByName<hldb::Module>("test", m_design->getTopModules());
    ASSERT_NE(topInstance, nullptr) << "23.3.1: the top instance is named after module test";
    EXPECT_TRUE(topInstance->getTopModule()) << "23.3.1: test is never instantiated, so it is a top-level module";
  } else {
    EXPECT_FALSE(test->getTopModule()) << "all modules are non-top modules in non-elaborated model";
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
