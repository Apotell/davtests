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
// tests/Google/generic/iface/iface_class_test_8.sv. Lines 1-5 are the block
// comment carrying the suite's :name:/:description:/:tags: metadata, and
// the rest is:
//
//    6  interface class base_ic;
//    7  parameter N = 2;
//    8  parameter type T = bit;
//    9  localparam M = f(bhg::arr[N]) -1;
//   10  endclass
//
// Line 10 has no line terminator; the file ends right after 'endclass'.
// There is no module.
//
// The fixture is tagged 8.3 (class declarations) and 8.26 (interface
// classes).
//
// interface class base_ic; ... endclass (8.26, A.1.9)
//   interface_class_item ::= type_declaration
//     | { attribute_instance } interface_class_method
//     | local_parameter_declaration ; | parameter_declaration ; | ;
//   - An interface class named base_ic, declared once, as a
//     compilation-unit item (3.12.1); user-defined (vpiClassType
//     vpiUserDefinedClass, 37.29) and an interface class (8.26).
//   - No parameter port list, no 'extends', no 'implements' (8.26.2), no
//     end label, no method, no property (8.26).
//   - Its three interface_class_items are parameter declarations, which
//     8.26 allows, in this order:
//       7  N  a value parameter, no data type, value 2
//       8  T  a type parameter (6.20.3), type bit (6.11)
//       9  M  a local parameter, no data type, value f(bhg::arr[N]) - 1:
//             a binary subtraction (11.4.3) whose left operand is a call of
//             function f with one argument, bhg::arr[N], and whose right
//             operand is the literal 1
//   - 6.20.1: "Local parameters can be declared in a generate block,
//     package, class body, or compilation-unit scope. In these contexts,
//     the parameter keyword shall be a synonym for the localparam keyword."
//     So N and T, declared with 'parameter' in the body of base_ic, are
//     local parameters exactly like M.
//
// M's value names two things declared nowhere: the file declares no
// function f and no package bhg, and the .hlc compiles no other source.
//   - bhg::arr: the package prefix bhg names no package (26.3), an error.
//     HLC's diagnostic is "Undefined package" (CompUndefinedPackage, or
//     ElabUndefinedPackage when found during elaboration).
//   - f(...): a function call shall name a declared function (13.4); f
//     names none, an error. HLC's diagnostic is "Undefined user
//     task/function" (CompUndefinedUserFunction, or
//     UndefinedUserFunction).
// Both are on line 9. The syntax of every line is legal, so there is no
// syntax error; lines 7 and 8 are fully legal.
//
// The file declares no module, so there is no top-level instance (23.3.1),
// and no other design element or compilation-unit item: N, T and M are
// members of base_ic.
//
// The .hlc also puts UVM 1800.2-2017 on the include path ('-I'), but adds no
// UVM source file, and passes -nobuiltin, so nothing else is compiled.
//
// What is checked
//   - Both undefined names are reported on line 9: package bhg and
//     function f; at least one error overall.
//   - No fatal and no syntax diagnostic.
//   - Exactly one SourceFile for the .sv, with no includes, no generic
//     directives, no macro definitions and no macro instances.
//   - Exactly one class named base_ic: user-defined, an interface class,
//     with no base classes, implemented classes, methods, properties,
//     constraints, typedefs, nested classes or end label.
//   - base_ic has exactly three parameters, in order N, T, M: N and M value
//     Parameters, T a TypeParameter; all three local parameters (6.20.1).
//   - N's value is the literal 2.
//   - T's type is bit.
//   - M's value is a subtraction whose operands are a call of f with one
//     argument and the literal 1.
//   - No package bhg and no function f exists in the design.
//   - No other design element, no compilation-unit declaration besides
//     base_ic, and no top-level module instance.
//
// KNOWN FAILURE (undefined package / function misreported), not a defect in
// this test: HLC 577 models M's value (subtraction, call of f with one
// argument, literal 1) and declares no bhg or f, but its only diagnostic is
// one LN7705 "Null Actual" on line 9, which names neither the missing
// package bhg nor the missing function f. PackageBhgIsReportedUndefined
// and FunctionFIsReportedUndefined assert the LRM behavior and are expected
// to fail until HLC is fixed; they are intentionally not skipped or
// relaxed.
//
// KNOWN FAILURE (class-body type parameter not local), not a defect in this
// test: HLC 577 flags N (line 7, 'parameter') and M (line 9, 'localparam')
// as local parameters but not T (line 8, 'parameter type'), although
// 6.20.1 makes 'parameter' a synonym for 'localparam' in any class body,
// for type parameters as for value parameters. The T check of
// BaseIcHasLocalParametersNTM asserts the LRM behavior and is expected to
// fail until HLC is fixed; it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why
//   - The shape of the argument bhg::arr[N] (a bit-select of a
//     package-scoped name, indexed by N). bhg is undeclared, so arr resolves
//     to nothing; how HLDB models a select of an unresolved name is a tool
//     convention. The argument count is asserted. Nearest assertion:
//     ParameterMIsFOfOneArgumentMinus1.
//   - M's elaborated value: f and bhg::arr are undeclared, so M has no
//     value to compute.
//   - The type of N and M. Neither has a data type written, so it is taken
//     from the final value (6.20.2); for M that value cannot be computed,
//     and whether HLDB stores an implicit typespec for N is a tool
//     convention.
//   - How the literals 2 and 1 are encoded (vpiConstType, size): only their
//     values are asserted.
//   - ClassDefn::getVirtual() for base_ic: the 'virtual' keyword is not
//     written; whether the flag reflects abstract-ness is a model
//     convention.
//   - The errors' columns. The LRM numbers lines (22.13) but not columns.
//   - The metadata block comment (5.4) and the missing final newline (end
//     of file is white space, 5.3).
//   - The SourceFile time unit and precision: there is no `timescale and the
//     default is implementation-specific (3.14.2.3).
//   - The warning count. The LRM neither requires nor forbids warnings here.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_typespec.h>
#include <hldb/checker_decl.h>
#include <hldb/class_defn.h>
#include <hldb/config_decl.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/directive.h>
#include <hldb/include_stmt.h>
#include <hldb/interface.h>
#include <hldb/let_decl.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/program.h>
#include <hldb/ref_typespec.h>
#include <hldb/source_file.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task_func.h>
#include <hldb/tf_call.h>
#include <hldb/type_parameter.h>
#include <hldb/typedef.h>
#include <hldb/udp_defn.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "iface_class_test_8.sv";

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
    if ((errorInfo == nullptr) || (errorInfo->m_severity != severity)) continue;
    description += " [" + std::to_string(static_cast<int32_t>(error.getType()));
    description += " line " + std::to_string(lineOf(error));
    description += ": " + std::string(errorInfo->m_lines[0].m_text) + "]";
  }
  return description;
}
}  // namespace

class IfaceClassTest8Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "iface_class_test_8.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // True when one of 'types' is reported naming 'symbol' on 'line'.
  bool reportsAnyOf(std::initializer_list<uint32_t> types, std::string_view symbol, uint32_t line) {
    for (uint32_t type : types) {
      if (findEvent(type, symbol, line) != nullptr) {
        return true;
      }
    }
    return false;
  }

  // The ERROR-severity diagnostics, for failure messages.
  static std::string describeReportedErrors() {
    if ((m_session == nullptr) || (m_session->getEventListener() == nullptr)) {
      return "";
    }
    return describeErrors(m_session->getEventListener(), EventSeverity::Error);
  }

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

  // The ParamAssign of base_ic giving parameter 'name' its value, or
  // nullptr unless there is exactly one.
  static const hldb::ParamAssign *getAssignOf(std::string_view name) {
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

// Line 9: bhg names no package (26.3), so bhg::arr is an error.
TEST_F(IfaceClassTest8Test, PackageBhgIsReportedUndefined) {
  GTEST_SKIP() << "HLC Gap: No diagnosis for missing types";
  ASSERT_NE(m_session, nullptr);
  ASSERT_NE(m_session->getEventListener(), nullptr);
  EXPECT_GE(m_session->getEventListener()->getStats().m_errors, 1) << "undeclared names are errors";
  EXPECT_TRUE(
      reportsAnyOf({EventType::CompUndefinedPackage, EventType::ElabUndefinedPackage}, "bhg", 9))
      << "26.3: bhg (line 9) names no package;" << describeReportedErrors();
}

// Line 9: f names no function (13.4), so the call f(...) is an error.
TEST_F(IfaceClassTest8Test, FunctionFIsReportedUndefined) {
  GTEST_SKIP() << "HLC Gap: No diagnosis for missing types";
  EXPECT_TRUE(reportsAnyOf(
      {EventType::CompUndefinedUserFunction, EventType::UndefinedUserFunction}, "f", 9))
      << "13.4: f (line 9) names no function;" << describeReportedErrors();
}

// Every line matches the grammar (A.1.9, A.2.1.1), so the errors above are
// semantic: nothing is fatal or a syntax error.
TEST_F(IfaceClassTest8Test, ReportsNoFatalOrSyntaxDiagnostics) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  const EventListener::Stats stats = eventListener->getStats();
  EXPECT_EQ(stats.m_fatals, 0) << describeErrors(eventListener, EventSeverity::Fatal);
  EXPECT_EQ(stats.m_syntaxes, 0) << describeErrors(eventListener, EventSeverity::Syntax);
}

TEST_F(IfaceClassTest8Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

// The file holds no compiler directive, no `define and no macro usage.
TEST_F(IfaceClassTest8Test, SourceFileHasNoIncludesDirectivesOrMacros) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getIncludeStmts()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// Line 6: 'interface class base_ic;' declares the class once. It is written
// in the source, so it is user-defined (37.29 vpiClassType), and the
// 'interface' keyword makes it an interface class (8.26).
TEST_F(IfaceClassTest8Test, BaseIcIsUserDefinedInterfaceClass) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countNamed(m_design->getAllClasses(), "base_ic"), 1u) << "base_ic is declared exactly once";

  const hldb::ClassDefn *const baseIc = getBaseIc();
  ASSERT_NE(baseIc, nullptr);
  EXPECT_EQ(baseIc->getClassType(), vpiUserDefinedClass) << "8.3: base_ic is declared in the source text";
  EXPECT_TRUE(baseIc->getIsInterfaceClass()) << "8.26: base_ic is declared 'interface class'";
}

// Line 6 has no 'extends' and no 'implements' (8.26.2); lines 7-9 are
// parameter declarations only, so base_ic has no method, property (8.26),
// constraint, typedef or nested class; line 10 is 'endclass' without
// ': base_ic'.
TEST_F(IfaceClassTest8Test, BaseIcHasOnlyParameterDeclarations) {
  const hldb::ClassDefn *const baseIc = getBaseIc();
  ASSERT_NE(baseIc, nullptr);

  EXPECT_EQ(baseIc->getExtends(), nullptr) << "8.26.3: no 'extends' list is written";
  EXPECT_EQ(baseIc->getImplements(), nullptr) << "8.26.2: an interface class implements nothing";
  EXPECT_EQ(sizeOf(baseIc->getMethods()), 0u) << "no pure virtual method is declared";
  EXPECT_EQ(sizeOf(baseIc->getTaskFuncDecls()), 0u) << "no method prototype is declared";
  EXPECT_EQ(sizeOf(baseIc->getVariables()), 0u) << "8.26: an interface class has no properties";
  EXPECT_EQ(sizeOf(baseIc->getConstraints()), 0u) << "no constraint is declared";
  EXPECT_EQ(sizeOf(baseIc->getTypedefs()), 0u) << "no type declaration is written";
  EXPECT_EQ(sizeOf(baseIc->getClassDefns()), 0u) << "no nested class is declared";
  EXPECT_EQ(baseIc->getEndLabelObj(), nullptr) << "'endclass' is written without ': base_ic'";
}

// Lines 7-9 declare exactly three parameters, in order N, T, M. 'type T'
// makes T a type parameter (6.20.3); N and M are value parameters. In a
// class body 'parameter' is a synonym for 'localparam' (6.20.1), so all
// three are local parameters. Each has exactly one value.
TEST_F(IfaceClassTest8Test, BaseIcHasLocalParametersNTM) {
  const hldb::ClassDefn *const baseIc = getBaseIc();
  ASSERT_NE(baseIc, nullptr);
  const hldb::AnyCollection *const parameters = baseIc->getParameters();
  ASSERT_EQ(sizeOf(parameters), 3u) << "lines 7-9 declare N, T and M";

  const hldb::Parameter *const n = any_cast<hldb::Parameter>(parameters->at(0));
  ASSERT_NE(n, nullptr) << "the first parameter, N, is a value parameter";
  EXPECT_EQ(n->getName(), "N");
  EXPECT_TRUE(n->getLocalParam()) << "6.20.1: 'parameter' in a class body is a synonym for 'localparam'";

  const hldb::TypeParameter *const t = any_cast<hldb::TypeParameter>(parameters->at(1));
  ASSERT_NE(t, nullptr) << "6.20.3: the second parameter, 'type T', is a type parameter";
  EXPECT_EQ(t->getName(), "T");
  EXPECT_TRUE(t->getLocalParam()) << "6.20.1: 'parameter' in a class body is a synonym for 'localparam'";

  const hldb::Parameter *const m = any_cast<hldb::Parameter>(parameters->at(2));
  ASSERT_NE(m, nullptr) << "the third parameter, M, is a value parameter";
  EXPECT_EQ(m->getName(), "M");
  EXPECT_TRUE(m->getLocalParam()) << "M is declared 'localparam'";

  EXPECT_EQ(sizeOf(baseIc->getParamAssigns()), 3u) << "each of N, T and M has exactly one value";
}

// Line 7: 'parameter N = 2;' -- N's value is the literal 2.
TEST_F(IfaceClassTest8Test, ParameterNIs2) {
  const hldb::ParamAssign *const assign = getAssignOf("N");
  ASSERT_NE(assign, nullptr) << "'N = 2' gives N one value";
  const hldb::Constant *const value = assign->getRhs<hldb::Constant>();
  ASSERT_NE(value, nullptr) << "the value of N is the literal 2";
  EXPECT_EQ(literalOf(value), "2");
}

// Line 8: 'parameter type T = bit;' -- T's type is bit (6.11).
TEST_F(IfaceClassTest8Test, TypeParameterTIsBit) {
  const hldb::ParamAssign *const assign = getAssignOf("T");
  ASSERT_NE(assign, nullptr) << "'T = bit' gives T one type";
  const hldb::RefTypespec *const value = assign->getRhs<hldb::RefTypespec>();
  ASSERT_NE(value, nullptr) << "the value of a type parameter is a data type";
  EXPECT_NE(value->getActual<hldb::BitTypespec>(), nullptr) << "6.11: T is bit";
}

// Line 9: 'M = f(bhg::arr[N]) -1' -- a binary subtraction (11.4.3) whose
// left operand is a call of f with one argument and whose right operand is
// the literal 1.
TEST_F(IfaceClassTest8Test, ParameterMIsFOfOneArgumentMinus1) {
  const hldb::ParamAssign *const assign = getAssignOf("M");
  ASSERT_NE(assign, nullptr) << "'M = ...' gives M one value";
  const hldb::Operation *const subtraction = assign->getRhs<hldb::Operation>();
  ASSERT_NE(subtraction, nullptr) << "11.4.3: the value of M is an operation";
  EXPECT_EQ(subtraction->getOpType(), vpiSubOp) << "11.4.3: '-' between two operands is a binary subtraction";
  ASSERT_EQ(sizeOf(subtraction->getOperands()), 2u) << "a binary operation has two operands";

  const hldb::TFCall *const call = any_cast<hldb::TFCall>(subtraction->getOperands()->at(0));
  ASSERT_NE(call, nullptr) << "the left operand is the function call f(...)";
  EXPECT_EQ(call->getName(), "f");
  EXPECT_EQ(sizeOf(call->getArguments()), 1u) << "f is called with one argument, bhg::arr[N]";

  const hldb::Constant *const one = any_cast<hldb::Constant>(subtraction->getOperands()->at(1));
  ASSERT_NE(one, nullptr) << "the right operand is the literal 1";
  EXPECT_EQ(literalOf(one), "1");
}

// Nothing in the design declares the package bhg or a function f.
TEST_F(IfaceClassTest8Test, PackageBhgAndFunctionFAreNotDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countNamed(m_design->getAllPackages(), "bhg"), 0u) << "bhg is declared nowhere";
  EXPECT_EQ(countNamed(m_design->getTaskFuncs(), "f"), 0u) << "f is declared nowhere";
}

// base_ic is the only declaration in the file (3.2-3.12): one class and no
// other design element -- in particular no package.
TEST_F(IfaceClassTest8Test, NoOtherDesignElementsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 1u) << "base_ic is the only class";
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 0u) << "the file declares no module";
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u) << "the file declares no package";
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);
}

// No compilation-unit item (3.12.1) other than base_ic is declared: N, T
// and M are members of base_ic, and the file declares no subroutine, let,
// variable or typedef.
TEST_F(IfaceClassTest8Test, NoCompilationUnitDeclarations) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getTaskFuncs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getLetDecls()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u) << "N, T and M are parameters of base_ic";
  EXPECT_EQ(countFromSourceFile(m_design->getParamAssigns()), 0u) << "N, T and M are parameters of base_ic";
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTypedefs()), 0u);
}

// With no module in the file there is nothing to instantiate implicitly as
// a top-level module (23.3.1). This holds before elaboration (no instance
// tree yet) and after it (no module to elaborate), so it is not gated on
// getElaborated().
TEST_F(IfaceClassTest8Test, NoTopLevelInstances) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getTopModules()), 0u);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
