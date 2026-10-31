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
// tests/Google/generic/member/class_member_test_16.sv. Lines 1-5 are the
// block comment carrying the suite's :name:/:description:/:tags: metadata,
// and the rest is:
//
//   6  class myclass;
//   7  extern function void subroutine;
//   8  endclass
//
// The fixture is tagged 8.3 (class declarations) and has no
// :should_fail_because: tag.
//
// class myclass; ... endclass (8.3, A.1.2)
//   - A class named myclass, declared once, as a compilation-unit item;
//     user-defined (vpiClassType vpiUserDefinedClass, 37.29), not an
//     interface class (8.26) and not abstract (8.21: no 'virtual').
//   - No parameter port list, no 'extends', no 'implements', no property,
//     no constraint, no nested type and no end label.
//   - Exactly one class item: the extern method prototype subroutine.
//
// extern function void subroutine; (8.24, 13.4, A.1.9)
//   class_method ::= extern { method_qualifier } method_prototype ;
//   - A function prototype of myclass, a class method (8.6) with no
//     'virtual' (8.20) or 'pure' (8.21) qualifier, returning void (13.4.1),
//     with no arguments.
//   - No out-of-block declaration 'function void myclass::subroutine ...'
//     exists in the compiled sources, so the prototype has no
//     implementation and myclass has no method with a body.
//
// Unlike class_member_test_15, the file declares neither the out-of-block
// body nor a module, so there is no top-level module (23.3.1). The .hlc also
// puts UVM 1800.2-2017 on the include path ('-I') but adds no UVM source
// file, and passes -nobuiltin, so nothing else is compiled.
//
// What is checked
//   - No fatal and no syntax diagnostic (the grammar accepts the source).
//   - Exactly one SourceFile for the .sv, with no includes, directives,
//     macro definitions or macro instances.
//   - Exactly one class myclass: user-defined, not an interface class, not
//     virtual, with no parameters, base class, implemented class, property,
//     constraint, typedef, nested class or end label.
//   - myclass has exactly one method prototype, subroutine: a function, a
//     class method, not virtual, not pure, returning void, with no
//     arguments and no implementation; and no method with a body.
//   - No module, no top-level module, no other design element and no
//     compilation-unit declaration (no stray compilation-unit function
//     subroutine).
//
// Model note: HLDB records method prototypes as TaskFuncDecl (here a
// FunctionDecl) in ClassDefn::getTaskFuncDecls(); getMethods() holds
// methods with a body, and TaskFuncDecl::getTaskFunc() links a prototype to
// its body. A written 'void' return type is a VoidTypespec.
//
// What is NOT checked, and why
//   - Whether an error is reported for the missing out-of-block body: 8.24
//     describes how an extern method's out-of-block declaration must match
//     its prototype, but the test author found no LRM sentence that makes
//     an extern prototype without a body an error by itself (tools commonly
//     report it only when the method is referenced, which never happens
//     here), and the fixture carries no :should_fail_because:. So neither
//     an error nor its absence is asserted; only fatal and syntax
//     diagnostics are ruled out.
//   - The prototype's lifetime flag: a prototype has no body whose
//     variables a lifetime could govern.
//   - subroutine's access (local/protected): none is written, so it is
//     public (8.18); not the subject of this fixture.
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
constexpr std::string_view kSourceFileName = "class_member_test_16.sv";

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

class ClassMemberTest16Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_member_test_16.hlc"}); }
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
};

// The grammar accepts lines 6-8 (A.1.9), so nothing is fatal or a syntax
// error. Whether the missing out-of-block body is an error is not asserted
// (see the header).
TEST_F(ClassMemberTest16Test, NoFatalOrSyntaxErrors) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  const EventListener::Stats stats = eventListener->getStats();
  EXPECT_EQ(stats.m_fatals, 0) << describeErrors(eventListener, EventSeverity::Fatal);
  EXPECT_EQ(stats.m_syntaxes, 0) << describeErrors(eventListener, EventSeverity::Syntax);
}

TEST_F(ClassMemberTest16Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

// The file holds no compiler directive, no `define and no macro usage.
TEST_F(ClassMemberTest16Test, SourceFileHasNoIncludesDirectivesOrMacros) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getIncludeStmts()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// Line 6: 'class myclass;' declares the class once. It is written in the
// source, so it is user-defined (37.29 vpiClassType); it is neither an
// interface class (8.26) nor an abstract 'virtual class' (8.21).
TEST_F(ClassMemberTest16Test, MyClassIsUserDefinedConcreteClass) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countNamed(m_design->getAllClasses(), "myclass"), 1u) << "myclass is declared exactly once";

  const hldb::ClassDefn *const myClass = getMyClass();
  ASSERT_NE(myClass, nullptr);
  EXPECT_EQ(myClass->getClassType(), vpiUserDefinedClass) << "8.3: myclass is declared in the source text";
  EXPECT_FALSE(myClass->getIsInterfaceClass()) << "8.26: 'interface' is not written";
  EXPECT_FALSE(myClass->getVirtual()) << "8.21: 'virtual' is not written before 'class'";
}

// Line 6 has no parameter port list (8.25), no 'extends' (8.13) and no
// 'implements' (8.26); line 7 is the only class item, so myclass has no
// property, constraint, typedef or nested class; line 8 is 'endclass'
// without ': myclass'.
TEST_F(ClassMemberTest16Test, MyClassHasNoOtherItems) {
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

// Line 7: 'extern function void subroutine;' is the only method prototype
// of myclass (8.24): a function returning void (13.4.1), a class method
// (8.6), not virtual (8.20), not pure (8.21), with no arguments.
TEST_F(ClassMemberTest16Test, MyClassHasOneExternVoidFunctionPrototype) {
  const hldb::ClassDefn *const myClass = getMyClass();
  ASSERT_NE(myClass, nullptr);
  const hldb::TaskFuncDeclCollection *const prototypes = myClass->getTaskFuncDecls();
  ASSERT_EQ(sizeOf(prototypes), 1u) << "8.24: line 7 declares the only method prototype";

  const hldb::TaskFuncDecl *const prototype = prototypes->at(0);
  EXPECT_EQ(prototype->getName(), std::string_view("subroutine"));
  EXPECT_TRUE(prototype->getMethod()) << "8.6: subroutine is declared in the body of myclass";
  EXPECT_FALSE(prototype->getVirtual()) << "8.20: 'virtual' is not written";
  EXPECT_FALSE(prototype->getPure()) << "8.21: 'pure' is not written";
  EXPECT_EQ(sizeOf(prototype->getIODecls()), 0u) << "no argument list is written";

  const hldb::FunctionDecl *const functionDecl = ::any_cast<hldb::FunctionDecl>(prototype);
  ASSERT_NE(functionDecl, nullptr) << "13.4: subroutine is declared 'function'";
  const hldb::RefTypespec *const returnType = functionDecl->getReturn();
  ASSERT_NE(returnType, nullptr) << "13.4.1: 'void' is written as the return type";
  EXPECT_NE(returnType->getActual<hldb::VoidTypespec>(), nullptr) << "13.4.1: subroutine returns void";
}

// No 'function void myclass::subroutine ... endfunction' is written, so the
// extern prototype has no implementation (8.24) and myclass has no method
// with a body.
TEST_F(ClassMemberTest16Test, PrototypeHasNoImplementation) {
  const hldb::ClassDefn *const myClass = getMyClass();
  ASSERT_NE(myClass, nullptr);
  EXPECT_EQ(sizeOf(myClass->getMethods()), 0u) << "8.24: no out-of-block body of subroutine is written";

  const hldb::TaskFuncDecl *const prototype =
      hldb::findByName<hldb::TaskFuncDecl>("subroutine", myClass->getTaskFuncDecls());
  ASSERT_NE(prototype, nullptr);
  EXPECT_EQ(prototype->getTaskFunc(), nullptr) << "8.24: no out-of-block body of subroutine is written";
}

// The file declares no module (23.2), so there is no top-level module
// (23.3.1), and besides myclass no other design element (3.2-3.12).
TEST_F(ClassMemberTest16Test, NoModulesOrOtherDesignElementsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 1u) << "myclass is the only class";
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 0u) << "no module is declared";
  EXPECT_EQ(countFromSourceFile(m_design->getTopModules()), 0u) << "23.3.1: no module, so no top-level module";
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);
}

// No compilation-unit item (3.12.1) other than myclass is declared: the
// prototype belongs to myclass, and the file declares no function, task,
// let, parameter, variable or typedef.
TEST_F(ClassMemberTest16Test, NoOtherCompilationUnitDeclarations) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getTaskFuncs()), 0u) << "subroutine is a prototype of myclass";
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
