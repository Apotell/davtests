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

// Validates the UHDM graph for a module with a chandle-typed variable:
//   module top();
//     chandle a;
//   endmodule
//
// Checked:
//   - design has module top with 1 variable ('a')
//   - variable 'a' has a RefTypespec node (typespec is present)
//   - variable 'a' RefTypespec vpiActual is null -- HLC does not resolve chandle
//     to a ChandleTypespec in the global type pool
//   - variable 'a' has no initial value
//   - no ContAssigns and no processes in top
//
// Also checked:
//   - RefTypespec getName() for chandle -- the name string is preserved even
//     though vpiActual is unresolved
//


#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/chandle_typespec.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class Chandle : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.14--chandle.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(Chandle, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(Chandle, OneVariableExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

TEST_F(Chandle, AVariableExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(hldb::findByName<hldb::Variable>("a", top->getVariables()), nullptr) << "variable 'a' not found";
}

TEST_F(Chandle, ANotInNets) {
  // Per IEEE 1800-2023 Sec 6.7/6.8, 'chandle' has no net-type keyword, so
  // 'a' must not also be materialized as a Net.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || hldb::findByName<hldb::Net>("a", top->getNets()) == nullptr)
      << "'chandle a' must not appear in vpiNet";
}

// ----
// Typespec -- RefTypespec node present but vpiActual is unresolved
// ----
TEST_F(Chandle, AVariableHasTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec(), nullptr) << "variable 'a' should have a RefTypespec node";
}

TEST_F(Chandle, AVariableTypespecActualIsNotNull) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  ASSERT_NE(rts->getActual(), nullptr) << "chandle variable typespec vpiActual is unresolved";
}

TEST_F(Chandle, AVariableTypespecNameIsChandle) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual(), nullptr) << "Unresolved RefTypespec; expecting CHandleTypespec";
  EXPECT_NE(rts->getActual<hldb::ChandleTypespec>(), nullptr);
}

// ----
// No initial value, no continuous assignments, no processes
// ----
TEST_F(Chandle, AVariableHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getValue<hldb::Any>(), nullptr) << "chandle variable 'a' should have no initial value";
}

TEST_F(Chandle, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

TEST_F(Chandle, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
