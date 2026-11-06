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

// Spec-based validation of IEEE 1800-2017 ss.6.20.2 parameter with an
// explicitly-sized hexadecimal default value.
// SV: tests/Google/chapter-6/6.20.2--parameter_range.sv
//
//   module top();
//       parameter p = 16'h1234;
//   endmodule
//
// -- ss.6.20.2 + ss.5.7.1 rules under test ------------------------------------
//
// A parameter declaration (ss.6.20.2):
//   * Default value is a constant_expression (ss.11.2.1).
//   * The default value '16'h1234' is a sized hex integer literal (ss.5.7.1).
//
// Sized integer constants (ss.5.7.1):
//   * Format: <size>'<base><digits>
//       size  = 16   -- the bit-width of the constant; stored in getSize()
//       base  = 'h   -- hexadecimal; encoded as vpiHexConst (5) in getConstType()
//       value = 1234 -- hex digits; decimal equivalent is 4660
//   * The size specifier '16' sets the bit-width explicitly.  This is what
//     the description calls the "implied range": the parameter has a known
//     16-bit vector range [15:0] derived from the size of its default value.
//   * The hex base specifier 'h' is represented in UHDM as vpiHexConst (5).
//
// -- UHDM tree ----------------------------------------------------------------
//
//   Module name:top
//   +-- getParameters() (AnyCollection, 1 item)
//       +-- [0] Parameter name:"p"  localParam: false
//               expr: Constant
//                       constType: vpiHexConst (5)
//                       size:      16
//
// NOTE: a parameter's default value is its own Parameter::getExpr(); a
// declaration creates no ParamAssign (those carry overrides only).
//
// -- VPI constants ------------------------------------------------------------
//   vpiHexConst = 5    (hexadecimal integer constant, vpi_user.h)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>

#include <string>

namespace hlc {

class ParameterRangeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.20.2--parameter_range.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("top", d->getAllModules());
}

static const hldb::Parameter *getParameter(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  return (m == nullptr) ? nullptr : hldb::findByName<hldb::Parameter>(name, m->getParameters());
}

// Returns the named parameter's default value as a Constant, or nullptr.
static const hldb::Constant *getRhsConst(const hldb::Design *d, std::string_view name) {
  const hldb::Parameter *p = getParameter(d, name);
  return (p == nullptr) ? nullptr : p->getExpr<hldb::Constant>();
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(ParameterRangeTest, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'top' not found"; }

// ===========================================================================
// Parameter collection
// ===========================================================================

TEST_F(ParameterRangeTest, ParameterCollectionExists) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getParameters(), nullptr) << "module 'top' must have a parameter collection";
}

TEST_F(ParameterRangeTest, ParameterCount) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 1u) << "module 'top' declares exactly one parameter: p";
}

// ===========================================================================
// Parameter p = 16'h1234
// ===========================================================================

TEST_F(ParameterRangeTest, P_Exists) { EXPECT_NE(getParameter(m_design, "p"), nullptr) << "'p' not found in parameters"; }

// ss.6.20.2: 'parameter' is not a localparam; it is overridable.
TEST_F(ParameterRangeTest, P_IsNotLocalParam) {
  const hldb::Parameter *p = getParameter(m_design, "p");
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam()) << "ss.6.20.2: 'parameter p' must not be marked as a localparam";
}

// ss.11.2.1: the default value must be a constant_expression, represented
// here as a Constant node in the parameter's own expression.
TEST_F(ParameterRangeTest, P_RhsIsConstant) {
  const hldb::Parameter *pa = getParameter(m_design, "p");
  ASSERT_NE(pa, nullptr) << "Parameter 'p' not found";
  EXPECT_NE(pa->getExpr<hldb::Constant>(), nullptr) << "'p = 16'h1234': RHS must be a Constant";
}

// ss.5.7.1: the hex base specifier 'h' in '16'h1234' is encoded as
// vpiHexConst (5) in the Constant's constType field.
TEST_F(ParameterRangeTest, P_Rhs_ConstType_IsHex) {
  const hldb::Constant *c = getRhsConst(m_design, "p");
  ASSERT_NE(c, nullptr) << "RHS Constant for 'p' not found";
  EXPECT_EQ(c->getConstType(), vpiHexConst) << "ss.5.7.1: '16'h1234' must have constType vpiHexConst (5)";
}

// ss.5.7.1: the size specifier '16' in '16'h1234' sets the bit-width of the
// constant to 16, which is what getSize() must return.
TEST_F(ParameterRangeTest, P_Rhs_Size_Is16) {
  const hldb::Constant *c = getRhsConst(m_design, "p");
  ASSERT_NE(c, nullptr) << "RHS Constant for 'p' not found";
  EXPECT_EQ(c->getSize(), 16) << "ss.5.7.1: '16'h1234' must have a bit-width of 16";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
