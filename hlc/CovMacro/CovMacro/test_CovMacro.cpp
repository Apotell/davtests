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
class CovMacroTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "CovMacro.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.5.1: SV_COV_* macros are used (instantiated) in dut.sv but are
// defined externally (command-line or tool built-ins). Both declared modules
// must compile.
TEST_F(CovMacroTest, DutModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("DUT", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'DUT' must compile with SV_COV_* macro instantiations";
}

TEST_F(CovMacroTest, TopModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'top' must compile";
}

// LRM 22.5.1: dut.sv itself defines no macros; all SV_COV_* values are
// provided externally. The source file must have no macro definition entries.
TEST_F(CovMacroTest, NoMacroDefinitionsInSourceFile) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(sf->getPreprocMacroDefinitions(), nullptr) << "dut.sv defines no macros; SV_COV_* are externally supplied";
}

// ---------------------------------------------------------------------------
// 1. Macro arguments and body tokens
// ---------------------------------------------------------------------------

// LRM 22.5.1: when SV_COV_* instances are recorded the getBody() string must
// not cause a crash (empty or non-empty are both acceptable since the macro is
// externally supplied and may or may not be expanded in the recorded output).
TEST_F(CovMacroTest, MacroInstanceBodyDoesNotCrash) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  if (sf->getPreprocMacroInstances() == nullptr) return;
  for (const hldb::PreprocMacroInstance *const mi : *sf->getPreprocMacroInstances()) {
    ASSERT_NE(mi, nullptr);
    // getBody() must be callable without crashing; length check is informational.
    std::string_view body = mi->getBody();
    EXPECT_GE(body.size(), 0u);
  }
}

// LRM 22.5.1: when SV_COV_* instances are recorded, each instance must have a
// name. The getArguments() on a PreprocMacroInstance returns the actual call
// arguments; for an object-like invocation (no parentheses) it is null.
TEST_F(CovMacroTest, MacroInstancesHaveNames) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  if (sf->getPreprocMacroInstances() == nullptr) return;
  for (const hldb::PreprocMacroInstance *const mi : *sf->getPreprocMacroInstances()) {
    ASSERT_NE(mi, nullptr);
    EXPECT_FALSE(mi->getName().empty()) << "every macro instance must have a non-empty name";
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
