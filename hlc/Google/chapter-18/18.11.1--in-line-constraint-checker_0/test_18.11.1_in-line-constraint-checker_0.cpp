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

// Source under test: tests/Google/chapter-18/18.11.1--in-line-constraint-checker_0.sv
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a;
//       rand int x;
//       int v;
//       constraint c1 { x < v; };
//   endclass
//
//   class env extends uvm_env;
//     a obj = new;
//     int ret;
//     function new(string name, uvm_component parent = null); super.new(name, parent); endfunction
//     task run_phase(uvm_phase phase);
//       phase.raise_objection(this);
//       begin
//         obj.x = 2;
//         obj.v = 1;
//         ret = obj.randomize(null);
//         if(ret == 0 && obj.x == 2 && obj.v == 1) `uvm_info(...) else `uvm_error(...);
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
// checks static structure -- it cannot run randomize(null), run_phase(),
// or run_test(). Note for context (not asserted): Sec 18.6's
// "randomize(null)" form randomizes NO variables at all (an explicit
// empty active-variable list), so this call is a pure constraint-check
// against x/v's current values (2 and 1) -- since "x < v" (2 < 1) is
// false, spec-correct behavior is randomize() returning 0, which is
// exactly the file's own "SUCCESS" branch condition ("ret == 0 &&
// obj.x==2 && obj.v==1"). This is useful context for why the file is
// shaped this way, but is a runtime fact this tool cannot execute.
//
// Checked (against IEEE 1800-2023, not against whatever HLC happens to
// output today -- see davtests test writing guide):
//   - Sec 18.4.1: "rand int x;" has vpiRandType == vpiRand; "int v;" (no
//     rand/randc modifier) has vpiRandType == vpiNotRand.
//   - Sec 6.11: both "x" and "v" resolve to a signed IntTypespec.
//   - Sec 18.5.1: "constraint c1 { x < v; };" is an in-class
//     constraint_declaration (body given directly, no "extern"), so it
//     should be found directly on the owning class's own
//     ClassDefn::getConstraints() (following the same lexical-scope
//     pattern confirmed for out-of-class constraints in the
//     18.5.1--*-external-constraint_0.sv tests; not independently
//     re-verified by compiling this specific file this session -- see
//     "Not checked").
//   - Sec 11.4.4 (relational operators) / Sec 18.5.4: the constraint's
//     single item is a Distribution node (matching the "expression_or_
//     dist" grammar; no "dist" clause here, so getDistItems() is null)
//     whose getExpr() is an Operation with opType vpiLtOp and 2 operands
//     -- RefObj "x" and RefObj "v" (not a literal on either side, unlike
//     the "b == 0"/"b == 5" cases already proven in the 18.5.1--*
//     tests).
//   - Class "env" is derived from a base class named "uvm_env" (Sec
//     8.13) -- checked via ClassDefn::getExtends().
//   - Module "top" exists.
//
// Not checked:
//   - The in-class constraint's attachment to ClassDefn::getConstraints()
//     was not re-verified by compiling this session (build skipped per
//     request); it follows the same pattern already confirmed for a
//     different (out-of-class) case.
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
class InLineConstraintChecker0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.11.1--in-line-constraint-checker_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(InLineConstraintChecker0Test, ClassAWithRandXAndPlainVExist) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);

  const hldb::Variable *const x = hldb::findByName<hldb::Variable>("x", a->getVariables());
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("v", a->getVariables());
  ASSERT_NE(x, nullptr);
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(x->getRandType(), vpiRand);
  EXPECT_EQ(v->getRandType(), vpiNotRand) << "'int v;' has no rand/randc modifier";

  const hldb::IntTypespec *const xTs = hldb::getTypespec<hldb::IntTypespec>(x);
  const hldb::IntTypespec *const vTs = hldb::getTypespec<hldb::IntTypespec>(v);
  ASSERT_NE(xTs, nullptr);
  ASSERT_NE(vTs, nullptr);
  EXPECT_TRUE(xTs->getSigned());
  EXPECT_TRUE(vTs->getSigned());
}

TEST_F(InLineConstraintChecker0Test, ConstraintC1IsXLessThanV) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);

  const hldb::Constraint *const c1 = hldb::findByName<hldb::Constraint>("c1", a->getConstraints());
  ASSERT_NE(c1, nullptr) << "in-class 'constraint c1 { ... }' should be found on the owning class directly";

  const hldb::AnyCollection *const items = c1->getConstraintItems();
  ASSERT_NE(items, nullptr);
  ASSERT_EQ(items->size(), 1u);

  const hldb::Operation *const lt = any_cast<hldb::Operation>((*items)[0]);
  ASSERT_NE(lt, nullptr) << "'x < v' should be an Operation";
  EXPECT_EQ(lt->getOpType(), vpiLtOp);

  const hldb::AnyCollection *const operands = lt->getOperands();
  ASSERT_NE(operands, nullptr);
  ASSERT_EQ(operands->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>((*operands)[0]);
  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>((*operands)[1]);
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(lhs->getName(), "x");
  EXPECT_EQ(rhs->getName(), "v");
}

TEST_F(InLineConstraintChecker0Test, ClassEnvExtendsUvmEnv) {
  const hldb::ClassDefn *const env = hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses());
  ASSERT_NE(env, nullptr);
  EXPECT_NE(env->getExtends(), nullptr) << "Sec 8.13: 'class env extends uvm_env;' should record its base class";
}

TEST_F(InLineConstraintChecker0Test, ModuleTopExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
