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
class FpuRptrMacrosTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "fpu_rptr_macros.v.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.5.1: both modules that use the `define macros in fpu_rptr_macros.v
// must compile cleanly.
TEST_F(FpuRptrMacrosTest, FpuBufrptGrp64Compiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("fpu_bufrpt_grp64", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'fpu_bufrpt_grp64' must compile";
}

TEST_F(FpuRptrMacrosTest, FpuBufrptGrp32Compiles) {
  const hldb::Module *const module =
      hldb::findByName<hldb::Module>("fpu_bufrpt_grp32", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'fpu_bufrpt_grp32' must compile";
}

// LRM 22.5.1: fpu_rptr_macros.v defines macros only via expansion; the
// source file itself does not produce macro definition records.
TEST_F(FpuRptrMacrosTest, NoMacroDefinitionsInSourceFile) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("fpu_rptr_macros.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(sf->getPreprocMacroDefinitions(), nullptr)
      << "fpu_rptr_macros.v does not define any macros at source-file level";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
