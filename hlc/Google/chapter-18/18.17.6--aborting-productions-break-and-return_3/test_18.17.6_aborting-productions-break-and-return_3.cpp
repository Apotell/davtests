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
// SV source under test: tests/Google/chapter-18/18.17.6--aborting-productions-break-and-return_3.sv
// (IEEE 1800-2023 18.17 randsequence, 18.17.6 "Aborting productions--break and return")
//
//   /*
//   :name: aborting_productions_break_and_return_3
//   :description: return statement test
//   :tags: uvm-random uvm
//   */
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class env extends uvm_env;
//     int x;
//     int return_on = 1;
//
//     function new(string name, uvm_component parent = null);
//       super.new(name, parent);
//     endfunction
//
//     task run_phase(uvm_phase phase);
//       phase.raise_objection(this);
//       begin
//         randsequence( main )
//           main : first second third;
//           first : { x = x + 20; };
//           second : { if(return_on == 1) return; x = x + 10; };
//           third : { x = x + 5;};
//         endsequence
//
//         if(x == 25) begin
//           `uvm_info("RESULT", $sformatf("x = %0d SUCCESS", x), UVM_LOW);
//         end else begin
//           `uvm_error("RESULT", $sformatf("x = %0d FAILED", x));
//         end
//       end
//       phase.drop_objection(this);
//     endtask: run_phase
//
//   endclass
//
//   module top;
//
//     env environment;
//
//     initial begin
//       environment = new("env");
//       run_test();
//     end
//
//   endmodule
//
// ---------------------------------------------------------------------------
// WHAT WE HAVE TO CHECK (derived from the SV text + IEEE 1800-2023 only):
//   1. The file is legal -> no fatal/syntax/error (18.17.6: return may appear in any code block of a production).
//   2. env declares exactly 2 member(s), plain int (not rand): 'x' (no initializer) and 'return_on' initialized to the
//   literal 1.
//   3. The inner begin...end of run_phase holds exactly two statements in
//      order: the randsequence statement, then the if that checks x. The
//      production code blocks must NOT appear as plain statements of that
//      block.
//   4. 18.17: production identifiers (main, first, second, third) are local to the
//      randsequence -- not variables of run_phase, not subroutines.
//   5. RUN TIME (18.17.6): "The return statement aborts the generation of the
//      current production ... Sequence generation continues with the next
//      production": first adds 20, second returns before its '+10'
//      (return_on == 1), third adds 5, so x == 25.
//  ENV/TOP (UVM file):
//   - Class 'env' extends the UVM base class 'uvm_env' (8.13).
//   - Module 'top' is a top-level instance (23.3.1) and declares
//     'environment' as a handle of class 'env'.
//
// CHECKED (TEST_F below):
//   - items 1 to 4 and ENV/TOP.
//   - item 5 (run time) runs only on an elaborated design
//     (m_design->getElaborated()); otherwise it is skipped.
//
// NOT CHECKED (and why):
//   - The productions themselves (names, alternatives '|', weights ':=',
//     code blocks, case/repeat/rand join controls): IEEE 1800-2023 Clause 37 defines
//     NO VPI object for randsequence, and the hldb model has none either, so
//     there is no standard API to assert them against. Only what is
//     observable around the statement is checked (it is kept as exactly one
//     statement, its code blocks are not flattened into the enclosing body,
//     and production names do not leak into the enclosing scope).
//   - The UVM reporting macros (`uvm_info / `uvm_error) and the if
//     condition that prints SUCCESS/FAILED: UVM plumbing, not part of the
//     clause under test.
// ---------------------------------------------------------------------------

#include <hlc/Common/FileSystem.h>
#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
#include <hlc/Tests/Test.h>
#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/case_item.h>
#include <hldb/case_stmt.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/extends.h>
#include <hldb/function.h>
#include <hldb/if_else.h>
#include <hldb/if_stmt.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/return_stmt.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task.h>
#include <hldb/tf_call.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <filesystem>
#include <string_view>
#include <vector>

namespace {
// Collects, in source order, every statement of type T reachable from 'node' (nested blocks included).
template <typename T>
void collectStmts(const hldb::Any *node, std::vector<const T *> *out) {
  if (node == nullptr) return;
  if (const T *const match = any_cast<T>(node)) out->emplace_back(match);
  if (const hldb::Begin *const begin = any_cast<hldb::Begin>(node)) {
    if (const hldb::AnyCollection *const stmts = begin->getStmts()) {
      for (const hldb::Any *const stmt : *stmts) collectStmts(stmt, out);
    }
  } else if (const hldb::CaseStmt *const cs = any_cast<hldb::CaseStmt>(node)) {
    if (const hldb::CaseItemCollection *const items = cs->getCaseItems()) {
      for (const hldb::CaseItem *const item : *items) collectStmts(item->getStmt(), out);
    }
  } else if (const hldb::IfElse *const ifElse = any_cast<hldb::IfElse>(node)) {
    collectStmts(ifElse->getStmt(), out);
    collectStmts(ifElse->getElseStmt(), out);
  } else if (const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(node)) {
    collectStmts(ifStmt->getStmt(), out);
  }
}

template <typename T>
std::vector<const T *> collectStmts(const hldb::Any *node) {
  std::vector<const T *> found;
  collectStmts(node, &found);
  return found;
}

hldb::AnyType typespecKind(const hldb::RefTypespec *ref) {
  if ((ref == nullptr) || (ref->getActual() == nullptr)) return hldb::AnyType::Any;
  return ref->getActual()->getAnyType();
}

const hldb::ClassDefn *classOf(const hldb::Variable *var) {
  if ((var == nullptr) || (var->getTypespec() == nullptr)) return nullptr;
  const hldb::ClassTypespec *const ts = var->getTypespec()->getActual<hldb::ClassTypespec>();
  return (ts == nullptr) ? nullptr : ts->getClassDefn();
}
}  // namespace

namespace hlc {
class AbortingProductionsBreakAndReturn3Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.17.6--aborting-productions-break-and-return_3.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::ClassDefn *getClass(std::string_view name) const {
    return hldb::findByName<hldb::ClassDefn>(name, m_design->getAllClasses());
  }
  const hldb::Task *getRunPhase() const {
    const hldb::ClassDefn *const env = getClass("env");
    return (env == nullptr) ? nullptr : hldb::findByName<hldb::Task>("run_phase", env->getMethods());
  }
  // A subroutine declared at compilation-unit scope (design level or the $unit package).
  const hldb::TaskFunc *findUnitSubroutine(std::string_view name) const {
    if (const hldb::TaskFunc *const tf = hldb::findByName<hldb::TaskFunc>(name, m_design->getTaskFuncs())) return tf;
    if (m_design->getAllPackages() != nullptr) {
      for (const hldb::Package *const pkg : *m_design->getAllPackages()) {
        if (const hldb::TaskFunc *const tf = hldb::findByName<hldb::TaskFunc>(name, pkg->getTaskFuncs())) return tf;
      }
    }
    return nullptr;
  }
  const hldb::Function *getF() const { return any_cast<hldb::Function>(findUnitSubroutine("F")); }
};

// Check: legal file. Fatal and syntax diagnostics are checked for the whole
// compile. Error diagnostics are checked only where they point into this file:
// HLC reports errors inside the UVM 1800.2-2017 library itself (the same set as
// BuildUVMPkg), and those are not about this source.
TEST_F(AbortingProductionsBreakAndReturn3Test, CompilerReportsNoErrors) {
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
      if (std::filesystem::path(path).filename() != "18.17.6--aborting-productions-break-and-return_3.sv") continue;
      ++count;
      ADD_FAILURE() << "error " << error.getType() << " reported at " << path << ":" << loc.m_line << ":"
                    << loc.m_column;
    }
  }
  EXPECT_EQ(count, 0u) << "the source is legal SystemVerilog";
}

// Check: env extends uvm_env and declares its members.
TEST_F(AbortingProductionsBreakAndReturn3Test, EnvExtendsUvmEnvAndDeclaresMembers) {
  const hldb::ClassDefn *const env = getClass("env");
  ASSERT_NE(env, nullptr) << "class 'env' not found";
  ASSERT_NE(env->getExtends(), nullptr) << "env has an 'extends uvm_env' clause";
  ASSERT_NE(env->getExtends()->getClassTypespecs(), nullptr);
  ASSERT_EQ(env->getExtends()->getClassTypespecs()->size(), 1u);
  const hldb::ClassTypespec *const base =
      env->getExtends()->getClassTypespecs()->at(0)->getActual<hldb::ClassTypespec>();
  ASSERT_NE(base, nullptr);
  ASSERT_NE(base->getClassDefn(), nullptr) << "base class should bind to the uvm_env definition";
  EXPECT_EQ(base->getClassDefn()->getName(), "uvm_env");
  ASSERT_NE(env->getVariables(), nullptr);
  EXPECT_EQ(env->getVariables()->size(), 2u) << "env declares exactly: x, return_on";
  {
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>("x", env->getVariables());
    ASSERT_NE(var, nullptr) << "member 'x' not found";
    EXPECT_EQ(var->getRandType(), vpiNotRand) << "x is not declared rand";
    EXPECT_EQ(typespecKind(var->getTypespec()), hldb::AnyType::IntTypespec) << "x is 'int'";
  }
  {
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>("return_on", env->getVariables());
    ASSERT_NE(var, nullptr) << "member 'return_on' not found";
    EXPECT_EQ(var->getRandType(), vpiNotRand) << "return_on is not declared rand";
    EXPECT_EQ(typespecKind(var->getTypespec()), hldb::AnyType::IntTypespec) << "return_on is 'int'";
    const hldb::Any *const init = (var->getExpr() != nullptr) ? var->getExpr() : var->getValue();
    const hldb::Constant *const value = any_cast<hldb::Constant>(init);
    ASSERT_NE(value, nullptr) << "return_on is initialized to the literal 1";
    EXPECT_EQ(value->getDecompile(), "1");
  }
}

// Check: the run_phase block is "randsequence ... endsequence" followed by the result check.
TEST_F(AbortingProductionsBreakAndReturn3Test, RunPhaseBlockIsRandsequenceThenIf) {
  const hldb::Task *const runPhase = getRunPhase();
  ASSERT_NE(runPhase, nullptr) << "env::run_phase not found";
  // The inner begin...end is the block that directly holds the if statement.
  const hldb::Begin *block = nullptr;
  for (const hldb::Begin *const begin : collectStmts<hldb::Begin>(runPhase->getStmt())) {
    if (begin->getStmts() == nullptr) continue;
    for (const hldb::Any *const stmt : *begin->getStmts()) {
      if ((stmt != nullptr) && (stmt->getAnyType() == hldb::AnyType::IfElse)) block = begin;
    }
  }
  ASSERT_NE(block, nullptr) << "no begin...end block holds the if statement";
  ASSERT_EQ(block->getStmts()->size(), 2u) << "the block has two statements: the randsequence and the if";
  const hldb::Any *const first = block->getStmts()->at(0);
  ASSERT_NE(first, nullptr) << "the randsequence statement is kept";
  // There is no VPI object for randsequence (Clause 37), so only its presence and position are checked.
  EXPECT_NE(first->getAnyType(), hldb::AnyType::IfElse);
  EXPECT_NE(first->getAnyType(), hldb::AnyType::Assignment)
      << "the code-block assignments belong to their productions, not to the enclosing block";
  EXPECT_EQ(block->getStmts()->at(1)->getAnyType(), hldb::AnyType::IfElse) << "the if statement comes second";
}

// Check: production names are local to the randsequence (18.17) -- they are not declared in run_phase.
TEST_F(AbortingProductionsBreakAndReturn3Test, ProductionNamesAreLocalToTheRandsequence) {
  const hldb::Task *const scope = getRunPhase();
  ASSERT_NE(scope, nullptr) << "env::run_phase not found";
  for (std::string_view name : {"main", "first", "second", "third"}) {
    EXPECT_EQ(hldb::findByName<hldb::Variable>(name, scope->getVariables()), nullptr)
        << "production '" << name << "' must not become a variable of run_phase";
    EXPECT_EQ(findUnitSubroutine(name), nullptr) << "production '" << name << "' must not become a subroutine";
  }
}

// Check: module top declares 'env environment'.
TEST_F(AbortingProductionsBreakAndReturn3Test, TopDeclaresEnvironmentHandle) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  if (m_design->getElaborated()) {
    // IEEE 1800-2023 Sec 23.3.1: a top-level module is implicitly instantiated once, under its own name.
    EXPECT_NE(hldb::findByName<hldb::Module>("top", m_design->getTopModules()), nullptr)
        << "top-level instance 'top' not found";
  }
  const hldb::Variable *const environment = hldb::findByName<hldb::Variable>("environment", top->getVariables());
  ASSERT_NE(environment, nullptr) << "variable 'environment' not found in top";
  EXPECT_EQ(classOf(environment), getClass("env")) << "'environment' is declared with type 'env'";
}

// Check (run time): runs only on an elaborated design.
TEST_F(AbortingProductionsBreakAndReturn3Test, RuntimeReturnAbortsOnlyTheCurrentProduction) {
  if (!m_design->getElaborated()) {
    GTEST_SKIP() << "Design is not elaborated, so no run-time value is available. "
                    "IEEE 1800-2023 Sec 18.17.6: return aborts only second; first (+20) and third (+5) run, "
                    "so x == 25.";
  }
  const hldb::ClassDefn *const env = getClass("env");
  ASSERT_NE(env, nullptr);
  const hldb::Variable *const x = hldb::findByName<hldb::Variable>("x", env->getVariables());
  ASSERT_NE(x, nullptr);
  ASSERT_NE(x->getValue(), nullptr) << "x should hold its value after run_phase executed";
  const hldb::Constant *const value = x->getValue<hldb::Constant>();
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getDecompile(), "25");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
