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

// Source under test: tests/Google/chapter-18/18.5.2--constraint-inheritance_1.sv
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a;
//       rand int b;
//       constraint c { b == 5; };
//   endclass
//
//   class a2 extends a;
//       rand int b2;
//       constraint c2 { b2 == b; }
//   endclass
//
//   class env extends uvm_env;
//     a2 obj = new;
//     function new(string name, uvm_component parent = null);
//       super.new(name, parent);
//     endfunction
//     task run_phase(uvm_phase phase);
//       phase.raise_objection(this);
//       begin
//         obj.randomize();
//         if(obj.b2 == 5) begin
//           `uvm_info("RESULT", $sformatf("b2 = %0d SUCCESS", obj.b2), UVM_LOW);
//         end else begin
//           `uvm_error("RESULT", $sformatf("b2 = %0d FAILED", obj.b2));
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
// IEEE 1800-2023 Sec 18.5.2: a2 inherits 'c' (b == 5) and adds 'c2'
// (b2 == b). randomize() is virtual and honors ALL constraints of the
// object, so after obj.randomize() b2 must be 5 (the SUCCESS branch).
//
// What we have to check:
//   - Same class-level corners as constraint-inheritance_0, now compiled
//     together with UVM:
//       * a: rand int b, constraint c = (b == 5)
//       * a2 extends a: rand int b2, constraint c2 = (b2 == b)
//       * 'b' in c2 binds to the inherited a::b
//   - env extends uvm_env (UVM base class resolved from uvm_pkg).
//   - env::obj is a handle of class type a2.
//   - env has a constructor 'new' (function) and task 'run_phase'.
//   - module top declares 'environment' of class type env.
//   - No bind error for a2 / env / uvm_env / b.
//   - Runtime result: after randomize(), obj.b2 == 5.
//
// Checked:
//   - All static items above.
//
// Not checked:
//   - Resolution of uvm_env to a class definition: the .hlc only passes
//     -I for the UVM include files and never compiles uvm_pkg.sv, so
//     uvm_env cannot bind in this setup (a CompFailedToBind for it is
//     expected and correct). Only the extends clause text is checked.
//   - The UVM reporting calls in run_phase (uvm_info / uvm_error): they
//     need uvm_pkg, which the .hlc does not compile. Which branch runs
//     (SUCCESS / FAILED) is runtime-only. The randomize() call on 'obj'
//     and the if-else condition ARE checked
//     (RunPhaseRandomizesObjThenChecksResult).
//   - Exact vpiConstType of the literal '5'.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/extends.h>
#include <hldb/if_else.h>
#include <hldb/if_stmt.h>
#include <hldb/int_typespec.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task_func.h>
#include <hldb/variable.h>

#include <string_view>
#include <vector>

namespace hlc {
class ConstraintInheritance1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.2--constraint-inheritance_1.hlc"}); }
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

  // Returns the single '==' operation of constraint 'name' in 'cls', or nullptr.
  static const hldb::Operation *singleEqItem(const hldb::ClassDefn *cls, std::string_view name) {
    const hldb::Constraint *const cons = hldb::findByName<hldb::Constraint>(name, cls->getConstraints());
    if (cons == nullptr || cons->getConstraintItems() == nullptr || cons->getConstraintItems()->size() != 1)
      return nullptr;
    const hldb::Operation *const op = any_cast<hldb::Operation>(cons->getConstraintItems()->front());
    if (op == nullptr || op->getOpType() != vpiEqOp) return nullptr;
    if (op->getOperands() == nullptr || op->getOperands()->size() != 2) return nullptr;
    return op;
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

TEST_F(ConstraintInheritance1Test, BaseClassConstraintBEq5) {
  const hldb::ClassDefn *const a = findClass("a");
  ASSERT_NE(a, nullptr) << "class 'a' not found";
  EXPECT_FALSE(a->getVirtual());
  const hldb::Variable *const b = findVariable(a, "b");
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getRandType(), vpiRand);
  ASSERT_NE(b->getTypespec(), nullptr);
  ASSERT_NE(b->getTypespec()->getActual(), nullptr);
  EXPECT_EQ(b->getTypespec()->getActual()->getAnyType(), hldb::AnyType::IntTypespec);

  const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", a->getConstraints());
  ASSERT_NE(c, nullptr);
  EXPECT_FALSE(c->getPure());
  const hldb::Operation *const op = singleEqItem(a, "c");
  ASSERT_NE(op, nullptr) << "'c' must hold exactly one item: b == 5";
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(lhs->getName(), "b");
  EXPECT_EQ(lhs->getActual(), b);
  EXPECT_EQ(rhs->getDecompile(), "5");
}

TEST_F(ConstraintInheritance1Test, DerivedClassAddsC2UsingInheritedB) {
  const hldb::ClassDefn *const a = findClass("a");
  const hldb::ClassDefn *const a2 = findClass("a2");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a2, nullptr) << "class 'a2' not found";
  EXPECT_FALSE(a2->getVirtual());
  EXPECT_EQ(baseClassOf(a2), a) << "'a2 extends a' must resolve to class 'a'";

  const hldb::Variable *const b2 = findVariable(a2, "b2");
  ASSERT_NE(b2, nullptr);
  EXPECT_EQ(b2->getRandType(), vpiRand);
  EXPECT_EQ(findVariable(a2, "b"), nullptr) << "'b' is inherited, not redeclared";

  const hldb::Constraint *const c2 = hldb::findByName<hldb::Constraint>("c2", a2->getConstraints());
  ASSERT_NE(c2, nullptr);
  EXPECT_FALSE(c2->getPure());
  const hldb::Operation *const op = singleEqItem(a2, "c2");
  ASSERT_NE(op, nullptr) << "'c2' must hold exactly one item: b2 == b";
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(op->getOperands()->at(1));
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(lhs->getName(), "b2");
  EXPECT_EQ(lhs->getActual(), b2);
  EXPECT_EQ(rhs->getName(), "b");
  EXPECT_EQ(rhs->getActual(), findVariable(a, "b")) << "'b' in c2 binds to inherited a::b (Sec 8.13)";
}

TEST_F(ConstraintInheritance1Test, EnvClassExtendsUvmEnvAndHoldsA2Handle) {
  const hldb::ClassDefn *const env = findClass("env");
  ASSERT_NE(env, nullptr) << "class 'env' not found";
  EXPECT_FALSE(env->getVirtual());
  // uvm_pkg.sv is not compiled by the .hlc (only -I to the UVM sources), so
  // uvm_env cannot resolve to a class here; only the extends clause is checked.
  ASSERT_NE(env->getExtends(), nullptr) << "'env' has an extends clause";
  ASSERT_NE(env->getExtends()->getClassTypespecs(), nullptr);
  ASSERT_EQ(env->getExtends()->getClassTypespecs()->size(), 1u);
  EXPECT_EQ(env->getExtends()->getClassTypespecs()->front()->getName(), "uvm_env");

  const hldb::Variable *const obj = findVariable(env, "obj");
  ASSERT_NE(obj, nullptr) << "'obj' not found in 'env'";
  EXPECT_EQ(classOfHandle(obj), findClass("a2")) << "'obj' is a handle of class a2";
  EXPECT_EQ(obj->getRandType(), vpiNotRand) << "'obj' itself is not declared rand";

  ASSERT_NE(env->getMethods(), nullptr);
  const hldb::TaskFunc *const ctor = hldb::findByName<hldb::TaskFunc>("new", env->getMethods());
  const hldb::TaskFunc *const run = hldb::findByName<hldb::TaskFunc>("run_phase", env->getMethods());
  ASSERT_NE(ctor, nullptr) << "constructor 'new' not found";
  ASSERT_NE(run, nullptr) << "task 'run_phase' not found";
  EXPECT_EQ(ctor->getAnyType(), hldb::AnyType::Function);
  EXPECT_EQ(run->getAnyType(), hldb::AnyType::Task);
}

TEST_F(ConstraintInheritance1Test, TopModuleHoldsEnvHandle) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  const hldb::Variable *const environment = hldb::findByName<hldb::Variable>("environment", top->getVariables());
  ASSERT_NE(environment, nullptr) << "'environment' not found in 'top'";
  EXPECT_EQ(classOfHandle(environment), findClass("env")) << "'environment' is a handle of class env";
}

TEST_F(ConstraintInheritance1Test, RandomizedB2Is5) {
  const hldb::ClassDefn *const a2 = findClass("a2");
  ASSERT_NE(a2, nullptr);
  const hldb::Variable *const b2 = findVariable(a2, "b2");
  ASSERT_NE(b2, nullptr);
  if (m_design->getElaborated()) {
    const hldb::Constant *const value = b2->getValue<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "'b2' should hold its randomized value";
    EXPECT_EQ(value->getDecompile(), "5");
  }
}
TEST_F(ConstraintInheritance1Test, ConstraintsShouldBeEnabledByDefaultButAreNot) {
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
TEST_F(ConstraintInheritance1Test, RunPhaseRandomizesObjThenChecksResult) {
  // run_phase calls obj.randomize() (Sec 18.6.1) and then tests the
  // randomized value in an if-else (obj.b2 == 5).
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
  ASSERT_NE(cond, nullptr) << "condition is 'obj.b2 == 5'";
  EXPECT_EQ(cond->getOpType(), vpiEqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "5");
  EXPECT_NE(check->getStmt(), nullptr) << "SUCCESS branch must be kept";
  EXPECT_NE(check->getElseStmt(), nullptr) << "FAILED branch must be kept";
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
