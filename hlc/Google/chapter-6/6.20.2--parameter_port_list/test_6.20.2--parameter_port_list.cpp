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

// Spec-based validation of IEEE 1800-2017 ss.6.20.2 parameter port list.
// SV: tests/Google/chapter-6/6.20.2--parameter_port_list.sv
//
//   module top #(p = 12);
//   endmodule
//
// -- ss.6.20.2 rules under test -----------------------------------------------
//
// A module may declare parameters in its parameter port list using #(...):
//   * The parameter port list is the primary mechanism for allowing callers to
//     override parameter values at instantiation (ss.23.2.1.4).
//   * Each entry is a parameter declaration with an optional default value.
//   * 'p = 12' declares parameter 'p' with default constant_expression 12
//     (ss.11.2.1).
//   * Parameters declared in the port list are NOT localparams; they are
//     overridable at instantiation (ss.6.20.2).
//   * UHDM stores the parameter in the module's parameter collection and its
//     default value in a ParamAssign node, exactly as for body-declared
//     parameters.
//
// -- UHDM tree ----------------------------------------------------------------
//
//   Module name:work@top
//   +-- getParameters() (AnyCollection, 1 item)
//   |   +-- [0] Parameter name:"p"  localParam: false
//   +-- getParamAssigns() (ParamAssignCollection, 1 item)
//       +-- [0] ParamAssign
//               lhs: Parameter name:"p"
//               rhs: Constant { decompile: "12" }
//
// NOTE: Surelog stores parameter default expressions via ParamAssign nodes
// (Scope::getParamAssigns()), not directly in Parameter::getExpr(). Access
// the default value by finding the ParamAssign by LHS name and calling
// getRhs<T>().
//
// -- VPI constants ------------------------------------------------------------
//   vpiUIntConst = 9    (unsigned int constant, vpi_user.h)

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

class ParameterPortListTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.20.2--parameter_port_list.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
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

// ===========================================================================
// Module
// ===========================================================================

TEST_F(ParameterPortListTest, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found"; }

// ===========================================================================
// Parameter collection
// ===========================================================================

TEST_F(ParameterPortListTest, ParameterCollectionExists) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getParameters(), nullptr) << "module 'top' must have a parameter collection";
}

// ss.23.2.1.4: the parameter port list '#(p = 12)' declares exactly one
// parameter on this module.
TEST_F(ParameterPortListTest, ParameterCount) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 1u) << "module 'top' declares exactly one parameter: p";
}

// ===========================================================================
// Parameter p = 12
// ===========================================================================

TEST_F(ParameterPortListTest, P_Exists) {
  EXPECT_NE(getParam(m_design, "p"), nullptr) << "'p' not found in parameters";
}

// ss.6.20.2: parameters declared in a parameter port list are overridable and
// must NOT be treated as localparams.
TEST_F(ParameterPortListTest, P_IsNotLocalParam) {
  const hldb::Parameter *p = getParam(m_design, "p");
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam()) << "ss.6.20.2: '#(p = 12)' declares a parameter, not a localparam";
}

// ss.6.20.2: the default value '12' is a constant_expression (ss.11.2.1)
// stored as a Constant in the parameter's ParamAssign RHS.
TEST_F(ParameterPortListTest, P_RhsIsConstant) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p' not found";
  EXPECT_NE(pa->getRhs<hldb::Constant>(), nullptr) << "'p = 12': RHS must be a Constant";
}

TEST_F(ParameterPortListTest, P_RhsDecompile) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p' not found";
  const hldb::Constant *c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "'p = 12': RHS must be a Constant";
  EXPECT_EQ(std::string(c->getDecompile()), "12") << "'p = 12': decompile must be \"12\"";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
