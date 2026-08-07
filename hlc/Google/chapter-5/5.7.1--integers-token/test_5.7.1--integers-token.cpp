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

// Validates that the 'integer' variable type is parsed and represented in UHDM
// with an IntegerTypespec marked as signed.
//
// SV source:
//   module top();
//     integer a;
//   endmodule
//
// The SV 'integer' keyword declares a 32-bit signed 2's-complement variable.
// In UHDM the variable 'a' carries a RefTypespec whose actual typespec is an
// IntegerTypespec with vpiSigned: true.  This is distinct from 'logic' which
// produces a LogicTypespec, and from 'int' (SystemVerilog 2-state type).

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/integer_typespec.h>
#include <hldb/module.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class IntegersToken : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.7.1--integers-token.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("top", d->getAllModules());
}

// ----
// Module structure
// ----
TEST_F(IntegersToken, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'top' not found"; }

TEST_F(IntegersToken, OneVariableExists) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getVariables(), nullptr);
  EXPECT_EQ(m->getVariables()->size(), 1u) << "expected exactly 1 variable ('a')";
}

TEST_F(IntegersToken, VariableIsNamedA) {
  const hldb::Variable *const var = hldb::findByName<hldb::Variable>("a", getTop(m_design)->getVariables());
  EXPECT_NE(var, nullptr) << "var 'a' not found";
}

// `integer` is a variable keyword, not one of the net-type keywords listed
// in IEEE 1800-2023 Sec 6.7/6.8, so 'a' must not also appear as a Net.
TEST_F(IntegersToken, ModuleHasNoNets) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getNets() || m->getNets()->empty()) << "'integer a' must not appear as a Net";
}

TEST_F(IntegersToken, NoProcesses) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getProcesses() || m->getProcesses()->empty()) << "module should have no initial/always processes";
}

// ----
// 'integer a' -> Var typespec is IntegerTypespec, marked signed.
// The typespec chain: Variable::getTypespec() -> RefTypespec -> getActual<IntegerTypespec>()
// ----
TEST_F(IntegersToken, VariableTypespecIsInteger) {
  const hldb::Variable *const var = hldb::findByName<hldb::Variable>("a", getTop(m_design)->getVariables());
  ASSERT_NE(var, nullptr);
  const hldb::RefTypespec *const ref = var->getTypespec();
  ASSERT_NE(ref, nullptr) << "var 'a' has no typespec";
  const auto *const intTs = ref->getActual<hldb::IntegerTypespec>();
  EXPECT_NE(intTs, nullptr) << "'integer a' should have an IntegerTypespec, not LogicTypespec or other";
}

TEST_F(IntegersToken, IntegerTypespecIsSigned) {
  const hldb::Variable *const var = hldb::findByName<hldb::Variable>("a", getTop(m_design)->getVariables());
  ASSERT_NE(var, nullptr);
  const hldb::RefTypespec *const ref = var->getTypespec();
  ASSERT_NE(ref, nullptr);
  const auto *const intTs = ref->getActual<hldb::IntegerTypespec>();
  ASSERT_NE(intTs, nullptr);
  EXPECT_TRUE(intTs->getSigned()) << "SV 'integer' is a signed 32-bit type; vpiSigned should be true";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
