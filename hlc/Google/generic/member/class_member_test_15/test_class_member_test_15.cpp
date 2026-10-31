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
// tests/Google/generic/member/class_member_test_15.sv. Lines 1-7 are
// one-line "//" license comments, lines 8-9 are blank, lines 10-14 are the
// block comment carrying the suite's :name:/:description:/:tags: metadata,
// and the rest is:
//
//   15  class myclass;
//   16  extern function void subroutine;
//   17  endclass
//   18
//   19  function void myclass::subroutine; endfunction
//   20
//   21  module test;
//   22  endmodule
//
// The fixture is tagged 8.3 (class declarations).
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
//     'virtual' (8.20) or 'pure' (8.21) qualifier.
//   - Its return type is void (13.4.1: a void function returns no value).
//     'void' is written explicitly; an omitted return type would instead be
//     an implicit logic (13.4.1), so the model must record void.
//   - No argument list, so no arguments.
//
// function void myclass::subroutine; endfunction (8.24, A.2.6)
//   - 8.24: the out-of-block declaration is the body of the extern method
//     subroutine of myclass ('myclass::' binds it to the class). It is a
//     class method, not a compilation-unit function.
//   - It is a function returning void, automatic (8.6: "Methods declared as
//     part of a class type have automatic lifetime"), not virtual, not
//     pure, with no arguments (8.24: it matches the prototype), no
//     statement and no end label.
//   - The prototype's implementation is this body.
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
//   - Exactly one class myclass: user-defined, not an interface class, not
//     virtual, with no parameters, base class, implemented class, property,
//     constraint, typedef, nested class or end label.
//   - myclass has exactly one method prototype, subroutine: a function, a
//     class method, not virtual, not pure, returning void, with no
//     arguments.
//   - myclass has exactly one method with a body, subroutine: a function, a
//     class method, automatic, not virtual, not pure, returning void, with
//     no arguments, no statement and no end label.
//   - The prototype's implementation is that body.
//   - Exactly one module test, with no port or module item and no end label.
//   - No other design element and no other compilation-unit declaration (in
//     particular, the out-of-block body is not a compilation-unit function).
//   - Top-level status of test: after elaboration test is a top-level module
//     with one top instance named test; before elaboration no module is
//     flagged top (maintainer's convention for non-elaborated models).
//
// KNOWN FAILURE (out-of-block body not flagged as a method), not a defect
// in this test: HLC 577 records the body 'function void
// myclass::subroutine' in myclass's getMethods(), with a void return, and
// links the prototype to it, but getMethod() (vpiMethod) is false on that
// body, although 8.24 makes it the body of a method of myclass (the
// prototype itself is flagged). The getMethod() check of
// MyClassHasOutOfBlockSubroutineBody asserts the LRM behavior and is
// expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// Model note: HLDB records method prototypes as TaskFuncDecl (here a
// FunctionDecl) in ClassDefn::getTaskFuncDecls(); getMethods() holds
// methods with a body, and TaskFuncDecl::getTaskFunc() links a prototype to
// its body. A written 'void' return type is a VoidTypespec.
//
// What is NOT checked, and why
//   - How the 'myclass::' class_scope is encoded on the body (prefix or
//     otherwise): the binding it expresses is checked through getMethods()
//     and getTaskFunc() instead.
//   - The prototype's lifetime flag: a prototype has no body whose
//     variables a lifetime could govern; the body's lifetime is checked.
//   - subroutine's access (local/protected): none is written, so it is
//     public (8.18); not the subject of this fixture.
//   - Function::getFuncType(): an encoding of the return type, which is
//     checked directly through getReturn().
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
#include <hldb/function.h>
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
constexpr std::string_view kSourceFileName = "class_member_test_15.sv";

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

// Checks that 'returnType' is the written return type 'void' (13.4.1).
void expectVoidReturn(const hldb::RefTypespec *returnType, std::string_view where) {
  ASSERT_NE(returnType, nullptr) << where << ": 13.4.1: 'void' is written as the return type";
  EXPECT_NE(returnType->getActual<hldb::VoidTypespec>(), nullptr) << where << ": 13.4.1: subroutine returns void";
}
}  // namespace

class ClassMemberTest15Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_member_test_15.hlc"}); }
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

  // The extern prototype subroutine of myclass (line 16), or nullptr.
  static const hldb::TaskFuncDecl *getSubroutinePrototype() {
    const hldb::ClassDefn *const myClass = getMyClass();
    if (myClass == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::TaskFuncDecl>("subroutine", myClass->getTaskFuncDecls());
  }

  // The out-of-block body of subroutine (line 19), a method of myclass, or nullptr.
  static const hldb::TaskFunc *getSubroutineBody() {
    const hldb::ClassDefn *const myClass = getMyClass();
    if (myClass == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::TaskFunc>("subroutine", myClass->getMethods());
  }

  // The module definition test, or nullptr.
  static const hldb::Module *getTestModule() {
    if (m_design == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  }
};

// Lines 15-22 are a legal class with an extern method (8.24), its
// out-of-block body and a module (23.2), so nothing is fatal, a syntax error
// or an error.
TEST_F(ClassMemberTest15Test, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  const EventListener::Stats stats = eventListener->getStats();
  EXPECT_EQ(stats.m_fatals, 0) << describeErrors(eventListener, EventSeverity::Fatal);
  EXPECT_EQ(stats.m_syntaxes, 0) << describeErrors(eventListener, EventSeverity::Syntax);
  EXPECT_EQ(stats.m_errors, 0) << describeErrors(eventListener, EventSeverity::Error);
}

TEST_F(ClassMemberTest15Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

// The file holds no compiler directive, no `define and no macro usage.
TEST_F(ClassMemberTest15Test, SourceFileHasNoIncludesDirectivesOrMacros) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getIncludeStmts()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// Line 15: 'class myclass;' declares the class once. It is written in the
// source, so it is user-defined (37.29 vpiClassType); it is neither an
// interface class (8.26) nor an abstract 'virtual class' (8.21).
TEST_F(ClassMemberTest15Test, MyClassIsUserDefinedConcreteClass) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countNamed(m_design->getAllClasses(), "myclass"), 1u) << "myclass is declared exactly once";

  const hldb::ClassDefn *const myClass = getMyClass();
  ASSERT_NE(myClass, nullptr);
  EXPECT_EQ(myClass->getClassType(), vpiUserDefinedClass) << "8.3: myclass is declared in the source text";
  EXPECT_FALSE(myClass->getIsInterfaceClass()) << "8.26: 'interface' is not written";
  EXPECT_FALSE(myClass->getVirtual()) << "8.21: 'virtual' is not written before 'class'";
}

// Line 15 has no parameter port list (8.25), no 'extends' (8.13) and no
// 'implements' (8.26); line 16 is the only class item, so myclass has no
// property, constraint, typedef or nested class; line 17 is 'endclass'
// without ': myclass'.
TEST_F(ClassMemberTest15Test, MyClassHasNoOtherItems) {
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

// Line 16: 'extern function void subroutine;' is the only method prototype
// of myclass (8.24): a function returning void (13.4.1), a class method
// (8.6), not virtual (8.20), not pure (8.21), with no arguments.
TEST_F(ClassMemberTest15Test, MyClassHasOneExternVoidFunctionPrototype) {
  const hldb::ClassDefn *const myClass = getMyClass();
  ASSERT_NE(myClass, nullptr);
  const hldb::TaskFuncDeclCollection *const prototypes = myClass->getTaskFuncDecls();
  ASSERT_EQ(sizeOf(prototypes), 1u) << "8.24: line 16 declares the only method prototype";

  const hldb::TaskFuncDecl *const prototype = prototypes->at(0);
  EXPECT_EQ(prototype->getName(), std::string_view("subroutine"));
  EXPECT_TRUE(prototype->getMethod()) << "8.6: subroutine is declared in the body of myclass";
  EXPECT_FALSE(prototype->getVirtual()) << "8.20: 'virtual' is not written";
  EXPECT_FALSE(prototype->getPure()) << "8.21: 'pure' is not written";
  EXPECT_EQ(sizeOf(prototype->getIODecls()), 0u) << "no argument list is written";

  const hldb::FunctionDecl *const functionDecl = ::any_cast<hldb::FunctionDecl>(prototype);
  ASSERT_NE(functionDecl, nullptr) << "13.4: subroutine is declared 'function'";
  expectVoidReturn(functionDecl->getReturn(), "prototype subroutine");
}

// Line 19: 'function void myclass::subroutine; endfunction' is the
// out-of-block body of the extern method (8.24), so it is the only method
// of myclass with a body: a function returning void, a class method,
// automatic (8.6), not virtual, not pure, with no arguments, no statement
// and no end label.
TEST_F(ClassMemberTest15Test, MyClassHasOutOfBlockSubroutineBody) {
  const hldb::ClassDefn *const myClass = getMyClass();
  ASSERT_NE(myClass, nullptr);
  const hldb::TaskFuncCollection *const methods = myClass->getMethods();
  ASSERT_EQ(sizeOf(methods), 1u) << "8.24: line 19 is the body of myclass's extern method";

  const hldb::TaskFunc *const body = methods->at(0);
  EXPECT_EQ(body->getName(), std::string_view("subroutine"));
  EXPECT_TRUE(body->getMethod()) << "8.24: 'myclass::subroutine' is a method of myclass";
  EXPECT_TRUE(body->getAutomatic()) << "8.6: methods of a class have automatic lifetime";
  EXPECT_FALSE(body->getVirtual()) << "8.20: 'virtual' is not written";
  EXPECT_FALSE(body->getPure()) << "8.21: an extern method with a body is not pure";
  EXPECT_EQ(sizeOf(body->getIODecls()), 0u) << "8.24: like the prototype, no argument list is written";
  EXPECT_EQ(body->getStmt(), nullptr) << "13.4: the function body holds no statement";
  EXPECT_EQ(body->getEndLabelObj(), nullptr) << "'endfunction' is written without ': subroutine'";

  const hldb::Function *const function = ::any_cast<hldb::Function>(body);
  ASSERT_NE(function, nullptr) << "13.4: subroutine is declared 'function'";
  expectVoidReturn(function->getReturn(), "body subroutine");
}

// 8.24: the out-of-block declaration on line 19 implements the extern
// prototype on line 16.
TEST_F(ClassMemberTest15Test, PrototypeIsImplementedByOutOfBlockBody) {
  const hldb::TaskFuncDecl *const prototype = getSubroutinePrototype();
  ASSERT_NE(prototype, nullptr);
  const hldb::TaskFunc *const body = getSubroutineBody();
  ASSERT_NE(body, nullptr);

  ASSERT_NE(prototype->getTaskFunc(), nullptr) << "8.24: extern subroutine is defined on line 19";
  EXPECT_EQ(prototype->getTaskFunc(), body) << "8.24: the prototype's implementation is myclass::subroutine";
}

// Lines 21-22: 'module test; endmodule' has no port list and no module
// items (23.2), and 'endmodule' carries no ': test' end label.
TEST_F(ClassMemberTest15Test, TestModuleIsEmpty) {
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
  EXPECT_EQ(sizeOf(test->getTaskFuncs()), 0u) << "subroutine is a method of myclass, not of test";
  EXPECT_EQ(sizeOf(test->getClassDefns()), 0u) << "myclass is declared outside module test";
}

// Besides myclass and test the file declares no design element (3.2-3.12):
// myclass is the only class, and it is in no package.
TEST_F(ClassMemberTest15Test, NoOtherDesignElementsDeclared) {
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
// out-of-block body on line 19 is a method of myclass (8.24), not a
// compilation-unit function, and the file declares no let, parameter,
// variable or typedef.
TEST_F(ClassMemberTest15Test, NoOtherCompilationUnitDeclarations) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getTaskFuncs()), 0u) << "8.24: myclass::subroutine is a method of myclass";
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
TEST_F(ClassMemberTest15Test, TestIsTheTopLevelModule) {
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
