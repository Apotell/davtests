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
class PreprocKwdTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "t_preproc_kwd.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.14: `begin_keywords / `end_keywords delimit a region using the keyword
// set of the specified standard. All versioned sub-modules must compile.
TEST_F(PreprocKwdTest, TopModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("work@t", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "top module 't' must compile with begin_keywords/end_keywords in scope";
}

// LRM 22.14: each versioned module block (v95, v01, v05, s05, s09, s12, s17)
// inside begin_keywords/end_keywords must compile as a separate named module.
TEST_F(PreprocKwdTest, V95ModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("work@v95", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'v95' (1995 keyword set) must compile";
}

TEST_F(PreprocKwdTest, S09ModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("work@s09", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 's09' (SystemVerilog 2009 keyword set) must compile";
}

TEST_F(PreprocKwdTest, A23ModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("work@a23", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'a23' must compile";
}

// LRM 22.14: the file must not define any preprocessor macros at the top level;
// begin_keywords/end_keywords is purely a keyword-set directive.
TEST_F(PreprocKwdTest, NoTopLevelMacros) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("t_preproc_kwd.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(sf->getPreprocMacroDefinitions(), nullptr)
      << "t_preproc_kwd.v must define no macros -- begin_keywords is not a macro directive";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
