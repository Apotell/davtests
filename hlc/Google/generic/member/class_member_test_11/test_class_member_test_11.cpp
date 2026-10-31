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
// tests/Google/generic/member/class_member_test_11.sv. Lines 1-7 are
// one-line "//" license comments, lines 8-9 are blank, lines 10-14 are the
// block comment carrying the suite's :name:/:description:/:tags: metadata,
// and the rest is:
//
//   15  class myclass;
//   16    int buzz_count;
//   17  endclass
//   18
//   19  module test;
//   20  endmodule
//
// The fixture is tagged 8.3 (class declarations).
//
// class myclass; ... endclass (8.3, A.1.2)
//   - A class named myclass, declared once, as a compilation-unit item;
//     user-defined (vpiClassType vpiUserDefinedClass, 37.29), not an
//     interface class (8.26) and not abstract (8.21: no 'virtual').
//   - No parameter port list, no 'extends', no 'implements', no constraint,
//     no typedef, no nested class, no method and no end label.
//   - Exactly one class item: the property buzz_count.
//
// int buzz_count; (8.5, A.1.9)
//   class_property ::= { property_qualifier } data_declaration
//   - A class property (8.5: "the data in the class") named buzz_count, of
//     type int (6.11: a 2-state, 32-bit signed integer).
//   - No property qualifier: not 'rand'/'randc' (18.4, so vpiRandType is
//     vpiNotRand), not 'const' (8.19), not 'local'/'protected' (8.18:
//     "Unless otherwise specified, class members are public", so
//     vpiVisibility is vpiPublicVis).
//   - No initializer.
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
//     virtual, with no parameters, base class, implemented class,
//     constraint, typedef, nested class, method, method prototype or end
//     label, and exactly one property, buzz_count.
//   - buzz_count is a signed int, not rand, public, not const, with no
//     initializer.
//   - Exactly one module test, with no port or module item and no end label.
//   - No other design element and no other compilation-unit declaration (in
//     particular, buzz_count is not a compilation-unit variable).
//   - Top-level status of test: after elaboration test is a top-level module
//     with one top instance named test; before elaboration no module is
//     flagged top (maintainer's convention for non-elaborated models).
//
// What is NOT checked, and why
//   - Whether buzz_count is static: 'static' is not written, so it is an
//     instance property (8.9). The suite reads 'static' as getAutomatic()
//     == false (see PackageDpi), but the LRM gives an instance property no
//     automatic/static lifetime, so getAutomatic() is not asserted here.
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
#include <hldb/int_typespec.h>
#include <hldb/interface.h>
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
#include <hldb/vpi_user.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "class_member_test_11.sv";

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

class ClassMemberTest11Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_member_test_11.hlc"}); }
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

  // The property buzz_count of myclass (line 16), or nullptr.
  static const hldb::Variable *getBuzzCount() {
    const hldb::ClassDefn *const myClass = getMyClass();
    if (myClass == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>("buzz_count", myClass->getVariables());
  }

  // The module definition test, or nullptr.
  static const hldb::Module *getTestModule() {
    if (m_design == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  }
};

// Lines 15-20 are a legal class with one property (8.5) and a module
// (23.2), so nothing is fatal, a syntax error or an error.
TEST_F(ClassMemberTest11Test, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  const EventListener::Stats stats = eventListener->getStats();
  EXPECT_EQ(stats.m_fatals, 0) << describeErrors(eventListener, EventSeverity::Fatal);
  EXPECT_EQ(stats.m_syntaxes, 0) << describeErrors(eventListener, EventSeverity::Syntax);
  EXPECT_EQ(stats.m_errors, 0) << describeErrors(eventListener, EventSeverity::Error);
}

TEST_F(ClassMemberTest11Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

// The file holds no compiler directive, no `define and no macro usage.
TEST_F(ClassMemberTest11Test, SourceFileHasNoIncludesDirectivesOrMacros) {
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
TEST_F(ClassMemberTest11Test, MyClassIsUserDefinedConcreteClass) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countNamed(m_design->getAllClasses(), "myclass"), 1u) << "myclass is declared exactly once";

  const hldb::ClassDefn *const myClass = getMyClass();
  ASSERT_NE(myClass, nullptr);
  EXPECT_EQ(myClass->getClassType(), vpiUserDefinedClass) << "8.3: myclass is declared in the source text";
  EXPECT_FALSE(myClass->getIsInterfaceClass()) << "8.26: 'interface' is not written";
  EXPECT_FALSE(myClass->getVirtual()) << "8.21: 'virtual' is not written before 'class'";
}

// Line 15 has no parameter port list (8.25), no 'extends' (8.13) and no
// 'implements' (8.26); line 16 is the only class item, so myclass has
// exactly one property and no constraint, typedef, nested class or method;
// line 17 is 'endclass' without ': myclass'.
TEST_F(ClassMemberTest11Test, MyClassHasOnlyOneProperty) {
  const hldb::ClassDefn *const myClass = getMyClass();
  ASSERT_NE(myClass, nullptr);

  EXPECT_EQ(sizeOf(myClass->getParameters()), 0u) << "8.25: no parameter port list is written";
  EXPECT_EQ(myClass->getExtends(), nullptr) << "8.13: no 'extends' is written";
  EXPECT_EQ(myClass->getImplements(), nullptr) << "8.26: no 'implements' is written";
  EXPECT_EQ(sizeOf(myClass->getConstraints()), 0u) << "no constraint is declared";
  EXPECT_EQ(sizeOf(myClass->getTypedefs()), 0u) << "no type declaration is written inside myclass";
  EXPECT_EQ(sizeOf(myClass->getClassDefns()), 0u) << "no nested class is declared";
  EXPECT_EQ(sizeOf(myClass->getMethods()), 0u) << "8.6: no method is declared";
  EXPECT_EQ(sizeOf(myClass->getTaskFuncDecls()), 0u) << "no method prototype is declared";
  EXPECT_EQ(myClass->getEndLabelObj(), nullptr) << "'endclass' is written without ': myclass'";

  const hldb::VariableCollection *const properties = myClass->getVariables();
  ASSERT_EQ(sizeOf(properties), 1u) << "8.5: line 16 declares the only class property";
  EXPECT_EQ(properties->at(0)->getName(), std::string_view("buzz_count"));
}

// Line 16: 'int buzz_count;' is a class property (8.5) of type int, a
// 32-bit signed integer (6.11), with no property qualifier and no
// initializer: not rand (18.4), public (8.18), not const (8.19).
TEST_F(ClassMemberTest11Test, BuzzCountIsPublicNonRandIntProperty) {
  const hldb::Variable *const buzzCount = getBuzzCount();
  ASSERT_NE(buzzCount, nullptr) << "8.5: buzz_count is a property of myclass";

  const hldb::RefTypespec *const type = buzzCount->getTypespec();
  ASSERT_NE(type, nullptr) << "'int buzz_count' has an explicit data type";
  const hldb::IntTypespec *const intType = type->getActual<hldb::IntTypespec>();
  ASSERT_NE(intType, nullptr) << "6.11: buzz_count is an int";
  EXPECT_TRUE(intType->getSigned()) << "6.11: int is a signed type";

  EXPECT_EQ(buzzCount->getRandType(), vpiNotRand) << "18.4: 'rand'/'randc' is not written";
  EXPECT_EQ(buzzCount->getVisibility(), vpiPublicVis) << "8.18: class members are public unless otherwise specified";
  EXPECT_FALSE(buzzCount->getConstantVariable()) << "8.19: 'const' is not written";
  EXPECT_EQ(buzzCount->getValue(), nullptr) << "no initializer is written";
}

// Lines 19-20: 'module test; endmodule' has no port list and no module
// items (23.2), and 'endmodule' carries no ': test' end label.
TEST_F(ClassMemberTest11Test, TestModuleIsEmpty) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u) << "the file declares exactly one module";

  const hldb::Module *const test = getTestModule();
  ASSERT_NE(test, nullptr);
  EXPECT_EQ(test->getEndLabelObj(), nullptr) << "'endmodule' is written without ': test'";
  EXPECT_EQ(sizeOf(test->getPorts()), 0u) << "'module test;' has no port list";
  EXPECT_EQ(sizeOf(test->getProcesses()), 0u);
  EXPECT_EQ(sizeOf(test->getNets()), 0u);
  EXPECT_EQ(sizeOf(test->getVariables()), 0u) << "buzz_count is a property of myclass, not a variable of test";
  EXPECT_EQ(sizeOf(test->getContAssigns()), 0u);
  EXPECT_EQ(sizeOf(test->getPrimitives()), 0u);
  EXPECT_EQ(sizeOf(test->getModules()), 0u);
  EXPECT_EQ(sizeOf(test->getTaskFuncs()), 0u);
  EXPECT_EQ(sizeOf(test->getClassDefns()), 0u) << "myclass is declared outside module test";
}

// Besides myclass and test the file declares no design element (3.2-3.12):
// myclass is the only class, and it is in no package.
TEST_F(ClassMemberTest11Test, NoOtherDesignElementsDeclared) {
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
// buzz_count is a property of myclass, and the file declares no task,
// function, let, parameter or typedef.
TEST_F(ClassMemberTest11Test, NoOtherCompilationUnitDeclarations) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u) << "8.5: buzz_count is a property of myclass";
  EXPECT_EQ(countFromSourceFile(m_design->getTaskFuncs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getLetDecls()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParamAssigns()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTypedefs()), 0u);
}

// 23.3.1: test is never instantiated, so it is a top-level module,
// implicitly instantiated once under its own name. The instance tree and
// the top-module flag only exist after elaboration; in a non-elaborated
// model no module is flagged top.
TEST_F(ClassMemberTest11Test, TestIsTheTopLevelModule) {
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
