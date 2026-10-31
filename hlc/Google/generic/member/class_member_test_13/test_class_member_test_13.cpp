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
// tests/Google/generic/member/class_member_test_13.sv. Lines 1-5 are the
// block comment carrying the suite's :name:/:description:/:tags: metadata,
// and the rest is:
//
//    6  class protected_stuff;
//    7    protected int count;
//    8    protected const int countess = 1;
//    9    protected var int counter = 0;
//   10    protected const var int counted = 1;
//   11    protected const myclass::msg_t null_msg = {1'b1, 1'b0};
//   12  endclass
//
// The fixture is tagged 8.3 (class declarations). It has no
// :should_fail_because: tag, but see "Expected diagnostic" below.
//
// class protected_stuff; ... endclass (8.3, A.1.2)
//   - A class named protected_stuff, declared once, as a compilation-unit
//     item; user-defined (vpiClassType vpiUserDefinedClass, 37.29), not an
//     interface class (8.26) and not abstract (8.21: no 'virtual').
//   - No parameter port list, no 'extends', no 'implements', no constraint,
//     no typedef, no nested class, no method and no end label.
//   - Exactly five class items, all properties (8.5): count, countess,
//     counter, counted and null_msg.
//
// The properties (8.5, 8.18, 8.19, 6.8, A.1.9)
//   class_property ::= { property_qualifier } data_declaration
//                    | const { class_item_qualifier } data_type
//                      const_identifier [ = constant_expression ] ;
//   - Every property is declared 'protected' (8.18), so its visibility is
//     protected (vpiVisibility vpiProtectedVis).
//   - None is 'rand'/'randc' (18.4), so vpiRandType is vpiNotRand.
//   - 'const' makes a property a constant (8.19); 'var' only states that it
//     is a variable (6.8) and does not change its constness.
//       line  name      const  type              initializer
//        7    count     no     int (6.11)        none
//        8    countess  yes    int               1
//        9    counter   no     int ('var int')   0
//       10    counted   yes    int ('var int')   1
//       11    null_msg  yes    myclass::msg_t    {1'b1, 1'b0}
//   - int is a 2-state, 32-bit signed integer (6.11).
//
// Expected diagnostic
//   Line 11: 'myclass::msg_t' names a type through the scope 'myclass'
//   (class_scope or package_scope, A.8.4 / 8.23 / 26.3), but nothing named
//   myclass (class or package) is declared anywhere in the compiled
//   sources, so the type cannot be resolved and the compiler must report it.
//   Since 'myclass::' may be read as a class scope or a package scope, the
//   test accepts any of HLC's dedicated codes CompUndefinedClass (5816),
//   CompUndefinedPackage (5817) or CompUndefinedType (5818), located on
//   line 11.
//
// The file declares no module, so there is no top-level module (23.3.1).
// The .hlc also puts UVM 1800.2-2017 on the include path ('-I') but adds no
// UVM source file, and passes -nobuiltin, so nothing else is compiled.
//
// What is checked
//   - No fatal and no syntax diagnostic (the grammar accepts the source).
//   - An undefined class/package/type diagnostic on line 11.
//   - Exactly one SourceFile for the .sv, with no includes, directives,
//     macro definitions or macro instances.
//   - Exactly one class protected_stuff: user-defined, not an interface
//     class, not virtual, with no parameters, base class, implemented
//     class, constraint, typedef, nested class, method, method prototype or
//     end label, and exactly the five properties above.
//   - For each property: protected, not rand, const as in the table, and
//     the type and initializer as in the table.
//   - null_msg's type reference is bound to nothing (no type
//     myclass::msg_t exists).
//   - No module, no top-level module, no other design element and no other
//     compilation-unit declaration.
//
// KNOWN FAILURE, not a defect in this test (HLC 577):
//   - Undefined scope not reported: HLC raises no diagnostic at all for
//     'myclass::msg_t' on line 11, although nothing named myclass exists,
//     so UndefinedMyclassScopeIsReported fails. The sv-tests fixture also
//     lacks a :should_fail_because: tag.
//   - 'const' not recorded: on countess (line 8), counted (line 10) and
//     null_msg (line 11), all declared 'protected const ...',
//     getConstantVariable() is false although 8.19 makes them constants.
//     (The same getter is true for a 'const static' class property in
//     PackageDpi, so the 'const' qualifier is lost here.) The
//     getConstantVariable() checks of CountessIsProtectedConstIntInitializedTo1,
//     CountedIsProtectedConstVarIntInitializedTo1 and
//     NullMsgIsProtectedConstWithUnresolvedType fail.
//   All of these assert the LRM behavior and are intentionally not skipped
//   or relaxed. Types, visibility and initializers are modeled correctly.
//
// Model note: a variable's initializer is Variable::getValue() (as in
// PackageDpi); getExpr() is not the declaration's initializer.
//
// What is NOT checked, and why
//   - The exact form of null_msg's initializer (concatenation or assignment
//     pattern): with its type unresolved the LRM cannot say how '{...}' is
//     interpreted; only its presence is checked.
//   - Whether the properties are static: 'static' is not written, so they
//     are instance properties (8.9). The suite reads 'static' as
//     getAutomatic() == false (see PackageDpi), but the LRM gives an instance
//     property no automatic/static lifetime, so getAutomatic() is not asserted.
//   - The constants' internal encodings (vpiConstType, size): only the
//     decompiled text, which the source fixes, is compared.
//   - The total error count: the LRM does not fix how many diagnostics one
//     unresolved scoped type yields.
//   - The license and metadata comments (5.4): comments carry no meaning.
//   - The SourceFile time unit and precision: they come from the
//     -timescale command-line option, not from the SystemVerilog text.
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
#include <hldb/constant.h>
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
constexpr std::string_view kSourceFileName = "class_member_test_13.sv";

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

// Checks what every property of protected_stuff shares: declared
// 'protected' (8.18) and not rand (18.4).
void expectProtectedNonRand(const hldb::Variable *property) {
  EXPECT_EQ(property->getVisibility(), vpiProtectedVis) << "8.18: " << property->getName() << " is 'protected'";
  EXPECT_EQ(property->getRandType(), vpiNotRand) << "18.4: 'rand'/'randc' is not written for " << property->getName();
}

// Checks that 'property' is of type int (6.11), a signed type.
void expectIntType(const hldb::Variable *property) {
  const hldb::RefTypespec *const type = property->getTypespec();
  ASSERT_NE(type, nullptr) << property->getName() << " has an explicit data type";
  const hldb::IntTypespec *const intType = type->getActual<hldb::IntTypespec>();
  ASSERT_NE(intType, nullptr) << "6.11: " << property->getName() << " is an int";
  EXPECT_TRUE(intType->getSigned()) << "6.11: int is a signed type";
}

// Checks that 'property' is initialized with the constant written as 'text'.
void expectConstantInitializer(const hldb::Variable *property, std::string_view text) {
  ASSERT_NE(property->getValue(), nullptr) << property->getName() << " has an initializer";
  const hldb::Constant *const value = property->getValue<hldb::Constant>();
  ASSERT_NE(value, nullptr) << property->getName() << " is initialized with a literal";
  EXPECT_EQ(value->getDecompile(), text) << property->getName() << " is initialized to " << text;
}
}  // namespace

class ClassMemberTest13Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_member_test_13.hlc"}); }
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

  // The class protected_stuff, or nullptr.
  static const hldb::ClassDefn *getProtectedStuff() {
    if (m_design == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::ClassDefn>("protected_stuff", m_design->getAllClasses());
  }

  // The property 'name' of protected_stuff, or nullptr.
  static const hldb::Variable *getProperty(std::string_view name) {
    const hldb::ClassDefn *const protectedStuff = getProtectedStuff();
    if (protectedStuff == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>(name, protectedStuff->getVariables());
  }
};

// The grammar accepts every line (A.1.9); the unresolved scope on line 11 is
// a semantic error. So nothing is fatal or a syntax error.
TEST_F(ClassMemberTest13Test, NoFatalOrSyntaxErrors) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  const EventListener::Stats stats = eventListener->getStats();
  EXPECT_EQ(stats.m_fatals, 0) << describeErrors(eventListener, EventSeverity::Fatal);
  EXPECT_EQ(stats.m_syntaxes, 0) << describeErrors(eventListener, EventSeverity::Syntax);
}

// Line 11: no class or package named myclass is declared, so
// 'myclass::msg_t' cannot be resolved and is reported there.
TEST_F(ClassMemberTest13Test, UndefinedMyclassScopeIsReported) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  const bool reported = (findEvent(EventType::CompUndefinedClass, 11) != nullptr) ||
                        (findEvent(EventType::CompUndefinedPackage, 11) != nullptr) ||
                        (findEvent(EventType::CompUndefinedType, 11) != nullptr);
  EXPECT_TRUE(reported) << "8.23/26.3: nothing named myclass is declared, so 'myclass::msg_t' on line 11 is undefined;"
                        << describeErrors(eventListener, EventSeverity::Error);
}

TEST_F(ClassMemberTest13Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

// The file holds no compiler directive, no `define and no macro usage.
TEST_F(ClassMemberTest13Test, SourceFileHasNoIncludesDirectivesOrMacros) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getIncludeStmts()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// Line 6: 'class protected_stuff;' declares the class once. It is written in
// the source, so it is user-defined (37.29 vpiClassType); it is neither an
// interface class (8.26) nor an abstract 'virtual class' (8.21).
TEST_F(ClassMemberTest13Test, ProtectedStuffIsUserDefinedConcreteClass) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countNamed(m_design->getAllClasses(), "protected_stuff"), 1u) << "protected_stuff is declared exactly once";

  const hldb::ClassDefn *const protectedStuff = getProtectedStuff();
  ASSERT_NE(protectedStuff, nullptr);
  EXPECT_EQ(protectedStuff->getClassType(), vpiUserDefinedClass) << "8.3: protected_stuff is declared in the source";
  EXPECT_FALSE(protectedStuff->getIsInterfaceClass()) << "8.26: 'interface' is not written";
  EXPECT_FALSE(protectedStuff->getVirtual()) << "8.21: 'virtual' is not written before 'class'";
}

// Line 6 has no parameter port list (8.25), no 'extends' (8.13) and no
// 'implements' (8.26); lines 7-11 are five properties and nothing else;
// line 12 is 'endclass' without ': protected_stuff'.
TEST_F(ClassMemberTest13Test, ProtectedStuffHasExactlyFiveProperties) {
  const hldb::ClassDefn *const protectedStuff = getProtectedStuff();
  ASSERT_NE(protectedStuff, nullptr);

  EXPECT_EQ(sizeOf(protectedStuff->getParameters()), 0u) << "8.25: no parameter port list is written";
  EXPECT_EQ(protectedStuff->getExtends(), nullptr) << "8.13: no 'extends' is written";
  EXPECT_EQ(protectedStuff->getImplements(), nullptr) << "8.26: no 'implements' is written";
  EXPECT_EQ(sizeOf(protectedStuff->getConstraints()), 0u) << "no constraint is declared";
  EXPECT_EQ(sizeOf(protectedStuff->getTypedefs()), 0u) << "no type declaration is written inside protected_stuff";
  EXPECT_EQ(sizeOf(protectedStuff->getClassDefns()), 0u) << "no nested class is declared";
  EXPECT_EQ(sizeOf(protectedStuff->getMethods()), 0u) << "8.6: no method is declared";
  EXPECT_EQ(sizeOf(protectedStuff->getTaskFuncDecls()), 0u) << "no method prototype is declared";
  EXPECT_EQ(protectedStuff->getEndLabelObj(), nullptr) << "'endclass' is written without ': protected_stuff'";

  const hldb::VariableCollection *const properties = protectedStuff->getVariables();
  EXPECT_EQ(sizeOf(properties), 5u) << "8.5: lines 7-11 declare five class properties";
  const std::vector<std::string_view> expectedNames = {"count", "countess", "counter", "counted", "null_msg"};
  for (const std::string_view name : expectedNames) {
    EXPECT_EQ(countNamed(properties, name), 1u) << "property " << name << " is declared exactly once";
  }
}

// Line 7: 'protected int count;' is a protected, non-const int property with
// no initializer.
TEST_F(ClassMemberTest13Test, CountIsProtectedIntWithoutInitializer) {
  const hldb::Variable *const count = getProperty("count");
  ASSERT_NE(count, nullptr);
  expectProtectedNonRand(count);
  expectIntType(count);
  EXPECT_FALSE(count->getConstantVariable()) << "8.19: 'const' is not written";
  EXPECT_EQ(count->getValue(), nullptr) << "no initializer is written";
}

// Line 8: 'protected const int countess = 1;' is a protected constant int
// property (8.19) initialized to 1.
TEST_F(ClassMemberTest13Test, CountessIsProtectedConstIntInitializedTo1) {
  const hldb::Variable *const countess = getProperty("countess");
  ASSERT_NE(countess, nullptr);
  expectProtectedNonRand(countess);
  expectIntType(countess);
  EXPECT_TRUE(countess->getConstantVariable()) << "8.19: countess is declared 'const'";
  expectConstantInitializer(countess, "1");
}

// Line 9: 'protected var int counter = 0;' is a protected int variable
// (6.8: 'var' only marks it a variable), not const, initialized to 0.
TEST_F(ClassMemberTest13Test, CounterIsProtectedVarIntInitializedTo0) {
  const hldb::Variable *const counter = getProperty("counter");
  ASSERT_NE(counter, nullptr);
  expectProtectedNonRand(counter);
  expectIntType(counter);
  EXPECT_FALSE(counter->getConstantVariable()) << "8.19: 'const' is not written";
  expectConstantInitializer(counter, "0");
}

// Line 10: 'protected const var int counted = 1;' is a protected constant
// int property (8.19; 'var' does not change constness, 6.8) initialized to
// 1.
TEST_F(ClassMemberTest13Test, CountedIsProtectedConstVarIntInitializedTo1) {
  const hldb::Variable *const counted = getProperty("counted");
  ASSERT_NE(counted, nullptr);
  expectProtectedNonRand(counted);
  expectIntType(counted);
  EXPECT_TRUE(counted->getConstantVariable()) << "8.19: counted is declared 'const'";
  expectConstantInitializer(counted, "1");
}

// Line 11: 'protected const myclass::msg_t null_msg = {1'b1, 1'b0};' is a
// protected constant property (8.19) with an initializer. Its type names
// myclass::msg_t, which does not exist, so the type reference is bound to
// nothing.
TEST_F(ClassMemberTest13Test, NullMsgIsProtectedConstWithUnresolvedType) {
  const hldb::Variable *const nullMsg = getProperty("null_msg");
  ASSERT_NE(nullMsg, nullptr);
  expectProtectedNonRand(nullMsg);
  EXPECT_TRUE(nullMsg->getConstantVariable()) << "8.19: null_msg is declared 'const'";
  EXPECT_NE(nullMsg->getValue(), nullptr) << "'= {1'b1, 1'b0}' is written";

  const hldb::RefTypespec *const type = nullMsg->getTypespec();
  ASSERT_NE(type, nullptr) << "'myclass::msg_t' is written as null_msg's data type";
  EXPECT_EQ(type->getActual(), nullptr) << "8.23/26.3: no type myclass::msg_t is declared";
}

// The file declares no module (23.2), so there is no top-level module
// (23.3.1), and besides protected_stuff no other design element (3.2-3.12).
TEST_F(ClassMemberTest13Test, NoModulesOrOtherDesignElementsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 1u) << "protected_stuff is the only class";
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 0u) << "no module is declared";
  EXPECT_EQ(countFromSourceFile(m_design->getTopModules()), 0u) << "23.3.1: no module, so no top-level module";
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u) << "no package myclass is declared either";
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);
}

// No compilation-unit item (3.12.1) other than protected_stuff is declared:
// the five properties belong to the class, and the file declares no task,
// function, let, parameter or typedef.
TEST_F(ClassMemberTest13Test, NoOtherCompilationUnitDeclarations) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u) << "8.5: the properties belong to protected_stuff";
  EXPECT_EQ(countFromSourceFile(m_design->getTaskFuncs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getLetDecls()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParamAssigns()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTypedefs()), 0u) << "no typedef msg_t is declared";
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
