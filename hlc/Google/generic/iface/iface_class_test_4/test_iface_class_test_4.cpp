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
// tests/Google/generic/iface/iface_class_test_4.sv. Lines 1-5 are the block
// comment carrying the suite's :name:/:description:/:tags: metadata, and
// the rest is:
//
//    6  interface class base_ic extends base1, base2, base3;
//    7  endclass
//
// Line 7 has no line terminator; the file ends right after 'endclass'.
// There is no module.
//
// The fixture is tagged 8.3 (class declarations) and 8.26 (interface
// classes).
//
// interface class base_ic extends base1, base2, base3; endclass
// (8.26, A.1.9)
//   interface_class_declaration ::= interface class class_identifier
//     [ parameter_port_list ] [ extends interface_class_type { ,
//     interface_class_type } ] ; { interface_class_item } endclass
//     [ : class_identifier ]
//   - The syntax is legal: an interface class, unlike a non-interface
//     class, may extend several interface classes (8.26.3), and the
//     'extends' list here names three interface_class_types, in order
//     base1, base2, base3.
//   - An interface class named base_ic, declared once, as a
//     compilation-unit item (3.12.1); user-defined (vpiClassType
//     vpiUserDefinedClass, 37.29) and an interface class (8.26).
//   - No parameter port list, no 'implements' (only non-interface classes
//     implement, 8.26.2), no interface_class_item and no end label.
//
// None of base1, base2 and base3 is declared: the file declares no class,
// interface class, typedef or package other than base_ic, and the .hlc
// compiles no other source. 8.26.3 lets an interface class extend other
// interface classes; the names in the 'extends' list shall be interface
// class types, and a name that resolves to no declaration is no type at
// all. So each of the three references cannot be resolved, and the
// compiler shall report an error for each. HLC's dedicated diagnostic is
// COMP_UNDEFINED_BASE_CLASS ('Undefined base class "%s" extended by ...'),
// so that code, naming base1, base2 and base3 on line 6, is expected.
//
// The file declares no module, so there is no top-level instance (23.3.1),
// and no other design element or compilation-unit item.
//
// The .hlc also puts UVM 1800.2-2017 on the include path ('-I'), but adds no
// UVM source file, and passes -nobuiltin, so nothing else is compiled --
// in particular no class named base1, base2 or base3 exists anywhere in
// the design.
//
// What is checked
//   - The unresolved bases are reported: at least one error, and a
//     COMP_UNDEFINED_BASE_CLASS naming each of base1, base2 and base3 on
//     line 6.
//   - No fatal and no syntax diagnostic: lines 6-7 match A.1.9, including
//     the three-element 'extends' list.
//   - Exactly one SourceFile for the .sv, with no includes, no generic
//     directives, no macro definitions and no macro instances.
//   - Exactly one class named base_ic: user-defined, an interface class.
//   - base_ic's Extends lists exactly three class types, in source order
//     base1, base2, base3, none of which resolves to a class definition.
//   - No class named base1, base2 or base3 exists anywhere in the design.
//   - base_ic has no parameters, no implemented classes, no members and no
//     end label.
//   - No other design element, no compilation-unit declaration besides
//     base_ic, and no top-level module instance.
//
// KNOWN FAILURE (undefined base interface classes not reported), not a
// defect in this test: as in iface_class_test_3, HLC 577 records the three
// 'extends' references, each resolving to no class
// (BaseIcExtendsUnresolvedBase1Base2Base3 passes), yet reports no error at
// all -- zero errors, no COMP_UNDEFINED_BASE_CLASS -- although none of the
// three is declared and 8.26.3 requires the names in an interface class's
// 'extends' list to be interface class types. ReportsUndefinedBaseClasses
// asserts the LRM behavior and is expected to fail until HLC is fixed; it
// is intentionally not skipped or relaxed.
//
// What is NOT checked, and why
//   - Whether HLC also raises a generic COMP_UNDEFINED_TYPE or
//     COMP_FAILED_TO_BIND for the three names, and the exact error count.
//     The LRM requires an error for each unresolved name, not a particular
//     code or count; the dedicated base-class code is asserted per name.
//     Nearest assertion: ReportsUndefinedBaseClasses.
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
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/checker_decl.h>
#include <hldb/class_defn.h>
#include <hldb/config_decl.h>
#include <hldb/design.h>
#include <hldb/directive.h>
#include <hldb/extends.h>
#include <hldb/include_stmt.h>
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
constexpr std::string_view kSourceFileName = "iface_class_test_4.sv";

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

// Number of classes in 'collection' whose name or definition name is 'name'.
size_t countClassesNamed(const hldb::ClassDefnCollection *collection, std::string_view name) {
  size_t count = 0;
  if (collection != nullptr) {
    for (const hldb::ClassDefn *cls : *collection) {
      if ((cls->getName() == name) || (cls->getDefName() == name)) ++count;
    }
  }
  return count;
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

class IfaceClassTest4Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "iface_class_test_4.hlc"}); }
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

  // The interface class base_ic, or nullptr.
  static const hldb::ClassDefn *getBaseIc() {
    if (m_design == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::ClassDefn>("base_ic", m_design->getAllClasses());
  }

  // The class types listed by base_ic's 'extends', or nullptr.
  static const hldb::RefTypespecCollection *getBaseIcExtendedTypes() {
    const hldb::ClassDefn *const baseIc = getBaseIc();
    if ((baseIc == nullptr) || (baseIc->getExtends() == nullptr)) {
      return nullptr;
    }
    return baseIc->getExtends()->getClassTypespecs();
  }
};

// None of base1, base2, base3 is declared, so each name in 'extends base1,
// base2, base3' (line 6) names no interface class type (8.26.3) and an
// error is required for each. HLC's dedicated diagnostic is
// COMP_UNDEFINED_BASE_CLASS.
TEST_F(IfaceClassTest4Test, ReportsUndefinedBaseClasses) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);

  const ErrorContainer::Stats stats = errorContainer->getErrorStats();
  EXPECT_GE(stats.nbError, 1) << "8.26.3: an undeclared base interface class is an error";
  for (std::string_view name : {"base1", "base2", "base3"}) {
    EXPECT_NE(findError(ErrorDefinition::COMP_UNDEFINED_BASE_CLASS, name, 6), nullptr)
        << name << " (line 6) is extended but never declared;"
        << describeErrors(errorContainer, ErrorDefinition::ERROR);
  }
}

// Lines 6-7 match interface_class_declaration (A.1.9), so the error above is
// semantic: nothing is fatal or a syntax error.
TEST_F(IfaceClassTest4Test, ReportsNoFatalOrSyntaxDiagnostics) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);

  const ErrorContainer::Stats stats = errorContainer->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0) << describeErrors(errorContainer, ErrorDefinition::FATAL);
  EXPECT_EQ(stats.nbSyntax, 0) << describeErrors(errorContainer, ErrorDefinition::SYNTAX);
  EXPECT_EQ(findError(ErrorDefinition::PP_SYNTAX_ERROR), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_SYNTAX_ERROR), nullptr);
}

TEST_F(IfaceClassTest4Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

// The file holds no compiler directive, no `define and no macro usage.
TEST_F(IfaceClassTest4Test, SourceFileHasNoIncludesDirectivesOrMacros) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getIncludeStmts()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// Line 6: 'interface class base_ic' declares the class once. It is written
// in the source, so it is user-defined (37.29 vpiClassType), and the
// 'interface' keyword makes it an interface class (8.26). The unresolved
// bases do not undo the declaration itself.
TEST_F(IfaceClassTest4Test, BaseIcIsUserDefinedInterfaceClass) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countClassesNamed(m_design->getAllClasses(), "base_ic"), 1u) << "base_ic is declared exactly once";

  const hldb::ClassDefn *const baseIc = getBaseIc();
  ASSERT_NE(baseIc, nullptr);
  EXPECT_EQ(baseIc->getClassType(), vpiUserDefinedClass) << "8.3: base_ic is declared in the source text";
  EXPECT_TRUE(baseIc->getIsInterfaceClass()) << "8.26: base_ic is declared 'interface class'";
}

// Line 6: 'extends base1, base2, base3' lists exactly three interface class
// types (8.26.3), kept in source order. Since none of them is declared,
// no reference resolves to a class definition.
TEST_F(IfaceClassTest4Test, BaseIcExtendsUnresolvedBase1Base2Base3) {
  const hldb::ClassDefn *const baseIc = getBaseIc();
  ASSERT_NE(baseIc, nullptr);
  ASSERT_NE(baseIc->getExtends(), nullptr) << "8.26.3: line 6 writes an 'extends' list";

  const hldb::RefTypespecCollection *const extendedTypes = getBaseIcExtendedTypes();
  ASSERT_EQ(sizeOf(extendedTypes), 3u) << "'extends base1, base2, base3' names exactly three types";
  const std::vector<std::string_view> expectedNames = {"base1", "base2", "base3"};
  for (size_t index = 0; index < expectedNames.size(); ++index) {
    const hldb::RefTypespec *const base = extendedTypes->at(index);
    EXPECT_EQ(base->getName(), expectedNames[index]) << "extended type " << (index + 1) << " in source order";
    EXPECT_EQ(hldb::getReferenced<hldb::ClassDefn>(base), nullptr)
        << expectedNames[index] << " is declared nowhere, so it resolves to no class";
  }
}

// Nothing in the design declares base1, base2 or base3: no class of any of
// these names exists.
TEST_F(IfaceClassTest4Test, BasesAreNotDeclared) {
  ASSERT_NE(m_design, nullptr);
  for (std::string_view name : {"base1", "base2", "base3"}) {
    EXPECT_EQ(countClassesNamed(m_design->getAllClasses(), name), 0u) << name << " is declared nowhere";
  }
}

// Line 6 has no parameter port list (8.25) and an interface class never
// has 'implements' (8.26.2); lines 6-7 hold no interface_class_item (8.26);
// line 7 is 'endclass' without ': base_ic'.
TEST_F(IfaceClassTest4Test, BaseIcHasNoParametersImplementsMembersOrEndLabel) {
  const hldb::ClassDefn *const baseIc = getBaseIc();
  ASSERT_NE(baseIc, nullptr);

  EXPECT_EQ(sizeOf(baseIc->getParameters()), 0u) << "8.25: no parameter port list is written";
  EXPECT_EQ(sizeOf(baseIc->getParamAssigns()), 0u) << "8.25: no parameter port list is written";
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
// other design element.
TEST_F(IfaceClassTest4Test, NoOtherDesignElementsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 1u) << "base_ic is the only class";
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 0u) << "the file declares no module";
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);
}

// No compilation-unit item (3.12.1) other than base_ic is declared: no
// subroutine, let, parameter, variable or typedef (in particular no typedef
// named base1, base2 or base3).
TEST_F(IfaceClassTest4Test, NoCompilationUnitDeclarations) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getTaskFuncs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getLetDecls()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParamAssigns()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTypedefs()), 0u);
}

// With no module in the file there is nothing to instantiate implicitly as
// a top-level module (23.3.1). This holds before elaboration (no instance
// tree yet) and after it (no module to elaborate), so it is not gated on
// getElaborated().
TEST_F(IfaceClassTest4Test, NoTopLevelInstances) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getTopModules()), 0u);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
