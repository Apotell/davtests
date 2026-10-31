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
// tests/Google/generic/member/class_member_test_1.sv. Lines 1-5 are the block
// comment carrying the suite's :name:/:description:/:tags: metadata, and the
// rest is:
//
//   6  class c;
//   7    task intf.task1();
//   8    endtask
//   9  endclass
//
// The fixture is tagged 8.3 (class declarations). It has no
// :should_fail_because: tag, but see "Expected diagnostic" below.
//
// class c; ... endclass (8.3, A.1.2)
//   - A class named c, declared once, as a compilation-unit item;
//     user-defined (vpiClassType vpiUserDefinedClass, 37.29), not an
//     interface class (8.26) and not abstract (8.21: no 'virtual').
//   - No parameter port list, no 'extends', no 'implements', no property,
//     no constraint, no nested type, no method prototype and no end label.
//   - Exactly one class item: the method task1.
//
// task intf.task1(); endtask (8.6, 13.3, 25.7.4, A.2.7)
//   class_method ::= { method_qualifier } task_declaration
//   task_body_declaration ::= [ interface_identifier . | class_scope ]
//                             task_identifier ( [ tf_port_list ] ) ; ...
//                             endtask [ : task_identifier ]
//   - The grammar accepts the 'intf.' prefix, so there is no syntax error.
//   - The method is named task1; 'intf' is its interface_identifier prefix,
//     not part of its name.
//   - A task declared in the class body is a class method (8.6), automatic
//     (8.6: "Methods declared as part of a class type have automatic
//     lifetime"), with no method qualifier: not virtual (8.20), not pure
//     (8.21).
//   - '()' is an empty port list, so no argument (13.3); the body holds no
//     statement; 'endtask' has no ': task1' end label.
//
// Expected diagnostic
//   25.7.4: the interface_identifier prefix names the interface through
//   which a task is exported. No interface (and no interface port) named
//   intf is declared anywhere in the compiled sources, so 'intf' cannot be
//   resolved and the compiler must report it. HLC's dedicated code is
//   CompUndefinedInterface (5809, 'Undefined interface "intf"').
//
// KNOWN FAILURE (undeclared interface prefix not reported), not a defect in
// this test: HLC 577 builds task1 with an 'intf' prefix that is bound to
// nothing, but reports no diagnostic at all for it, so
// UndefinedInterfaceIntfIsReported fails. It asserts the LRM behavior and
// is intentionally not skipped or relaxed. The sv-tests fixture also lacks
// a :should_fail_because: tag.
//
// The file declares no module, so there is no top-level module (23.3.1).
// The .hlc also puts UVM 1800.2-2017 on the include path ('-I') but adds no
// UVM source file, and passes -nobuiltin, so nothing else is compiled.
//
// What is checked
//   - No fatal and no syntax diagnostic; CompUndefinedInterface is reported
//     for intf.
//   - Exactly one SourceFile for the .sv, with no includes, directives,
//     macro definitions or macro instances.
//   - Exactly one class c: user-defined, not an interface class, not
//     virtual, with no parameters, base class, implemented class, property,
//     constraint, typedef, nested class, method prototype or end label.
//   - c has exactly one method with a body, task1.
//   - task1 is a task, a class method, automatic, not virtual, not pure,
//     with no arguments, no statement and no end label.
//   - task1's prefix names intf and is bound to nothing.
//   - No module, no other design element and no other compilation-unit
//     declaration; no top-level module.
//
// What is NOT checked, and why
//   - The total error count: the guide forbids counting beyond the coarse
//     checks, and the LRM does not fix how many diagnostics one undeclared
//     prefix yields.
//   - task1's access (local/protected): none is written, so it is public
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
#include <hlc/Events/EventType.h>
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
#include <hldb/ref_obj.h>
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
constexpr std::string_view kSourceFileName = "class_member_test_1.sv";

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
}  // namespace

class ClassMemberTest1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_member_test_1.hlc"}); }
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

  // The class c, or nullptr.
  static const hldb::ClassDefn *getClassC() {
    if (m_design == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::ClassDefn>("c", m_design->getAllClasses());
  }

  // The method task1 of c, or nullptr. It has a body, so HLDB records it in
  // ClassDefn::getMethods().
  static const hldb::TaskFunc *getTask1() {
    const hldb::ClassDefn *const classC = getClassC();
    if (classC == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::TaskFunc>("task1", classC->getMethods());
  }
};

// Line 7: the grammar accepts the 'intf.' prefix (A.2.7), so nothing is
// fatal or a syntax error.
TEST_F(ClassMemberTest1Test, NoFatalOrSyntaxErrors) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  const EventListener::Stats stats = eventListener->getStats();
  EXPECT_EQ(stats.m_fatals, 0) << describeErrors(eventListener, EventSeverity::Fatal);
  EXPECT_EQ(stats.m_syntaxes, 0) << describeErrors(eventListener, EventSeverity::Syntax);
}

// Line 7: 25.7.4 makes 'intf' the interface through which task1 is
// exported, but no interface named intf is declared, so it is reported.
TEST_F(ClassMemberTest1Test, UndefinedInterfaceIntfIsReported) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  EXPECT_NE(findEvent(EventType::CompUndefinedInterface, "intf"), nullptr)
      << "25.7.4: no interface named intf is declared;" << describeErrors(eventListener, EventSeverity::Error);
}

TEST_F(ClassMemberTest1Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

// The file holds no compiler directive, no `define and no macro usage.
TEST_F(ClassMemberTest1Test, SourceFileHasNoIncludesDirectivesOrMacros) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getIncludeStmts()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// Line 6: 'class c;' declares the class once. It is written in the source,
// so it is user-defined (37.29 vpiClassType); it is neither an interface
// class (8.26) nor an abstract 'virtual class' (8.21).
TEST_F(ClassMemberTest1Test, ClassCIsUserDefinedConcreteClass) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countNamed(m_design->getAllClasses(), "c"), 1u) << "c is declared exactly once";

  const hldb::ClassDefn *const classC = getClassC();
  ASSERT_NE(classC, nullptr);
  EXPECT_EQ(classC->getClassType(), vpiUserDefinedClass) << "8.3: c is declared in the source text";
  EXPECT_FALSE(classC->getIsInterfaceClass()) << "8.26: 'interface' is not written";
  EXPECT_FALSE(classC->getVirtual()) << "8.21: 'virtual' is not written before 'class'";
}

// Line 6 has no parameter port list (8.25), no 'extends' (8.13) and no
// 'implements' (8.26); lines 7-8 are the only class item, so c has no
// property, constraint, typedef, nested class or method prototype; line 9
// is 'endclass' without ': c'.
TEST_F(ClassMemberTest1Test, ClassCHasOnlyOneMethod) {
  const hldb::ClassDefn *const classC = getClassC();
  ASSERT_NE(classC, nullptr);

  EXPECT_EQ(sizeOf(classC->getParameters()), 0u) << "8.25: no parameter port list is written";
  EXPECT_EQ(classC->getExtends(), nullptr) << "8.13: no 'extends' is written";
  EXPECT_EQ(classC->getImplements(), nullptr) << "8.26: no 'implements' is written";
  EXPECT_EQ(sizeOf(classC->getVariables()), 0u) << "8.5: no class property is declared";
  EXPECT_EQ(sizeOf(classC->getConstraints()), 0u) << "no constraint is declared";
  EXPECT_EQ(sizeOf(classC->getTypedefs()), 0u) << "no type declaration is written inside c";
  EXPECT_EQ(sizeOf(classC->getClassDefns()), 0u) << "no nested class is declared";
  EXPECT_EQ(sizeOf(classC->getTaskFuncDecls()), 0u) << "8.21/8.24: no method prototype is written";
  EXPECT_EQ(classC->getEndLabelObj(), nullptr) << "'endclass' is written without ': c'";

  const hldb::TaskFuncCollection *const methods = classC->getMethods();
  ASSERT_EQ(sizeOf(methods), 1u) << "lines 7-8 declare the only method";
  EXPECT_EQ(methods->at(0)->getName(), std::string_view("task1")) << "A.2.7: 'intf.' is a prefix, not part of the name";
}

// Lines 7-8: a task declared in the class body is a class method (8.6),
// declared as a task (13.3), with no method qualifier: not virtual (8.20),
// not pure (8.21). 8.6: class methods have automatic lifetime.
TEST_F(ClassMemberTest1Test, Task1IsAutomaticNonVirtualTaskMethod) {
  const hldb::TaskFunc *const task1 = getTask1();
  ASSERT_NE(task1, nullptr);

  EXPECT_NE(::any_cast<hldb::Task>(task1), nullptr) << "13.3: task1 is declared 'task'";
  EXPECT_TRUE(task1->getMethod()) << "8.6: task1 is declared in the body of c";
  EXPECT_TRUE(task1->getAutomatic()) << "8.6: methods of a class have automatic lifetime";
  EXPECT_FALSE(task1->getVirtual()) << "8.20: 'virtual' is not written";
  EXPECT_FALSE(task1->getPure()) << "8.21: 'pure' is not written";
}

// Lines 7-8: '()' is an empty port list, so no argument (13.3); the body
// holds no statement; 'endtask' has no ': task1' end label.
TEST_F(ClassMemberTest1Test, Task1HasNoArgumentsAndEmptyBody) {
  const hldb::TaskFunc *const task1 = getTask1();
  ASSERT_NE(task1, nullptr);

  EXPECT_EQ(sizeOf(task1->getIODecls()), 0u) << "13.3: '()' declares no argument";
  EXPECT_EQ(task1->getStmt(), nullptr) << "13.3: the task body holds no statement";
  EXPECT_EQ(task1->getEndLabelObj(), nullptr) << "'endtask' is written without ': task1'";
}

// Line 7: 'intf.' is task1's interface_identifier prefix (A.2.7). No
// interface named intf is declared, so the prefix is bound to nothing.
TEST_F(ClassMemberTest1Test, Task1PrefixNamesUnresolvedIntf) {
  const hldb::TaskFunc *const task1 = getTask1();
  ASSERT_NE(task1, nullptr);

  const hldb::Any *const prefix = task1->getPrefix();
  ASSERT_NE(prefix, nullptr) << "A.2.7: 'intf.' is written before task1";
  EXPECT_EQ(prefix->getName(), std::string_view("intf"));
  const hldb::RefObj *const prefixRef = ::any_cast<hldb::RefObj>(prefix);
  ASSERT_NE(prefixRef, nullptr) << "the prefix refers to the interface named intf";
  EXPECT_EQ(prefixRef->getActual(), nullptr) << "25.7.4: no interface named intf is declared";
}

// Besides c the file declares no design element (3.2-3.12): no module, so
// no top-level module either (23.3.1).
TEST_F(ClassMemberTest1Test, NoOtherDesignElementsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 1u) << "c is the only class";
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 0u) << "no module is declared";
  EXPECT_EQ(countFromSourceFile(m_design->getTopModules()), 0u) << "23.3.1: no module, so no top-level module";
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u) << "intf is not declared";
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);
}

// No compilation-unit item (3.12.1) other than c is declared: task1 is a
// method of c, and the file declares no let, parameter, variable or
// typedef.
TEST_F(ClassMemberTest1Test, NoOtherCompilationUnitDeclarations) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getTaskFuncs()), 0u) << "task1 is a method of c";
  EXPECT_EQ(countFromSourceFile(m_design->getLetDecls()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParamAssigns()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTypedefs()), 0u);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
