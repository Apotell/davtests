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
// tests/Google/generic/iface/iface_class_test_2.sv. Lines 1-7 are one-line
// "//" license comments, lines 8-9 are blank, lines 10-14 are the block
// comment carrying the suite's :name:/:description:/:tags: metadata, and
// the rest is:
//
//   15  interface class base_ic #(int N = 8, type T = string);
//   16  endclass
//   17
//   18  module test;
//   19  endmodule
//
// The fixture is tagged 8.3 (class declarations) and 8.26 (interface
// classes).
//
// interface class base_ic #(int N = 8, type T = string); endclass
// (8.26, A.1.9)
//   interface_class_declaration ::= interface class class_identifier
//     [ parameter_port_list ] [ extends interface_class_type { ,
//     interface_class_type } ] ; { interface_class_item } endclass
//     [ : class_identifier ]
//   - An interface class named base_ic, declared once, as a
//     compilation-unit item (3.12.1): it is in no module or package.
//   - It is a class declared in the source text, i.e. a user-defined class
//     (vpiClassType vpiUserDefinedClass, 37.29), and it is an interface
//     class (8.26).
//   - A parameter port list '#(int N = 8, type T = string)' (8.25, A.2.1.1
//     parameter_port_list) with two parameters, in this order:
//       N  a value parameter of data type int (6.11), default value 8
//       T  a type parameter (6.20.3), default type string (6.16)
//     A parameter declared in a class parameter port list is a parameter,
//     not a localparam (6.20.1): it can be overridden by a specialization
//     such as base_ic #(4, int).
//   - No 'extends' list (an interface class may extend only other interface
//     classes, 8.26.3; none is written), and no 'implements' (only
//     non-interface classes implement, 8.26.2).
//   - No interface_class_item: no pure virtual method prototype, no
//     typedef, no body parameter, no property. 8.26 allows an interface class to
//     hold only pure virtual methods, type declarations and parameter
//     declarations, so an empty body is legal.
//   - 'endclass' carries no ': base_ic' end label.
//
// module test; endmodule (23.2)
//   - A module named test, declared once, with no port list and no module
//     items: no port, process, net, variable, continuous assignment,
//     primitive, instance or class.
//   - test is never instantiated, so it is a top-level module, implicitly
//     instantiated once under its own name (23.3.1). The instance tree and
//     the top-module flag only exist after elaboration.
//
// The file is legal, so it compiles without errors. Nothing references
// base_ic, which is legal: 8.26 forbids constructing an interface class
// object, not declaring an unused interface class. Nothing specializes
// base_ic either, so only the defaults N = 8 and T = string exist.
//
// The .hlc also puts UVM 1800.2-2017 on the include path ('-I'), but adds no
// UVM source file, and passes -nobuiltin, so nothing else is compiled.
// Every class and module is found by name; base_ic, being parameterized,
// is found by its definition name (getDefName() == "base_ic"), this
// suite's convention, since its getName() also lists its parameter values.
//
// What is checked
//   - No fatal, syntax or error diagnostic.
//   - Exactly one SourceFile for the .sv, with no includes, no generic
//     directives, no macro definitions and no macro instances.
//   - Exactly one class named base_ic: user-defined, an interface class.
//   - base_ic has exactly two parameters, N then T: N a value Parameter, T
//     a TypeParameter, neither a localparam.
//   - N is of type int (signed, 6.11) with default value 8.
//   - T defaults to the type string.
//   - base_ic has no base classes, no implemented classes and no end label.
//   - base_ic has no members: no methods, no method prototypes, no
//     properties, no constraints, no typedefs and no nested classes.
//   - Exactly one module named test, with no port, process, net, variable,
//     continuous assignment, primitive, module instance or class, and no
//     end label; base_ic is not declared inside test.
//   - No other design element and no compilation-unit declaration besides
//     base_ic and test.
//   - Top-level status of test: after elaboration test is a top-level
//     module with one top instance named test; before elaboration no module
//     is flagged top (maintainer's convention for non-elaborated models).
//
// What is NOT checked, and why
//   - ClassDefn::getVirtual() for base_ic. An interface class cannot be
//     instantiated (8.26), like a virtual class, but the 'virtual' keyword
//     is not written; whether the flag reflects the keyword or the
//     abstract-ness is a model convention. Nearest assertion:
//     BaseIcIsUserDefinedInterfaceClass.
//   - Where HLDB stores the parameters and their defaults: this suite's
//     established convention (see chapter-6 6.20.3--parameter_type) is
//     that value and type parameters both sit in getParameters(), and each
//     default is the RHS of a ParamAssign whose LHS carries the parameter's
//     name. That convention is followed; the LRM fixes only the names,
//     kinds, order, N's type and the two defaults, which is what is
//     asserted.
//   - How the literal 8 is encoded (vpiConstType, size): 8 is an unsized
//     decimal literal (5.7.1); only its value is asserted.
//   - Any specialization of base_ic: none is written, so no class
//     specialization or parameter override exists to check.
//   - Class and module lifetimes (automatic/static): none is written, and
//     the defaults are a model convention for a declaration with no
//     subroutines or variables.
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
#include <hldb/parameter.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/program.h>
#include <hldb/ref_typespec.h>
#include <hldb/source_file.h>
#include <hldb/string_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task_func.h>
#include <hldb/type_parameter.h>
#include <hldb/typedef.h>
#include <hldb/udp_defn.h>
#include <hldb/variable.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "iface_class_test_2.sv";

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

// Number of objects in 'collection' whose definition name is 'defName'. A
// parameterized class's getName() also lists its parameters (e.g.
// "base_ic #(.N(8), .T(string))"); getDefName() is the class identifier.
template <typename T>
size_t countDefNamed(const std::vector<T *> *collection, std::string_view defName) {
  size_t count = 0;
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (object->getDefName() == defName) ++count;
    }
  }
  return count;
}

// The literal of 'constant' as written: its vpiDecompile text (37.59), or,
// when that is empty, its value text after any "<format>:" prefix.
std::string_view literalOf(const hldb::Constant *constant) {
  const std::string_view decompile = constant->getDecompile();
  if (!decompile.empty()) {
    return decompile;
  }
  const std::string_view value = constant->getValue();
  const size_t colon = value.find(':');
  if (colon == std::string_view::npos) {
    return value;
  }
  return value.substr(colon + 1);
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

class IfaceClassTest2Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "iface_class_test_2.hlc"}); }
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

  // The interface class base_ic, or nullptr. Looked up by definition name,
  // as for every parameterized design unit in this suite (see
  // GenBlockVar and 8.8--typed_constructor_param).
  static const hldb::ClassDefn *getBaseIc() {
    if (m_design == nullptr) {
      return nullptr;
    }
    return hldb::findByDefName<hldb::ClassDefn>("base_ic", m_design->getAllClasses());
  }

  // The module definition test, or nullptr.
  static const hldb::Module *getTestModule() {
    if (m_design == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  }

  // The value parameter N of base_ic, or nullptr.
  static const hldb::Parameter *getParameterN() {
    const hldb::ClassDefn *const baseIc = getBaseIc();
    if (baseIc == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Parameter>("N", baseIc->getParameters());
  }

  // The type parameter T of base_ic, or nullptr.
  static const hldb::TypeParameter *getTypeParameterT() {
    const hldb::ClassDefn *const baseIc = getBaseIc();
    if (baseIc == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::TypeParameter>("T", baseIc->getParameters());
  }

  // The ParamAssign of base_ic giving parameter 'name' its default, or
  // nullptr unless there is exactly one.
  static const hldb::ParamAssign *getDefaultOf(std::string_view name) {
    const hldb::ClassDefn *const baseIc = getBaseIc();
    if ((baseIc == nullptr) || (baseIc->getParamAssigns() == nullptr)) {
      return nullptr;
    }
    const hldb::ParamAssign *found = nullptr;
    size_t count = 0;
    for (const hldb::ParamAssign *paramAssign : *baseIc->getParamAssigns()) {
      const hldb::Any *const lhs = paramAssign->getLhs();
      if ((lhs != nullptr) && (lhs->getName() == name)) {
        found = paramAssign;
        ++count;
      }
    }
    if (count != 1) {
      return nullptr;
    }
    return found;
  }
};

// Lines 15-19 are a legal interface class declaration (8.26) and a legal
// module declaration (23.2), so nothing is fatal, a syntax error or an
// error.
TEST_F(IfaceClassTest2Test, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);

  const ErrorContainer::Stats stats = errorContainer->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0) << describeErrors(errorContainer, ErrorDefinition::FATAL);
  EXPECT_EQ(stats.nbSyntax, 0) << describeErrors(errorContainer, ErrorDefinition::SYNTAX);
  EXPECT_EQ(stats.nbError, 0) << describeErrors(errorContainer, ErrorDefinition::ERROR);
}

TEST_F(IfaceClassTest2Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

// The file holds no compiler directive, no `define and no macro usage.
TEST_F(IfaceClassTest2Test, SourceFileHasNoIncludesDirectivesOrMacros) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getIncludeStmts()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// Line 15: 'interface class base_ic #(...);' declares the class once; its
// class identifier is base_ic. It is written in the source, so it is
// user-defined (37.29 vpiClassType), and the 'interface' keyword makes it
// an interface class (8.26).
TEST_F(IfaceClassTest2Test, BaseIcIsUserDefinedInterfaceClass) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countDefNamed(m_design->getAllClasses(), "base_ic"), 1u) << "base_ic is declared exactly once";

  const hldb::ClassDefn *const baseIc = getBaseIc();
  ASSERT_NE(baseIc, nullptr);
  EXPECT_EQ(baseIc->getDefName(), "base_ic") << "the class identifier is base_ic";
  EXPECT_EQ(baseIc->getClassType(), vpiUserDefinedClass) << "8.3: base_ic is declared in the source text";
  EXPECT_TRUE(baseIc->getIsInterfaceClass()) << "8.26: base_ic is declared 'interface class'";
}

// Line 15: '#(int N = 8, type T = string)' declares exactly two parameters,
// N then T (8.25). 'type T' makes T a type parameter (6.20.3); N is a value
// parameter. Parameters of a parameter port list are not localparams
// (6.20.1), and each one has exactly one default.
TEST_F(IfaceClassTest2Test, BaseIcHasTwoParametersNThenT) {
  const hldb::ClassDefn *const baseIc = getBaseIc();
  ASSERT_NE(baseIc, nullptr);
  const hldb::AnyCollection *const parameters = baseIc->getParameters();
  ASSERT_EQ(sizeOf(parameters), 2u) << "8.25: the parameter port list declares N and T";

  const hldb::Parameter *const n = ::any_cast<hldb::Parameter>(parameters->at(0));
  ASSERT_NE(n, nullptr) << "the first parameter, N, is a value parameter";
  EXPECT_EQ(n->getName(), "N");
  EXPECT_FALSE(n->getLocalParam()) << "6.20.1: N is a parameter port, not a localparam";

  const hldb::TypeParameter *const t = ::any_cast<hldb::TypeParameter>(parameters->at(1));
  ASSERT_NE(t, nullptr) << "6.20.3: the second parameter, 'type T', is a type parameter";
  EXPECT_EQ(t->getName(), "T");
  EXPECT_FALSE(t->getLocalParam()) << "6.20.1: T is a parameter port, not a localparam";

  EXPECT_EQ(sizeOf(baseIc->getParamAssigns()), 2u) << "each of N and T has exactly one default";
}

// 'int N = 8': N's data type is int, a 2-state signed 32-bit integer
// (6.11), and its default value is the literal 8.
TEST_F(IfaceClassTest2Test, ParameterNIsIntWithDefault8) {
  const hldb::Parameter *const n = getParameterN();
  ASSERT_NE(n, nullptr);
  const hldb::RefTypespec *const typespec = n->getTypespec();
  ASSERT_NE(typespec, nullptr) << "'int N' has an explicit data type";
  const hldb::IntTypespec *const intTypespec = typespec->getActual<hldb::IntTypespec>();
  ASSERT_NE(intTypespec, nullptr) << "6.11: N is declared 'int'";
  EXPECT_TRUE(intTypespec->getSigned()) << "6.11: int is signed";

  const hldb::ParamAssign *const assign = getDefaultOf("N");
  ASSERT_NE(assign, nullptr) << "'N = 8' gives N one default";
  const hldb::Constant *const value = assign->getRhs<hldb::Constant>();
  ASSERT_NE(value, nullptr) << "the default of N is the literal 8";
  EXPECT_EQ(literalOf(value), "8");
}

// 'type T = string': the default of type parameter T is the data type
// string (6.16, 6.20.3).
TEST_F(IfaceClassTest2Test, TypeParameterTDefaultsToString) {
  ASSERT_NE(getTypeParameterT(), nullptr);

  const hldb::ParamAssign *const assign = getDefaultOf("T");
  ASSERT_NE(assign, nullptr) << "'T = string' gives T one default";
  const hldb::RefTypespec *const value = assign->getRhs<hldb::RefTypespec>();
  ASSERT_NE(value, nullptr) << "the default of a type parameter is a data type";
  EXPECT_NE(value->getActual<hldb::StringTypespec>(), nullptr) << "6.16: the default type is string";
}

// Line 15 has no 'extends' list (8.26.3); an interface class never has
// 'implements' (8.26.2). Line 16 is 'endclass' without ': base_ic'.
TEST_F(IfaceClassTest2Test, BaseIcHasNoBaseClassesOrEndLabel) {
  const hldb::ClassDefn *const baseIc = getBaseIc();
  ASSERT_NE(baseIc, nullptr);

  EXPECT_EQ(baseIc->getExtends(), nullptr) << "8.26.3: no 'extends' list is written";
  EXPECT_EQ(baseIc->getImplements(), nullptr) << "8.26.2: an interface class implements nothing";
  EXPECT_EQ(baseIc->getEndLabelObj(), nullptr) << "'endclass' is written without ': base_ic'";
}

// Lines 15-16 hold no interface_class_item (8.26), so base_ic has no
// member of any kind.
TEST_F(IfaceClassTest2Test, BaseIcHasNoMembers) {
  const hldb::ClassDefn *const baseIc = getBaseIc();
  ASSERT_NE(baseIc, nullptr);

  EXPECT_EQ(sizeOf(baseIc->getMethods()), 0u) << "no pure virtual method is declared";
  EXPECT_EQ(sizeOf(baseIc->getTaskFuncDecls()), 0u) << "no method prototype is declared";
  EXPECT_EQ(sizeOf(baseIc->getVariables()), 0u) << "no property is declared";
  EXPECT_EQ(sizeOf(baseIc->getConstraints()), 0u) << "no constraint is declared";
  EXPECT_EQ(sizeOf(baseIc->getTypedefs()), 0u) << "no type declaration is written";
  EXPECT_EQ(sizeOf(baseIc->getClassDefns()), 0u) << "no nested class is declared";
}

// Lines 18-19: 'module test; endmodule' has no port list and no module
// items (23.2), and 'endmodule' carries no ': test' end label. base_ic is
// declared before 'module test', outside it, so test holds no class.
TEST_F(IfaceClassTest2Test, TestModuleIsEmpty) {
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

// Besides base_ic and test the file declares no design element (3.2-3.12):
// base_ic is the only class, and it is in no package.
TEST_F(IfaceClassTest2Test, NoOtherDesignElementsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 1u) << "base_ic is the only class";
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);
}

// No compilation-unit item (3.12.1) other than base_ic is declared: no
// subroutine, let, parameter, variable or typedef.
TEST_F(IfaceClassTest2Test, NoCompilationUnitDeclarations) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getTaskFuncs()), 0u);
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
TEST_F(IfaceClassTest2Test, TestIsTheTopLevelModule) {
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
