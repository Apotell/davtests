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
class PreprocIncBadTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "t_preproc_inc_bad.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.4: a syntax error inside an included file must not prevent the
// compiler from completing compilation of the including file's modules.
TEST_F(PreprocIncBadTest, MainModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("t", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "main module 't' must compile despite syntax error in included file";
}

// The included file defines module 'xx'; it must be recorded even with a
// syntax error, because the error is in the module body, not the declaration.
TEST_F(PreprocIncBadTest, IncludedModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("xx", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'xx' from included file must be compiled";
}

// LRM 22.4: the included file must be recorded in the including file's include list.
TEST_F(PreprocIncBadTest, IncludedFileRecorded) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_inc_bad.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("t_preproc_inc_inc_bad.vh", sf->getIncludes());
  EXPECT_NE(inc, nullptr) << "t_preproc_inc_inc_bad.vh must be recorded as an include";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
