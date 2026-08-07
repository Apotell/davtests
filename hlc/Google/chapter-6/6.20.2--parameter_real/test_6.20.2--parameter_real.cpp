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

// Spec-based validation of IEEE 1800-2017 ss.6.20.2 parameter with a real
// constant default value.
// SV: tests/Google/chapter-6/6.20.2--parameter_real.sv
//
//   module top();
//       parameter p = 4.76;
//   endmodule
//
// -- ss.6.20.2 + ss.5.7.2 rules under test ------------------------------------
//
// A parameter declaration (ss.6.20.2):
//   * Default value is a constant_expression (ss.11.2.1).
//   * The default value '4.76' is a real number literal (ss.5.7.2).
//   * When no explicit type is given and the default is a real constant, the
//     parameter implicitly takes type 'real' (ss.6.20.2).
//
// Real number literals (ss.5.7.2):
//   * A real constant requires a decimal point with digits on both sides.
//   * '4.76' is a fixed-point real literal.
//   * In UHDM the constant is tagged with constType vpiRealConst (2).
//   * Real constants follow IEEE 754 double-precision format (64-bit).
//     The size field on the Constant node is not set by the source literal
//     (unlike sized integer constants), so it is not tested here.
//
// -- UHDM tree ----------------------------------------------------------------
//
//   Module name:top
//   +-- getParameters() (AnyCollection, 1 item)
//   |   +-- [0] Parameter name:"p"  localParam: false
//   +-- getParamAssigns() (ParamAssignCollection, 1 item)
//       +-- [0] ParamAssign
//               lhs: Parameter name:"p"
//               rhs: Constant { constType: vpiRealConst (2) }
//
// NOTE: Surelog stores parameter default expressions via ParamAssign nodes
// (Scope::getParamAssigns()), not directly in Parameter::getExpr(). Access
// the default value by finding the ParamAssign by LHS name and calling
// getRhs<T>().
//
// -- VPI constants ------------------------------------------------------------
//   vpiRealConst = 2    (real constant, vpi_user.h)

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

class ParameterRealTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.20.2--parameter_real.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("top", d->getAllModules());
}

static const hldb::Parameter *getParam(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getParameters()) return nullptr;
  return hldb::findByName<hldb::Parameter>(name, m->getParameters());
}

// Returns the ParamAssign for the named parameter (matched via getLhs name).
static const hldb::ParamAssign *getParamAssign(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m) return nullptr;
  return hldb::findByName<hldb::ParamAssign>(name, m->getParamAssigns());
}

// Returns the RHS Constant of the named parameter's ParamAssign, or nullptr.
static const hldb::Constant *getRhsConst(const hldb::Design *d, std::string_view name) {
  const hldb::ParamAssign *pa = getParamAssign(d, name);
  if (!pa) return nullptr;
  return pa->getRhs<hldb::Constant>();
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(ParameterRealTest, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'top' not found"; }

// ===========================================================================
// Parameter collection
// ===========================================================================

TEST_F(ParameterRealTest, ParameterCollectionExists) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getParameters(), nullptr) << "module 'top' must have a parameter collection";
}

TEST_F(ParameterRealTest, ParameterCount) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 1u) << "module 'top' declares exactly one parameter: p";
}

// ===========================================================================
// Parameter p = 4.76
// ===========================================================================

TEST_F(ParameterRealTest, P_Exists) { EXPECT_NE(getParam(m_design, "p"), nullptr) << "'p' not found in parameters"; }

// ss.6.20.2: 'parameter' is not a localparam; it is overridable.
TEST_F(ParameterRealTest, P_IsNotLocalParam) {
  const hldb::Parameter *p = getParam(m_design, "p");
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam()) << "ss.6.20.2: 'parameter p' must not be marked as a localparam";
}

// ss.11.2.1: the default value must be a constant_expression, represented
// as a Constant node in the parameter's ParamAssign RHS.
TEST_F(ParameterRealTest, P_RhsIsConstant) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p' not found";
  EXPECT_NE(pa->getRhs<hldb::Constant>(), nullptr) << "'p = 4.76': RHS must be a Constant";
}

// ss.5.7.2: '4.76' is a real number literal; UHDM encodes this as
// vpiRealConst (2) in the Constant's constType field.
TEST_F(ParameterRealTest, P_Rhs_ConstType_IsReal) {
  const hldb::Constant *c = getRhsConst(m_design, "p");
  ASSERT_NE(c, nullptr) << "RHS Constant for 'p' not found";
  EXPECT_EQ(c->getConstType(), vpiRealConst) << "ss.5.7.2: '4.76' must have constType vpiRealConst (2)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
