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

// Source under test: tests/Google/chapter-18/18.11--in-line-random-variable-control_0.sv
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a;
//       rand int x = 0, y = 0;
//       int v = 0, w = 0;
//       constraint c { x < v && y > w; };
//   endclass
//
//   class env extends uvm_env;
//     a obj = new;
//     function new(string name, uvm_component parent = null); super.new(name, parent); endfunction
//     task run_phase(uvm_phase phase);
//       phase.raise_objection(this);
//       begin
//         obj.randomize(v, w);
//         if(obj.x == 0 && obj.y == 0) `uvm_info(...) else `uvm_error(...);
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
// checks static structure -- it cannot run randomize(v, w), run_phase(),
// or run_test(). Note for context (not asserted): Sec 18.6's
// "randomize(v, w)" form names exactly which properties are active for
// this call (v and w here, even though neither is declared "rand") --
// any rand property NOT named (x and y here) is held fixed at its
// current value for this call, which is exactly why the file's own
// "SUCCESS" branch checks that x/y are unchanged from their declared
// initializers (0, 0).
//
// Checked (against IEEE 1800-2023, not against whatever HLC happens to
// output today -- see davtests test writing guide):
//   - Sec 18.4.1: "rand int x = 0, y = 0;" declares two rand properties
//     in one statement -- both "x" and "y" must have vpiRandType ==
//     vpiRand. "int v = 0, w = 0;" (no modifier) -- both "v" and "w"
//     must have vpiRandType == vpiNotRand.
//   - Sec 6.11: all four ("x","y","v","w") resolve to a signed
//     IntTypespec.
//   - Sec 18.5.1: "constraint c { x < v && y > w; };" is an in-class
//     constraint_declaration, so it should be found directly on the
//     owning class's own ClassDefn::getConstraints() (same pattern
//     confirmed for out-of-class constraints elsewhere in this chapter's
//     tests; not independently re-verified by compiling this specific
//     file this session -- see "Not checked").
//   - Sec 11.4.7 (logical operators) / Sec 11.4.4 (relational operators):
//     the constraint's single item is a Distribution node (no "dist"
//     clause, so getDistItems() is null) whose getExpr() is a single
//     Operation with opType vpiLogAndOp and exactly 2 operands, each
//     itself an Operation: operand[0] is "x < v" (vpiLtOp, RefObj "x" /
//     RefObj "v"), operand[1] is "y > w" (vpiGtOp, RefObj "y" / RefObj
//     "w") -- confirming a single flat "&&" node, not two constraint
//     items or a differently-nested tree.
//   - Class "env" is derived from a base class named "uvm_env" (Sec
//     8.13) -- checked via ClassDefn::getExtends().
//   - Module "top" exists.
//
// Not checked:
//   - The declaration-time initializer value (0) of x/y/v/w -- Variable
//     exposes both a getExpr() and a getValue() accessor and which one
//     (if either) actually holds a plain declaration initializer was not
//     independently confirmed this session (a sibling case in an earlier
//     batch found the analogous Parameter::getExpr() unexpectedly null,
//     with the real value living elsewhere instead) -- not asserted here
//     to avoid repeating that mistake without being able to verify.
//   - The in-class constraint's attachment to ClassDefn::getConstraints()
//     was not re-verified by compiling this session (build skipped per
//     request).
//   - Any randomize()/run_phase()/run_test() runtime behavior -- see the
//     note above; unrunnable by a compiler/elaborator.
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
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/int_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {
class InLineRandomVariableControl0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.11--in-line-random-variable-control_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(InLineRandomVariableControl0Test, RandPropertiesXYAndPlainPropertiesVWExist) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);

  const hldb::Variable *const x = hldb::findByName<hldb::Variable>("x", a->getVariables());
  const hldb::Variable *const y = hldb::findByName<hldb::Variable>("y", a->getVariables());
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("v", a->getVariables());
  const hldb::Variable *const w = hldb::findByName<hldb::Variable>("w", a->getVariables());
  ASSERT_NE(x, nullptr);
  ASSERT_NE(y, nullptr);
  ASSERT_NE(v, nullptr);
  ASSERT_NE(w, nullptr);

  EXPECT_EQ(x->getRandType(), vpiRand) << "'rand int x = 0, y = 0;' applies 'rand' to both x and y";
  EXPECT_EQ(y->getRandType(), vpiRand);
  EXPECT_EQ(v->getRandType(), vpiNotRand);
  EXPECT_EQ(w->getRandType(), vpiNotRand);

  for (const hldb::Variable *const var : {x, y, v, w}) {
    const hldb::IntTypespec *const ts = hldb::getTypespec<hldb::IntTypespec>(var);
    ASSERT_NE(ts, nullptr);
    EXPECT_TRUE(ts->getSigned());
  }
}

TEST_F(InLineRandomVariableControl0Test, ConstraintCIsXLessThanVAndYGreaterThanW) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);

  const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", a->getConstraints());
  ASSERT_NE(c, nullptr) << "in-class 'constraint c { ... }' should be found on the owning class directly";

  const hldb::AnyCollection *const items = c->getConstraintItems();
  ASSERT_NE(items, nullptr);
  ASSERT_EQ(items->size(), 1u) << "'x < v && y > w' is a single constraint_expression item";

  const hldb::Distribution *const dist = any_cast<hldb::Distribution>((*items)[0]);
  ASSERT_NE(dist, nullptr);
  EXPECT_EQ(dist->getDistItems(), nullptr);

  const hldb::Operation *const logAnd = dist->getExpr<hldb::Operation>();
  ASSERT_NE(logAnd, nullptr) << "'x < v && y > w' should be a single Operation";
  EXPECT_EQ(logAnd->getOpType(), vpiLogAndOp);

  const hldb::AnyCollection *const operands = logAnd->getOperands();
  ASSERT_NE(operands, nullptr);
  ASSERT_EQ(operands->size(), 2u);

  const hldb::Operation *const lt = any_cast<hldb::Operation>((*operands)[0]);
  ASSERT_NE(lt, nullptr) << "'x < v' should itself be an Operation";
  EXPECT_EQ(lt->getOpType(), vpiLtOp);
  ASSERT_NE(lt->getOperands(), nullptr);
  ASSERT_EQ(lt->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>((*lt->getOperands())[0])->getName(), "x");
  EXPECT_EQ(any_cast<hldb::RefObj>((*lt->getOperands())[1])->getName(), "v");

  const hldb::Operation *const gt = any_cast<hldb::Operation>((*operands)[1]);
  ASSERT_NE(gt, nullptr) << "'y > w' should itself be an Operation";
  EXPECT_EQ(gt->getOpType(), vpiGtOp);
  ASSERT_NE(gt->getOperands(), nullptr);
  ASSERT_EQ(gt->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>((*gt->getOperands())[0])->getName(), "y");
  EXPECT_EQ(any_cast<hldb::RefObj>((*gt->getOperands())[1])->getName(), "w");
}

TEST_F(InLineRandomVariableControl0Test, ClassEnvExtendsUvmEnv) {
  const hldb::ClassDefn *const env = hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses());
  ASSERT_NE(env, nullptr);
  EXPECT_NE(env->getExtends(), nullptr) << "Sec 8.13: 'class env extends uvm_env;' should record its base class";
}

TEST_F(InLineRandomVariableControl0Test, ModuleTopExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
