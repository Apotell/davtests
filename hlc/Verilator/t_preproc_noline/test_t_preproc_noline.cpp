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
#include <hldb/preproc_macro_definition.h>
#include <hldb/source_file.h>

namespace hlc {
class PreprocNolineTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "t_preproc_noline.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.5.1: `define CHECK(a,b) is a valid function-like macro. The source
// file must contain it even when the file has a `line directive syntax error.
TEST_F(PreprocNolineTest, CheckMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_noline.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("CHECK", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "CHECK macro must be recorded despite `line syntax error";
}

// LRM 22.5.1: CHECK has a replacement text body; getTokens() must be non-null,
// meaning the body was captured.
TEST_F(PreprocNolineTest, CheckMacroHasBody) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_noline.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("CHECK", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_NE(macro->getTokens(), nullptr) << "CHECK must have a body (getTokens() must not be null)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
