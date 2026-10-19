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

// Source under test: tests/Google/chapter-18/18.5.5--uniqueness-constraints_1.sv
// (file name uses the 1800-2017 numbering; this is Sec 18.5.4 in 1800-2023)
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a;
//       rand int b1, b2;
//       constraint c1 { b1 inside {3, 10}; }
//       constraint c2 { b2 inside {3, 10}; }
//       constraint c3 { unique {b1, b2}; }
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
//         if((obj.b1 == 3 && obj.b2 == 10) ||
//            (obj.b2 == 3 && obj.b1 == 10))
//         begin
//           `uvm_info("RESULT", $sformatf("b1 = %0d b2 = %0d SUCCESS", obj.b1, obj.b2), UVM_LOW);
//         end else begin
//           `uvm_error("RESULT", $sformatf("b1 = %0d b2 = %0d FAILED", obj.b1, obj.b2));
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
// IEEE 1800-2023 Sec 18.5.4: 'unique {b1, b2}' forces b1 != b2. Members are
// rand (no randc allowed) and of equivalent type (both int). With c1/c2
// restricting both to {3, 10}, the only solutions are (3,10) and (10,3).
//
// What we have to check:
//   - 'rand int b1, b2;' -> two rand signed int variables, no initializer.
//   - Constraints c1, c2, c3 in source order.
//   - c1 = b1 inside {3, 10}, c2 = b2 inside {3, 10} (vpiInsideOp, operands
//     in BNF order).
//   - c3 = one Uniqueness item over exactly ref b1, ref b2 (bound members).
//   - env extends uvm_env; env::obj is a handle of class a.
//   - env has function 'new' and task 'run_phase'.
//   - module top declares 'environment' of class type env.
//   - No constraint / bind error.
//   - Runtime result: after randomize(), {b1, b2} == {3, 10} in some order.
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
//   - The body of run_phase (UVM reporting, not a constraint corner).
//   - Exact vpiConstType of the literals.

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
#include <hldb/uniqueness.h>
#include <hldb/variable.h>

#include <string_view>
#include <vector>

namespace hlc {
class UniquenessConstraints1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.5--uniqueness-constraints_1.hlc"}); }
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

  // Checks constraint 'consName' of class a is exactly '<varName> inside {3, 10}'.
  static void expectInside3And10(std::string_view consName, std::string_view varName) {
    const hldb::ClassDefn *const a = findClass("a");
    ASSERT_NE(a, nullptr);
    const hldb::Constraint *const cons = hldb::findByName<hldb::Constraint>(consName, a->getConstraints());
    ASSERT_NE(cons, nullptr) << "constraint '" << consName << "' not found";
    EXPECT_FALSE(cons->getPure());
    ASSERT_NE(cons->getConstraintItems(), nullptr);
    ASSERT_EQ(cons->getConstraintItems()->size(), 1u);
    const hldb::Operation *const op = any_cast<hldb::Operation>(cons->getConstraintItems()->front());
    ASSERT_NE(op, nullptr);
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
    EXPECT_EQ(lhs->getName(), varName);
    EXPECT_EQ(lhs->getActual(), findVariable(a, varName));
    EXPECT_EQ(v0->getDecompile(), "3");
    EXPECT_EQ(v1->getDecompile(), "10");
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

  // Members of 'unique { ... }', in source order. They are either the value
  // ranges themselves, or the operands of a single list operation that is
  // the only value range.
  static std::vector<const hldb::Any *> uniqueMembers(const hldb::Uniqueness *uq) {
    std::vector<const hldb::Any *> members;
    if (uq->getValueRanges() == nullptr) return members;
    if (uq->getValueRanges()->size() == 1) {
      const hldb::Operation *const list = any_cast<hldb::Operation>(uq->getValueRanges()->front());
      if (list != nullptr) {
        if (list->getOperands() != nullptr) {
          for (const hldb::Any *const m : *list->getOperands()) members.emplace_back(m);
        }
        return members;
      }
    }
    for (const hldb::Any *const m : *uq->getValueRanges()) members.emplace_back(m);
    return members;
  }
};

TEST_F(UniquenessConstraints1Test, RandAppliesToBothDeclarators) {
  const hldb::ClassDefn *const a = findClass("a");
  ASSERT_NE(a, nullptr) << "class 'a' not found";
  for (std::string_view name : {"b1", "b2"}) {
    const hldb::Variable *const v = findVariable(a, name);
    ASSERT_NE(v, nullptr) << "'" << name << "' not found";
    EXPECT_EQ(v->getRandType(), vpiRand) << name << " is rand (randc not allowed in unique group)";
    EXPECT_EQ(v->getValue(), nullptr);
    ASSERT_NE(v->getTypespec(), nullptr);
    ASSERT_NE(v->getTypespec()->getActual(), nullptr);
    ASSERT_EQ(v->getTypespec()->getActual()->getAnyType(), hldb::AnyType::IntTypespec)
        << "unique members must be of equivalent type";
    EXPECT_TRUE(v->getTypespec()->getActual<hldb::IntTypespec>()->getSigned());
  }
}

TEST_F(UniquenessConstraints1Test, ConstraintsInSourceOrder) {
  const hldb::ClassDefn *const a = findClass("a");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getConstraints(), nullptr);
  ASSERT_EQ(a->getConstraints()->size(), 3u);
  EXPECT_EQ(a->getConstraints()->at(0)->getName(), "c1");
  EXPECT_EQ(a->getConstraints()->at(1)->getName(), "c2");
  EXPECT_EQ(a->getConstraints()->at(2)->getName(), "c3");
}

TEST_F(UniquenessConstraints1Test, SetMembershipConstraints) {
  expectInside3And10("c1", "b1");
  expectInside3And10("c2", "b2");
}

TEST_F(UniquenessConstraints1Test, UniqueGroupIsB1AndB2) {
  const hldb::ClassDefn *const a = findClass("a");
  ASSERT_NE(a, nullptr);
  const hldb::Constraint *const c3 = hldb::findByName<hldb::Constraint>("c3", a->getConstraints());
  ASSERT_NE(c3, nullptr);
  ASSERT_NE(c3->getConstraintItems(), nullptr);
  ASSERT_EQ(c3->getConstraintItems()->size(), 1u);
  const hldb::Uniqueness *const uq = any_cast<hldb::Uniqueness>(c3->getConstraintItems()->front());
  ASSERT_NE(uq, nullptr) << "'unique {b1, b2}' is a uniqueness constraint";
  ASSERT_NE(uq->getValueRanges(), nullptr);
  const std::vector<const hldb::Any *> members = uniqueMembers(uq);
  ASSERT_EQ(members.size(), 2u) << "group has 2 members, so the constraint has effect";
  const hldb::RefObj *const m0 = any_cast<hldb::RefObj>(members[0]);
  const hldb::RefObj *const m1 = any_cast<hldb::RefObj>(members[1]);
  ASSERT_NE(m0, nullptr);
  ASSERT_NE(m1, nullptr);
  EXPECT_EQ(m0->getName(), "b1");
  EXPECT_EQ(m1->getName(), "b2");
  EXPECT_EQ(m0->getActual(), findVariable(a, "b1"));
  EXPECT_EQ(m1->getActual(), findVariable(a, "b2"));
}

TEST_F(UniquenessConstraints1Test, EnvExtendsUvmEnvAndHoldsAHandle) {
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

TEST_F(UniquenessConstraints1Test, TopModuleHoldsEnvHandle) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  const hldb::Variable *const environment = hldb::findByName<hldb::Variable>("environment", top->getVariables());
  ASSERT_NE(environment, nullptr);
  EXPECT_EQ(classOfHandle(environment), findClass("env"));
}

TEST_F(UniquenessConstraints1Test, NoConstraintOrBindError) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "c1"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "c2"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "c3"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "env"), nullptr);
}

TEST_F(UniquenessConstraints1Test, RandomizedPairIs3And10) {
  const hldb::ClassDefn *const a = findClass("a");
  ASSERT_NE(a, nullptr);
  const hldb::Variable *const b1 = findVariable(a, "b1");
  const hldb::Variable *const b2 = findVariable(a, "b2");
  ASSERT_NE(b1, nullptr);
  ASSERT_NE(b2, nullptr);
  if (m_design->getElaborated()) {
    const hldb::Constant *const v1 = b1->getValue<hldb::Constant>();
    const hldb::Constant *const v2 = b2->getValue<hldb::Constant>();
    ASSERT_NE(v1, nullptr) << "'b1' should hold its randomized value";
    ASSERT_NE(v2, nullptr) << "'b2' should hold its randomized value";
    const bool ok = (v1->getDecompile() == "3" && v2->getDecompile() == "10") ||
                    (v1->getDecompile() == "10" && v2->getDecompile() == "3");
    EXPECT_TRUE(ok) << "b1 = " << v1->getDecompile() << " b2 = " << v2->getDecompile();
  }
}

TEST_F(UniquenessConstraints1Test, ConstraintsShouldBeEnabledByDefaultButAreNot) {
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
}  // namespace hlc

#ifndef DAVTESTS_BULK_BUILD
int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
