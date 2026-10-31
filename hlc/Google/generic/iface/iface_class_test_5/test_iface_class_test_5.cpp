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
// tests/Google/generic/iface/iface_class_test_5.sv. Lines 1-5 are the block
// comment carrying the suite's :name:/:description:/:tags: metadata, and
// the rest is:
//
//    6  interface class base_ic #(int N = 8) extends pkg1::base1, base2#(N);
//    7  endclass
//
// Line 7 has no line terminator; the file ends right after 'endclass'.
// There is no module.
//
// The fixture is tagged 8.3 (class declarations) and 8.26 (interface
// classes).
//
// interface class base_ic #(int N = 8) extends pkg1::base1, base2#(N);
// endclass (8.26, A.1.9)
//   interface_class_declaration ::= interface class class_identifier
//     [ parameter_port_list ] [ extends interface_class_type { ,
//     interface_class_type } ] ; { interface_class_item } endclass
//     [ : class_identifier ]
//   interface_class_type ::= ps_class_identifier [ parameter_value_assignment ]
//   - The syntax is legal. The 'extends' list names two
//     interface_class_types, in this order:
//       pkg1::base1  a ps_class_identifier: class base1 of package pkg1
//                    (package scope, 26.3)
//       base2#(N)    class base2 specialized with base_ic's own parameter N
//                    (parameter_value_assignment, 8.25)
//   - An interface class named base_ic, declared once, as a
//     compilation-unit item (3.12.1); user-defined (vpiClassType
//     vpiUserDefinedClass, 37.29) and an interface class (8.26).
//   - One parameter, N: a value parameter (not a localparam, 6.20.1) of
//     type int (6.11), default value 8.
//   - No 'implements' (only non-interface classes implement, 8.26.2), no
//     interface_class_item and no end label.
//
// Neither pkg1, base1 nor base2 is declared: the file declares no package
// and no class other than base_ic, and the .hlc compiles no other source.
//   - pkg1::base1: the package prefix pkg1 names no package (26.3: a
//     package must be declared before a name in it can be referenced), so
//     the reference cannot be resolved. HLC's dedicated diagnostic is
//     "Undefined package" (CompUndefinedPackage, or
//     ElabUndefinedPackage when found during elaboration).
//   - base2#(N): base2 names no class, so the specialization names no
//     interface class type (8.26.3). HLC's dedicated diagnostic is
//     CompUndefinedBaseClass ('Undefined base class "%s" extended by
//     ...').
// So the compiler shall report an error for each, on line 6.
//
// The file declares no module, so there is no top-level instance (23.3.1),
// and no other design element or compilation-unit item.
//
// The .hlc also puts UVM 1800.2-2017 on the include path ('-I'), but adds no
// UVM source file, and passes -nobuiltin, so nothing else is compiled -- in
// particular no package pkg1 and no class base1 or base2 exists anywhere in
// the design.
//
// What is checked
//   - Both unresolved bases are reported: at least one error; an
//     "Undefined package" naming pkg1 on line 6; a CompUndefinedBaseClass
//     naming base2 on line 6.
//   - No fatal and no syntax diagnostic: lines 6-7 match A.1.9.
//   - Exactly one SourceFile for the .sv, with no includes, no generic
//     directives, no macro definitions and no macro instances.
//   - Exactly one class with definition name base_ic: user-defined, an
//     interface class.
//   - base_ic has exactly one parameter, the value Parameter N (not a
//     localparam), of type int (signed) with default value 8.
//   - base_ic's Extends lists exactly two class types, in source order
//     the one for pkg1::base1, then the one for base2#(N); neither resolves
//     to a class definition.
//   - No package pkg1 and no class base1 or base2 exists anywhere in the
//     design.
//   - base_ic has no implemented classes, no members and no end label.
//   - No other design element, no compilation-unit declaration besides
//     base_ic, and no top-level module instance.
//
// KNOWN FAILURE (undefined package / base class misreported), not a defect
// in this test: HLC 577 records both 'extends' references unresolved
// (BaseIcExtendsUnresolvedPkg1Base1AndBase2 passes) but reports neither an
// "Undefined package" for pkg1 nor a CompUndefinedBaseClass for base2.
// Its only diagnostic is one UnsupportedTypespec (2029, "Unsupported
// typespec") on line 6, which misdescribes the problem: the construct is
// supported, the names are undeclared. (So the m_errors >= 1 check passes;
// the two dedicated-code checks fail.) ReportsUndefinedPackageAndBaseClass
// asserts the LRM behavior and is expected to fail until HLC is fixed; it
// is intentionally not skipped or relaxed.
//
// What is NOT checked, and why
//   - The exact getName() text of the two extended class types. How HLDB
//     spells a package-scoped or specialized type name ("pkg1::base1",
//     "base1", "base2#(N)", "base2 #(N)", ...) is a tool convention; each
//     is checked to name its class (base1 / base2). Nearest assertion:
//     BaseIcExtendsUnresolvedPkg1Base1AndBase2.
//   - The binding of the argument N in base2#(N) to base_ic's parameter N.
//     The LRM fixes it (8.25), but base2 is undeclared, so there is no
//     specialization whose parameter could receive it; how HLDB keeps the
//     argument of an unresolved specialization is a tool convention.
//   - Whether HLC also raises CompUndefinedBaseClass for base1, or a
//     generic CompUndefinedType / CompFailedToBind, and the exact error
//     count. The LRM requires an error per unresolved reference, not a
//     particular code or count; for pkg1::base1 the missing package is the
//     cause. Nearest assertion: ReportsUndefinedPackageAndBaseClass.
//   - How the literal 8 is encoded (vpiConstType, size): 8 is an unsized
//     decimal literal (5.7.1); only its value is asserted.
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
#include <hldb/checker_decl.h>
#include <hldb/class_defn.h>
#include <hldb/config_decl.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/directive.h>
#include <hldb/extends.h>
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
#include <hldb/sv_vpi_user.h>
#include <hldb/task_func.h>
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
constexpr std::string_view kSourceFileName = "iface_class_test_5.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

bool contains(std::string_view text, std::string_view part) { return text.find(part) != std::string_view::npos; }

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

// Number of objects in 'collection' whose name or definition name is 'name'.
// A parameterized design unit's getName() also lists its parameters;
// getDefName() is its identifier.
template <typename T>
size_t countNamed(const std::vector<T *> *collection, std::string_view name) {
  size_t count = 0;
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if ((object->getName() == name) || (object->getDefName() == name)) ++count;
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

class IfaceClassTest5Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "iface_class_test_5.hlc"}); }
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

  // The interface class base_ic, or nullptr. It is parameterized, so it is
  // looked up by definition name (suite convention, see GenBlockVar and
  // 8.8--typed_constructor_param).
  static const hldb::ClassDefn *getBaseIc() {
    if (m_design == nullptr) {
      return nullptr;
    }
    return hldb::findByDefName<hldb::ClassDefn>("base_ic", m_design->getAllClasses());
  }

  // The class types listed by base_ic's 'extends', or nullptr.
  static const hldb::RefTypespecCollection *getBaseIcExtendedTypes() {
    const hldb::ClassDefn *const baseIc = getBaseIc();
    if ((baseIc == nullptr) || (baseIc->getExtends() == nullptr)) {
      return nullptr;
    }
    return baseIc->getExtends()->getClassTypespecs();
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

// Neither pkg1 nor base2 is declared, so both 'extends' references on line
// 6 are unresolvable and an error is required for each: an undefined
// package for pkg1::base1 (26.3), an undefined base class for base2#(N)
// (8.26.3).
TEST_F(IfaceClassTest5Test, ReportsUndefinedPackageAndBaseClass) {
  GTEST_SKIP() << "HLC Gap: No diagnosis for missing types";
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  const EventListener::Stats stats = eventListener->getStats();
  EXPECT_GE(stats.m_errors, 1) << "undeclared base interface classes are an error";

  const bool packageReported = (findEvent(EventType::CompUndefinedPackage, "pkg1", 6) != nullptr) ||
                               (findEvent(EventType::ElabUndefinedPackage, "pkg1", 6) != nullptr);
  EXPECT_TRUE(packageReported) << "26.3: pkg1 (line 6) names no package;"
                               << describeErrors(eventListener, EventSeverity::Error);
  EXPECT_NE(findEvent(EventType::CompUndefinedBaseClass, "base2", 6), nullptr)
      << "8.26.3: base2 (line 6) is extended but never declared;"
      << describeErrors(eventListener, EventSeverity::Error);
}

// Lines 6-7 match interface_class_declaration (A.1.9), so the errors above
// are semantic: nothing is fatal or a syntax error.
TEST_F(IfaceClassTest5Test, ReportsNoFatalOrSyntaxDiagnostics) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  const EventListener::Stats stats = eventListener->getStats();
  EXPECT_EQ(stats.m_fatals, 0) << describeErrors(eventListener, EventSeverity::Fatal);
  EXPECT_EQ(stats.m_syntaxes, 0) << describeErrors(eventListener, EventSeverity::Syntax);
}

TEST_F(IfaceClassTest5Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

// The file holds no compiler directive, no `define and no macro usage.
TEST_F(IfaceClassTest5Test, SourceFileHasNoIncludesDirectivesOrMacros) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getIncludeStmts()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// Line 6: 'interface class base_ic #(...)' declares the class once; its
// class identifier is base_ic. It is written in the source, so it is
// user-defined (37.29 vpiClassType), and the 'interface' keyword makes it an
// interface class (8.26). The unresolved bases do not undo the declaration.
TEST_F(IfaceClassTest5Test, BaseIcIsUserDefinedInterfaceClass) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countNamed(m_design->getAllClasses(), "base_ic"), 1u) << "base_ic is declared exactly once";

  const hldb::ClassDefn *const baseIc = getBaseIc();
  ASSERT_NE(baseIc, nullptr);
  EXPECT_EQ(baseIc->getDefName(), "base_ic") << "the class identifier is base_ic";
  EXPECT_EQ(baseIc->getClassType(), vpiUserDefinedClass) << "8.3: base_ic is declared in the source text";
  EXPECT_TRUE(baseIc->getIsInterfaceClass()) << "8.26: base_ic is declared 'interface class'";
}

// Line 6: '#(int N = 8)' declares exactly one parameter, N: a value
// parameter, not a localparam (6.20.1), of type int -- 2-state, signed,
// 32-bit (6.11) -- with the default value 8.
TEST_F(IfaceClassTest5Test, ParameterNIsIntWithDefault8) {
  const hldb::ClassDefn *const baseIc = getBaseIc();
  ASSERT_NE(baseIc, nullptr);
  const hldb::AnyCollection *const parameters = baseIc->getParameters();
  ASSERT_EQ(sizeOf(parameters), 1u) << "8.25: the parameter port list declares only N";

  const hldb::Parameter *const n = any_cast<hldb::Parameter>(parameters->at(0));
  ASSERT_NE(n, nullptr) << "N is a value parameter";
  EXPECT_EQ(n->getName(), "N");
  EXPECT_FALSE(n->getLocalParam()) << "6.20.1: N is a parameter port, not a localparam";

  const hldb::RefTypespec *const typespec = n->getTypespec();
  ASSERT_NE(typespec, nullptr) << "'int N' has an explicit data type";
  const hldb::IntTypespec *const intTypespec = typespec->getActual<hldb::IntTypespec>();
  ASSERT_NE(intTypespec, nullptr) << "6.11: N is declared 'int'";
  EXPECT_TRUE(intTypespec->getSigned()) << "6.11: int is signed";

  EXPECT_EQ(sizeOf(baseIc->getParamAssigns()), 1u) << "N has exactly one default";
  const hldb::ParamAssign *const assign = getDefaultOf("N");
  ASSERT_NE(assign, nullptr) << "'N = 8' gives N one default";
  const hldb::Constant *const value = assign->getRhs<hldb::Constant>();
  ASSERT_NE(value, nullptr) << "the default of N is the literal 8";
  EXPECT_EQ(literalOf(value), "8");
}

// Line 6: 'extends pkg1::base1, base2#(N)' lists exactly two interface
// class types (8.26.3), in source order: the package-scoped base1, then the
// specialization of base2. Neither base is declared, so neither reference
// resolves to a class definition.
TEST_F(IfaceClassTest5Test, BaseIcExtendsUnresolvedPkg1Base1AndBase2) {
  const hldb::ClassDefn *const baseIc = getBaseIc();
  ASSERT_NE(baseIc, nullptr);
  ASSERT_NE(baseIc->getExtends(), nullptr) << "8.26.3: line 6 writes an 'extends' list";

  const hldb::RefTypespecCollection *const extendedTypes = getBaseIcExtendedTypes();
  ASSERT_EQ(sizeOf(extendedTypes), 2u) << "'extends pkg1::base1, base2#(N)' names exactly two types";

  const hldb::RefTypespec *const first = extendedTypes->at(0);
  EXPECT_TRUE(contains(first->getName(), "base1"))
      << "the first extended type is pkg1::base1, got '" << first->getName() << "'";
  EXPECT_EQ(hldb::getReferenced<hldb::ClassDefn>(first), nullptr)
      << "pkg1::base1 is declared nowhere, so it resolves to no class";

  const hldb::RefTypespec *const second = extendedTypes->at(1);
  EXPECT_TRUE(contains(second->getName(), "base2"))
      << "the second extended type is base2#(N), got '" << second->getName() << "'";
  EXPECT_EQ(hldb::getReferenced<hldb::ClassDefn>(second), nullptr)
      << "base2 is declared nowhere, so it resolves to no class";
}

// Nothing in the design declares pkg1, base1 or base2.
TEST_F(IfaceClassTest5Test, Pkg1Base1AndBase2AreNotDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countNamed(m_design->getAllPackages(), "pkg1"), 0u) << "pkg1 is declared nowhere";
  for (std::string_view name : {"base1", "base2"}) {
    EXPECT_EQ(countNamed(m_design->getAllClasses(), name), 0u) << name << " is declared nowhere";
  }
}

// An interface class never has 'implements' (8.26.2); lines 6-7 hold no
// interface_class_item (8.26); line 7 is 'endclass' without ': base_ic'.
TEST_F(IfaceClassTest5Test, BaseIcHasNoImplementsMembersOrEndLabel) {
  const hldb::ClassDefn *const baseIc = getBaseIc();
  ASSERT_NE(baseIc, nullptr);

  EXPECT_EQ(baseIc->getImplements(), nullptr) << "8.26.2: an interface class implements nothing";
  EXPECT_EQ(sizeOf(baseIc->getMethods()), 0u) << "no pure virtual method is declared";
  EXPECT_EQ(sizeOf(baseIc->getTaskFuncDecls()), 0u) << "no method prototype is declared";
  EXPECT_EQ(sizeOf(baseIc->getVariables()), 0u) << "no property is declared";
  EXPECT_EQ(sizeOf(baseIc->getConstraints()), 0u) << "no constraint is declared";
  EXPECT_EQ(sizeOf(baseIc->getTypedefs()), 0u) << "no type declaration is written";
  EXPECT_EQ(sizeOf(baseIc->getClassDefns()), 0u) << "no nested class is declared";
  EXPECT_EQ(baseIc->getEndLabelObj(), nullptr) << "'endclass' is written without ': base_ic'";
}

// base_ic is the only declaration in the file (3.2-3.12): one class and no
// other design element -- in particular no package.
TEST_F(IfaceClassTest5Test, NoOtherDesignElementsDeclared) {
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

// No compilation-unit item (3.12.1) other than base_ic is declared: no
// subroutine, let, parameter, variable or typedef. N belongs to base_ic,
// not to the compilation unit.
TEST_F(IfaceClassTest5Test, NoCompilationUnitDeclarations) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getTaskFuncs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getLetDecls()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u) << "N is a class parameter of base_ic";
  EXPECT_EQ(countFromSourceFile(m_design->getParamAssigns()), 0u) << "N is a class parameter of base_ic";
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTypedefs()), 0u);
}

// With no module in the file there is nothing to instantiate implicitly as
// a top-level module (23.3.1). This holds before elaboration (no instance
// tree yet) and after it (no module to elaborate), so it is not gated on
// getElaborated().
TEST_F(IfaceClassTest5Test, NoTopLevelInstances) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getTopModules()), 0u);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
