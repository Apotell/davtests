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

// Spec-based validation of IEEE 1800-2023 Sec 5.10 ("Assignment pattern
// expressions") for an unpacked-array localparam initialized with a single
// `'{default: 3}` entry, declared directly inside a module (no enclosing
// package), plus Sec 20.3 (typedef'd unpacked-array function return type).
// SV: tests/DefaultPatternModule/dut.sv
//
//   module top #()();
//     localparam int unsigned CNT = 2;
//     localparam int unsigned V[CNT] = '{default: 3};
//     typedef int unsigned ASSIGN_VADDR_RET_T[CNT];
//     function static ASSIGN_VADDR_RET_T ASSIGN_VADDR();
//       for (int i = 0; i < CNT; i++) ASSIGN_VADDR[i] = V[i];
//     endfunction
//     localparam int unsigned VADDR[CNT] = ASSIGN_VADDR();
//     ... if (...) $info(...) ...
//   endmodule
//   module main; top #() top1(); endmodule
//
// This mirrors tests/DefaultPatternInt/dut.sv but with CNT/V declared as
// module-scope localparams instead of package-scope ones -- verifying the
// '{default: ...} pattern parses identically regardless of the enclosing
// scope kind (module vs. package), per Sec 5.10 (the assignment pattern
// grammar does not depend on the scope of the declaration it initializes).
//
// -- Sec 5.10 rules under test ------------------------------------------
//   * `'{default: 3}` for an unpacked array is represented as an Operation
//     (vpiAssignmentPatternOp) whose sole operand is a TaggedPattern
//     (tag = RefObj "default", pattern = Constant "3").
//
// -- Sec 6.20.4 rules under test -----------------------------------------
//   * `localparam int unsigned CNT = 2` and `localparam int unsigned
//     V[CNT] = ...` are both localparams (getLocalParam() true) declared
//     directly in module 'top', not in a package.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/tagged_pattern.h>
#include <hldb/vpi_user.h>

#include <string>

namespace hlc {

class DefaultPatternModuleTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DefaultPatternModule.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Module *getModule(const hldb::Design *d, std::string_view name) {
  return hldb::findByName<hldb::Module>(name, d->getAllModules());
}

static const hldb::ParamAssign *getTopParamAssign(const hldb::Design *d, std::string_view name) {
  const hldb::Module *const top = getModule(d, "top");
  if (!top) return nullptr;
  return hldb::findByName(name, top->getParamAssigns());
}

// ===========================================================================
// Existence
// ===========================================================================

TEST_F(DefaultPatternModuleTest, BothModulesExist) {
  EXPECT_NE(getModule(m_design, "top"), nullptr) << "module 'top' not found";
  EXPECT_NE(getModule(m_design, "main"), nullptr) << "module 'main' not found";
}

// No package should exist -- CNT/V live directly in module 'top' in this test.
TEST_F(DefaultPatternModuleTest, NoPackagesDeclared) {
  EXPECT_TRUE(m_design->getAllPackages() == nullptr || m_design->getAllPackages()->empty())
      << "dut.sv declares no package; CNT/V must be module-scope localparams";
}

// ===========================================================================
// localparam int unsigned CNT = 2  -- declared directly in module 'top'
// ===========================================================================

TEST_F(DefaultPatternModuleTest, CntIsLocalParamInModule) {
  const hldb::Module *const top = getModule(m_design, "top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  const hldb::Parameter *const cnt = hldb::findByName<hldb::Parameter>("CNT", top->getParameters());
  ASSERT_NE(cnt, nullptr) << "'CNT' parameter not found in module 'top'";
  EXPECT_TRUE(cnt->getLocalParam()) << "Sec 6.20.4: 'localparam int unsigned CNT' must be a localparam";
}

TEST_F(DefaultPatternModuleTest, CntValueIsTwo) {
  const hldb::ParamAssign *const pa = getTopParamAssign(m_design, "CNT");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'CNT' not found";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), std::string_view("2"));
}

// ===========================================================================
// localparam int unsigned V[CNT] = '{default: 3}  -- Sec 5.10
// ===========================================================================

TEST_F(DefaultPatternModuleTest, VIsLocalParamInModule) {
  const hldb::Module *const top = getModule(m_design, "top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  const hldb::Parameter *const v = hldb::findByName<hldb::Parameter>("V", top->getParameters());
  ASSERT_NE(v, nullptr) << "'V' parameter not found in module 'top'";
  EXPECT_TRUE(v->getLocalParam()) << "Sec 6.20.4: 'localparam int unsigned V[CNT]' must be a localparam";
}

TEST_F(DefaultPatternModuleTest, VRhsIsAssignmentPatternOp) {
  const hldb::ParamAssign *const pa = getTopParamAssign(m_design, "V");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'V' not found";
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'{default: 3}' RHS must be an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiAssignmentPatternOp);
}

TEST_F(DefaultPatternModuleTest, VPatternHasOneOperand) {
  const hldb::ParamAssign *const pa = getTopParamAssign(m_design, "V");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  EXPECT_EQ(rhs->getOperands()->size(), 1u)
      << "a single 'default:' entry must produce one operand regardless of array size";
}

TEST_F(DefaultPatternModuleTest, VOperandIsDefaultTaggedPattern) {
  const hldb::ParamAssign *const pa = getTopParamAssign(m_design, "V");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 1u);

  const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>((*rhs->getOperands())[0]);
  ASSERT_NE(tp, nullptr) << "'default: 3' must be represented as a TaggedPattern";
  const hldb::RefObj *const tag = tp->getTag<hldb::RefObj>();
  ASSERT_NE(tag, nullptr) << "TaggedPattern tag must be a RefObj (the 'default' keyword)";
  EXPECT_EQ(tag->getName(), std::string_view("default"));
}

TEST_F(DefaultPatternModuleTest, VPatternValueIsThree) {
  const hldb::ParamAssign *const pa = getTopParamAssign(m_design, "V");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 1u);

  const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>((*rhs->getOperands())[0]);
  ASSERT_NE(tp, nullptr);
  const hldb::Constant *const pattern = tp->getPattern<hldb::Constant>();
  ASSERT_NE(pattern, nullptr) << "'default: 3' pattern value must be a Constant";
  EXPECT_EQ(pattern->getValue(), std::string_view("3"));
}

// ===========================================================================
// Function ASSIGN_VADDR and VADDR's initializer
// ===========================================================================

TEST_F(DefaultPatternModuleTest, FunctionAssignVaddrExists) {
  const hldb::Module *const top = getModule(m_design, "top");
  ASSERT_NE(top, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Function>("ASSIGN_VADDR", top->getTaskFuncs()), nullptr)
      << "function 'ASSIGN_VADDR' not found in module 'top'";
}

TEST_F(DefaultPatternModuleTest, VaddrRhsIsNotAssignmentPattern) {
  const hldb::ParamAssign *const pa = getTopParamAssign(m_design, "VADDR");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'VADDR' not found";
  ASSERT_NE(pa->getRhs(), nullptr) << "'VADDR = ASSIGN_VADDR()' must have a non-null RHS";
  EXPECT_NE(pa->getRhs()->getAnyType(), hldb::AnyType::Operation)
      << "'VADDR = ASSIGN_VADDR()' RHS is a function call, not an assignment-pattern Operation";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
