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

// Source under test: tests/Google/chapter-18/18.10--dynamic-constraint-modification_0.sv
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a;
//       rand int b;
//       constraint c { b dist { 3 := 0, 10 := 1}; }
//   endclass
//
//   class env extends uvm_env;
//     a obj = new;
//     function new(string name, uvm_component parent = null); super.new(name, parent); endfunction
//     task run_phase(uvm_phase phase);
//       phase.raise_objection(this);
//       begin
//         obj.randomize() with { b dist { 3 := 1, 10 := 0}; };
//         if(obj.b == 3) `uvm_info(...) else `uvm_error(...);
//       end
//       phase.drop_objection(this);
//     endtask: run_phase
//   endclass
//
//   module top;
//     env environment;
//     initial begin environment = new("env"); run_test(); end
//   endmodule
//
// This tool is a compiler/elaborator, not a simulator, so this test only
// checks static structure -- it cannot run randomize(), run_phase(), or
// run_test().
//
// Checked (against IEEE 1800-2023, not against whatever HLC happens to
// output today -- see davtests test writing guide):
//   - Sec 18.4.1: "rand int b;" is a class property with vpiRandType ==
//     vpiRand (same shape already proven for the sibling
//     18.4.1--rand-modifier.sv; not re-derived field-by-field here).
//   - Sec 18.5.1/18.5.4: "constraint c { b dist {...}; }" is an in-class
//     constraint_declaration (a body is given directly, not via "extern"
//     + an out-of-class definition), so it should be found directly in
//     the owning class's own ClassDefn::getConstraints() -- unlike the
//     out-of-class-defined constraints in the 18.5.1--*-external-
//     constraint_0.sv tests, which were confirmed to register at
//     Design::getConstraints() with a getPrefix() back-reference instead
//     because their body lives outside any class scope in the source.
//     This in-class-attachment expectation follows that same general
//     lexical-scope pattern but was not independently re-confirmed by
//     compiling this specific file this session (build was skipped per
//     request -- see "Not checked").
//   - Sec 18.5.4 ("Distribution"): "b dist { 3 := 0, 10 := 1}" is a real
//     dist constraint, so its single constraint_expression item is a
//     Distribution node whose getDistItems() is populated with exactly 2
//     DistItem entries (unlike the plain-equality constraints checked in
//     the 18.5.1--*-external-constraint_0.sv tests, where getDistItems()
//     was confirmed null/empty): DistItem 0 has valueRange Constant "3"
//     and weight Constant "0"; DistItem 1 has valueRange Constant "10"
//     and weight Constant "1". Distribution::getExpr() is a RefObj named
//     "b" (the dist expression's own left-hand side).
//   - Class "env" is derived from a base class named "uvm_env" (Sec 8.13,
//     "extends") -- checked via ClassDefn::getExtends().
//   - Module "top" exists.
//
// Not checked:
//   - The exact VPI constant distinguishing ":=" from ":/" dist-weight
//     syntax (DistItem::getDistType()) -- not independently confirmed
//     this session (no build was run), so not asserted to avoid guessing
//     a specific enumerant value.
//   - The class-level attachment of the in-class constraint (see above)
//     was not re-verified by compiling; it follows the same pattern
//     already confirmed for a different (out-of-class) case.
//   - The in-line "randomize() with { b dist {...}; }" call-site
//     constraint block, and any randomize()/run_phase()/run_test()
//     runtime behavior -- unrunnable by a compiler/elaborator, and its
//     exact object shape (e.g. whether it attaches to a MethodFuncCall
//     argument or elsewhere) was not investigated.
//   - vpiFullName on any object (never asserted per the test writing
//     guide; use getName() only).

#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/class_defn.h>
#include <hldb/variable.h>
#include <hldb/extends.h>
#include <hldb/constraint.h>
#include <hldb/distribution.h>
#include <hldb/dist_item.h>
#include <hldb/ref_obj.h>
#include <hldb/constant.h>
#include <hldb/vpi_user.h>

namespace hlc {
class DynamicConstraintModification0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.10--dynamic-constraint-modification_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(DynamicConstraintModification0Test, ClassAWithRandPropertyExists) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", a->getVariables());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getRandType(), vpiRand);
}

TEST_F(DynamicConstraintModification0Test, InClassConstraintHasDistWithTwoWeightedItems) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);

  const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", a->getConstraints());
  ASSERT_NE(c, nullptr) << "in-class 'constraint c { ... }' should be found on the owning class directly";

  const hldb::AnyCollection *const items = c->getConstraintItems();
  ASSERT_NE(items, nullptr);
  ASSERT_EQ(items->size(), 1u);

  const hldb::Distribution *const dist = any_cast<hldb::Distribution>((*items)[0]);
  ASSERT_NE(dist, nullptr);
  const hldb::RefObj *const expr = dist->getExpr<hldb::RefObj>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getName(), "b");

  const hldb::DistItemCollection *const distItems = dist->getDistItems();
  ASSERT_NE(distItems, nullptr) << "Sec 18.5.4: a real 'dist {...}' clause must populate DistItems, unlike a "
                                    "plain relational constraint expression";
  ASSERT_EQ(distItems->size(), 2u);

  const hldb::Constant *const value0 = any_cast<hldb::Constant>((*distItems)[0]->getValueRange());
  const hldb::Constant *const weight0 = (*distItems)[0]->getWeight<hldb::Constant>();
  ASSERT_NE(value0, nullptr);
  ASSERT_NE(weight0, nullptr);
  EXPECT_EQ(value0->getDecompile(), "3");
  EXPECT_EQ(weight0->getDecompile(), "0");

  const hldb::Constant *const value1 = any_cast<hldb::Constant>((*distItems)[1]->getValueRange());
  const hldb::Constant *const weight1 = (*distItems)[1]->getWeight<hldb::Constant>();
  ASSERT_NE(value1, nullptr);
  ASSERT_NE(weight1, nullptr);
  EXPECT_EQ(value1->getDecompile(), "10");
  EXPECT_EQ(weight1->getDecompile(), "1");
}

TEST_F(DynamicConstraintModification0Test, ClassEnvExtendsUvmEnv) {
  const hldb::ClassDefn *const env = hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses());
  ASSERT_NE(env, nullptr);
  EXPECT_NE(env->getExtends(), nullptr) << "Sec 8.13: 'class env extends uvm_env;' should record its base class";
}

TEST_F(DynamicConstraintModification0Test, ModuleTopExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
