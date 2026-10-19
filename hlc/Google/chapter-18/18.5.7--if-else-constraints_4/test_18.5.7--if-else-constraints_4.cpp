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

// Source under test: tests/Google/chapter-18/18.5.7--if-else-constraints_4.sv
// (file name uses the 1800-2017 numbering; this is Sec 18.5.6 in 1800-2023)
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a;
//       rand int b1, b2, b3;
//       constraint c1 { b1 == 5; }
//       constraint c2 { b2 == 3; }
//       constraint c3 { if (b1 == 5)
//                         if (b2 == 2) b3 == 4;
//                         else b3 == 10;}
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
//         if(obj.b3 == 10) begin
//           `uvm_info("RESULT", $sformatf("b3 = %0d SUCCESS", obj.b3), UVM_LOW);
//         end else begin
//           `uvm_error("RESULT", $sformatf("b3 = %0d FAILED", obj.b3));
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
// IEEE 1800-2023 Sec 18.5.6 (dangling else): the else binds to the closest
// previous if that lacks an else -> the INNER 'if (b2 == 2)'.
//     if (b1 == 5)                    <- outer: NO else
//         { if (b2 == 2) b3 == 4
//           else         b3 == 10 }
// b1 == 5 (c1) -> outer true; b2 == 3 (c2) -> inner false -> b3 == 10, which
// is the SUCCESS branch. (Had the else wrongly bound to the outer if, b3
// would be 4-or-free instead.)
//
// What we have to check:
//   - 'rand int b1, b2, b3;' -> three rand signed int variables.
//   - Constraints c1, c2, c3 in source order.
//   - c1: b1 == 5;  c2: b2 == 3.
//   - c3: outer ConstrIf (no else), condition b1 == 5, then-part exactly
//     one ConstrIfElse with condition b2 == 2, then b3 == 4, else b3 == 10.
//   - env extends uvm_env; env::obj is a handle of class a.
//   - env has function 'new' and task 'run_phase'.
//   - module top declares 'environment' of class type env.
//   - No constraint / bind error.
//   - Runtime result: b3 == 10.
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
// Known model risk: leaf parts (b3 == 4 / 10) are Operations, but HLDB
// types the then/else collections as ConstraintExpr only (IEEE Sec 37.38
// allows a plain expr). If HLC cannot store them, the leaf checks fail
// (HLDB model gap). The dangling-else structure checks do not depend on it.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/constr_if.h>
#include <hldb/constr_if_else.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/extends.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task_func.h>
#include <hldb/variable.h>

#include <string_view>

namespace hlc {
class IfElseConstraints4Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.7--if-else-constraints_4.hlc"}); }
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

  // The single item of constraint 'name' in class a, or nullptr.
  static const hldb::Any *singleItem(std::string_view name) {
    const hldb::ClassDefn *const a = findClass("a");
    if (a == nullptr) return nullptr;
    const hldb::Constraint *const cons = hldb::findByName<hldb::Constraint>(name, a->getConstraints());
    if (cons == nullptr || cons->getConstraintItems() == nullptr || cons->getConstraintItems()->size() != 1)
      return nullptr;
    return cons->getConstraintItems()->front();
  }

  // Checks 'e' is '<var> == <lit>' with <var> bound to the member of class a.
  static void expectVarEqLit(const hldb::Any *e, std::string_view var, std::string_view lit) {
    ASSERT_NE(e, nullptr) << "expected '" << var << " == " << lit << "'";
    ASSERT_EQ(e->getAnyType(), hldb::AnyType::Operation);
    const hldb::Operation *const op = any_cast<hldb::Operation>(e);
    EXPECT_EQ(op->getOpType(), vpiEqOp);
    ASSERT_NE(op->getOperands(), nullptr);
    ASSERT_EQ(op->getOperands()->size(), 2u);
    const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(op->getOperands()->at(0));
    const hldb::Constant *const rhs = any_cast<hldb::Constant>(op->getOperands()->at(1));
    ASSERT_NE(lhs, nullptr);
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(lhs->getName(), var);
    const hldb::ClassDefn *const a = findClass("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(lhs->getActual(), findVariable(a, var));
    EXPECT_EQ(rhs->getDecompile(), lit);
  }

  // Checks a then/else collection holds exactly '<var> == <lit>'.
  static void expectSingleVarEqLit(const hldb::AnyCollection *coll, std::string_view var,
                                   std::string_view lit) {
    ASSERT_NE(coll, nullptr) << "branch '" << var << " == " << lit << "' must be kept";
    ASSERT_EQ(coll->size(), 1u);
    expectVarEqLit(coll->front(), var, lit);
  }
};

TEST_F(IfElseConstraints4Test, RandIntVariables) {
  const hldb::ClassDefn *const a = findClass("a");
  ASSERT_NE(a, nullptr) << "class 'a' not found";
  for (std::string_view name : {"b1", "b2", "b3"}) {
    const hldb::Variable *const v = findVariable(a, name);
    ASSERT_NE(v, nullptr) << "'" << name << "' not found";
    EXPECT_EQ(v->getRandType(), vpiRand);
    ASSERT_NE(v->getTypespec(), nullptr);
    ASSERT_NE(v->getTypespec()->getActual(), nullptr);
    EXPECT_EQ(v->getTypespec()->getActual()->getAnyType(), hldb::AnyType::IntTypespec);
  }
}

TEST_F(IfElseConstraints4Test, ConstraintsInSourceOrder) {
  const hldb::ClassDefn *const a = findClass("a");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getConstraints(), nullptr);
  ASSERT_EQ(a->getConstraints()->size(), 3u);
  EXPECT_EQ(a->getConstraints()->at(0)->getName(), "c1");
  EXPECT_EQ(a->getConstraints()->at(1)->getName(), "c2");
  EXPECT_EQ(a->getConstraints()->at(2)->getName(), "c3");
}

TEST_F(IfElseConstraints4Test, SimpleEqualityConstraints) {
  expectVarEqLit(singleItem("c1"), "b1", "5");
  expectVarEqLit(singleItem("c2"), "b2", "3");
}

TEST_F(IfElseConstraints4Test, DanglingElseBindsToInnerIf) {
  const hldb::Any *const item = singleItem("c3");
  ASSERT_NE(item, nullptr) << "'c3' must hold exactly one (outer) item";
  ASSERT_EQ(item->getAnyType(), hldb::AnyType::ConstrIf) << "outer if has no else (Sec 18.5.6)";
  const hldb::ConstrIf *const outer = any_cast<hldb::ConstrIf>(item);
  expectVarEqLit(outer->getCondition(), "b1", "5");
  ASSERT_NE(outer->getConstraintExprs(), nullptr);
  ASSERT_EQ(outer->getConstraintExprs()->size(), 1u);
  const hldb::ConstrIfElse *const inner = any_cast<hldb::ConstrIfElse>(outer->getConstraintExprs()->front());
  ASSERT_NE(inner, nullptr) << "inner 'if ... else ...' owns the dangling else";
  expectVarEqLit(inner->getCondition(), "b2", "2");
}

TEST_F(IfElseConstraints4Test, InnerBranchesShouldBeKeptButAreDropped) {
  const hldb::ConstrIf *const outer = any_cast<hldb::ConstrIf>(singleItem("c3"));
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getConstraintExprs(), nullptr);
  ASSERT_EQ(outer->getConstraintExprs()->size(), 1u);
  const hldb::ConstrIfElse *const inner = any_cast<hldb::ConstrIfElse>(outer->getConstraintExprs()->front());
  ASSERT_NE(inner, nullptr);
  expectSingleVarEqLit(inner->getIfConstraintExprs(), "b3", "4");
  expectSingleVarEqLit(inner->getElseConstraintExprs(), "b3", "10");
}

TEST_F(IfElseConstraints4Test, EnvExtendsUvmEnvAndHoldsAHandle) {
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

TEST_F(IfElseConstraints4Test, TopModuleHoldsEnvHandle) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  const hldb::Variable *const environment = hldb::findByName<hldb::Variable>("environment", top->getVariables());
  ASSERT_NE(environment, nullptr);
  EXPECT_EQ(classOfHandle(environment), findClass("env"));
}

TEST_F(IfElseConstraints4Test, NoConstraintOrBindError) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "c3"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "env"), nullptr);
}

TEST_F(IfElseConstraints4Test, RandomizedB3Is10) {
  const hldb::ClassDefn *const a = findClass("a");
  ASSERT_NE(a, nullptr);
  const hldb::Variable *const b3 = findVariable(a, "b3");
  ASSERT_NE(b3, nullptr);
  if (m_design->getElaborated()) {
    const hldb::Constant *const value = b3->getValue<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "'b3' should hold its randomized value";
    EXPECT_EQ(value->getDecompile(), "10");
  }
}
}  // namespace hlc

#ifndef DAVTESTS_BULK_BUILD
int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
