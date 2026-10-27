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
// SV source under test: tests/Google/chapter-18/18.5.14.2--discarding-soft-constraints_5.sv
// (IEEE 1800-2017 18.5.14.2 == IEEE 1800-2023 18.5.13.2 "Disabling soft
// constraints", which is the text used here)
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a;
//       rand int b;
//
//       constraint c1 {
//           soft b > 4;
//           soft b < 12; }
//
//       constraint c2 { disable soft b; }
//       constraint c3 { soft b == 20; }
//   endclass
//
//   class env extends uvm_env;
//
//     a obj = new;
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
//   2. Class 'a' declares exactly 'rand int b' (vpiRand, int).
//   3. Class 'a' has exactly three constraints in syntactic order c1, c2,
//      c3 (37.34 detail 4) -- the order defines the priorities that the
//      disable relies on (18.5.13.1). Each: parent 'a', non-static
//      (vpiAutomatic == 1), active (18.9), in-body (vpiAccessType 0).
//   4. c1 holds, in order: [0] soft "b > 4" (vpiGtOp), [1] soft "b < 12"
//      (vpiLtOp), both vpiSoft == 1 and bound to a::b.
//   5. c2 holds exactly one item, a "soft disable" constraint expression
//      (37.38) whose expression is a reference bound to a::b.
//   6. c3 holds exactly one item: soft "b == 20" (vpiEqOp, vpiSoft == 1)
//      bound to a::b.
//   7. Class 'env' extends 'uvm_env'; member 'obj' is a non-rand handle of
//      class 'a'.
//   8. run_phase calls obj.randomize() exactly once, without 'with'
//      (37.42: vpiWith null).
//   9. Module 'top' declares 'environment' of class 'env'.
//  10. RUN TIME (18.5.13.2): "disable soft b" discards all LOWER priority
//      soft constraints that reference b, i.e. both items of c1. c3 is
//      declared after c2, so it has higher priority and survives. The only
//      remaining constraint is soft b == 20, so after randomize() b == 20.
//
// CHECKED (TEST_F below):
//   - items 1 to 9.
//   - item 10 (run time) runs only on an elaborated design
//     (m_design->getElaborated()); otherwise it is skipped.
//
// NOT CHECKED (and why):
//   - The UVM reporting macros and the SUCCESS/FAILED if statement: UVM
//     plumbing, not part of 18.5.13.2.
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
#include <hldb/soft_disable.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <filesystem>
#include <string>
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

// Checks that 'item' is "disable soft <var>;".
void expectSoftDisableOf(const hldb::Any *item, const hldb::Variable *var) {
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->getAnyType(), hldb::AnyType::SoftDisable) << "'disable soft' is a soft disable item (37.38)";
  const hldb::SoftDisable *const disable = any_cast<hldb::SoftDisable>(item);
  ASSERT_NE(disable, nullptr);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(disable->getExpr());
  ASSERT_NE(ref, nullptr) << "'disable soft' names the random variable '" << var->getName() << "'";
  EXPECT_EQ(ref->getName(), var->getName());
  EXPECT_EQ(ref->getActual(), static_cast<const hldb::Any *>(var))
      << "'" << ref->getName() << "' binds to the variable";
}
}  // namespace

namespace hlc {
class DiscardingSoftConstraints5Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.14.2--discarding-soft-constraints_5.hlc"}); }
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
TEST_F(DiscardingSoftConstraints5Test, CompilerReportsNoErrors) {
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
      if (std::filesystem::path(path).filename() != "18.5.14.2--discarding-soft-constraints_5.sv") continue;
      ++count;
      ADD_FAILURE() << "error " << error.getType() << " reported at " << path << ":" << loc.m_line << ":"
                    << loc.m_column;
    }
  }
  EXPECT_EQ(count, 0u) << "the source is legal SystemVerilog";
}

// Check 2: class 'a' with 'rand int b'.
TEST_F(DiscardingSoftConstraints5Test, ClassAHasRandIntB) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr) << "class 'a' not found";
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 1u);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", cls->getVariables());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getRandType(), vpiRand);
  EXPECT_EQ(typespecKind(b), hldb::AnyType::IntTypespec);
}

// Check 3: three constraints c1, c2, c3 in declaration order.
TEST_F(DiscardingSoftConstraints5Test, ConstraintsC1C2C3InDeclarationOrder) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getConstraints(), nullptr);
  ASSERT_EQ(cls->getConstraints()->size(), 3u);
  EXPECT_EQ(cls->getConstraints()->at(0)->getName(), "c1");
  EXPECT_EQ(cls->getConstraints()->at(1)->getName(), "c2");
  EXPECT_EQ(cls->getConstraints()->at(2)->getName(), "c3");
  for (const hldb::Constraint *const c : *cls->getConstraints()) {
    EXPECT_EQ(c->getParent(), static_cast<const hldb::Any *>(cls)) << c->getName();
    EXPECT_TRUE(c->getAutomatic()) << c->getName() << ": not static, so vpiAutomatic is 1 (37.34 detail 1)";
    EXPECT_TRUE(c->getIsConstraintEnabled()) << c->getName() << ": initially active (18.9)";
    EXPECT_EQ(c->getAccessType(), 0) << c->getName() << ": in-body, not extern";
  }
}

// Check 4: c1 is "soft b > 4; soft b < 12;".
TEST_F(DiscardingSoftConstraints5Test, C1HasTwoSoftItemsInOrder) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", cls->getVariables());
  ASSERT_NE(b, nullptr);
  const hldb::Constraint *const c1 = hldb::findByName<hldb::Constraint>("c1", cls->getConstraints());
  ASSERT_NE(c1, nullptr);
  ASSERT_NE(c1->getConstraintItems(), nullptr);
  ASSERT_EQ(c1->getConstraintItems()->size(), 2u);
  expectRelationOnLiteral(c1->getConstraintItems()->at(0), vpiGtOp, b, "4", true);
  expectRelationOnLiteral(c1->getConstraintItems()->at(1), vpiLtOp, b, "12", true);
}

// Check 5: c2 is "disable soft b;".
TEST_F(DiscardingSoftConstraints5Test, C2IsDisableSoftB) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", cls->getVariables());
  ASSERT_NE(b, nullptr);
  const hldb::Constraint *const c2 = hldb::findByName<hldb::Constraint>("c2", cls->getConstraints());
  ASSERT_NE(c2, nullptr);
  ASSERT_NE(c2->getConstraintItems(), nullptr);
  ASSERT_EQ(c2->getConstraintItems()->size(), 1u);
  expectSoftDisableOf(c2->getConstraintItems()->at(0), b);
}

// Check 6: c3 is "soft b == 20".
TEST_F(DiscardingSoftConstraints5Test, C3IsSoftBEqualsTwenty) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", cls->getVariables());
  ASSERT_NE(b, nullptr);
  const hldb::Constraint *const c3 = hldb::findByName<hldb::Constraint>("c3", cls->getConstraints());
  ASSERT_NE(c3, nullptr);
  ASSERT_NE(c3->getConstraintItems(), nullptr);
  ASSERT_EQ(c3->getConstraintItems()->size(), 1u);
  expectRelationOnLiteral(c3->getConstraintItems()->at(0), vpiEqOp, b, "20", true);
}

// Check 7: env extends uvm_env and holds a non-rand handle 'obj' of class 'a'.
TEST_F(DiscardingSoftConstraints5Test, EnvExtendsUvmEnvAndHoldsObj) {
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
  EXPECT_EQ(classOf(obj), getClass("a"));
  EXPECT_EQ(obj->getRandType(), vpiNotRand);
}

// Check 8: run_phase calls obj.randomize() once without 'with'.
TEST_F(DiscardingSoftConstraints5Test, RunPhaseRandomizesObj) {
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

// Check 9: module top declares 'env environment'.
TEST_F(DiscardingSoftConstraints5Test, TopDeclaresEnvironmentHandle) {
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

// Check 10 (run time): c2 discards c1, c3 is declared later and survives, so b == 20.
TEST_F(DiscardingSoftConstraints5Test, RuntimeBRandomizesToTwenty) {
  if (!m_design->getElaborated()) {
    GTEST_SKIP() << "Design is not elaborated, so no post-randomize value is available. "
                    "IEEE 1800-2023 Sec 18.5.13.2: disable soft b drops c1; the later soft b == 20 survives.";
  }
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", cls->getVariables());
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
