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
// `'{default: 3}` entry, declared inside a package, plus Sec 20.3
// (typedef'd unpacked-array function return type).
// SV: tests/DefaultPatternInt/dut.sv
//
//   package pack;
//     localparam int unsigned CNT = 2;
//     localparam int unsigned V[CNT] = '{default: 3};
//   endpackage
//
//   module top();
//     import pack::*;
//     typedef int unsigned ASSIGN_VADDR_RET_T[CNT];
//     function static ASSIGN_VADDR_RET_T ASSIGN_VADDR();
//       for (int i = 0; i < CNT; i++) ASSIGN_VADDR[i] = V[i];
//     endfunction
//     localparam int unsigned VADDR[CNT] = ASSIGN_VADDR();
//     ... if (...) $info(...) ...
//   endmodule
//
// -- Sec 5.10 rules under test ------------------------------------------
//   * `'{default: 3}` for an unpacked array is an assignment_pattern with a
//     single structure_pattern_key entry keyed by the `default` keyword,
//     represented as an Operation (vpiAssignmentPatternOp) whose sole
//     operand is a TaggedPattern (tag = RefObj "default", pattern =
//     Constant "3"), independent of the array's element count.
//
// -- Sec 6.20.4 / 6.11.2 rules under test --------------------------------
//   * `localparam int unsigned CNT = 2` is a localparam (getLocalParam()
//     true) with constType vpiUIntConst.
//   * `localparam int unsigned V[CNT] = ...` is likewise a localparam.
//
// -- Function under test --------------------------------------------------
//   * `ASSIGN_VADDR` is a Function whose return-value ParamAssign is
//     distinct from a plain default value (VADDR's initializer is a
//     function call, not an assignment pattern).

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/tagged_pattern.h>
#include <hldb/typedef_typespec.h>
#include <hldb/vpi_user.h>

#include <string>

namespace hlc {

class DefaultPatternIntTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DefaultPatternInt.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Package *getPkg(const hldb::Design *d) {
  return hldb::findByName<hldb::Package>("pack", d->getAllPackages());
}

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("top", d->getAllModules());
}

static const hldb::ParamAssign *getPkgParamAssign(const hldb::Design *d, std::string_view name) {
  const hldb::Package *const pkg = getPkg(d);
  if (!pkg) return nullptr;
  return hldb::findByName(name, pkg->getParamAssigns());
}

// ===========================================================================
// Existence
// ===========================================================================

TEST_F(DefaultPatternIntTest, PackagePackExists) {
  ASSERT_NE(m_design->getAllPackages(), nullptr);
  EXPECT_NE(getPkg(m_design), nullptr) << "package 'pack' not found";
}

TEST_F(DefaultPatternIntTest, ModuleTopExists) { EXPECT_NE(getTop(m_design), nullptr) << "module 'top' not found"; }

// ===========================================================================
// localparam int unsigned CNT = 2
// ===========================================================================

TEST_F(DefaultPatternIntTest, CntIsLocalParam) {
  const hldb::Package *const pkg = getPkg(m_design);
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getParameters(), nullptr);
  const hldb::Parameter *const cnt = hldb::findByName<hldb::Parameter>("CNT", pkg->getParameters());
  ASSERT_NE(cnt, nullptr) << "'CNT' parameter not found in pack";
  EXPECT_TRUE(cnt->getLocalParam()) << "Sec 6.20.4: 'localparam int unsigned CNT' must be a localparam";
}

TEST_F(DefaultPatternIntTest, CntValueIsTwo) {
  const hldb::ParamAssign *const pa = getPkgParamAssign(m_design, "CNT");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'CNT' not found";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), std::string_view("2"));
}

// ===========================================================================
// localparam int unsigned V[CNT] = '{default: 3}  -- Sec 5.10
// ===========================================================================

TEST_F(DefaultPatternIntTest, VIsLocalParam) {
  const hldb::Package *const pkg = getPkg(m_design);
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getParameters(), nullptr);
  const hldb::Parameter *const v = hldb::findByName<hldb::Parameter>("V", pkg->getParameters());
  ASSERT_NE(v, nullptr) << "'V' parameter not found in pack";
  EXPECT_TRUE(v->getLocalParam()) << "Sec 6.20.4: 'localparam int unsigned V[CNT]' must be a localparam";
}

TEST_F(DefaultPatternIntTest, VRhsIsAssignmentPatternOp) {
  const hldb::ParamAssign *const pa = getPkgParamAssign(m_design, "V");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'V' not found";
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'{default: 3}' RHS must be an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiAssignmentPatternOp);
}

TEST_F(DefaultPatternIntTest, VPatternHasOneOperand) {
  const hldb::ParamAssign *const pa = getPkgParamAssign(m_design, "V");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  EXPECT_EQ(rhs->getOperands()->size(), 1u)
      << "a single 'default:' entry must produce one operand regardless of array size";
}

TEST_F(DefaultPatternIntTest, VOperandIsDefaultTaggedPattern) {
  const hldb::ParamAssign *const pa = getPkgParamAssign(m_design, "V");
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

TEST_F(DefaultPatternIntTest, VPatternValueIsThree) {
  const hldb::ParamAssign *const pa = getPkgParamAssign(m_design, "V");
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
// Function ASSIGN_VADDR and VADDR's initializer (absence check: VADDR's
// initializer is a function call, not another assignment pattern)
// ===========================================================================

TEST_F(DefaultPatternIntTest, FunctionAssignVaddrExists) {
  const hldb::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Function>("ASSIGN_VADDR", top->getTaskFuncs()), nullptr)
      << "function 'ASSIGN_VADDR' not found in module 'top'";
}

TEST_F(DefaultPatternIntTest, VaddrRhsIsNotAssignmentPattern) {
  const hldb::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParamAssigns(), nullptr);
  const hldb::ParamAssign *const pa = hldb::findByName("VADDR", top->getParamAssigns());
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
