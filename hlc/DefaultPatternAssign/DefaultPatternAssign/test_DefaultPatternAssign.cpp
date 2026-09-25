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
// expressions") for a cast assignment pattern used as a package parameter's
// default value, and Sec 6.20.2 for the resulting chain of module
// parameters that derive from it.
// SV: tests/DefaultPatternAssign/dut.sv
//
//   package foo_pkg;
//     parameter int VALUE_TEMP = int'{default: 1};
//   endpackage
//
//   module bottom (); parameter int VALUE = 8; endmodule
//   module lower ();
//     import foo_pkg::*;
//     parameter int VALUE = VALUE_TEMP + 2;
//     bottom #(.VALUE(VALUE)) bottom_u();
//   endmodule
//   module upper (); parameter int VALUE = 7; lower lower_u (); endmodule
//   module top ();   parameter int VALUE = 5; upper upper_u(); endmodule
//
// -- Sec 5.10 rules under test ------------------------------------------
//   * `int'{default: 1}` is an assignment_pattern_expression: an
//     assignment_pattern preceded by a cast to `int`.
//   * A single `default: 1` entry is a structure_pattern_key (the
//     `default` keyword) mapped to the constant 1, represented in UHDM as
//     an Operation (vpiAssignmentPatternOp) whose sole operand is a
//     TaggedPattern with tag = RefObj "default" and pattern = Constant "1".
//
// -- Sec 6.20.2 rules under test ----------------------------------------
//   * Each module parameter without further override keeps its own
//     default value (bottom: 8, upper: 7, top: 5).
//   * `lower`'s parameter VALUE = VALUE_TEMP + 2 is a binary '+' Operation
//     (vpiAddOp) whose operands are a RefObj to the imported package
//     parameter VALUE_TEMP and a Constant "2".

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/tagged_pattern.h>
#include <hldb/vpi_user.h>

#include <string>

namespace hlc {

class DefaultPatternAssignTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DefaultPatternAssign.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Package *getPkg(const hldb::Design *d) {
  return hldb::findByName<hldb::Package>("foo_pkg", d->getAllPackages());
}

static const hldb::Module *getModule(const hldb::Design *d, std::string_view name) {
  return hldb::findByName<hldb::Module>(name, d->getAllModules());
}

static const hldb::ParamAssign *getPkgParamAssign(const hldb::Design *d, std::string_view name) {
  const hldb::Package *const pkg = getPkg(d);
  if (!pkg) return nullptr;
  return hldb::findByName(name, pkg->getParamAssigns());
}

static const hldb::ParamAssign *getModuleParamAssign(const hldb::Design *d, std::string_view module,
                                                       std::string_view param) {
  const hldb::Module *const m = getModule(d, module);
  if (!m) return nullptr;
  return hldb::findByName(param, m->getParamAssigns());
}

// ===========================================================================
// Existence
// ===========================================================================

TEST_F(DefaultPatternAssignTest, PackageFooPkgExists) {
  ASSERT_NE(m_design->getAllPackages(), nullptr);
  EXPECT_NE(getPkg(m_design), nullptr) << "package 'foo_pkg' not found";
}

TEST_F(DefaultPatternAssignTest, AllFourModulesExist) {
  EXPECT_NE(getModule(m_design, "bottom"), nullptr) << "module 'bottom' not found";
  EXPECT_NE(getModule(m_design, "lower"), nullptr) << "module 'lower' not found";
  EXPECT_NE(getModule(m_design, "upper"), nullptr) << "module 'upper' not found";
  EXPECT_NE(getModule(m_design, "top"), nullptr) << "module 'top' not found";
}

TEST_F(DefaultPatternAssignTest, PackageParameterValueTempExists) {
  const hldb::Package *const pkg = getPkg(m_design);
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Parameter>("VALUE_TEMP", pkg->getParameters()), nullptr)
      << "'VALUE_TEMP' parameter not found in foo_pkg";
}

// ===========================================================================
// VALUE_TEMP = int'{default: 1}  -- Sec 5.10 assignment pattern expression
// ===========================================================================

TEST_F(DefaultPatternAssignTest, ValueTempRhsIsAssignmentPatternOp) {
  const hldb::ParamAssign *const pa = getPkgParamAssign(m_design, "VALUE_TEMP");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'VALUE_TEMP' not found";
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'int'{default: 1}' RHS must be an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiAssignmentPatternOp);
}

TEST_F(DefaultPatternAssignTest, ValueTempPatternHasOneOperand) {
  const hldb::ParamAssign *const pa = getPkgParamAssign(m_design, "VALUE_TEMP");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  EXPECT_EQ(rhs->getOperands()->size(), 1u) << "a single 'default:' entry must produce exactly one operand";
}

TEST_F(DefaultPatternAssignTest, ValueTempOperandIsDefaultTaggedPattern) {
  const hldb::ParamAssign *const pa = getPkgParamAssign(m_design, "VALUE_TEMP");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 1u);

  const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>((*rhs->getOperands())[0]);
  ASSERT_NE(tp, nullptr) << "'default: 1' must be represented as a TaggedPattern";
  const hldb::RefObj *const tag = tp->getTag<hldb::RefObj>();
  ASSERT_NE(tag, nullptr) << "TaggedPattern tag must be a RefObj (the 'default' keyword)";
  EXPECT_EQ(tag->getName(), std::string_view("default"));
}

TEST_F(DefaultPatternAssignTest, ValueTempPatternValueIsOne) {
  const hldb::ParamAssign *const pa = getPkgParamAssign(m_design, "VALUE_TEMP");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 1u);

  const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>((*rhs->getOperands())[0]);
  ASSERT_NE(tp, nullptr);
  const hldb::Constant *const pattern = tp->getPattern<hldb::Constant>();
  ASSERT_NE(pattern, nullptr) << "'default: 1' pattern value must be a Constant";
  EXPECT_EQ(pattern->getValue(), std::string_view("1"));
}

// ===========================================================================
// Independent module parameters keep their own default values (Sec 6.20.2)
// ===========================================================================

TEST_F(DefaultPatternAssignTest, BottomValueDefaultIsEight) {
  const hldb::ParamAssign *const pa = getModuleParamAssign(m_design, "bottom", "VALUE");
  ASSERT_NE(pa, nullptr) << "ParamAssign for bottom::VALUE not found";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "bottom::VALUE default must be a plain Constant, not an assignment pattern";
  EXPECT_EQ(rhs->getValue(), std::string_view("8"));
}

TEST_F(DefaultPatternAssignTest, UpperValueDefaultIsSeven) {
  const hldb::ParamAssign *const pa = getModuleParamAssign(m_design, "upper", "VALUE");
  ASSERT_NE(pa, nullptr) << "ParamAssign for upper::VALUE not found";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), std::string_view("7"));
}

TEST_F(DefaultPatternAssignTest, TopValueDefaultIsFive) {
  const hldb::ParamAssign *const pa = getModuleParamAssign(m_design, "top", "VALUE");
  ASSERT_NE(pa, nullptr) << "ParamAssign for top::VALUE not found";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), std::string_view("5"));
}

// ===========================================================================
// lower::VALUE = VALUE_TEMP + 2  -- references the package parameter derived
// from the '{default: 1} pattern
// ===========================================================================

TEST_F(DefaultPatternAssignTest, LowerValueRhsIsAddOp) {
  const hldb::ParamAssign *const pa = getModuleParamAssign(m_design, "lower", "VALUE");
  ASSERT_NE(pa, nullptr) << "ParamAssign for lower::VALUE not found";
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'VALUE_TEMP + 2' RHS must be an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiAddOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  EXPECT_EQ(rhs->getOperands()->size(), 2u);
}

TEST_F(DefaultPatternAssignTest, LowerValueFirstOperandReferencesValueTemp) {
  const hldb::ParamAssign *const pa = getModuleParamAssign(m_design, "lower", "VALUE");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);

  const hldb::RefObj *const lhsRef = any_cast<hldb::RefObj>((*rhs->getOperands())[0]);
  ASSERT_NE(lhsRef, nullptr) << "first operand of 'VALUE_TEMP + 2' must be a RefObj";
  EXPECT_EQ(lhsRef->getName(), std::string_view("VALUE_TEMP"));
}

TEST_F(DefaultPatternAssignTest, LowerValueSecondOperandIsConstantTwo) {
  const hldb::ParamAssign *const pa = getModuleParamAssign(m_design, "lower", "VALUE");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const rhs = pa->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);

  const hldb::Constant *const c = any_cast<hldb::Constant>((*rhs->getOperands())[1]);
  ASSERT_NE(c, nullptr) << "second operand of 'VALUE_TEMP + 2' must be a Constant";
  EXPECT_EQ(c->getValue(), std::string_view("2"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
