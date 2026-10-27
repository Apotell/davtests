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

// ---------------------------------------------------------------------------
// SV source under test: tests/Google/chapter-18/18.5.14.1--soft-constraint-priorities_1.sv
// (IEEE 1800-2017 18.5.14.1 == IEEE 1800-2023 18.5.13.1 "Soft constraint
// priorities", which is the text used here)
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a1;
//       rand int b;
//
//       constraint c1 {
//           soft b > 4;
//           soft b < 12; }
//   endclass
//
//   class a2 extends a1;
//       constraint c2 { soft b == 20; }
//   endclass
//
//   class env extends uvm_env;
//
//     a2 obj = new;
//
//     function new(string name, uvm_component parent = null);
//       super.new(name, parent);
//     endfunction
//
//     task run_phase(uvm_phase phase);
//       phase.raise_objection(this);
//       begin
//         obj.randomize();
//         if(obj.b == 20) begin
//           `uvm_info("RESULT", $sformatf("obj.b = %0d SUCCESS", obj.b), UVM_LOW);
//         end else begin
//           `uvm_error("RESULT", $sformatf("obj.b = %0d FAILED", obj.b));
//         end
//       end
//       phase.drop_objection(this);
//     endtask: run_phase
//
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
// ---------------------------------------------------------------------------
// WHAT WE HAVE TO CHECK (derived from the SV text + IEEE 1800-2023 only):
//   1. The file is legal -> no fatal/syntax/error.
//   2. a1 declares exactly 'rand int b' (soft constraints need a rand, not
//      randc, variable -- 18.5.13.1).
//   3. a1::c1 (parent a1, non-static -> vpiAutomatic == 1, active, in-body)
//      holds, in source order: [0] soft "b > 4", [1] soft "b < 12".
//   4. a2 extends a1 (extends binds to a1, a1 lists a2 as derived) and does
//      not redeclare b.
//   5. a2::c2 (parent a2, non-static, active, in-body) holds soft "b == 20"
//      whose b binds to the inherited a1::b.
//   6. env extends uvm_env; member 'obj' is a non-rand handle of class a2.
//   7. run_phase calls obj.randomize() exactly once, without 'with'.
//   8. Module 'top' declares 'environment' of class 'env'.
//   9. RUN TIME (18.5.13.1): priority is c2 (derived class) > c1[1] > c1[0].
//      The solver keeps c2 (b == 20), must discard c1[1] (b < 12 conflicts
//      with 20), and keeps c1[0] (20 > 4 holds), so obj.b == 20.
//
// CHECKED (TEST_F below):
//   - items 1 to 8.
//   - item 9 (run time) runs only on an elaborated design
//     (m_design->getElaborated()); otherwise it is skipped.
//
// NOT CHECKED (and why):
//   - The UVM reporting macros and the SUCCESS/FAILED if statement: UVM
//     plumbing, not part of 18.5.13.1.
// ---------------------------------------------------------------------------

#include <hlc/Common/FileSystem.h>
#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
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
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <algorithm>
#include <filesystem>
#include <string_view>
#include <vector>

namespace {
struct CallSite {
  const hldb::MethodFuncCall *m_call = nullptr;
  // Hierarchical reference "x.method(...)" that holds the call, if any.
  const hldb::RefObj *m_owner = nullptr;
};

// Collects, in source order, every method call named 'name' reachable from a statement.
void collectMethodCalls(const hldb::Any *node, const hldb::RefObj *owner, std::string_view name,
                        std::vector<CallSite> *out) {
  if (node == nullptr) return;
  if (const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(node)) {
    if (call->getName() == name) out->emplace_back(CallSite{call, owner});
  } else if (const hldb::RefObj *const ref = any_cast<hldb::RefObj>(node)) {
    if (const hldb::AnyCollection *const elems = ref->getPathElems()) {
      for (const hldb::Any *const elem : *elems) collectMethodCalls(elem, ref, name, out);
    }
  } else if (const hldb::Begin *const begin = any_cast<hldb::Begin>(node)) {
    if (const hldb::AnyCollection *const stmts = begin->getStmts()) {
      for (const hldb::Any *const stmt : *stmts) collectMethodCalls(stmt, nullptr, name, out);
    }
  } else if (const hldb::IfElse *const ifElse = any_cast<hldb::IfElse>(node)) {
    collectMethodCalls(ifElse->getStmt(), nullptr, name, out);
    collectMethodCalls(ifElse->getElseStmt(), nullptr, name, out);
  } else if (const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(node)) {
    collectMethodCalls(ifStmt->getStmt(), nullptr, name, out);
  }
}

// Name of the i-th element of a hierarchical reference, or "" when absent.
std::string_view pathElemName(const hldb::RefObj *owner, size_t index) {
  if ((owner == nullptr) || (owner->getPathElems() == nullptr) || (owner->getPathElems()->size() <= index)) return "";
  return owner->getPathElems()->at(index)->getName();
}

const hldb::ClassDefn *classOf(const hldb::Variable *var) {
  if ((var == nullptr) || (var->getTypespec() == nullptr)) return nullptr;
  const hldb::ClassTypespec *const ts = var->getTypespec()->getActual<hldb::ClassTypespec>();
  return (ts == nullptr) ? nullptr : ts->getClassDefn();
}

hldb::AnyType typespecKind(const hldb::Variable *var) {
  const hldb::RefTypespec *const ref = var->getTypespec();
  if ((ref == nullptr) || (ref->getActual() == nullptr)) return hldb::AnyType::Any;
  return ref->getActual()->getAnyType();
}

// Checks that 'item' is the relation "<var> <opType> <literal>" with the given soft flag.
void expectRelationOnLiteral(const hldb::Any *item, int32_t opType, const hldb::Variable *var, std::string_view literal,
                             bool soft) {
  const hldb::Operation *const op = any_cast<hldb::Operation>(item);
  ASSERT_NE(op, nullptr) << "constraint item should be an Operation";
  EXPECT_EQ(op->getOpType(), opType);
  EXPECT_EQ(op->getSoft(), soft) << "soft flag (37.38 vpiSoft)";
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(ref, nullptr) << "left operand should reference '" << var->getName() << "'";
  EXPECT_EQ(ref->getName(), var->getName());
  EXPECT_EQ(ref->getActual(), static_cast<const hldb::Any *>(var))
      << "'" << ref->getName() << "' binds to the variable";
  const hldb::Constant *const value = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(value, nullptr) << "right operand should be the literal " << literal;
  EXPECT_EQ(value->getDecompile(), literal);
}

void expectPlainConstraint(const hldb::Constraint *c, const hldb::ClassDefn *owner) {
  EXPECT_EQ(c->getParent(), static_cast<const hldb::Any *>(owner)) << c->getName() << ": parent class";
  EXPECT_TRUE(c->getAutomatic()) << c->getName() << ": not static, so vpiAutomatic is 1 (37.34 detail 1)";
  EXPECT_TRUE(c->getIsConstraintEnabled()) << c->getName() << ": initially active (18.9)";
  EXPECT_EQ(c->getAccessType(), 0) << c->getName() << ": in-body, not extern";
}
}  // namespace

namespace hlc {
class SoftConstraintPriorities1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.14.1--soft-constraint-priorities_1.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::ClassDefn *getClass(std::string_view name) const {
    return hldb::findByName<hldb::ClassDefn>(name, m_design->getAllClasses());
  }
};

// Check 1: legal file. Fatal and syntax diagnostics are checked for the whole
// compile. Error diagnostics are checked only where they point into this file:
// HLC reports errors inside the UVM 1800.2-2017 library itself (the same set as
// BuildUVMPkg), and those are not about this source.
TEST_F(SoftConstraintPriorities1Test, CompilerReportsNoErrors) {
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);

  const ErrorDefinition::ErrorMap &infos = ErrorDefinition::getErrorInfoMap();
  size_t count = 0;
  for (const Error &error : m_session->getErrorContainer()->getErrors()) {
    const ErrorDefinition::ErrorMap::const_iterator info = infos.find(error.getType());
    if (info == infos.cend() || info->second.m_severity > ErrorDefinition::ERROR) continue;
    for (const Location &loc : error.getLocations()) {
      const std::string_view path = FileSystem::toPath(loc.m_fileId);
      if (std::filesystem::path(path).filename() != "18.5.14.1--soft-constraint-priorities_1.sv") continue;
      ++count;
      ADD_FAILURE() << "error " << error.getType() << " reported at " << path << ":" << loc.m_line << ":"
                    << loc.m_column;
    }
  }
  EXPECT_EQ(count, 0u) << "the source is legal SystemVerilog";
}

// Check 2: a1 declares 'rand int b'.
TEST_F(SoftConstraintPriorities1Test, A1HasRandIntB) {
  const hldb::ClassDefn *const a1 = getClass("a1");
  ASSERT_NE(a1, nullptr) << "class 'a1' not found";
  ASSERT_NE(a1->getVariables(), nullptr);
  EXPECT_EQ(a1->getVariables()->size(), 1u);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", a1->getVariables());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getRandType(), vpiRand) << "soft constraints only on rand (not randc) variables (18.5.13.1)";
  EXPECT_EQ(typespecKind(b), hldb::AnyType::IntTypespec);
}

// Check 3: a1::c1 is "soft b > 4; soft b < 12;" in that order.
TEST_F(SoftConstraintPriorities1Test, A1C1HasTwoSoftItemsInOrder) {
  const hldb::ClassDefn *const a1 = getClass("a1");
  ASSERT_NE(a1, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", a1->getVariables());
  ASSERT_NE(b, nullptr);
  ASSERT_NE(a1->getConstraints(), nullptr);
  ASSERT_EQ(a1->getConstraints()->size(), 1u);
  const hldb::Constraint *const c1 = a1->getConstraints()->at(0);
  EXPECT_EQ(c1->getName(), "c1");
  expectPlainConstraint(c1, a1);
  ASSERT_NE(c1->getConstraintItems(), nullptr);
  ASSERT_EQ(c1->getConstraintItems()->size(), 2u);
  expectRelationOnLiteral(c1->getConstraintItems()->at(0), vpiGtOp, b, "4", true);
  expectRelationOnLiteral(c1->getConstraintItems()->at(1), vpiLtOp, b, "12", true);
}

// Check 4: a2 extends a1 and does not redeclare b.
TEST_F(SoftConstraintPriorities1Test, A2ExtendsA1AndInheritsB) {
  const hldb::ClassDefn *const a1 = getClass("a1");
  const hldb::ClassDefn *const a2 = getClass("a2");
  ASSERT_NE(a1, nullptr);
  ASSERT_NE(a2, nullptr) << "class 'a2' not found";
  ASSERT_NE(a2->getExtends(), nullptr);
  ASSERT_NE(a2->getExtends()->getClassTypespecs(), nullptr);
  ASSERT_EQ(a2->getExtends()->getClassTypespecs()->size(), 1u);
  const hldb::ClassTypespec *const base =
      a2->getExtends()->getClassTypespecs()->at(0)->getActual<hldb::ClassTypespec>();
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->getClassDefn(), a1);
  ASSERT_NE(a1->getDerivedClasses(), nullptr);
  EXPECT_NE(std::find(a1->getDerivedClasses()->begin(), a1->getDerivedClasses()->end(), a2),
            a1->getDerivedClasses()->end());
  EXPECT_EQ(hldb::findByName<hldb::Variable>("b", a2->getVariables()), nullptr) << "b is inherited from a1";
}

// Check 5: a2::c2 is "soft b == 20" on the inherited a1::b.
TEST_F(SoftConstraintPriorities1Test, A2C2IsSoftBEqualsTwentyOnInheritedB) {
  const hldb::ClassDefn *const a1 = getClass("a1");
  const hldb::ClassDefn *const a2 = getClass("a2");
  ASSERT_NE(a1, nullptr);
  ASSERT_NE(a2, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", a1->getVariables());
  ASSERT_NE(b, nullptr);
  ASSERT_NE(a2->getConstraints(), nullptr);
  ASSERT_EQ(a2->getConstraints()->size(), 1u);
  const hldb::Constraint *const c2 = a2->getConstraints()->at(0);
  EXPECT_EQ(c2->getName(), "c2");
  expectPlainConstraint(c2, a2);
  ASSERT_NE(c2->getConstraintItems(), nullptr);
  ASSERT_EQ(c2->getConstraintItems()->size(), 1u);
  expectRelationOnLiteral(c2->getConstraintItems()->at(0), vpiEqOp, b, "20", true);
}

// Check 6: env extends uvm_env and holds a non-rand handle 'obj' of class a2.
TEST_F(SoftConstraintPriorities1Test, EnvExtendsUvmEnvAndHoldsObj) {
  const hldb::ClassDefn *const env = getClass("env");
  ASSERT_NE(env, nullptr) << "class 'env' not found";
  ASSERT_NE(env->getExtends(), nullptr);
  ASSERT_NE(env->getExtends()->getClassTypespecs(), nullptr);
  ASSERT_EQ(env->getExtends()->getClassTypespecs()->size(), 1u);
  const hldb::ClassTypespec *const base =
      env->getExtends()->getClassTypespecs()->at(0)->getActual<hldb::ClassTypespec>();
  ASSERT_NE(base, nullptr);
  ASSERT_NE(base->getClassDefn(), nullptr);
  EXPECT_EQ(base->getClassDefn()->getName(), "uvm_env");
  const hldb::Variable *const obj = hldb::findByName<hldb::Variable>("obj", env->getVariables());
  ASSERT_NE(obj, nullptr) << "env member 'obj' not found";
  EXPECT_EQ(classOf(obj), getClass("a2")) << "obj is declared with type a2";
  EXPECT_EQ(obj->getRandType(), vpiNotRand);
}

// Check 7: run_phase calls obj.randomize() once without 'with'.
TEST_F(SoftConstraintPriorities1Test, RunPhaseRandomizesObj) {
  const hldb::ClassDefn *const env = getClass("env");
  ASSERT_NE(env, nullptr);
  const hldb::Task *const runPhase = hldb::findByName<hldb::Task>("run_phase", env->getMethods());
  ASSERT_NE(runPhase, nullptr) << "env::run_phase not found";
  std::vector<CallSite> calls;
  collectMethodCalls(runPhase->getStmt(), nullptr, "randomize", &calls);
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(pathElemName(calls[0].m_owner, 0), "obj");
  EXPECT_EQ(calls[0].m_call->getWith(), nullptr);
}

// Check 8: module top declares 'env environment'.
TEST_F(SoftConstraintPriorities1Test, TopDeclaresEnvironmentHandle) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  if (m_design->getElaborated()) {
    // IEEE 1800-2023 Sec 23.3.1: a top-level module is implicitly instantiated once, under its own name.
    EXPECT_NE(hldb::findByName<hldb::Module>("top", m_design->getTopModules()), nullptr)
        << "top-level instance 'top' not found";
  }
  const hldb::Variable *const environment = hldb::findByName<hldb::Variable>("environment", top->getVariables());
  ASSERT_NE(environment, nullptr);
  EXPECT_EQ(classOf(environment), getClass("env"));
}

// Check 9 (run time): derived-class soft c2 wins, b == 20.
TEST_F(SoftConstraintPriorities1Test, RuntimeBRandomizesToTwenty) {
  if (!m_design->getElaborated()) {
    GTEST_SKIP() << "Design is not elaborated, so no post-randomize value is available. "
                    "IEEE 1800-2023 Sec 18.5.13.1: derived-class soft c2 has highest priority -> b == 20.";
  }
  const hldb::ClassDefn *const a1 = getClass("a1");
  ASSERT_NE(a1, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", a1->getVariables());
  ASSERT_NE(b, nullptr);
  ASSERT_NE(b->getValue(), nullptr) << "b should hold its randomized value";
  const hldb::Constant *const value = b->getValue<hldb::Constant>();
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getDecompile(), "20");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
