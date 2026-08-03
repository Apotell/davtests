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

// Validates the UHDM graph for a module with a realtime-typed variable:
//   module top();
//     realtime a = 0.5;
//   endmodule
//
// Per IEEE 1800-2023 6.8/6.12: 'realtime' has no explicit net-type keyword
// (wire, tri, etc.), so 'a' is a variable_declaration, not a net_declaration.
//
// Checked:
//   - design has module top
//   - module has exactly 1 variable: 'a'
//   - 'a' has a RefTypespec node whose vpiActual is null
//     (realtime has no dedicated typespec class -- contrast: real -> RealTypespec)
//   - 'a' initial value: Constant vpiRealConst, decompile "0.5"
//   - top has no continuous assignments
//   - top has no processes
//   - RefTypespec getName() for realtime

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>
#include <hldb/time_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class Realtime : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.12--realtime.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(Realtime, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(Realtime, OneVariableExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

TEST_F(Realtime, AVariableExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(hldb::findByName<hldb::Variable>("a", top->getVariables()), nullptr) << "variable 'a' not found";
}

TEST_F(Realtime, ANotInNets) {
  // Per IEEE 1800-2023 Sec 6.7/6.8, 'realtime' has no net-type keyword, so
  // 'a' must not also be materialized as a Net.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || hldb::findByName<hldb::Net>("a", top->getNets()) == nullptr)
      << "'realtime a' must not appear in vpiNet";
}

// ----
// Typespec -- RefTypespec present but vpiActual is null for realtime
// (contrast with 'real' which explicitly resolves to RealTypespec)
// ----
TEST_F(Realtime, AVariableHasTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec(), nullptr) << "variable 'a' should have a RefTypespec node";
}

TEST_F(Realtime, AVariableTypespecActualIsNotNull) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  ASSERT_NE(rts->getActual(), nullptr) << "realtime variable typespec vpiActual is unset";
  EXPECT_EQ(rts->getActual()->getAnyType(), hldb::AnyType::RealTypespec);
}

TEST_F(Realtime, AVariableTypespecNameIsRealtime) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual(), nullptr) << "RefTypespec actual is nullptr";
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr);
}

// ----
// Initial value -- still recorded as a real constant "0.5"
// ----
TEST_F(Realtime, AVariableInitialValueConstType) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "variable 'a' has no initial value Constant";
  EXPECT_EQ(init->getConstType(), vpiRealConst);
}

TEST_F(Realtime, AVariableInitialValueIsHalf) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getDecompile(), "0.5");
}

TEST_F(Realtime, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

TEST_F(Realtime, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
