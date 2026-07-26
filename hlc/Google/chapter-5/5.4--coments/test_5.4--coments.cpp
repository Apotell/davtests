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

// Validates that both comment forms are accepted and stripped from UHDM:
//   /* multi
//      line
//      comment
//    */
//   // single line comment
//
// UHDM structure:
//   Module name:empty — no nets, no processes, no attributes
//
// Comments leave no trace in the UHDM tree; this test verifies compilation
// succeeds and the resulting module is empty.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>

namespace hlc {

class Coments : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.4--coments.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(Coments, ModuleExists) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  ASSERT_EQ(m_design->getAllModules()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Module>("empty", m_design->getAllModules()), nullptr);
}

TEST_F(Coments, ModuleHasNoNets) {
  const hldb::Module *const m = hldb::findByName<hldb::Module>("empty", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getNets() || m->getNets()->empty()) << "empty module should have no nets";
}

TEST_F(Coments, ModuleHasNoProcesses) {
  const hldb::Module *const m = hldb::findByName<hldb::Module>("empty", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getProcesses() || m->getProcesses()->empty()) << "empty module should have no processes";
}

TEST_F(Coments, ModuleHasNoAttributes) {
  const hldb::Module *const m = hldb::findByName<hldb::Module>("empty", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getAttributes() || m->getAttributes()->empty()) << "empty module should have no attributes";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
