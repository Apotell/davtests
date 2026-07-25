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
class PreprocNestedDefineTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "preproc_test_8.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.5.1: the replacement text of a macro may contain a `define directive
// that is not evaluated until the macro is invoked. The module must compile.
TEST_F(PreprocNestedDefineTest, ModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("work@test", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'test' must compile";
}

// LRM 22.5.1: INCEPTION must appear in the source file's macro table.
TEST_F(PreprocNestedDefineTest, InceptionMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_8.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("INCEPTION", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "INCEPTION macro must be defined";
}

// LRM 22.5.1: DEEPER is defined inside INCEPTION's replacement text; it must
// not appear in the source file's macro table until INCEPTION is actually
// invoked. At parse time, only INCEPTION is defined.
TEST_F(PreprocNestedDefineTest, DeeperNotDefinedAtParseTime) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_8.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("DEEPER", sf->getPreprocMacroDefinitions());
  EXPECT_EQ(macro, nullptr) << "DEEPER must not be defined until INCEPTION is invoked; it lives only in the macro body";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
