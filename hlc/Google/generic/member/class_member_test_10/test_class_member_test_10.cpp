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
// tests/Google/generic/member/class_member_test_10.sv. Lines 1-7 are
// one-line "//" license comments, lines 8-9 are blank, lines 10-14 are the
// block comment carrying the suite's :name:/:description:/:tags: metadata,
// and the rest is:
//
//   15  class outerclass;
//   16    class innerclass;
//   17      class reallyinnerclass;
//   18        task subtask;
//   19        endtask
//   20      endclass
//   21    endclass
//   22  endclass
//   23
//   24  module test;
//   25  endmodule
//
// The fixture is tagged 8.3 (class declarations).
//
// Nested classes (8.3, 8.23, A.1.9)
//   class_item ::= ... | class_declaration
//   - A class declaration is a legal class item, so a class can be declared
//     inside another class (8.23: "Classes can be nested"). The nesting
//     here is three deep:
//       outerclass        a compilation-unit class (line 15)
//       innerclass        nested in outerclass (line 16)
//       reallyinnerclass  nested in innerclass (line 17)
//   - Each class is declared once, is user-defined (vpiClassType
//     vpiUserDefinedClass, 37.29), is not an interface class (8.26) and is
//     not declared 'virtual' (8.21).
//   - No class has a parameter port list, 'extends', 'implements', property,
//     constraint, typedef or end label.
//   - outerclass's only item is innerclass; innerclass's only item is
//     reallyinnerclass; reallyinnerclass's only item is the method subtask.
//     So outerclass and innerclass have no methods of their own, and
//     reallyinnerclass has no nested class.
//
// task subtask; endtask (8.6, 13.3)
//   - A task declared in the body of reallyinnerclass, so a class method
//     (8.6) with a body, automatic (8.6: "Methods declared as part of a
//     class type have automatic lifetime"), not virtual (8.20), not pure
//     (8.21), with no arguments, no statement and no end label.
//
// module test; endmodule (23.2)
//   - A module named test, declared once, with no port list and no module
//     items. test is never instantiated, so it is a top-level module (23.3.1).
//     The instance tree and the top-module flag only exist after elaboration.
//
// The file is legal, so it compiles without errors. The .hlc also puts UVM
// 1800.2-2017 on the include path ('-I') but adds no UVM source file, and
// passes -nobuiltin, so nothing else is compiled.
//
// What is checked
//   - No fatal, syntax or error diagnostic.
//   - Exactly one SourceFile for the .sv, with no includes, directives,
//     macro definitions or macro instances.
//   - outerclass: declared once, user-defined, not an interface class, not
//     virtual, with no parameters, base class, implemented class, property,
//     constraint, typedef, method or end label; exactly one nested class,
//     innerclass.
//   - innerclass: nested in outerclass, user-defined, not an interface
//     class, not virtual, with no parameters, base class, implemented
//     class, property, constraint, typedef, method or end label; exactly one
//     nested class, reallyinnerclass.
//   - reallyinnerclass: nested in innerclass, user-defined, not an interface
//     class, not virtual, with no parameters, base class, implemented class,
//     property, constraint, typedef, nested class or end label; exactly one
//     method, subtask, and no method prototype.
//   - subtask is a task, a class method, automatic, not virtual, not pure,
//     with no arguments, no statement and no end label.
//   - Exactly one module test, with no port or module item and no end label.
//   - No other design element and no compilation-unit declaration other than
//     outerclass.
//   - Top-level status of test: after elaboration test is a top-level module
//     with one top instance named test; before elaboration no module is
//     flagged top (maintainer's convention for non-elaborated models).
//
// What is NOT checked, and why
//   - Whether Design::getAllClasses() also lists the nested classes: the
//     name suggests a flat list of every class, and the LRM does not fix
//     the content of that tool collection. The nesting itself is checked
//     through getClassDefns() and getParent().
//   - subtask's access (local/protected): none is written, so it is public
//     (8.18); the access-type encoding is a model convention.
//   - The license and metadata comments (5.4): comments carry no meaning.
//   - The SourceFile time unit and precision: they come from the
//     -timescale command-line option, not from the SystemVerilog text.
//   - Source line numbers: a start line encodes nothing about
//     SystemVerilog semantics.
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
#include <hldb/interface.h>
#include <hldb/io_decl.h>
#include <hldb/let_decl.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/program.h>
#include <hldb/source_file.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task.h>
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
constexpr std::string_view kSourceFileName = "class_member_test_10.sv";

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

// Every diagnostic of 'severity' as " [<code> line <n>: <text>]", for failure
// messages, so a red run shows what was reported and where. <text> is HLC's
// message template, so its placeholders are not filled in.
std::string describeErrors(const EventListener *eventListener, EventSeverity severity) {
  std::string description;
  for (const Event &error : eventListener->getEvents()) {
    const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
    if ((errorInfo == nullptr) || (errorInfo->m_severity != severity)) {
      continue;
    }
    description += " [" + std::to_string(static_cast<int32_t>(error.getType()));
    description += " line " + std::to_string(lineOf(error));
    description += ": " + std::string(errorInfo->m_lines[0].m_text) + "]";
  }
  return description;
}

// Checks what every class of this file shares: user-defined (37.29), not an
// interface class (8.26), not 'virtual' (8.21), and no parameter port list,
// 'extends', 'implements', property, constraint, typedef or end label.
void expectPlainUserClass(const hldb::ClassDefn *classDefn, std::string_view name) {
  EXPECT_EQ(classDefn->getClassType(), vpiUserDefinedClass) << "8.3: " << name << " is declared in the source text";
  EXPECT_FALSE(classDefn->getIsInterfaceClass()) << "8.26: 'interface' is not written for " << name;
  EXPECT_FALSE(classDefn->getVirtual()) << "8.21: 'virtual' is not written before 'class " << name << "'";
  EXPECT_EQ(sizeOf(classDefn->getParameters()), 0u) << "8.25: " << name << " has no parameter port list";
  EXPECT_EQ(classDefn->getExtends(), nullptr) << "8.13: " << name << " has no 'extends'";
  EXPECT_EQ(classDefn->getImplements(), nullptr) << "8.26: " << name << " has no 'implements'";
  EXPECT_EQ(sizeOf(classDefn->getVariables()), 0u) << "8.5: " << name << " declares no property";
  EXPECT_EQ(sizeOf(classDefn->getConstraints()), 0u) << name << " declares no constraint";
  EXPECT_EQ(sizeOf(classDefn->getTypedefs()), 0u) << name << " declares no type";
  EXPECT_EQ(sizeOf(classDefn->getTaskFuncDecls()), 0u) << name << " declares no method prototype";
  EXPECT_EQ(classDefn->getEndLabelObj(), nullptr) << "'endclass' of " << name << " has no end label";
}
}  // namespace

class ClassMemberTest10Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_member_test_10.hlc"}); }
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

  // The compilation-unit class outerclass (line 15), or nullptr.
  static const hldb::ClassDefn *getOuterClass() {
    if (m_design == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::ClassDefn>("outerclass", m_design->getAllClasses());
  }

  // The class innerclass nested in outerclass (line 16), or nullptr.
  static const hldb::ClassDefn *getInnerClass() {
    const hldb::ClassDefn *const outerClass = getOuterClass();
    if (outerClass == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::ClassDefn>("innerclass", outerClass->getClassDefns());
  }

  // The class reallyinnerclass nested in innerclass (line 17), or nullptr.
  static const hldb::ClassDefn *getReallyInnerClass() {
    const hldb::ClassDefn *const innerClass = getInnerClass();
    if (innerClass == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::ClassDefn>("reallyinnerclass", innerClass->getClassDefns());
  }

  // The method subtask of reallyinnerclass (line 18), or nullptr.
  static const hldb::TaskFunc *getSubtask() {
    const hldb::ClassDefn *const reallyInnerClass = getReallyInnerClass();
    if (reallyInnerClass == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::TaskFunc>("subtask", reallyInnerClass->getMethods());
  }

  // The module definition test, or nullptr.
  static const hldb::Module *getTestModule() {
    if (m_design == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  }
};

// Lines 15-25 are legal nested classes (8.23) and a module (23.2), so
// nothing is fatal, a syntax error or an error.
TEST_F(ClassMemberTest10Test, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  const EventListener::Stats stats = eventListener->getStats();
  EXPECT_EQ(stats.m_fatals, 0) << describeErrors(eventListener, EventSeverity::Fatal);
  EXPECT_EQ(stats.m_syntaxes, 0) << describeErrors(eventListener, EventSeverity::Syntax);
  EXPECT_EQ(stats.m_errors, 0) << describeErrors(eventListener, EventSeverity::Error);
}

TEST_F(ClassMemberTest10Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

// The file holds no compiler directive, no `define and no macro usage.
TEST_F(ClassMemberTest10Test, SourceFileHasNoIncludesDirectivesOrMacros) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getIncludeStmts()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// Lines 15 and 22: 'class outerclass; ... endclass' is declared once as a
// compilation-unit class. Its only item is the nested class innerclass
// (lines 16-21), so it has no method of its own.
TEST_F(ClassMemberTest10Test, OuterClassHoldsOnlyInnerClass) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countNamed(m_design->getAllClasses(), "outerclass"), 1u) << "outerclass is declared exactly once";

  const hldb::ClassDefn *const outerClass = getOuterClass();
  ASSERT_NE(outerClass, nullptr);
  expectPlainUserClass(outerClass, "outerclass");
  EXPECT_EQ(sizeOf(outerClass->getMethods()), 0u) << "subtask is declared in reallyinnerclass, not in outerclass";

  const hldb::ClassDefnCollection *const nested = outerClass->getClassDefns();
  ASSERT_EQ(sizeOf(nested), 1u) << "8.23: innerclass is the only class nested directly in outerclass";
  EXPECT_EQ(nested->at(0)->getName(), std::string_view("innerclass"));
}

// Lines 16 and 21: 'class innerclass; ... endclass' is nested in
// outerclass (8.23). Its only item is the nested class reallyinnerclass
// (lines 17-20), so it has no method of its own.
TEST_F(ClassMemberTest10Test, InnerClassIsNestedInOuterClass) {
  const hldb::ClassDefn *const innerClass = getInnerClass();
  ASSERT_NE(innerClass, nullptr) << "8.23: innerclass is declared inside outerclass";
  EXPECT_EQ(innerClass->getParent(), getOuterClass()) << "8.23: innerclass is enclosed by outerclass";
  expectPlainUserClass(innerClass, "innerclass");
  EXPECT_EQ(sizeOf(innerClass->getMethods()), 0u) << "subtask is declared in reallyinnerclass, not in innerclass";

  const hldb::ClassDefnCollection *const nested = innerClass->getClassDefns();
  ASSERT_EQ(sizeOf(nested), 1u) << "8.23: reallyinnerclass is the only class nested directly in innerclass";
  EXPECT_EQ(nested->at(0)->getName(), std::string_view("reallyinnerclass"));
}

// Lines 17 and 20: 'class reallyinnerclass; ... endclass' is nested in
// innerclass (8.23). Its only item is the method subtask (lines 18-19), so
// it has no nested class.
TEST_F(ClassMemberTest10Test, ReallyInnerClassIsNestedInInnerClass) {
  const hldb::ClassDefn *const reallyInnerClass = getReallyInnerClass();
  ASSERT_NE(reallyInnerClass, nullptr) << "8.23: reallyinnerclass is declared inside innerclass";
  EXPECT_EQ(reallyInnerClass->getParent(), getInnerClass()) << "8.23: reallyinnerclass is enclosed by innerclass";
  expectPlainUserClass(reallyInnerClass, "reallyinnerclass");
  EXPECT_EQ(sizeOf(reallyInnerClass->getClassDefns()), 0u) << "no class is nested in reallyinnerclass";

  const hldb::TaskFuncCollection *const methods = reallyInnerClass->getMethods();
  ASSERT_EQ(sizeOf(methods), 1u) << "lines 18-19 declare the only method of reallyinnerclass";
  EXPECT_EQ(methods->at(0)->getName(), std::string_view("subtask"));
}

// Lines 18-19: 'task subtask; endtask' inside reallyinnerclass is a class
// method (8.6) declared as a task (13.3), with no method qualifier: not
// virtual (8.20), not pure (8.21). 8.6: class methods have automatic
// lifetime.
TEST_F(ClassMemberTest10Test, SubtaskIsAutomaticNonVirtualTaskMethod) {
  const hldb::TaskFunc *const subtask = getSubtask();
  ASSERT_NE(subtask, nullptr);

  EXPECT_NE(::any_cast<hldb::Task>(subtask), nullptr) << "13.3: subtask is declared 'task'";
  EXPECT_TRUE(subtask->getMethod()) << "8.6: subtask is declared in the body of reallyinnerclass";
  EXPECT_TRUE(subtask->getAutomatic()) << "8.6: methods of a class have automatic lifetime";
  EXPECT_FALSE(subtask->getVirtual()) << "8.20: 'virtual' is not written";
  EXPECT_FALSE(subtask->getPure()) << "8.21: 'pure' is not written";
}

// Lines 18-19: 'task subtask;' has no port list and no tf_item_declaration,
// so no argument (13.3); the body holds no statement; 'endtask' has no
// ': subtask' end label.
TEST_F(ClassMemberTest10Test, SubtaskHasNoArgumentsAndEmptyBody) {
  const hldb::TaskFunc *const subtask = getSubtask();
  ASSERT_NE(subtask, nullptr);

  EXPECT_EQ(sizeOf(subtask->getIODecls()), 0u) << "13.3: no argument is declared";
  EXPECT_EQ(subtask->getStmt(), nullptr) << "13.3: the task body holds no statement";
  EXPECT_EQ(subtask->getEndLabelObj(), nullptr) << "'endtask' is written without ': subtask'";
}

// Lines 24-25: 'module test; endmodule' has no port list and no module
// items (23.2), and 'endmodule' carries no ': test' end label.
TEST_F(ClassMemberTest10Test, TestModuleIsEmpty) {
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
  EXPECT_EQ(sizeOf(test->getTaskFuncs()), 0u) << "subtask is a method of reallyinnerclass, not of test";
  EXPECT_EQ(sizeOf(test->getClassDefns()), 0u) << "the classes are declared outside module test";
}

// Besides the classes and test the file declares no design element
// (3.2-3.12): no interface, program, package, primitive, config or checker.
TEST_F(ClassMemberTest10Test, NoOtherDesignElementsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);
}

// No compilation-unit item (3.12.1) other than outerclass is declared:
// subtask is a method of reallyinnerclass, and the file declares no let,
// parameter, variable or typedef.
TEST_F(ClassMemberTest10Test, NoOtherCompilationUnitDeclarations) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getTaskFuncs()), 0u) << "subtask is a method of reallyinnerclass";
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
TEST_F(ClassMemberTest10Test, TestIsTheTopLevelModule) {
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
