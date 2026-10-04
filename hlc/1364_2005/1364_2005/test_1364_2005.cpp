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

// Tests for dut.v (tags: 1364_2005)
//   `begin_keywords "1364-2005"
//   module main;
//      reg [3:0] foo, bar;
//      reg [1:0] adr;
//      reg	     bit, rst, clk;
//      reg	     load_enable, write_enable;
//   endmodule // main
//   `end_keywords
//
// "`begin_keywords "1364-2005"" switches the keyword set to plain Verilog,
// which is why "bit" (an SV-2012 keyword) is legal here as a plain
// identifier. All 8 declarations use "reg", which always elaborates as a
// vpiVariable (never a vpiNet), regardless of whether later SystemVerilog
// standards would also allow "logic" to resolve to a net in other contexts.
//
// Checked:
//   - module "main" has zero ports and zero nets
//   - module has exactly 8 variables, in declaration order: foo, bar, adr,
//     bit, rst, clk, load_enable, write_enable
//   - foo and bar are vector variables with an actual LogicTypespec ranged
//     [3:0]
//   - adr is a vector variable with an actual LogicTypespec ranged [1:0]
//   - bit, rst, clk, load_enable, write_enable are scalar variables with an
//     actual LogicTypespec that carries no range
//   - compiler reports zero errors
//
// NOT CHECKED: runtime effects cannot be observed -- HLC is a
// compiler/elaborator with no simulation capability, so no execution ever
// happens for this test to check.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class Keywords1364_2005Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "1364_2005.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() { return hldb::findByName<hldb::Module>("main", m_design->getAllModules()); }

  static void expectVectorRange(const hldb::Variable *var, int32_t left, int32_t right) {
    ASSERT_NE(var, nullptr);
    EXPECT_TRUE(var->getVector());
    ASSERT_NE(var->getTypespec(), nullptr);
    const hldb::LogicTypespec *const typespec = var->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(typespec, nullptr);
    ASSERT_NE(typespec->getRanges(), nullptr);
    ASSERT_EQ(typespec->getRanges()->size(), 1u);
    const hldb::Range *const range = typespec->getRanges()->at(0);
    ASSERT_NE(range, nullptr);
    const hldb::Constant *const leftExpr = range->getLeftExpr<hldb::Constant>();
    const hldb::Constant *const rightExpr = range->getRightExpr<hldb::Constant>();
    ASSERT_NE(leftExpr, nullptr);
    ASSERT_NE(rightExpr, nullptr);
    EXPECT_EQ(leftExpr->getDecompile(), std::to_string(left));
    EXPECT_EQ(rightExpr->getDecompile(), std::to_string(right));
  }

  static void expectScalarNoRange(const hldb::Variable *var) {
    ASSERT_NE(var, nullptr);
    EXPECT_TRUE(var->getScalar());
    ASSERT_NE(var->getTypespec(), nullptr);
    const hldb::LogicTypespec *const typespec = var->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(typespec, nullptr);
    EXPECT_EQ(typespec->getRanges(), nullptr);
  }
};

// --- module ------------------------------------------------------------------

TEST_F(Keywords1364_2005Test, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(Keywords1364_2005Test, ModuleHasNoPortsAndNoNets) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  EXPECT_EQ(mod->getPorts(), nullptr);
  EXPECT_EQ(mod->getNets(), nullptr) << "'reg' always elaborates as a variable, never a net";
}

// --- reg declarations resolve as variables ---------------------------------

TEST_F(Keywords1364_2005Test, ModuleHasEightVariablesInDeclarationOrder) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  ASSERT_EQ(mod->getVariables()->size(), 8u);

  static constexpr std::string_view kExpectedNames[8] = {"foo", "bar", "adr",         "bit",
                                                         "rst", "clk", "load_enable", "write_enable"};
  for (size_t i = 0; i < 8; ++i) {
    EXPECT_EQ(mod->getVariables()->at(i)->getName(), kExpectedNames[i]);
  }
}

TEST_F(Keywords1364_2005Test, FooAndBarAreFourBitVectors) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  expectVectorRange(hldb::findByName<hldb::Variable>("foo", mod->getVariables()), 3, 0);
  expectVectorRange(hldb::findByName<hldb::Variable>("bar", mod->getVariables()), 3, 0);
}

TEST_F(Keywords1364_2005Test, AdrIsATwoBitVector) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  expectVectorRange(hldb::findByName<hldb::Variable>("adr", mod->getVariables()), 1, 0);
}

TEST_F(Keywords1364_2005Test, RemainingDeclarationsAreScalar) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  for (std::string_view name : {"bit", "rst", "clk", "load_enable", "write_enable"}) {
    expectScalarNoRange(hldb::findByName<hldb::Variable>(name, mod->getVariables()));
  }
}

// --- compiler diagnostics ---------------------------------------------------

TEST_F(Keywords1364_2005Test, CompilesWithNoErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
