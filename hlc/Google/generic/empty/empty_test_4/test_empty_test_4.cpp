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
// tests/Google/generic/empty/empty_test_4.sv. The whole file is:
//
//    1  // Copyright (C) 2019-2021  The SymbiFlow Authors.
//    2  //
//    3  // Use of this source code is governed by a ISC-style
//    4  // license that can be found in the LICENSE file or at
//    5  // https://opensource.org/licenses/ISC
//    6  //
//    7  // SPDX-License-Identifier: ISC
//    8
//    9
//   10  /*
//   11  :name: empty_test_4
//   12  :description: Test
//   13  :type: preprocessing
//   14  :tags: 5.3 5.4
//   15  */
//   16  // comment
//
// i.e. seven one-line comments, two blank lines, one block comment and one
// more one-line comment. Every line, 16 included, ends with a CR LF line
// terminator, so the comment of line 16 is closed by its newline and
// nothing follows it.
//
// The fixture is tagged 5.3 (white space) and 5.4 (comments):
//   - 5.3: "White space characters ... shall be ignored except when they
//     serve to separate other lexical tokens." and "White space shall
//     contain the characters for spaces, tabs, newlines, formfeeds, and end
//     of file." The empty lines 8-9, the line terminators and the end of
//     file are white space.
//   - 5.4: "A one-line comment shall start with the two characters // and
//     end with a newline character. A block comment shall start with /* and
//     end with */." Lines 1-7 and 16 are one-line comments; lines 10-15 are
//     one block comment. A one-line comment may directly follow the line
//     that closes a block comment: '*/' ends the block comment and the next
//     '//' starts a new one-line comment. Comments carry no meaning: the
//     text inside them is not tokenized, so 'comment' (line 16),
//     ':name: empty_test_4', ':type: preprocessing', the
//     ':tags:' numbers, the URL with its '//' and the copyright text declare
//     nothing, use no macro and hold no directive.
//
// With comments and white space removed no token is left. An empty
// compilation unit is legal: source_text ::= [timeunits_declaration]
// { description } (A.1.2) allows zero descriptions. So the file compiles
// without errors and declares nothing -- no module, interface, program,
// package, class, primitive, config, checker, subroutine, parameter,
// variable, typedef or let -- and, with no module, there is no top-level
// instance (23.3.1).
//
// The .hlc also puts UVM 1800.2-2017 on the include path ('-I'), but adds no
// UVM source file, and passes -nobuiltin, so nothing else is compiled; every
// design-level count below is still restricted to objects whose location is
// this file, so a library compiled alongside would not change the result.
//
// What is checked
//   - No fatal, syntax or error diagnostic, and none of the preprocessor or
//     parser diagnostics a misreading of the comments would produce: a
//     preprocessor or parser syntax error (e.g. comment text taken as
//     tokens, or the block comment left unterminated), an "unknown macro",
//     or an unterminated string.
//   - Exactly one SourceFile for the .sv, with no includes and no generic
//     directives (`timescale, `default_nettype, ...).
//   - No macro definition and no macro instance in the source file.
//   - No design element and no compilation-unit declaration from this file.
//   - No top-level module instance from this file.
//
// What is NOT checked, and why
//   - The Comment objects HLDB may record for lines 1-7 and 10-15. The LRM
//     gives comments no meaning and no object model (vpiComment is marked
//     non-standard); how HLDB groups, classifies and attaches them is a tool
//     convention. Their effect -- nothing -- is checked instead. Nearest
//     assertions: NoDesignElementsDeclared and
//     SourceFileHasNoMacroDefinitionsOrInstances.
//   - White space (empty lines 8-9, line terminators): 5.3 gives it no
//     meaning beyond separating tokens, and there is no token to separate.
//     Its effect -- none -- is covered by CompilesWithoutErrors and
//     ReportsNoLexicalOrSyntaxDiagnostics.
//   - The SourceFile time unit and precision: there is no `timescale and the
//     default is implementation-specific (3.14.2.3). The .hlc's -timescale
//     option is a tool option, not SystemVerilog text.
//   - The warning count. The LRM neither requires nor forbids a warning for
//     an empty compilation unit (a tool may warn that there is no top
//     module). Nearest assertion: CompilesWithoutErrors.

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
#include <hldb/udp_defn.h>
#include <hldb/variable.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "empty_test_4.sv";

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

class EmptyTest4Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "empty_test_4.hlc"}); }
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

// Lines 1-16 are comments (5.4) and white space (5.3) only; an empty
// compilation unit is legal source_text (A.1.2). So nothing is fatal, a
// syntax error or an error.
TEST_F(EmptyTest4Test, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  const EventListener::Stats stats = eventListener->getStats();
  EXPECT_EQ(stats.m_fatals, 0) << describeErrors(eventListener, EventSeverity::Fatal);
  EXPECT_EQ(stats.m_syntaxes, 0) << describeErrors(eventListener, EventSeverity::Syntax);
  EXPECT_EQ(stats.m_errors, 0) << describeErrors(eventListener, EventSeverity::Error);
}

TEST_F(EmptyTest4Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

TEST_F(EmptyTest4Test, SourceFileHasNoIncludesOrGenericDirectives) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  // There is no `include in the file (22.4).
  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getIncludeStmts()), 0u);

  // The file holds no compiler directive at all: there is no grave accent
  // outside comments (or inside them).
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
}

// The file holds no `define and no `name usage, so there is no macro
// definition and no macro instance (22.5).
TEST_F(EmptyTest4Test, SourceFileHasNoMacroDefinitionsOrInstances) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// With comments and white space removed no source text is left, so the
// file declares no design element (3.2-3.12, A.1.2).
TEST_F(EmptyTest4Test, NoDesignElementsDeclared) {
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

// No compilation-unit item (3.12.1) exists either: the words in the
// comments (e.g. 'empty_test_4', 'preprocessing', 'Test') are not
// identifiers of any declaration.
TEST_F(EmptyTest4Test, NoCompilationUnitDeclarations) {
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
TEST_F(EmptyTest4Test, NoTopLevelInstances) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getTopModules()), 0u);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
