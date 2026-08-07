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
#include <hldb/source_file.h>

namespace hlc {
class PreprocUndefineallTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "t_preproc_undefineall.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.5.2: `undefineall must not prevent the module from compiling.
TEST_F(PreprocUndefineallTest, ModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("t", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 't' must compile despite `undefineall";
}

// LRM 22.5.1: UDALL is defined before `undefineall. The compiler must record
// its definition because macro recording reflects the textual structure, not
// the runtime effect of `undefineall on subsequent expansions.
TEST_F(PreprocUndefineallTest, UdallMacroRecorded) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_undefineall.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("UDALL", sf->getPreprocMacroDefinitions());
  EXPECT_NE(macro, nullptr) << "UDALL must be recorded as defined before `undefineall";
}

// ---------------------------------------------------------------------------
// 1. PreprocMacroDefinition arguments and tokens
// ---------------------------------------------------------------------------

// LRM 22.5.1: `define UDALL is a flag macro with no argument list and no
// replacement body.
TEST_F(PreprocUndefineallTest, UdallHasNoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_undefineall.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("UDALL", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_EQ(macro->getArguments(), nullptr) << "UDALL is a flag macro with no argument list";
}

TEST_F(PreprocUndefineallTest, UdallHasNoTokens) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_undefineall.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("UDALL", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getTokens() == nullptr || macro->getTokens()->empty())
      << "UDALL is a flag macro with no replacement body";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
