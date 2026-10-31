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
// tests/Google/generic/member/class_member_test_14.sv. Lines 1-7 are
// one-line "//" license comments, lines 8-9 are blank, lines 10-14 are the
// block comment carrying the suite's :name:/:description:/:tags: metadata,
// and the rest is:
//
//   15  class myclass;
//   16  function integer subroutine;
//   17    input a;
//   18    subroutine = a+42;
//   19  endfunction
//   20  endclass
//   21
//   22  module test;
//   23  endmodule
//
// The fixture is tagged 8.3 (class declarations).
//
// class myclass; ... endclass (8.3, A.1.2)
//   - A class named myclass, declared once, as a compilation-unit item;
//     user-defined (vpiClassType vpiUserDefinedClass, 37.29), not an
//     interface class (8.26) and not abstract (8.21: no 'virtual').
//   - No parameter port list, no 'extends', no 'implements', no property,
//     no constraint, no typedef, no nested class and no end label.
//   - Exactly one class item: the method subroutine.
//
// function integer subroutine; ... endfunction (8.6, 13.4, A.2.6)
//   function_body_declaration ::= function_data_type_or_implicit
//     function_identifier ; { tf_item_declaration }
//     { function_statement_or_null } endfunction [ : function_identifier ]
//   - A function declared in the body of myclass, so a class method (8.6)
//     with a body, automatic (8.6: "Methods declared as part of a class
//     type have automatic lifetime"), not virtual (8.20), not pure (8.21).
//   - Its return type is integer (6.11: a 4-state, 32-bit signed integer).
//   - Line 17 'input a;' is a non-ANSI tf_item_declaration: one argument, a,
//     an input. No data type is written, so it is logic (13.3/13.4: "If the
//     data type is not explicitly declared, the default data type is
//     logic"), a single bit with no packed dimension.
//   - Line 18 is the function's only statement: a blocking procedural
//     assignment (10.4.1) to the function name, which 13.4.1 makes the
//     implicit variable holding the return value, of the expression
//     'a + 42' (11.4.3 binary addition of the argument a and the literal 42).
//   - 'endfunction' has no ': subroutine' end label.
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
//     constraint, typedef, nested class, method prototype or end label, and
//     exactly one method, subroutine.
//   - subroutine is a function, a class method, automatic, not virtual, not
//     pure, with no end label; its return type is a signed integer.
//   - subroutine has exactly one argument, a: an input of type logic with
//     no packed dimension.
//   - subroutine's body is exactly one statement: a blocking assignment
//     whose left-hand side is subroutine (bound) and whose right-hand side
//     is the addition of a (bound to the argument) and the literal 42.
//   - Exactly one module test, with no port or module item and no end label.
//   - No other design element and no other compilation-unit declaration.
//   - Top-level status of test: after elaboration test is a top-level module
//     with one top instance named test; before elaboration no module is
//     flagged top (maintainer's convention for non-elaborated models).
//
// Model note: a single-statement body may be recorded directly as the
// statement or inside a begin block holding just that statement; the test
// accepts either, since the LRM has no begin-end here.
//
// What is NOT checked, and why
//   - Which object the left-hand side 'subroutine' binds to (the function
//     or a return variable): 13.4.1 only says the function name acts as an
//     implicit variable; the test checks it binds to something named
//     subroutine.
//   - Whether 'a' binds to the IODecl or to a variable for it: both are the
//     argument a; the test checks the binding's name.
//   - Assignment::getOpType() and the constants' internal encodings
//     (vpiConstType, size): only the decompiled literal text "42", which the
//     source fixes, is compared.
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
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/checker_decl.h>
#include <hldb/class_defn.h>
#include <hldb/config_decl.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/directive.h>
#include <hldb/function.h>
#include <hldb/include_stmt.h>
#include <hldb/integer_typespec.h>
#include <hldb/interface.h>
#include <hldb/io_decl.h>
#include <hldb/let_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/program.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/source_file.h>
#include <hldb/sv_vpi_user.h>
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
constexpr std::string_view kSourceFileName = "class_member_test_14.sv";

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

class ClassMemberTest14Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_member_test_14.hlc"}); }
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

  // The method subroutine of myclass (lines 16-19), or nullptr.
  static const hldb::TaskFunc *getSubroutine() {
    const hldb::ClassDefn *const myClass = getMyClass();
    if (myClass == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::TaskFunc>("subroutine", myClass->getMethods());
  }

  // The only statement of subroutine's body (line 18), or nullptr. A begin
  // block holding exactly one statement is looked through, since the source
  // has no begin-end.
  static const hldb::Any *getBodyStatement() {
    const hldb::TaskFunc *const subroutine = getSubroutine();
    if (subroutine == nullptr) {
      return nullptr;
    }
    const hldb::Any *const stmt = subroutine->getStmt();
    if (stmt == nullptr) {
      return nullptr;
    }
    const hldb::Begin *const block = ::any_cast<hldb::Begin>(stmt);
    if (block == nullptr) {
      return stmt;
    }
    if (sizeOf(block->getStmts()) != 1) {
      return nullptr;
    }
    return block->getStmts()->at(0);
  }

  // The module definition test, or nullptr.
  static const hldb::Module *getTestModule() {
    if (m_design == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  }
};

// Lines 15-23 are a legal class with a function method (8.6, 13.4) and a
// module (23.2), so nothing is fatal, a syntax error or an error.
TEST_F(ClassMemberTest14Test, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  const EventListener::Stats stats = eventListener->getStats();
  EXPECT_EQ(stats.m_fatals, 0) << describeErrors(eventListener, EventSeverity::Fatal);
  EXPECT_EQ(stats.m_syntaxes, 0) << describeErrors(eventListener, EventSeverity::Syntax);
  EXPECT_EQ(stats.m_errors, 0) << describeErrors(eventListener, EventSeverity::Error);
}

TEST_F(ClassMemberTest14Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

// The file holds no compiler directive, no `define and no macro usage.
TEST_F(ClassMemberTest14Test, SourceFileHasNoIncludesDirectivesOrMacros) {
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
TEST_F(ClassMemberTest14Test, MyClassIsUserDefinedConcreteClass) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countNamed(m_design->getAllClasses(), "myclass"), 1u) << "myclass is declared exactly once";

  const hldb::ClassDefn *const myClass = getMyClass();
  ASSERT_NE(myClass, nullptr);
  EXPECT_EQ(myClass->getClassType(), vpiUserDefinedClass) << "8.3: myclass is declared in the source text";
  EXPECT_FALSE(myClass->getIsInterfaceClass()) << "8.26: 'interface' is not written";
  EXPECT_FALSE(myClass->getVirtual()) << "8.21: 'virtual' is not written before 'class'";
}

// Line 15 has no parameter port list (8.25), no 'extends' (8.13) and no
// 'implements' (8.26); lines 16-19 are the only class item, so myclass has
// no property, constraint, typedef, nested class or method prototype; line
// 20 is 'endclass' without ': myclass'.
TEST_F(ClassMemberTest14Test, MyClassHasOnlyOneMethod) {
  const hldb::ClassDefn *const myClass = getMyClass();
  ASSERT_NE(myClass, nullptr);

  EXPECT_EQ(sizeOf(myClass->getParameters()), 0u) << "8.25: no parameter port list is written";
  EXPECT_EQ(myClass->getExtends(), nullptr) << "8.13: no 'extends' is written";
  EXPECT_EQ(myClass->getImplements(), nullptr) << "8.26: no 'implements' is written";
  EXPECT_EQ(sizeOf(myClass->getVariables()), 0u) << "8.5: no class property is declared";
  EXPECT_EQ(sizeOf(myClass->getConstraints()), 0u) << "no constraint is declared";
  EXPECT_EQ(sizeOf(myClass->getTypedefs()), 0u) << "no type declaration is written inside myclass";
  EXPECT_EQ(sizeOf(myClass->getClassDefns()), 0u) << "no nested class is declared";
  EXPECT_EQ(sizeOf(myClass->getTaskFuncDecls()), 0u) << "8.24: no method prototype is written";
  EXPECT_EQ(myClass->getEndLabelObj(), nullptr) << "'endclass' is written without ': myclass'";

  const hldb::TaskFuncCollection *const methods = myClass->getMethods();
  ASSERT_EQ(sizeOf(methods), 1u) << "lines 16-19 declare the only method";
  EXPECT_EQ(methods->at(0)->getName(), std::string_view("subroutine"));
}

// Line 16: 'function integer subroutine;' inside the class body is a class
// method (8.6) declared as a function (13.4) returning integer (6.11, a
// signed type), with no method qualifier: not virtual (8.20), not pure
// (8.21). 8.6: class methods have automatic lifetime. Line 19 'endfunction'
// has no end label.
TEST_F(ClassMemberTest14Test, SubroutineIsAutomaticFunctionMethodReturningInteger) {
  const hldb::TaskFunc *const subroutine = getSubroutine();
  ASSERT_NE(subroutine, nullptr);

  EXPECT_TRUE(subroutine->getMethod()) << "8.6: subroutine is declared in the body of myclass";
  EXPECT_TRUE(subroutine->getAutomatic()) << "8.6: methods of a class have automatic lifetime";
  EXPECT_FALSE(subroutine->getVirtual()) << "8.20: 'virtual' is not written";
  EXPECT_FALSE(subroutine->getPure()) << "8.21: 'pure' is not written";
  EXPECT_EQ(subroutine->getEndLabelObj(), nullptr) << "'endfunction' is written without ': subroutine'";

  const hldb::Function *const function = ::any_cast<hldb::Function>(subroutine);
  ASSERT_NE(function, nullptr) << "13.4: subroutine is declared 'function'";
  const hldb::RefTypespec *const returnType = function->getReturn();
  ASSERT_NE(returnType, nullptr) << "13.4: 'function integer' gives an explicit return type";
  const hldb::IntegerTypespec *const integerType = returnType->getActual<hldb::IntegerTypespec>();
  ASSERT_NE(integerType, nullptr) << "6.11: subroutine returns integer";
  EXPECT_TRUE(integerType->getSigned()) << "6.11: integer is a signed type";
}

// Line 17: 'input a;' declares one argument, a, an input. No data type is
// written, so it is logic (13.3/13.4), a single bit with no packed
// dimension.
TEST_F(ClassMemberTest14Test, SubroutineHasOneLogicInputA) {
  const hldb::TaskFunc *const subroutine = getSubroutine();
  ASSERT_NE(subroutine, nullptr);
  const hldb::IODeclCollection *const arguments = subroutine->getIODecls();
  ASSERT_EQ(sizeOf(arguments), 1u) << "line 17 declares the only argument";

  const hldb::IODecl *const a = arguments->at(0);
  EXPECT_EQ(a->getName(), std::string_view("a"));
  EXPECT_EQ(a->getDirection(), vpiInput) << "line 17 writes 'input'";

  const hldb::RefTypespec *const type = a->getTypespec();
  ASSERT_NE(type, nullptr) << "13.4: a has the default data type logic";
  const hldb::LogicTypespec *const logicType = type->getActual<hldb::LogicTypespec>();
  ASSERT_NE(logicType, nullptr) << "13.4: the default data type of an argument is logic";
  EXPECT_EQ(sizeOf(logicType->getRanges()), 0u) << "'input a' has no packed dimension, so a is a single bit";
}

// Line 18: 'subroutine = a+42;' is the only statement of the body: a
// blocking procedural assignment (10.4.1) to the function name (13.4.1),
// whose right-hand side adds the argument a and the literal 42 (11.4.3).
TEST_F(ClassMemberTest14Test, BodyAssignsAPlus42ToSubroutine) {
  const hldb::TaskFunc *const subroutine = getSubroutine();
  ASSERT_NE(subroutine, nullptr);
  ASSERT_NE(subroutine->getStmt(), nullptr) << "line 18 is a statement";
  const hldb::Any *const stmt = getBodyStatement();
  ASSERT_NE(stmt, nullptr) << "the body holds exactly one statement";

  const hldb::Assignment *const assignment = ::any_cast<hldb::Assignment>(stmt);
  ASSERT_NE(assignment, nullptr) << "10.4: line 18 is a procedural assignment";
  EXPECT_TRUE(assignment->getBlocking()) << "10.4.1: '=' is a blocking assignment";

  const hldb::RefObj *const lhs = ::any_cast<hldb::RefObj>(assignment->getLhs());
  ASSERT_NE(lhs, nullptr) << "the left-hand side is a name";
  EXPECT_EQ(lhs->getName(), std::string_view("subroutine"));
  ASSERT_NE(lhs->getActual(), nullptr) << "13.4.1: the function name is an implicit variable of subroutine";
  EXPECT_EQ(lhs->getActual()->getName(), std::string_view("subroutine"));

  const hldb::Operation *const sum = ::any_cast<hldb::Operation>(assignment->getRhs());
  ASSERT_NE(sum, nullptr) << "the right-hand side is an operation";
  EXPECT_EQ(sum->getOpType(), vpiAddOp) << "11.4.3: '+' is binary addition";
  const hldb::AnyCollection *const operands = sum->getOperands();
  ASSERT_EQ(sizeOf(operands), 2u) << "'a+42' has two operands";

  const hldb::RefObj *const aRef = ::any_cast<hldb::RefObj>(operands->at(0));
  ASSERT_NE(aRef, nullptr) << "the first operand is the name a";
  EXPECT_EQ(aRef->getName(), std::string_view("a"));
  ASSERT_NE(aRef->getActual(), nullptr) << "a is the argument declared on line 17";
  EXPECT_EQ(aRef->getActual()->getName(), std::string_view("a"));

  const hldb::Constant *const literal = ::any_cast<hldb::Constant>(operands->at(1));
  ASSERT_NE(literal, nullptr) << "the second operand is the literal 42";
  EXPECT_EQ(literal->getDecompile(), std::string_view("42"));
}

// Lines 22-23: 'module test; endmodule' has no port list and no module
// items (23.2), and 'endmodule' carries no ': test' end label.
TEST_F(ClassMemberTest14Test, TestModuleIsEmpty) {
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
TEST_F(ClassMemberTest14Test, NoOtherDesignElementsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 1u) << "myclass is the only class";
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);
}

// No compilation-unit item (3.12.1) other than myclass is declared:
// subroutine is a method of myclass, and the file declares no let,
// parameter, variable or typedef.
TEST_F(ClassMemberTest14Test, NoOtherCompilationUnitDeclarations) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getTaskFuncs()), 0u) << "subroutine is a method of myclass";
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
TEST_F(ClassMemberTest14Test, TestIsTheTopLevelModule) {
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
