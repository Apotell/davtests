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

// Spec-based validation of IEEE 1800-2017 ss.6.20.5 specparam declaration.
// SV: tests/Google/chapter-6/6.20.5--specparam.sv
//
//   module top();
//       specparam delay = 50;
//   endmodule
//
// -- ss.6.20.5 rules under test -----------------------------------------------
//
// Specparam (ss.6.20.5):
//   * Declared inside a module (or specify block) with the 'specparam' keyword.
//   * Used to specify timing and delay values; not overridable at instantiation.
//   * A specparam is NOT a module parameter or localparam. It is a distinct
//     node type: UHDM represents it as a SpecParam, found via
//     Module::getSpecParams(), NOT in Module::getParameters().
//   * The value '50' is a constant_expression (ss.11.2.1) stored directly in
//     SpecParam::getExprs() as a Constant node. There is no ParamAssign
//     indirection for specparams.
//
// -- UHDM tree ----------------------------------------------------------------
//
//   Module name:work@top
//   +-- getParameters()   -- NULL (no parameters declared)
//   +-- getSpecParams() (SpecParamCollection, 1 item)
//       +-- [0] SpecParam name:"delay"
//               getExprs() (ExprCollection, 1 item):
//                 +-- [0] Constant { vpiDecompile: "50" }

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/spec_param.h>

#include <string>

namespace hlc {

class SpecparamTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.20.5--specparam.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
}

static const hldb::SpecParam *getSpecParam(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getSpecParams()) return nullptr;
  return hldb::findByName<hldb::SpecParam>(name, m->getSpecParams());
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(SpecparamTest, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found"; }

// ===========================================================================
// SpecParam collection
// ===========================================================================

// ss.6.20.5: 'specparam' must populate the module's specparam collection,
// not the parameter collection.
TEST_F(SpecparamTest, SpecParamCollectionExists) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getSpecParams(), nullptr) << "ss.6.20.5: 'specparam delay' must produce a non-null specparam collection";
}

// ss.6.20.5: exactly one specparam is declared.
TEST_F(SpecparamTest, SpecParamCount) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getSpecParams(), nullptr);
  EXPECT_EQ(m->getSpecParams()->size(), 1u) << "module 'top' declares exactly one specparam: delay";
}

// ss.6.20.5: a specparam is NOT a module parameter; the parameter collection
// must be null (no parameters or localparams are declared).
TEST_F(SpecparamTest, ParameterCollection_IsNull) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getParameters(), nullptr) << "ss.6.20.5: 'specparam' must NOT appear in getParameters()";
}

// ===========================================================================
// specparam delay = 50
// ===========================================================================

// ss.6.20.5: 'specparam delay' must produce a SpecParam node named "delay".
TEST_F(SpecparamTest, Delay_Exists) {
  EXPECT_NE(getSpecParam(m_design, "delay"), nullptr) << "'delay' not found in specparam collection";
}

// ss.6.20.5 + ss.11.2.1: the value '50' is a constant_expression stored in
// SpecParam::getExprs(). The collection must be non-null and non-empty.
// NOTE: unlike parameters/localparams, there is no ParamAssign indirection
// for specparams -- the value lives directly in getExprs().
TEST_F(SpecparamTest, Delay_ExprCollectionExists) {
  const hldb::SpecParam *sp = getSpecParam(m_design, "delay");
  ASSERT_NE(sp, nullptr);
  const hldb::ExprCollection *exprs = sp->getExprs();
  ASSERT_NE(exprs, nullptr) << "'specparam delay = 50' must have a non-null expr collection";
  EXPECT_FALSE(exprs->empty()) << "ss.6.20.5: 'specparam delay = 50' must have at least one value expression";
}

// ss.11.2.1: the value '50' is a constant_expression; it must be represented
// as a Constant node in the expr collection.
TEST_F(SpecparamTest, Delay_Expr_IsConstant) {
  const hldb::SpecParam *sp = getSpecParam(m_design, "delay");
  ASSERT_NE(sp, nullptr);
  const hldb::ExprCollection *exprs = sp->getExprs();
  ASSERT_NE(exprs, nullptr);
  ASSERT_FALSE(exprs->empty());
  EXPECT_NE(any_cast<hldb::Constant>((*exprs)[0]), nullptr) << "ss.11.2.1: '50' must be represented as a Constant node";
}

// ss.6.20.5: the declared value is 50; the Constant must decompile to "50".
TEST_F(SpecparamTest, Delay_Expr_Decompile_Is50) {
  const hldb::SpecParam *sp = getSpecParam(m_design, "delay");
  ASSERT_NE(sp, nullptr);
  const hldb::ExprCollection *exprs = sp->getExprs();
  ASSERT_NE(exprs, nullptr);
  ASSERT_FALSE(exprs->empty());
  const hldb::Constant *c = any_cast<hldb::Constant>((*exprs)[0]);
  ASSERT_NE(c, nullptr) << "'50' must be a Constant node";
  EXPECT_EQ(std::string(c->getDecompile()), "50") << "'specparam delay = 50': decompile must be \"50\"";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
