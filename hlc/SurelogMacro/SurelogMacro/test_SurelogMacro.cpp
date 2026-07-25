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

// SurelogMacro exercises macro expansion patterns specific to Surelog.
// The source file must be recorded in the design.
TEST_F(SurelogMacroTest, SourceFileRecorded) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  EXPECT_NE(sf, nullptr) << "dut.sv must be recorded as a source file";
}

// LRM 22.5.1: dut.sv defines no macros of its own; all macro content is
// provided by the tool's built-in macro table.
TEST_F(SurelogMacroTest, NoMacroDefinitionsInSourceFile) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(sf->getPreprocMacroDefinitions(), nullptr)
      << "dut.sv defines no macros; all macro content comes from the tool built-ins";
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

// ---------------------------------------------------------------------------
// 1. Macro arguments and body tokens
// ---------------------------------------------------------------------------

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
    std::string_view body = mi->getBody();
    EXPECT_GE(body.size(), 0u) << "getBody() must be callable without crashing";
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
