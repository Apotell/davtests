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

// Source under test: tests/Google/chapter-18/18.5.4--distribution_1.sv
// (file name uses the 1800-2017 numbering; this is Sec 18.5.3 in 1800-2023)
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a;
//       rand int b;
//       constraint c { b dist {3 := 0, 10 := 5}; }
//   endclass
//
//   class env extends uvm_env;
//     a obj = new;
//     function new(string name, uvm_component parent = null);
//       super.new(name, parent);
//     endfunction
//     task run_phase(uvm_phase phase);
//       phase.raise_objection(this);
//       begin
//         obj.randomize();
//         if(obj.b == 10) begin
//           `uvm_info("RESULT", $sformatf("b = %0d SUCCESS", obj.b), UVM_LOW);
//         end else begin
//           `uvm_error("RESULT", $sformatf("b = %0d FAILED", obj.b));
//         end
//       end
//       phase.drop_objection(this);
//     endtask: run_phase
//   endclass
//
//   module top;
//     env environment;
//     initial begin
//       environment = new("env");
//       run_test();
//     end
//   endmodule
//
// IEEE 1800-2023 Sec 18.5.3 (Distribution):
//   "values not specified by the distribution and values specified with a
//    total weight of zero ... shall be treated as a constraint that the
//    expression not have such values."
// 3 has total weight 0 (it appears only once) -> 3 is EXCLUDED. Values other
// than 3/10 are not in the list -> also excluded. So b MUST be 10, which is
// exactly what the SUCCESS branch checks.
//
// What we have to check:
//   - a: rand (not randc) int b; dist on a rand variable is legal.
//   - c holds a single Distribution (not soft) with lhs ref b bound to a::b.
//   - Exactly 2 dist items, in order, both ':=' (vpiEqualDist):
//       item0: value 3,  weight 0   <- zero-weight item must be KEPT in the
//                                      model (it carries the exclusion
//                                      meaning), not dropped by the tool
//       item1: value 10, weight 5
//   - env extends uvm_env; env::obj is a handle of class a.
//   - env has function 'new' and task 'run_phase'.
//   - module top declares 'environment' of class type env.
//   - No constraint / bind error.
//   - Runtime result: after randomize(), b == 10.
//
// Checked:
//   - All static items above.
//
// Not checked:
//   - Resolution of uvm_env to a class definition: the .hlc only passes
//     -I for the UVM include files and never compiles uvm_pkg.sv, so
//     uvm_env cannot bind in this setup (a COMP_FAILED_TO_BIND for it is
//     expected and correct). Only the extends clause text is checked.
//   - The UVM reporting calls in run_phase (uvm_info / uvm_error): they
//     need uvm_pkg, which the .hlc does not compile. Which branch runs
//     (SUCCESS / FAILED) is runtime-only. The randomize() call on 'obj'
//     and the if-else condition ARE checked
//     (RunPhaseRandomizesObjThenChecksResult).
//   - Exact vpiConstType of the literals.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/dist_item.h>
#include <hldb/distribution.h>
#include <hldb/extends.h>
#include <hldb/if_else.h>
#include <hldb/if_stmt.h>
#include <hldb/int_typespec.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task_func.h>
#include <hldb/variable.h>

#include <string_view>
#include <vector>

namespace hlc {
class Distribution1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.4--distribution_1.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *findClass(std::string_view name) {
    return hldb::findByName<hldb::ClassDefn>(name, m_design->getAllClasses());
  }

  static const hldb::Variable *findVariable(const hldb::ClassDefn *cls, std::string_view name) {
    return hldb::findByName<hldb::Variable>(name, cls->getVariables());
  }

  // Base class named by 'extends', or nullptr.
  static const hldb::ClassDefn *baseClassOf(const hldb::ClassDefn *cls) {
    const hldb::Extends *const ext = cls->getExtends();
    if (ext == nullptr || ext->getClassTypespecs() == nullptr || ext->getClassTypespecs()->empty()) return nullptr;
    const hldb::RefTypespec *const rt = ext->getClassTypespecs()->front();
    const hldb::ClassTypespec *const ct = rt->getActual<hldb::ClassTypespec>();
    return (ct == nullptr) ? nullptr : ct->getClassDefn();
  }

  // Class that variable 'var' is a handle of, or nullptr.
  static const hldb::ClassDefn *classOfHandle(const hldb::Variable *var) {
    const hldb::RefTypespec *const rt = var->getTypespec();
    if (rt == nullptr) return nullptr;
    const hldb::ClassTypespec *const ct = rt->getActual<hldb::ClassTypespec>();
    return (ct == nullptr) ? nullptr : ct->getClassDefn();
  }

  // The single Distribution item of a::c, or nullptr.
  static const hldb::Distribution *findDist() {
    const hldb::ClassDefn *const a = findClass("a");
    if (a == nullptr) return nullptr;
    const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", a->getConstraints());
    if (c == nullptr || c->getConstraintItems() == nullptr || c->getConstraintItems()->size() != 1) return nullptr;
    return any_cast<hldb::Distribution>(c->getConstraintItems()->front());
  }

  // Checks one dist item: value, weight and kind.
  static void expectDistItem(const hldb::DistItem *item, std::string_view value, std::string_view weight,
                             int32_t distType) {
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->getDistType(), distType);
    const hldb::Constant *const v = item->getValueRange<hldb::Constant>();
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->getDecompile(), value);
    const hldb::Constant *const w = item->getWeight<hldb::Constant>();
    ASSERT_NE(w, nullptr) << "weight is written explicitly";
    EXPECT_EQ(w->getDecompile(), weight);
  }

  // Every statement reachable from 'stmt' through begin / if / if-else
  // blocks, in source (pre-)order.
  static void collectStmts(const hldb::Any *stmt, std::vector<const hldb::Any *> *out) {
    if (stmt == nullptr) return;
    out->emplace_back(stmt);
    if (const hldb::Begin *const blk = any_cast<hldb::Begin>(stmt)) {
      if (blk->getStmts() != nullptr) {
        for (const hldb::Any *const s : *blk->getStmts()) collectStmts(s, out);
      }
    } else if (const hldb::IfElse *const ie = any_cast<hldb::IfElse>(stmt)) {
      collectStmts(ie->getStmt(), out);
      collectStmts(ie->getElseStmt(), out);
    } else if (const hldb::IfStmt *const is = any_cast<hldb::IfStmt>(stmt)) {
      collectStmts(is->getStmt(), out);
    }
  }

  // The method call named 'name' in 'expr' (bare call, or last path element
  // of a RefObj), or nullptr.
  static const hldb::MethodFuncCall *findCallNamed(const hldb::Any *expr, std::string_view name) {
    const hldb::MethodFuncCall *call = any_cast<hldb::MethodFuncCall>(expr);
    if (call == nullptr) {
      const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
      if (ref == nullptr || ref->getPathElems() == nullptr || ref->getPathElems()->empty()) return nullptr;
      call = any_cast<hldb::MethodFuncCall>(ref->getPathElems()->back());
    }
    return (call != nullptr && call->getName() == name) ? call : nullptr;
  }

  // The RefObj naming the receiver 'obj' of the call in 'expr', or nullptr.
  static const hldb::RefObj *receiverOf(const hldb::Any *expr) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
    if (ref == nullptr) return nullptr;
    if (ref->getName() == "obj") return ref;
    if (ref->getPathElems() == nullptr || ref->getPathElems()->empty()) return nullptr;
    return any_cast<hldb::RefObj>(ref->getPathElems()->front());
  }
};

TEST_F(Distribution1Test, ClassAndRandVariable) {
  const hldb::ClassDefn *const a = findClass("a");
  ASSERT_NE(a, nullptr) << "class 'a' not found";
  EXPECT_FALSE(a->getVirtual());
  const hldb::Variable *const b = findVariable(a, "b");
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getRandType(), vpiRand) << "'b' is 'rand' (not randc), so dist is legal";
  ASSERT_NE(b->getTypespec(), nullptr);
  ASSERT_NE(b->getTypespec()->getActual(), nullptr);
  EXPECT_EQ(b->getTypespec()->getActual()->getAnyType(), hldb::AnyType::IntTypespec);
}

TEST_F(Distribution1Test, ConstraintHoldsSingleDistributionOnB) {
  const hldb::ClassDefn *const a = findClass("a");
  ASSERT_NE(a, nullptr);
  const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", a->getConstraints());
  ASSERT_NE(c, nullptr);
  EXPECT_FALSE(c->getPure());

  const hldb::Distribution *const dist = findDist();
  ASSERT_NE(dist, nullptr) << "'c' must hold exactly one distribution";
  EXPECT_FALSE(dist->getSoft());
  const hldb::RefObj *const lhs = dist->getExpr<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "b");
  EXPECT_EQ(lhs->getActual(), findVariable(a, "b"));
}

TEST_F(Distribution1Test, ZeroWeightItemIsKept) {
  const hldb::Distribution *const dist = findDist();
  ASSERT_NE(dist, nullptr);
  ASSERT_NE(dist->getDistItems(), nullptr);
  ASSERT_EQ(dist->getDistItems()->size(), 2u) << "'3 := 0' must not be dropped: it excludes 3 (Sec 18.5.3)";
  expectDistItem(dist->getDistItems()->at(0), "3", "0", vpiEqualDist);
  expectDistItem(dist->getDistItems()->at(1), "10", "5", vpiEqualDist);
}

TEST_F(Distribution1Test, EnvExtendsUvmEnvAndHoldsAHandle) {
  const hldb::ClassDefn *const env = findClass("env");
  ASSERT_NE(env, nullptr) << "class 'env' not found";
  // uvm_pkg.sv is not compiled by the .hlc (only -I to the UVM sources), so
  // uvm_env cannot resolve to a class here; only the extends clause is checked.
  ASSERT_NE(env->getExtends(), nullptr) << "'env' has an extends clause";
  ASSERT_NE(env->getExtends()->getClassTypespecs(), nullptr);
  ASSERT_EQ(env->getExtends()->getClassTypespecs()->size(), 1u);
  EXPECT_EQ(env->getExtends()->getClassTypespecs()->front()->getName(), "uvm_env");

  const hldb::Variable *const obj = findVariable(env, "obj");
  ASSERT_NE(obj, nullptr);
  EXPECT_EQ(classOfHandle(obj), findClass("a"));

  ASSERT_NE(env->getMethods(), nullptr);
  const hldb::TaskFunc *const ctor = hldb::findByName<hldb::TaskFunc>("new", env->getMethods());
  const hldb::TaskFunc *const run = hldb::findByName<hldb::TaskFunc>("run_phase", env->getMethods());
  ASSERT_NE(ctor, nullptr);
  ASSERT_NE(run, nullptr);
  EXPECT_EQ(ctor->getAnyType(), hldb::AnyType::Function);
  EXPECT_EQ(run->getAnyType(), hldb::AnyType::Task);
}

TEST_F(Distribution1Test, TopModuleHoldsEnvHandle) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  const hldb::Variable *const environment = hldb::findByName<hldb::Variable>("environment", top->getVariables());
  ASSERT_NE(environment, nullptr);
  EXPECT_EQ(classOfHandle(environment), findClass("env"));
}

TEST_F(Distribution1Test, NoConstraintOrBindError) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "c"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "b"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "env"), nullptr);
}

TEST_F(Distribution1Test, RandomizedBIs10) {
  const hldb::ClassDefn *const a = findClass("a");
  ASSERT_NE(a, nullptr);
  const hldb::Variable *const b = findVariable(a, "b");
  ASSERT_NE(b, nullptr);
  if (m_design->getElaborated()) {
    const hldb::Constant *const value = b->getValue<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "'b' should hold its randomized value";
    EXPECT_EQ(value->getDecompile(), "10");
  }
}
TEST_F(Distribution1Test, ConstraintsShouldBeEnabledByDefaultButAreNot) {
  // IEEE 1800-2023 Sec 18.9: all constraints are initially active
  // (constraint_mode() == 1). Pure constraints are obligations, not active
  // constraints, so they are skipped.
  for (std::string_view className : {"a", "a2"}) {
    const hldb::ClassDefn *const cls = hldb::findByName<hldb::ClassDefn>(className, m_design->getAllClasses());
    if (cls == nullptr || cls->getConstraints() == nullptr) continue;
    for (const hldb::Constraint *const cons : *cls->getConstraints()) {
      if (cons->getPure()) continue;
      EXPECT_TRUE(cons->getIsConstraintEnabled())
          << className << "::" << cons->getName() << " should be enabled by default (Sec 18.9)";
    }
  }
}
TEST_F(Distribution1Test, RunPhaseRandomizesObjThenChecksResult) {
  // run_phase calls obj.randomize() (Sec 18.6.1) and then tests the
  // randomized value in an if-else (obj.b == 10).
  const hldb::ClassDefn *const env = findClass("env");
  ASSERT_NE(env, nullptr);
  ASSERT_NE(env->getMethods(), nullptr);
  const hldb::TaskFunc *const run = hldb::findByName<hldb::TaskFunc>("run_phase", env->getMethods());
  ASSERT_NE(run, nullptr);
  std::vector<const hldb::Any *> stmts;
  collectStmts(run->getStmt(), &stmts);
  ASSERT_FALSE(stmts.empty()) << "run_phase must keep its body";

  const hldb::MethodFuncCall *call = nullptr;
  const hldb::RefObj *recv = nullptr;
  for (const hldb::Any *const s : stmts) {
    call = findCallNamed(s, "randomize");
    if (call != nullptr) {
      recv = receiverOf(s);
      break;
    }
  }
  ASSERT_NE(call, nullptr) << "'obj.randomize();' must be kept in run_phase";
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty()) << "randomize() has no arguments";
  ASSERT_NE(recv, nullptr) << "randomize() must be linked to its receiver 'obj'";
  EXPECT_EQ(recv->getName(), "obj");
  EXPECT_EQ(recv->getActual(), findVariable(env, "obj")) << "receiver binds to env::obj";

  const hldb::IfElse *check = nullptr;
  for (const hldb::Any *const s : stmts) {
    check = any_cast<hldb::IfElse>(s);
    if (check != nullptr) break;
  }
  ASSERT_NE(check, nullptr) << "the SUCCESS / FAILED check is an if-else";
  const hldb::Operation *const cond = any_cast<hldb::Operation>(check->getCondition());
  ASSERT_NE(cond, nullptr) << "condition is 'obj.b == 10'";
  EXPECT_EQ(cond->getOpType(), vpiEqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "10");
  EXPECT_NE(check->getStmt(), nullptr) << "SUCCESS branch must be kept";
  EXPECT_NE(check->getElseStmt(), nullptr) << "FAILED branch must be kept";
}
}  // namespace hlc

#ifndef DAVTESTS_BULK_BUILD
int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
