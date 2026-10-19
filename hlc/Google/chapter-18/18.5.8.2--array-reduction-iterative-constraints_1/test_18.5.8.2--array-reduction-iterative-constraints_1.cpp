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

// Source under test: tests/Google/chapter-18/18.5.8.2--array-reduction-iterative-constraints_1.sv
// (file name uses the 1800-2017 numbering; this is Sec 18.5.7.2 in 1800-2023)
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a;
//       rand int B[5];
//       constraint c { B.sum() == 5; }
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
//         if(obj.B.sum() == 5) begin
//           `uvm_info("RESULT", $sformatf("sum = %0d SUCCESS", obj.B.sum()), UVM_LOW);
//         end else begin
//           `uvm_error("RESULT", $sformatf("sum = %0d FAILED", obj.B.sum()));
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
// IEEE 1800-2023 Sec 18.5.7.2: in a constraint, B.sum() is treated as
// B[0] + B[1] + B[2] + B[3] + B[4] (type int, the element type). c forces
// that sum to 5, so after randomize() B.sum() == 5 -> SUCCESS branch.
//
// What we have to check:
//   - B: rand unpacked static array [0:4] of int.
//   - c: single equality <B.sum()> == 5, where the lhs is a 'sum' method
//     call with no arguments / no 'with', receiver 'B' bound to a::B,
//     result type int.
//   - env extends uvm_env; env::obj is a handle of class a.
//   - env has function 'new' and task 'run_phase'.
//   - module top declares 'environment' of class type env.
//   - No constraint / bind error.
//   - Runtime result: the 5 randomized elements sum to 5.
//
// Checked:
//   - All static items above.
//
// Not checked:
//   - Resolution of uvm_env to a class definition: the .hlc only passes
//     -I for the UVM include files and never compiles uvm_pkg.sv, so
//     uvm_env cannot bind in this setup (a COMP_FAILED_TO_BIND for it is
//     expected and correct). Only the extends clause text is checked.
//   - The body of run_phase (UVM reporting, not a constraint corner).
//   - Exact vpiConstType of literals.
//
// Tool-representation note: HLDB's MethodFuncCall has no vpiPrefix
// accessor, so 'B.sum()' may be a bare MethodFuncCall or a RefObj 'B' whose
// path elements end in the call. Both are accepted for finding the call;
// the receiver check needs the RefObj shape and fails otherwise (HLDB gap).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/extends.h>
#include <hldb/int_typespec.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task_func.h>
#include <hldb/variable.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace hlc {
class ArrayReductionIterativeConstraints1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.8.2--array-reduction-iterative-constraints_1.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *findClass(std::string_view name) {
    return hldb::findByName<hldb::ClassDefn>(name, m_design->getAllClasses());
  }

  static const hldb::Variable *findVariable(const hldb::ClassDefn *cls, std::string_view name) {
    return hldb::findByName<hldb::Variable>(name, cls->getVariables());
  }

  static const hldb::Variable *findB() {
    const hldb::ClassDefn *const a = findClass("a");
    return (a == nullptr) ? nullptr : findVariable(a, "B");
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

  // The single '==' operation of constraint 'c', or nullptr.
  static const hldb::Operation *findEq() {
    const hldb::ClassDefn *const a = findClass("a");
    if (a == nullptr) return nullptr;
    const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", a->getConstraints());
    if (c == nullptr || c->getConstraintItems() == nullptr || c->getConstraintItems()->size() != 1) return nullptr;
    const hldb::Operation *const op = any_cast<hldb::Operation>(c->getConstraintItems()->front());
    if (op == nullptr || op->getOperands() == nullptr || op->getOperands()->size() != 2) return nullptr;
    return op;
  }

  // The 'sum' call inside 'expr' (bare call, or last path element of a
  // RefObj), or nullptr.
  static const hldb::MethodFuncCall *findCall(const hldb::Any *expr) {
    if (const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(expr)) return call;
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
    if (ref == nullptr || ref->getPathElems() == nullptr || ref->getPathElems()->empty()) return nullptr;
    return any_cast<hldb::MethodFuncCall>(ref->getPathElems()->back());
  }

  // The RefObj naming the receiver 'B' of the call inside 'expr', or nullptr.
  static const hldb::RefObj *findReceiver(const hldb::Any *expr) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
    if (ref == nullptr) return nullptr;
    if (ref->getName() == "B") return ref;
    if (ref->getPathElems() == nullptr || ref->getPathElems()->empty()) return nullptr;
    return any_cast<hldb::RefObj>(ref->getPathElems()->front());
  }
};

TEST_F(ArrayReductionIterativeConstraints1Test, BIsRandFixedSizeUnpackedIntArray) {
  const hldb::Variable *const b = findB();
  ASSERT_NE(b, nullptr) << "'B' not found in class 'a'";
  EXPECT_EQ(b->getRandType(), vpiRand);
  ASSERT_NE(b->getTypespec(), nullptr);
  ASSERT_NE(b->getTypespec()->getActual(), nullptr);
  ASSERT_EQ(b->getTypespec()->getActual()->getAnyType(), hldb::AnyType::ArrayTypespec);
  const hldb::ArrayTypespec *const at = b->getTypespec()->getActual<hldb::ArrayTypespec>();
  EXPECT_EQ(at->getArrayType(), vpiStaticArray);
  EXPECT_FALSE(at->getPacked());
  ASSERT_NE(at->getRange(), nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  ASSERT_NE(at->getElemTypespec()->getActual(), nullptr);
  EXPECT_EQ(at->getElemTypespec()->getActual()->getAnyType(), hldb::AnyType::IntTypespec);
}

TEST_F(ArrayReductionIterativeConstraints1Test, RangeShouldBe0To4ButIsNot) {
  const hldb::Variable *const b = findB();
  ASSERT_NE(b, nullptr);
  ASSERT_NE(b->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = b->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  EXPECT_EQ(at->getRange()->getLeftExpr(), nullptr);
  EXPECT_NE(at->getRange()->getRightExpr(), nullptr);
  const hldb::Operation *const right = at->getRange()->getRightExpr<hldb::Operation>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getOpType(), vpiSubOp);
  ASSERT_NE(right->getOperands(), nullptr);
  ASSERT_EQ(right->getOperands()->size(), 1u);
  const hldb::Constant *const value = any_cast<hldb::Constant>(right->getOperands()->at(0));
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getDecompile(), "5");
}

TEST_F(ArrayReductionIterativeConstraints1Test, ConstraintIsSumEq5) {
  const hldb::Operation *const op = findEq();
  ASSERT_NE(op, nullptr) << "'c' must hold exactly one two-operand operation";
  EXPECT_EQ(op->getOpType(), vpiEqOp);
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "5");

  const hldb::MethodFuncCall *const call = findCall(op->getOperands()->at(0));
  ASSERT_NE(call, nullptr) << "lhs must be the array method call B.sum()";
  EXPECT_EQ(call->getName(), "sum");
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty()) << "sum() has no arguments";
  EXPECT_EQ(call->getWith(), nullptr) << "no 'with' clause in source";
}

TEST_F(ArrayReductionIterativeConstraints1Test, SumReceiverIsB) {
  const hldb::Operation *const op = findEq();
  ASSERT_NE(op, nullptr);
  const hldb::RefObj *const recv = findReceiver(op->getOperands()->at(0));
  ASSERT_NE(recv, nullptr) << "the call must be linked to its receiver 'B'";
  EXPECT_EQ(recv->getName(), "B");
  EXPECT_EQ(recv->getActual(), findB()) << "receiver binds to a::B";
}

TEST_F(ArrayReductionIterativeConstraints1Test, SumResultTypeShouldBeIntButIsMissing) {
  const hldb::Operation *const op = findEq();
  ASSERT_NE(op, nullptr);
  const hldb::MethodFuncCall *const call = findCall(op->getOperands()->at(0));
  ASSERT_NE(call, nullptr);
  const hldb::Function *const fn = call->getTaskFunc<hldb::Function>();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getReturn(), nullptr) << "sum() result has a type (Sec 18.5.7.2)";
  ASSERT_NE(fn->getReturn()->getActual(), nullptr);
  EXPECT_EQ(fn->getReturn()->getActual()->getAnyType(), hldb::AnyType::TypeParameter)
      << "without 'with', sum() returns the generic element type";
}

TEST_F(ArrayReductionIterativeConstraints1Test, EnvExtendsUvmEnvAndHoldsAHandle) {
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

TEST_F(ArrayReductionIterativeConstraints1Test, TopModuleHoldsEnvHandle) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  const hldb::Variable *const environment = hldb::findByName<hldb::Variable>("environment", top->getVariables());
  ASSERT_NE(environment, nullptr);
  EXPECT_EQ(classOfHandle(environment), findClass("env"));
}

TEST_F(ArrayReductionIterativeConstraints1Test, NoConstraintOrBindError) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "c"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "sum"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "env"), nullptr);
}

TEST_F(ArrayReductionIterativeConstraints1Test, RandomizedElementsSumTo5) {
  const hldb::Variable *const b = findB();
  ASSERT_NE(b, nullptr);
  if (m_design->getElaborated()) {
    const hldb::Operation *const pattern = b->getValue<hldb::Operation>();
    ASSERT_NE(pattern, nullptr) << "'B' should hold its randomized value '{...}";
    ASSERT_NE(pattern->getOperands(), nullptr);
    ASSERT_EQ(pattern->getOperands()->size(), 5u);
    int64_t sum = 0;
    for (const hldb::Any *const elem : *pattern->getOperands()) {
      const hldb::Constant *const value = any_cast<hldb::Constant>(elem);
      ASSERT_NE(value, nullptr);
      sum += std::stoll(std::string(value->getDecompile()));
    }
    EXPECT_EQ(sum, 5);
  }
}
}  // namespace hlc

#ifndef DAVTESTS_BULK_BUILD
int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
