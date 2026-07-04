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

// Spec-based validation of IEEE 1800-2017 ss.6.20.4 local parameter.
// SV: tests/Google/chapter-6/6.20.4--localparam.sv
//
//   module top();
//       localparam p = 123;
//   endmodule
//
// -- ss.6.20.4 rules under test -----------------------------------------------
//
// A local parameter (ss.6.20.4):
//   * Declared with the 'localparam' keyword in the module body.
//   * A localparam is a constant whose value CANNOT be changed at instantiation.
//     This is the defining distinction from 'parameter' (ss.6.20.2):
//     a localparam is NOT overridable.
//   * 'localparam p = 123' declares a local constant p with value 123.
//   * UHDM represents a localparam as a Parameter node with
//     vpiLocalParam == true (getLocalParam() returns true).
//   * The default value is a constant_expression (ss.11.2.1), accessed through
//     the module's ParamAssign collection, not directly via Parameter::getExpr().
//
// -- UHDM tree ----------------------------------------------------------------
//
//   Module name:work@top
//   +-- getParameters() (AnyCollection, 1 item)
//   |   +-- [0] Parameter name:"p"  localParam: true
//   +-- getParamAssigns() (ParamAssignCollection, 1 item)
//       +-- [0] ParamAssign
//               lhs: RefObj name:"p"  actual: Parameter name:"p"
//               rhs: Constant { decompile: "123" }
//
// NOTE: Both 'parameter' and 'localparam' appear as Parameter nodes in
// getParameters(). The only API difference is getLocalParam(): false for
// parameter, true for localparam. The default value is stored in the
// ParamAssign RHS -- Parameter::getExpr() is NOT populated by Surelog.

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

class LocalparamTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.20.4--localparam.hlc"}); }
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

static const hldb::ParamAssign *getParamAssign(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m) return nullptr;
  return hldb::findByName<hldb::ParamAssign>(name, m->getParamAssigns());
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(LocalparamTest, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found"; }

// ===========================================================================
// Parameter collection
// ===========================================================================

TEST_F(LocalparamTest, ParameterCollectionExists) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getParameters(), nullptr) << "module 'top' must have a parameter collection";
}

// ss.6.20.4: 'localparam p = 123' declares exactly one local parameter.
TEST_F(LocalparamTest, ParameterCount) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 1u) << "module 'top' declares exactly one localparam: p";
}

// ===========================================================================
// localparam p = 123
// ===========================================================================

// ss.6.20.4: 'localparam' must produce a Parameter node named "p".
TEST_F(LocalparamTest, P_Exists) { EXPECT_NE(getParam(m_design, "p"), nullptr) << "'p' not found in parameters"; }

// ss.6.20.4: THE defining rule -- a localparam is NOT overridable at
// instantiation. UHDM encodes this as vpiLocalParam == true.
TEST_F(LocalparamTest, P_IsLocalParam) {
  const hldb::Parameter *p = getParam(m_design, "p");
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getLocalParam()) << "ss.6.20.4: 'localparam p' must be marked as a localparam "
                                     "(not overridable at instantiation)";
}

// ss.6.20.4: the value '123' is a constant_expression (ss.11.2.1).
// It is stored in the ParamAssign RHS, not in Parameter::getExpr().
TEST_F(LocalparamTest, P_ParamAssignExists) {
  EXPECT_NE(getParamAssign(m_design, "p"), nullptr) << "ParamAssign for 'p' not found";
}

// ss.11.2.1: the default value must be a constant_expression; here it is
// the integer literal 123, represented as a Constant node.
TEST_F(LocalparamTest, P_RhsIsConstant) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p' not found";
  EXPECT_NE(pa->getRhs<hldb::Constant>(), nullptr) << "'localparam p = 123': RHS must be a Constant";
}

// ss.6.20.4: the declared value is 123; the Constant must decompile to "123".
TEST_F(LocalparamTest, P_RhsDecompile) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p' not found";
  const hldb::Constant *c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "'localparam p = 123': RHS must be a Constant";
  EXPECT_EQ(std::string(c->getDecompile()), "123") << "'localparam p = 123': decompile must be \"123\"";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
