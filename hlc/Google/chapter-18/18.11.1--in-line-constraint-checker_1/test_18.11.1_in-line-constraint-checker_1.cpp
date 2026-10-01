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

// Source under test: tests/Google/chapter-18/18.11.1--in-line-constraint-checker_1.sv
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a;
//       randc bit [7:0] x;
//       bit [7:0] v;
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
// Identical in shape to the sibling 18.11.1--in-line-constraint-checker_0.sv
// except "x" is "randc" (not "rand") and both "x" and "v" are declared
// "bit [7:0]" (2-state, unsigned, 8-bit vector) instead of "int" (2-state,
// signed, 32-bit scalar). This tool is a compiler/elaborator, not a
// simulator, so this test only checks static structure.
//
// Checked (against IEEE 1800-2023, not against whatever HLC happens to
// output today -- see davtests test writing guide):
//   - Sec 18.4.1: "randc bit [7:0] x;" has vpiRandType == vpiRandC; "bit
//     [7:0] v;" (no rand/randc modifier) has vpiRandType == vpiNotRand.
//   - Sec 6.11 ('bit' is 2-state) / Sec 6.9.1 (packed vector): both "x"
//     and "v" resolve to a BitTypespec, unsigned (no "signed" keyword),
//     with exactly one range ([7:0]).
//   - Sec 18.5.1: "constraint c1 { x < v; };" is an in-class
//     constraint_declaration, so it should be found directly on the
//     owning class's own ClassDefn::getConstraints() (same pattern as
//     the sibling "_0" file; not independently re-verified by compiling
//     this specific file this session -- see "Not checked").
//   - Sec 11.4.4 (relational operators) / Sec 18.5.4: the constraint's
//     single item is a Distribution node (no "dist" clause, so
//     getDistItems() is null) whose getExpr() is an Operation with
//     opType vpiLtOp and 2 operands -- RefObj "x" and RefObj "v".
//   - Class "env" is derived from a base class named "uvm_env" (Sec
//     8.13) -- checked via ClassDefn::getExtends().
//   - Module "top" exists.
//
// Not checked:
//   - The in-class constraint's attachment to ClassDefn::getConstraints()
//     was not re-verified by compiling this session (build skipped per
//     request); it follows the same pattern already confirmed for a
//     different (out-of-class) case.
//   - Any randc cyclic (permutation) randomization behavior, or any
//     randomize()/run_phase()/run_test() runtime behavior -- unrunnable
//     by a compiler/elaborator.
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
#include <hldb/bit_typespec.h>
#include <hldb/range.h>
#include <hldb/vpi_user.h>

namespace hlc {
class InLineConstraintChecker1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.11.1--in-line-constraint-checker_1.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(InLineConstraintChecker1Test, ClassAWithRandCXAndPlainVExist) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);

  const hldb::Variable *const x = hldb::findByName<hldb::Variable>("x", a->getVariables());
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("v", a->getVariables());
  ASSERT_NE(x, nullptr);
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(x->getRandType(), vpiRandC);
  EXPECT_EQ(v->getRandType(), vpiNotRand) << "'bit [7:0] v;' has no rand/randc modifier";

  const hldb::BitTypespec *const xTs = hldb::getTypespec<hldb::BitTypespec>(x);
  const hldb::BitTypespec *const vTs = hldb::getTypespec<hldb::BitTypespec>(v);
  ASSERT_NE(xTs, nullptr) << "'bit [7:0]' should resolve to a BitTypespec";
  ASSERT_NE(vTs, nullptr);
  EXPECT_FALSE(xTs->getSigned());
  EXPECT_FALSE(vTs->getSigned());
  ASSERT_NE(xTs->getRanges(), nullptr);
  EXPECT_EQ(xTs->getRanges()->size(), 1u);
  ASSERT_NE(vTs->getRanges(), nullptr);
  EXPECT_EQ(vTs->getRanges()->size(), 1u);
}

TEST_F(InLineConstraintChecker1Test, ConstraintC1IsXLessThanV) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);

  const hldb::Constraint *const c1 = hldb::findByName<hldb::Constraint>("c1", a->getConstraints());
  ASSERT_NE(c1, nullptr) << "in-class 'constraint c1 { ... }' should be found on the owning class directly";

  const hldb::AnyCollection *const items = c1->getConstraintItems();
  ASSERT_NE(items, nullptr);
  ASSERT_EQ(items->size(), 1u);

  const hldb::Distribution *const dist = any_cast<hldb::Distribution>((*items)[0]);
  ASSERT_NE(dist, nullptr);
  EXPECT_EQ(dist->getDistItems(), nullptr) << "no 'dist' clause is present in this constraint";

  const hldb::Operation *const lt = dist->getExpr<hldb::Operation>();
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

TEST_F(InLineConstraintChecker1Test, ClassEnvExtendsUvmEnv) {
  const hldb::ClassDefn *const env = hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses());
  ASSERT_NE(env, nullptr);
  EXPECT_NE(env->getExtends(), nullptr) << "Sec 8.13: 'class env extends uvm_env;' should record its base class";
}

TEST_F(InLineConstraintChecker1Test, ModuleTopExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
