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
class MacroVTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "macro_v.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// macro.v defines a module 'top' that exercises various macro expansions.
// It must compile cleanly (no preprocessing holes expected for this file).
TEST_F(MacroVTest, ModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'top' must compile cleanly from macro.v";
}

// LRM 22.5.1: macro.v uses macros inline (expanded before parse); there should
// be no macro definitions recorded in the source file's own table after expansion.
TEST_F(MacroVTest, NoMacroDefinitionsInMainFile) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macro.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(sf->getPreprocMacroDefinitions(), nullptr)
      << "macro.v does not define any macros itself; all macros come from included files";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
