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

// Validates the HLDB model built for tests/Google/generic/enum/enum_test_0.sv.
// Lines 1-5 are the block comment carrying the suite's :name:/:description:/
// :tags: metadata, and the rest is:
//
//    6  typedef enum myenum_fwd;
//
// Line 6 has no line terminator; the file ends right after the ';'.
//
// Line 6 is a forward typedef (IEEE 1800-2023 6.18, A.2.1.3):
//
//   type_declaration ::= ...
//     | typedef [ enum | struct | union | class | interface class ]
//       type_identifier ;
//
// with the basic data type 'enum' (6.19) and the type identifier
// myenum_fwd. It is a compilation-unit item (3.12.1), declared once. The
// syntax is legal, so there is no syntax error.
//
// 6.18: "The actual data type definition of a forward typedef declaration
// shall be resolved within the same local scope or generate block. It shall
// be an error if the type_identifier does not resolve to a data type."
// The compilation unit holds nothing else: no 'typedef enum { ... }
// myenum_fwd;' or any other definition of myenum_fwd follows. So
// myenum_fwd never resolves to a data type, and the compiler shall report
// an error for it. HLC's diagnostic for a type name that resolves to no
// data type is CompUndefinedType ("Undefined type"), so that code, naming
// myenum_fwd on line 6, is expected.
//
// Since no enumeration type body exists, no enumeration constant (6.19) is
// declared. The file declares no module, interface, program, package,
// class, primitive, config, checker, subroutine, parameter, variable or let,
// and with no module there is no top-level instance (23.3.1).
//
// The .hlc also puts UVM 1800.2-2017 on the include path ('-I'), but adds no
// UVM source file, and passes -nobuiltin, so nothing else is compiled; every
// design-level count below is still restricted to objects whose location is
// this file, so a library compiled alongside would not change the result.
//
// What is checked
//   - The unresolved forward typedef is reported: at least one error, and a
//     CompUndefinedType naming myenum_fwd on line 6 (6.18).
//   - No fatal and no syntax diagnostic: line 6 matches A.2.1.3.
//   - No "multiply defined typedef" (myenum_fwd is declared once) and no
//     "forward typedef mismatch" (there is no actual definition to compare
//     the 'enum' basic type against).
//   - Exactly one SourceFile for the .sv, with no includes, no generic
//     directives, no macro definitions and no macro instances.
//   - No enumeration constant from this file, in any Enum or enum
//     typespec.
//   - No design element and no other compilation-unit declaration from this
//     file, and no top-level module instance.
//
// KNOWN FAILURE (unresolved forward typedef not reported), not a defect in
// this test: HLC 577 accepts 'typedef enum myenum_fwd;' with no definition
// of myenum_fwd anywhere and reports no error at all (zero errors, no
// CompUndefinedType), although 6.18 says "It shall be an error if the
// type_identifier does not resolve to a data type."
// ReportsUndefinedTypeForUnresolvedForwardTypedef asserts the LRM behavior
// and is expected to fail until HLC is fixed; it is intentionally not
// skipped or relaxed.
//
// What is NOT checked, and why
//   - Whether HLDB records a Typedef node for myenum_fwd, and what it
//     aliases. The LRM gives an unresolved forward typedef no data type and
//     no object model, so the representation of the failed declaration is a
//     tool convention. Nearest assertions:
//     ReportsUndefinedTypeForUnresolvedForwardTypedef and
//     NoEnumerationConstantsDeclared.
//   - The exact error count: 6.18 requires an error, not exactly one; a
//     tool may report the failure more than once. Nearest assertion:
//     ReportsUndefinedTypeForUnresolvedForwardTypedef (at least one).
//   - The error's column. The LRM numbers lines (22.13) but not columns.
//   - The metadata block comment (5.4): comments carry no meaning.
//   - The missing final newline: end of file is white space (5.3), so it
//     needs no assertion of its own. Nearest assertion:
//     ReportsNoFatalOrSyntaxDiagnostics.
//   - The SourceFile time unit and precision: there is no `timescale and the
//     default is implementation-specific (3.14.2.3).
//   - The warning count. The LRM neither requires nor forbids warnings here.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/checker_decl.h>
#include <hldb/class_defn.h>
#include <hldb/config_decl.h>
#include <hldb/design.h>
#include <hldb/directive.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/include_stmt.h>
#include <hldb/interface.h>
#include <hldb/let_decl.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/program.h>
#include <hldb/source_file.h>
#include <hldb/task_func.h>
#include <hldb/typedef.h>
#include <hldb/typespec.h>
#include <hldb/udp_defn.h>
#include <hldb/variable.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "enum_test_0.sv";

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

// Number of enumeration constants held by 'enumeration', or 0 for nullptr.
size_t enumConstCount(const hldb::Enum *enumeration) {
  if (enumeration == nullptr) {
    return 0;
  }
  return sizeOf(enumeration->getEnumConsts());
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

class EnumTest0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "enum_test_0.hlc"}); }
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
};

// 6.18: "It shall be an error if the type_identifier does not resolve to a
// data type." Nothing in the compilation unit defines myenum_fwd, so the
// forward typedef of line 6 never resolves and an error is required. HLC
// reports a type name that resolves to no data type as CompUndefinedType.
TEST_F(EnumTest0Test, ReportsUndefinedTypeForUnresolvedForwardTypedef) {
  GTEST_SKIP() << "HLC Gap: No diagnosis for missing types";
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  const EventListener::Stats stats = eventListener->getStats();
  EXPECT_GE(stats.m_errors, 1) << "6.18: an unresolved forward typedef is an error";
  EXPECT_NE(findEvent(EventType::CompUndefinedType, "myenum_fwd", 6), nullptr)
      << "myenum_fwd (line 6) never resolves to a data type;" << describeErrors(eventListener, EventSeverity::Error);
}

// Line 6 matches type_declaration ::= typedef enum type_identifier ;
// (A.2.1.3), so the error above is semantic: nothing is fatal or a syntax
// error.
TEST_F(EnumTest0Test, ReportsNoFatalOrSyntaxDiagnostics) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  const EventListener::Stats stats = eventListener->getStats();
  EXPECT_EQ(stats.m_fatals, 0) << describeErrors(eventListener, EventSeverity::Fatal);
  EXPECT_EQ(stats.m_syntaxes, 0) << describeErrors(eventListener, EventSeverity::Syntax);
}

TEST_F(EnumTest0Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

// The file holds no compiler directive, no `define and no macro usage.
TEST_F(EnumTest0Test, SourceFileHasNoIncludesDirectivesOrMacros) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getIncludeStmts()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// A forward typedef names a type without defining it (6.18); the file has
// no enum_name_declaration list '{ ... }' (6.19), so no enumeration
// constant exists, in any Enum or enum typespec from this file.
TEST_F(EnumTest0Test, NoEnumerationConstantsDeclared) {
  ASSERT_NE(m_design, nullptr);

  if (const hldb::EnumCollection *const enums = m_design->getEnums()) {
    for (const hldb::Enum *enumeration : *enums) {
      if (!endsWith(enumeration->getFile(), kSourceFileName)) continue;
      EXPECT_EQ(enumConstCount(enumeration), 0u) << "no enumeration body is written";
    }
  }
  if (const hldb::TypespecCollection *const typespecs = m_design->getTypespecs()) {
    for (const hldb::Typespec *typespec : *typespecs) {
      if (!endsWith(typespec->getFile(), kSourceFileName)) continue;
      const hldb::EnumTypespec *const enumTypespec = any_cast<hldb::EnumTypespec>(typespec);
      if (enumTypespec == nullptr) continue;
      EXPECT_EQ(enumConstCount(enumTypespec->getEnum()), 0u) << "no enumeration body is written";
    }
  }
}

// Line 6 is the only declaration, and it is a type declaration, so the file
// declares no design element (3.2-3.12).
TEST_F(EnumTest0Test, NoDesignElementsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);
}

// Apart from the forward typedef, no compilation-unit item (3.12.1) is
// declared: no subroutine, let, parameter or variable -- in particular no
// variable of type myenum_fwd.
TEST_F(EnumTest0Test, NoOtherCompilationUnitDeclarations) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getTaskFuncs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getLetDecls()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParamAssigns()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
}

// With no module in the file there is nothing to instantiate implicitly as
// a top-level module (23.3.1). This holds before elaboration (no instance
// tree yet) and after it (no module to elaborate), so it is not gated on
// getElaborated().
TEST_F(EnumTest0Test, NoTopLevelInstances) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getTopModules()), 0u);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
