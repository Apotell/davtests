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

// Source under test: tests/Google/chapter-18/18.5.8.1--foreach-iterative-constraints_1.sv
// (file name uses the 1800-2017 numbering; this is Sec 18.5.7.1 in 1800-2023)
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a;
//       rand int B[5];
//       constraint c { foreach ( B [ i ] ) B[i] == 5; }
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
//         if(obj.B.sum() == 25) begin
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
// IEEE 1800-2023 Sec 7.4.2: 'int B[5]' == 'int B[0:4]'.
// IEEE 1800-2023 Sec 18.5.7.1: foreach (B[i]) B[i] == 5 constrains every
// element, i = 0..4, to 5. So after randomize() every element is 5 and
// B.sum() == 25 -> SUCCESS branch.
//
// What we have to check:
//   - B: rand, unpacked static array [0:4] of int.
//   - c: single ConstrForeach over B (bound to a::B), one loop var 'i',
//     body exactly B[i] == 5 with 'i' bound to the loop variable.
//   - env extends uvm_env; env::obj is a handle of class a.
//   - env has function 'new' and task 'run_phase'.
//   - module top declares 'environment' of class type env.
//   - No constraint / bind error.
//   - Runtime result: every element of B is 5 (sum 25).
//
// Checked:
//   - All static items above.
//
// Not checked:
//   - Resolution of uvm_env to a class definition: the .hlc only passes
//     -I for the UVM include files and never compiles uvm_pkg.sv, so
//     uvm_env cannot bind in this setup (a CompFailedToBind for it is
//     expected and correct). Only the extends clause text is checked.
//   - The body of run_phase (B.sum() in the check is UVM reporting; array
//     reduction itself is covered by 18.5.8.2 tests).
//   - Exact select class used for B[i] (BitSelect or VarSelect accepted).
//   - Exact vpiConstType of literals.
//
// Known model risk: HLDB types ConstrForeach::getConstraintExprs() as a
// ConstraintExpr collection, but IEEE Sec 37.38 allows a plain expr in it.
// If HLC cannot store 'B[i] == 5' there, the body check fails (HLDB gap).

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/bit_select.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/constr_foreach.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/extends.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task_func.h>
#include <hldb/var_select.h>
#include <hldb/variable.h>

#include <string_view>

namespace hlc {
class ForeachIterativeConstraints1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.8.1--foreach-iterative-constraints_1.hlc"}); }
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

  // The single ConstrForeach item of constraint 'c', or nullptr.
  static const hldb::ConstrForeach *findForeach() {
    const hldb::ClassDefn *const a = findClass("a");
    if (a == nullptr) return nullptr;
    const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", a->getConstraints());
    if (c == nullptr || c->getConstraintItems() == nullptr || c->getConstraintItems()->size() != 1) return nullptr;
    return any_cast<hldb::ConstrForeach>(c->getConstraintItems()->front());
  }

  // Index expression of an element select (BitSelect or VarSelect), or nullptr.
  static const hldb::Any *selectIndex(const hldb::Any *sel) {
    if (const hldb::BitSelect *const bs = any_cast<hldb::BitSelect>(sel)) return bs->getIndex();
    if (const hldb::VarSelect *const vs = any_cast<hldb::VarSelect>(sel)) return vs->getIndex();
    return nullptr;
  }
};

TEST_F(ForeachIterativeConstraints1Test, BIsRandFixedSizeUnpackedIntArray) {
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

TEST_F(ForeachIterativeConstraints1Test, RangeShouldBe0To4ButIsNot) {
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

TEST_F(ForeachIterativeConstraints1Test, ForeachOverB) {
  const hldb::ConstrForeach *const fe = findForeach();
  ASSERT_NE(fe, nullptr) << "'c' must hold exactly one foreach item";
  ASSERT_NE(fe->getVariable(), nullptr);
  EXPECT_EQ(fe->getVariable()->getName(), "B");
  EXPECT_EQ(fe->getVariable<hldb::RefObj>()->getActual(), findB());
}

TEST_F(ForeachIterativeConstraints1Test, LoopVariableShouldBeKeptButIsNot) {
  const hldb::ConstrForeach *const fe = findForeach();
  ASSERT_NE(fe, nullptr);
  ASSERT_NE(fe->getLoopVars(), nullptr);
  ASSERT_EQ(fe->getLoopVars()->size(), 1u);
  ASSERT_NE(fe->getLoopVars()->front(), nullptr);
  EXPECT_EQ(fe->getLoopVars()->front()->getName(), "i");
}

TEST_F(ForeachIterativeConstraints1Test, BodyShouldBeBiEq5ButIsDropped) {
  const hldb::ConstrForeach *const fe = findForeach();
  ASSERT_NE(fe, nullptr);
  ASSERT_NE(fe->getConstraintExprs(), nullptr) << "foreach body must be kept";
  ASSERT_EQ(fe->getConstraintExprs()->size(), 1u);
  const hldb::Any *const body = fe->getConstraintExprs()->front();
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(body->getAnyType(), hldb::AnyType::Operation);
  const hldb::Operation *const op = any_cast<hldb::Operation>(body);
  EXPECT_EQ(op->getOpType(), vpiEqOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const hldb::Any *const lhs = op->getOperands()->at(0);
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "B[i]");
  const hldb::RefObj *const iRef = any_cast<hldb::RefObj>(selectIndex(lhs));
  ASSERT_NE(iRef, nullptr) << "lhs must be B[<ref to i>]";
  EXPECT_EQ(iRef->getName(), "i");
  ASSERT_NE(fe->getLoopVars(), nullptr);
  ASSERT_FALSE(fe->getLoopVars()->empty());
  EXPECT_EQ(iRef->getActual(), fe->getLoopVars()->front()) << "'i' binds to the foreach loop variable";
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "5");
}

TEST_F(ForeachIterativeConstraints1Test, EnvExtendsUvmEnvAndHoldsAHandle) {
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

TEST_F(ForeachIterativeConstraints1Test, TopModuleHoldsEnvHandle) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  const hldb::Variable *const environment = hldb::findByName<hldb::Variable>("environment", top->getVariables());
  ASSERT_NE(environment, nullptr);
  EXPECT_EQ(classOfHandle(environment), findClass("env"));
}

TEST_F(ForeachIterativeConstraints1Test, RandomizedElementsAreAll5) {
  const hldb::Variable *const b = findB();
  ASSERT_NE(b, nullptr);
  if (m_design->getElaborated()) {
    ASSERT_NE(b->getValue(), nullptr) << "'B' should hold its randomized value '{5, 5, 5, 5, 5}";
    const hldb::Operation *const pattern = b->getValue<hldb::Operation>();
    ASSERT_NE(pattern, nullptr);
    ASSERT_NE(pattern->getOperands(), nullptr);
    ASSERT_EQ(pattern->getOperands()->size(), 5u);
    for (const hldb::Any *const elem : *pattern->getOperands()) {
      const hldb::Constant *const value = any_cast<hldb::Constant>(elem);
      ASSERT_NE(value, nullptr);
      EXPECT_EQ(value->getDecompile(), "5");
    }
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
