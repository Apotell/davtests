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

// Validates the HLDB model and diagnostics built for
// tests/Google/generic/member/class_member_test_5.sv. Lines 1-7 are one-line
// "//" license comments, lines 8-9 are blank, lines 10-15 are the block
// comment carrying the suite's :name:/:description:/:should_fail_because:/
// :tags: metadata, and the rest is:
//
//   16  class myclass;
//   17  pure virtual task pure_task1;
//   18  pure virtual task pure_task2(int arg);
//   19  endclass
//   20
//   21  module test;
//   22  endmodule
//
// The fixture is tagged 8.3 (class declarations) and is a negative test:
// ":should_fail_because: pure virtual methods can only be declared in
// virtual classes".
//
// Expected diagnostic (8.21)
//   8.21: a class declared 'virtual class' is abstract, and a pure virtual
//   method (a prototype without an implementation) may only be declared in
//   an abstract class. myclass is declared without 'virtual' (line 16), so
//   the pure virtual prototypes on lines 17-18 are illegal and the compiler
//   must report an error. HLC has no dedicated event code for this rule, so
//   the test requires at least one error, located on the class or on one of
//   the pure virtual prototypes (lines 16-18).
//
// class myclass; ... endclass (8.3, A.1.2)
//   - A class named myclass, declared once, as a compilation-unit item;
//     user-defined (vpiClassType vpiUserDefinedClass, 37.29), not an
//     interface class (8.26), and not declared 'virtual' (8.21).
//   - No parameter port list, no 'extends', no 'implements', no property,
//     no constraint, no nested type and no end label.
//
// pure virtual task pure_task1; / pure_task2(int arg); (8.21, A.1.9)
//   class_method ::= pure virtual method_prototype ;
//   - Exactly two method prototypes, in order pure_task1 and pure_task2,
//     both tasks, class methods (8.6), virtual and pure (8.21), and no
//     method with a body; neither prototype has an implementation.
//   - pure_task1 has no argument list, so no arguments.
//   - pure_task2 has one argument, arg, of type int (6.11). No direction is
//     written, so it is an input (13.3: "The default direction of a task
//     argument is input").
//
// module test; endmodule (23.2)
//   - A module named test, declared once, with no port list and no module
//     items. test is never instantiated, so it is a top-level module (23.3.1).
//     The instance tree and the top-module flag only exist after elaboration.
//
// The .hlc also puts UVM 1800.2-2017 on the include path ('-I') but adds no
// UVM source file, and passes -nobuiltin, so nothing else is compiled.
//
// What is checked
//   - No fatal and no syntax diagnostic (the grammar accepts the source).
//   - At least one error, located on lines 16-18 (8.21 violation).
//   - Exactly one SourceFile for the .sv, with no includes, directives,
//     macro definitions or macro instances.
//   - Exactly one class myclass: user-defined, not an interface class, not
//     virtual, with no parameters, base class, implemented class, property,
//     constraint, typedef, nested class or end label.
//   - myclass has exactly two method prototypes, pure_task1 and pure_task2,
//     and no method with a body; each is a task, a class method, virtual and
//     pure, bound to no implementation.
//   - pure_task1 has no arguments; pure_task2 has one input argument arg of
//     type int.
//   - Exactly one module test, with no port or module item and no end label.
//   - No other design element and no other compilation-unit declaration.
//   - Top-level status of test: after elaboration test is a top-level module
//     with one top instance named test; before elaboration no module is
//     flagged top (maintainer's convention for non-elaborated models).
//
// KNOWN FAILURE (8.21 not enforced), not a defect in this test: HLC 577
// accepts 'pure virtual' prototypes in the non-virtual class myclass and
// reports no error at all, so PureVirtualInNonVirtualClassIsReported fails.
// HLC also has no event code for this rule. The test asserts the LRM
// behavior and is intentionally not skipped or relaxed.
//
// Model note: HLDB records method prototypes as TaskFuncDecl in
// ClassDefn::getTaskFuncDecls(); getMethods() holds methods with a body.
//
// What is NOT checked, and why
//   - A specific event code: HLC defines none for this 8.21 rule.
//   - The exact error count: the LRM does not fix how many diagnostics the
//     two illegal prototypes yield.
//   - Method lifetimes and access types: a prototype has no body whose
//     variables a lifetime could govern, and none of local/protected is
//     written (8.18); both encodings are model conventions.
//   - The license and metadata comments (5.4): comments carry no meaning.
//   - The SourceFile time unit and precision: they come from the
//     -timescale command-line option, not from the SystemVerilog text.
//   - The warning count. The LRM neither requires nor forbids warnings here.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/checker_decl.h>
#include <hldb/class_defn.h>
#include <hldb/config_decl.h>
#include <hldb/design.h>
#include <hldb/directive.h>
#include <hldb/include_stmt.h>
#include <hldb/int_typespec.h>
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
#include <hldb/sv_vpi_user.h>
#include <hldb/task_decl.h>
#include <hldb/task_func.h>
#include <hldb/task_func_decl.h>
#include <hldb/typedef.h>
#include <hldb/udp_defn.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "class_member_test_5.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  if (text.size() < suffix.size()) {
    return false;
  }
  return text.substr(text.size() - suffix.size()) == suffix;
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
      if (endsWith(object->getFile(), kSourceFileName)) {
        ++count;
      }
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
      if (object->getName() == name) {
        ++count;
      }
    }
  }
  return count;
}

// The line a diagnostic points at, or 0 when it carries no location.
uint32_t lineOf(const Event &error) {
  if (error.getArguments().empty()) {
    return 0;
  }
  return error.getArguments().front().m_line;
}

// The definition (severity, message) of a diagnostic, or nullptr when its code is unknown.
const EventDefinition *definitionOf(const Event &error) {
  return hldb::findEventDefinition(hlc::getEventTable(), error.getType());
}

// Every diagnostic of 'severity' as " [<code> line <n>: <text>]", for failure
// messages, so a red run shows what was reported and where. <text> is HLC's
// message template, so its placeholders are not filled in.
std::string describeErrors(const EventListener *eventListener, EventSeverity severity) {
  std::string description;
  for (const Event &error : eventListener->getEvents()) {
    const EventDefinition *const errorInfo = definitionOf(error);
    if ((errorInfo == nullptr) || (errorInfo->m_severity != severity)) {
      continue;
    }
    description += " [" + std::to_string(static_cast<int32_t>(error.getType()));
    description += " line " + std::to_string(lineOf(error));
    description += ": " + std::string(errorInfo->m_lines[0].m_text) + "]";
  }
  return description;
}
}  // namespace

class ClassMemberTest5Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_member_test_5.hlc"}); }
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
      if (endsWith(sourceFile->getName(), kSourceFileName)) {
        sourceFiles.emplace_back(sourceFile);
      }
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

  // The class myclass, or nullptr.
  static const hldb::ClassDefn *getMyClass() {
    if (m_design == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::ClassDefn>("myclass", m_design->getAllClasses());
  }

  // The task prototype 'name' of myclass, or nullptr.
  static const hldb::TaskDecl *getTaskDecl(std::string_view name) {
    const hldb::ClassDefn *const myClass = getMyClass();
    if (myClass == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::TaskDecl>(name, myClass->getTaskFuncDecls());
  }

  // The module definition test, or nullptr.
  static const hldb::Module *getTestModule() {
    if (m_design == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  }
};

// The grammar accepts 'pure virtual' method prototypes in any class body
// (A.1.9); the 8.21 rule is semantic. So nothing is fatal or a syntax error.
TEST_F(ClassMemberTest5Test, NoFatalOrSyntaxErrors) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  const EventListener::Stats stats = eventListener->getStats();
  EXPECT_EQ(stats.m_fatals, 0) << describeErrors(eventListener, EventSeverity::Fatal);
  EXPECT_EQ(stats.m_syntaxes, 0) << describeErrors(eventListener, EventSeverity::Syntax);
}

// 8.21: pure virtual methods may only be declared in an abstract
// ('virtual') class. myclass (line 16) is not declared 'virtual', so the
// prototypes on lines 17-18 are illegal and an error is reported there.
TEST_F(ClassMemberTest5Test, PureVirtualInNonVirtualClassIsReported) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  EXPECT_GE(eventListener->getStats().m_errors, 1)
      << "8.21: pure virtual methods can only be declared in virtual classes";

  bool errorOnPureMethod = false;
  for (const Event &error : eventListener->getEvents()) {
    const EventDefinition *const errorInfo = definitionOf(error);
    if ((errorInfo == nullptr) || (errorInfo->m_severity != EventSeverity::Error)) {
      continue;
    }
    const uint32_t line = lineOf(error);
    if ((line >= 16) && (line <= 18)) {
      errorOnPureMethod = true;
    }
  }
  EXPECT_TRUE(errorOnPureMethod) << "8.21: the error points at myclass or its pure virtual prototypes (lines 16-18);"
                                 << describeErrors(eventListener, EventSeverity::Error);
}

TEST_F(ClassMemberTest5Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

// The file holds no compiler directive, no `define and no macro usage.
TEST_F(ClassMemberTest5Test, SourceFileHasNoIncludesDirectivesOrMacros) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getIncludeStmts()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// Line 16: 'class myclass;' declares the class once. It is written in the
// source, so it is user-defined (37.29 vpiClassType); it is not an interface
// class (8.26) and 'virtual' is not written before 'class' (8.21).
TEST_F(ClassMemberTest5Test, MyClassIsUserDefinedNonVirtualClass) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countNamed(m_design->getAllClasses(), "myclass"), 1u) << "myclass is declared exactly once";

  const hldb::ClassDefn *const myClass = getMyClass();
  ASSERT_NE(myClass, nullptr);
  EXPECT_EQ(myClass->getClassType(), vpiUserDefinedClass) << "8.3: myclass is declared in the source text";
  EXPECT_FALSE(myClass->getIsInterfaceClass()) << "8.26: 'interface' is not written";
  EXPECT_FALSE(myClass->getVirtual()) << "8.21: 'virtual' is not written before 'class'";
}

// Line 16 has no parameter port list (8.25), no 'extends' (8.13) and no
// 'implements' (8.26); lines 17-18 are method prototypes only, so myclass
// has no property, constraint, typedef or nested class; line 19 is
// 'endclass' without ': myclass'.
TEST_F(ClassMemberTest5Test, MyClassHasOnlyMethodPrototypes) {
  const hldb::ClassDefn *const myClass = getMyClass();
  ASSERT_NE(myClass, nullptr);

  EXPECT_EQ(sizeOf(myClass->getParameters()), 0u) << "8.25: no parameter port list is written";
  EXPECT_EQ(myClass->getExtends(), nullptr) << "8.13: no 'extends' is written";
  EXPECT_EQ(myClass->getImplements(), nullptr) << "8.26: no 'implements' is written";
  EXPECT_EQ(sizeOf(myClass->getVariables()), 0u) << "8.5: no class property is declared";
  EXPECT_EQ(sizeOf(myClass->getConstraints()), 0u) << "no constraint is declared";
  EXPECT_EQ(sizeOf(myClass->getTypedefs()), 0u) << "no type declaration is written inside myclass";
  EXPECT_EQ(sizeOf(myClass->getClassDefns()), 0u) << "no nested class is declared";
  EXPECT_EQ(myClass->getEndLabelObj(), nullptr) << "'endclass' is written without ': myclass'";
}

// Lines 17-18: exactly two methods, pure_task1 then pure_task2, both task
// prototypes declared 'pure virtual' (8.21). A pure virtual method has no
// implementation (8.21), so myclass has no method with a body and neither
// prototype is bound to one.
TEST_F(ClassMemberTest5Test, MyClassHasTwoPureVirtualTasks) {
  const hldb::ClassDefn *const myClass = getMyClass();
  ASSERT_NE(myClass, nullptr);
  EXPECT_EQ(sizeOf(myClass->getMethods()), 0u) << "8.21: a pure virtual method has no implementation";
  const hldb::TaskFuncDeclCollection *const prototypes = myClass->getTaskFuncDecls();
  ASSERT_EQ(sizeOf(prototypes), 2u) << "lines 17-18 declare two method prototypes";

  const std::vector<std::string_view> expectedNames = {"pure_task1", "pure_task2"};
  for (size_t index = 0; index < expectedNames.size(); ++index) {
    const hldb::TaskFuncDecl *const prototype = prototypes->at(index);
    EXPECT_EQ(prototype->getName(), expectedNames[index]) << "method " << (index + 1) << " in source order";
    EXPECT_NE(::any_cast<hldb::TaskDecl>(prototype), nullptr) << expectedNames[index] << " is declared 'task'";
    EXPECT_TRUE(prototype->getMethod()) << "8.6: " << expectedNames[index] << " is declared in the body of myclass";
    EXPECT_TRUE(prototype->getVirtual()) << "8.21: " << expectedNames[index] << " is declared 'virtual'";
    EXPECT_TRUE(prototype->getPure()) << "8.21: " << expectedNames[index] << " is declared 'pure'";
    EXPECT_EQ(prototype->getTaskFunc(), nullptr) << "8.21: " << expectedNames[index] << " has no implementation";
  }
}

// Line 17: 'pure virtual task pure_task1;' has no argument list, so the
// task has no arguments.
TEST_F(ClassMemberTest5Test, PureTask1HasNoArguments) {
  const hldb::TaskDecl *const task = getTaskDecl("pure_task1");
  ASSERT_NE(task, nullptr);
  EXPECT_EQ(sizeOf(task->getIODecls()), 0u) << "no argument list is written";
}

// Line 18: 'pure_task2(int arg)' has one argument, arg. No direction is
// written, so it is an input (13.3). Its type is int (6.11).
TEST_F(ClassMemberTest5Test, PureTask2HasIntInputArg) {
  const hldb::TaskDecl *const task = getTaskDecl("pure_task2");
  ASSERT_NE(task, nullptr);
  const hldb::IODeclCollection *const arguments = task->getIODecls();
  ASSERT_EQ(sizeOf(arguments), 1u) << "pure_task2 declares one argument";

  const hldb::IODecl *const arg = arguments->at(0);
  EXPECT_EQ(arg->getName(), std::string_view("arg"));
  EXPECT_EQ(arg->getDirection(), vpiInput) << "13.3: the default direction of a task argument is input";
  const hldb::RefTypespec *const type = arg->getTypespec();
  ASSERT_NE(type, nullptr) << "'int arg' has an explicit data type";
  EXPECT_NE(type->getActual<hldb::IntTypespec>(), nullptr) << "6.11: arg is an int";
}

// Lines 21-22: 'module test; endmodule' has no port list and no module
// items (23.2), and 'endmodule' carries no ': test' end label.
TEST_F(ClassMemberTest5Test, TestModuleIsEmpty) {
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
  EXPECT_EQ(sizeOf(test->getTaskFuncs()), 0u) << "the tasks are methods of myclass, not of test";
  EXPECT_EQ(sizeOf(test->getClassDefns()), 0u) << "myclass is declared outside module test";
}

// Besides myclass and test the file declares no design element (3.2-3.12):
// myclass is the only class, and it is in no package.
TEST_F(ClassMemberTest5Test, NoOtherDesignElementsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 1u) << "myclass is the only class";
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);
}

// No compilation-unit item (3.12.1) other than myclass is declared: the
// tasks are methods of myclass, and the file declares no let, parameter,
// variable or typedef.
TEST_F(ClassMemberTest5Test, NoOtherCompilationUnitDeclarations) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getTaskFuncs()), 0u) << "the tasks are methods of myclass";
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
TEST_F(ClassMemberTest5Test, TestIsTheTopLevelModule) {
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
