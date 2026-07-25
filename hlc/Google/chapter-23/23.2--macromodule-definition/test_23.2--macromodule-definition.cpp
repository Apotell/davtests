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
#include <hldb/source_file.h>

namespace hlc {
class MacromoduleDefinitionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "23.2--macromodule-definition.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 23.2: `macromodule' is a syntactic shorthand for `module'; it must
// compile to a module in the design database.
TEST_F(MacromoduleDefinitionTest, TopModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "macromodule 'top' must compile as a regular module";
}

// LRM 23.2: the macromodule keyword introduces no preprocessor macro
// definitions; the source file must have no macro definition entries.
TEST_F(MacromoduleDefinitionTest, NoMacroDefinitions) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("23.2--macromodule-definition.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(sf->getPreprocMacroDefinitions(), nullptr)
      << "macromodule keyword creates no preprocessor macro definitions";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
