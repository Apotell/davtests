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

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/source_file.h>

#include <string_view>

namespace hlc {
class SurelogMacroTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "SurelogMacro.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// dut.sv guards its sole module with `ifdef HLC. HLC does not predefine a
// macro named HLC (no such predefined-macro registration exists in the
// tool), so the `ifdef HLC branch is not taken and module top is never
// compiled; this is confirmed by the empty AST for dut.sv. This test file
// therefore validates the ifdef-false path (no user modules, only the
// builtin package survives), not any Surelog-specific behavior.
// The source file must be recorded in the design.
TEST_F(SurelogMacroTest, SourceFileRecorded) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  EXPECT_NE(sf, nullptr) << "dut.sv must be recorded as a source file";
}

// LRM 22.5.1: dut.sv itself contains no `define directives, so no
// PreprocMacroDefinition entries should be recorded for it.
TEST_F(SurelogMacroTest, NoMacroDefinitionsInSourceFile) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(sf->getPreprocMacroDefinitions(), nullptr) << "dut.sv contains no `define directives of its own";
}

// No user-defined modules are expected (only built-in packages survive).
TEST_F(SurelogMacroTest, NoUserModules) {
  if (m_design->getAllModules() != nullptr) {
    for (const hldb::Module *const m : *m_design->getAllModules()) {
      const std::string_view name = m->getName();
      // Built-in package modules (sv_builtin, uvm_pkg, etc.) are acceptable.
      EXPECT_TRUE(name.empty() || name.find("builtin") != std::string::npos || name.find("uvm") != std::string::npos)
          << "unexpected non-builtin module '" << name << "' in SurelogMacro";
    }
  }
}

// ----
// 1. Macro arguments and body tokens
// ----

// LRM 22.5.1: dut.sv defines no macros of its own; getArguments() and
// getTokens() are not exercised on the definition side. Any macro instances
// (from `ifdef HLC) must have accessible getBody() without crashing.
TEST_F(SurelogMacroTest, MacroInstancesBodyDoesNotCrash) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  if (sf->getPreprocMacroInstances() == nullptr) return;
  for (const hldb::PreprocMacroInstance *const mi : *sf->getPreprocMacroInstances()) {
    ASSERT_NE(mi, nullptr);
    EXPECT_FALSE(mi->getName().empty()) << "each macro instance must have a non-empty name";
    // getBody() is a smoke test here: dut.sv never expands a macro (its only
    // directive is the unresolved `ifdef HLC), so this loop should not
    // execute at all; if it ever does, getBody() must simply be callable.
    const std::string_view body = mi->getBody();
    (void)body;
  }
}

// LRM 22.5.1: when there are no locally-defined macros, every macro definition
// entry (if the collection is non-null) must individually have a name.
TEST_F(SurelogMacroTest, AnyLocalMacroDefinitionsHaveNames) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  if (sf->getPreprocMacroDefinitions() == nullptr) return;
  for (const hldb::PreprocMacroDefinition *const md : *sf->getPreprocMacroDefinitions()) {
    ASSERT_NE(md, nullptr);
    EXPECT_FALSE(md->getName().empty()) << "every macro definition must have a non-empty name";
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
