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

// Source under test: tests/Google/chapter-18/18.5.3--set-membership_1.sv
// (file name uses the 1800-2017 numbering; 1800-2023 Sec 18.5.3 + 11.4.13)
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a;
//       rand int b;
//       constraint c { b inside {3, 10}; }
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
//         if(obj.b inside {3, 10}) begin
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
// IEEE 1800-2023 Sec 11.4.13 / 18.5.3: 'b inside {3, 10}' restricts b to the
// set {3, 10}, each value equally likely. Operands in BNF order (Sec 37):
// b, 3, 10. After randomize(), b is 3 or 10.
//
// What we have to check:
//   - a: non-virtual, rand int b, one constraint 'c' (not pure, enabled).
//   - c holds a single vpiInsideOp operation (not a Distribution) with
//     operands, in order: ref b (bound to a::b), 3, 10.
//   - env extends uvm_env; env::obj is a handle of class a.
//   - env has function 'new' and task 'run_phase'.
//   - module top declares 'environment' of class type env.
//   - No constraint / bind error.
//   - Runtime result: after randomize(), b is 3 or 10.
//
// Checked:
//   - All static items above.
//
// Not checked:
//   - Whether the 'inside' set / 'unique' group is stored as flat operands or
//     wrapped in one list operation: IEEE does not fix this, so both are
//     accepted. The items themselves and their order are still checked.
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
class SetMembership1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.3--set-membership_1.hlc"}); }
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

  // Items of the range list of '<expr> inside {...}', in source order. They
  // are either the operands after the first, or the operands of a single
  // list operation that is the second operand.
  static std::vector<const hldb::Any *> insideSetItems(const hldb::Operation *op) {
    std::vector<const hldb::Any *> items;
    if (op->getOperands() == nullptr || op->getOperands()->size() < 2) return items;
    if (op->getOperands()->size() == 2) {
      const hldb::Operation *const list = any_cast<hldb::Operation>(op->getOperands()->at(1));
      if (list != nullptr) {
        if (list->getOperands() != nullptr) {
          for (const hldb::Any *const item : *list->getOperands()) items.emplace_back(item);
        }
        return items;
      }
    }
    for (size_t i = 1; i < op->getOperands()->size(); ++i) items.emplace_back(op->getOperands()->at(i));
    return items;
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

TEST_F(SetMembership1Test, ClassAndRandVariable) {
  const hldb::ClassDefn *const a = findClass("a");
  ASSERT_NE(a, nullptr) << "class 'a' not found";
  EXPECT_FALSE(a->getVirtual());
  const hldb::Variable *const b = findVariable(a, "b");
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getRandType(), vpiRand);
  ASSERT_NE(b->getTypespec(), nullptr);
  ASSERT_NE(b->getTypespec()->getActual(), nullptr);
  ASSERT_EQ(b->getTypespec()->getActual()->getAnyType(), hldb::AnyType::IntTypespec);
  EXPECT_TRUE(b->getTypespec()->getActual<hldb::IntTypespec>()->getSigned());
}

TEST_F(SetMembership1Test, ConstraintIsBInside3And10) {
  const hldb::ClassDefn *const a = findClass("a");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getConstraints(), nullptr);
  EXPECT_EQ(a->getConstraints()->size(), 1u);
  const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", a->getConstraints());
  ASSERT_NE(c, nullptr);
  EXPECT_FALSE(c->getPure());
  ASSERT_NE(c->getConstraintItems(), nullptr);
  ASSERT_EQ(c->getConstraintItems()->size(), 1u);
  const hldb::Any *const item = c->getConstraintItems()->front();
  ASSERT_NE(item, nullptr);
  ASSERT_EQ(item->getAnyType(), hldb::AnyType::Operation) << "'inside' is an operation, not a dist";

  const hldb::Operation *const op = any_cast<hldb::Operation>(item);
  EXPECT_EQ(op->getOpType(), vpiInsideOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_FALSE(op->getOperands()->empty());
  const std::vector<const hldb::Any *> items = insideSetItems(op);
  ASSERT_EQ(items.size(), 2u) << "set {3, 10} keeps both values";
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  const hldb::Constant *const v0 = any_cast<hldb::Constant>(items[0]);
  const hldb::Constant *const v1 = any_cast<hldb::Constant>(items[1]);
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(v0, nullptr);
  ASSERT_NE(v1, nullptr);
  EXPECT_EQ(lhs->getName(), "b");
  EXPECT_EQ(lhs->getActual(), findVariable(a, "b"));
  EXPECT_EQ(v0->getDecompile(), "3");
  EXPECT_EQ(v1->getDecompile(), "10");
}

TEST_F(SetMembership1Test, EnvExtendsUvmEnvAndHoldsAHandle) {
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
  EXPECT_EQ(classOfHandle(obj), findClass("a")) << "'obj' is a handle of class a";

  ASSERT_NE(env->getMethods(), nullptr);
  const hldb::TaskFunc *const ctor = hldb::findByName<hldb::TaskFunc>("new", env->getMethods());
  const hldb::TaskFunc *const run = hldb::findByName<hldb::TaskFunc>("run_phase", env->getMethods());
  ASSERT_NE(ctor, nullptr);
  ASSERT_NE(run, nullptr);
  EXPECT_EQ(ctor->getAnyType(), hldb::AnyType::Function);
  EXPECT_EQ(run->getAnyType(), hldb::AnyType::Task);
}

TEST_F(SetMembership1Test, TopModuleHoldsEnvHandle) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  const hldb::Variable *const environment = hldb::findByName<hldb::Variable>("environment", top->getVariables());
  ASSERT_NE(environment, nullptr);
  EXPECT_EQ(classOfHandle(environment), findClass("env"));
}

TEST_F(SetMembership1Test, NoConstraintOrBindError) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "c"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "b"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "env"), nullptr);
}

TEST_F(SetMembership1Test, RandomizedBIsInSet) {
  const hldb::ClassDefn *const a = findClass("a");
  ASSERT_NE(a, nullptr);
  const hldb::Variable *const b = findVariable(a, "b");
  ASSERT_NE(b, nullptr);
  if (m_design->getElaborated()) {
    const hldb::Constant *const value = b->getValue<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "'b' should hold its randomized value";
    EXPECT_TRUE(value->getDecompile() == "3" || value->getDecompile() == "10") << value->getDecompile();
  }
}
TEST_F(SetMembership1Test, ConstraintsShouldBeEnabledByDefaultButAreNot) {
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
TEST_F(SetMembership1Test, RunPhaseRandomizesObjThenChecksResult) {
  // run_phase calls obj.randomize() (Sec 18.6.1) and then tests the
  // randomized value in an if-else (obj.b inside {3, 10}).
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
  ASSERT_NE(cond, nullptr) << "condition is 'obj.b inside {3, 10}'";
  EXPECT_EQ(cond->getOpType(), vpiInsideOp);
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
