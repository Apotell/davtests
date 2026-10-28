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
// SV source under test: tests/Google/chapter-18/18.16--random-weighted-case-randcase_1.sv
// (IEEE 1800-2023 18.16 "Random weighted case--randcase")
//
//   /*
//   :name: random_weighted_case_randcase_1
//   :description: randcase test
//   :tags: uvm-random uvm
//   */
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   function int F();
//       int a;
//       randcase
//           0 : a = 5;
//           1 : a = 10;
//       endcase
//       return a;
//   endfunction
//
//
//   class env extends uvm_env;
//     int x;
//
//     function new(string name, uvm_component parent = null);
//       super.new(name, parent);
//     endfunction
//
//     task run_phase(uvm_phase phase);
//       phase.raise_objection(this);
//       begin
//         x = F();
//
//         if(x == 10) begin
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
//   1. The file is legal -> no fatal/syntax/error.
//   2. Function 'F' at compilation-unit scope: not a class method, returns
//      int, no argument, exactly one local 'int a'.
//   3. F's body: the randcase, then 'return a;' bound to a.
//   4. The randcase: no case expression, two items in order:
//      weight 0 : "a = 5;", weight 1 : "a = 10;".
//   5. env declares exactly one member, 'int x' (not rand).
//   6. run_phase assigns "x = F();": the rhs is a call named F with no
//      argument, bound to the compilation-unit function F.
//   7. RUN TIME (18.16): the zero-weight branch is never taken, so F()
//      returns 10 and x == 10 (deterministic).
//  ENV/TOP (UVM file):
//   - Class 'env' extends the UVM base class 'uvm_env' (8.13).
//   - Module 'top' is a top-level instance (23.3.1) and declares
//     'environment' as a handle of class 'env'.
//
//  LOCAL RAND TYPE: the local 'a' has vpiRandType == vpiNotRand
//   (37.17 details 15/22: the value is one of vpiRand/vpiRandC/vpiNotRand).
//
// CHECKED (TEST_F below):
//   - items 1 to 6 and ENV/TOP.
//   - item 7 (run time) runs only on an elaborated design
//     (m_design->getElaborated()); otherwise it is skipped.
//
// NOT CHECKED (and why):
//   - The UVM reporting macros (`uvm_info / `uvm_error) and the if
//     condition that prints SUCCESS/FAILED: UVM plumbing, not part of the
//     clause under test.
//   - Whether the case statement is marked as a randcase: IEEE 37.72
//     defines no randcase property.
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
// Top-level statements of a subroutine body (a multi-statement body may be wrapped in begin...end).
std::vector<const hldb::Any *> topStmts(const hldb::Any *body) {
  std::vector<const hldb::Any *> stmts;
  if (const hldb::Begin *const begin = any_cast<hldb::Begin>(body)) {
    if (begin->getStmts() != nullptr) {
      for (const hldb::Any *const stmt : *begin->getStmts()) stmts.emplace_back(stmt);
    }
  } else if (body != nullptr) {
    stmts.emplace_back(body);
  }
  return stmts;
}

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

// The assignment whose left-hand side is named 'lhs', or nullptr.
const hldb::Assignment *findAssignmentTo(const std::vector<const hldb::Assignment *> &assigns, std::string_view lhs) {
  for (const hldb::Assignment *const assign : assigns) {
    if ((assign->getLhs() != nullptr) && (assign->getLhs()->getName() == lhs)) return assign;
  }
  return nullptr;
}

size_t argumentCount(const hldb::TFCall *call) {
  return (call->getArguments() == nullptr) ? 0 : call->getArguments()->size();
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

// Checks that 'expr' is a reference named like 'var' that binds to 'var'.
void expectRefTo(const hldb::Any *expr, const hldb::Variable *var) {
  ASSERT_NE(expr, nullptr);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
  ASSERT_NE(ref, nullptr) << "operand should be a reference to '" << var->getName() << "'";
  EXPECT_EQ(ref->getName(), var->getName());
  EXPECT_EQ(ref->getActual(), static_cast<const hldb::Any *>(var))
      << "reference '" << ref->getName() << "' should bind to the declared variable";
}

// Checks that 'item' is the randcase branch "<weight> : <var> = <value>;" with a literal weight.
void expectLiteralWeightBranch(const hldb::CaseItem *item, std::string_view weight, const hldb::Variable *var,
                               std::string_view value) {
  ASSERT_NE(item, nullptr);
  ASSERT_NE(item->getExprs(), nullptr);
  ASSERT_EQ(item->getExprs()->size(), 1u) << "a randcase item has exactly one weight expression (Syntax 18-12)";
  const hldb::Constant *const w = any_cast<hldb::Constant>(item->getExprs()->at(0));
  ASSERT_NE(w, nullptr) << "the weight is the literal " << weight;
  EXPECT_EQ(w->getDecompile(), weight);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(item->getStmt());
  ASSERT_NE(assign, nullptr) << "the branch statement is '" << var->getName() << " = " << value << ";'";
  expectRefTo(assign->getLhs(), var);
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(assign->getRhs());
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), value);
}
}  // namespace

namespace hlc {
class RandomWeightedCaseRandcase1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.16--random-weighted-case-randcase_1.hlc"}); }
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
TEST_F(RandomWeightedCaseRandcase1Test, CompilerReportsNoErrors) {
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
      if (std::filesystem::path(path).filename() != "18.16--random-weighted-case-randcase_1.sv") continue;
      ++count;
      ADD_FAILURE() << "error " << error.getType() << " reported at " << path << ":" << loc.m_line << ":"
                    << loc.m_column;
    }
  }
  EXPECT_EQ(count, 0u) << "the source is legal SystemVerilog";
}

// Check: compilation-unit function 'F' -- int return, no arguments, one local int 'a'.
TEST_F(RandomWeightedCaseRandcase1Test, FunctionFSignatureAndLocalVariable) {
  const hldb::Function *const f = getF();
  ASSERT_NE(f, nullptr) << "function 'F' declared at compilation-unit scope not found";
  EXPECT_FALSE(f->getMethod()) << "F is not a class method";
  EXPECT_EQ(typespecKind(f->getReturn()), hldb::AnyType::IntTypespec) << "F returns int";
  EXPECT_TRUE((f->getIODecls() == nullptr) || f->getIODecls()->empty()) << "F() takes no argument";
  ASSERT_NE(f->getVariables(), nullptr);
  ASSERT_EQ(f->getVariables()->size(), 1u) << "F declares exactly one local variable, 'a'";
  const hldb::Variable *const local = f->getVariables()->at(0);
  EXPECT_EQ(local->getName(), "a");
  EXPECT_EQ(typespecKind(local->getTypespec()), hldb::AnyType::IntTypespec) << "a is 'int'";
}

// Check: the function-local variable is not rand (37.17 details 15/22).
TEST_F(RandomWeightedCaseRandcase1Test, LocalVariableRandTypeIsNotRand) {
  const hldb::Function *const f = getF();
  ASSERT_NE(f, nullptr);
  const hldb::Variable *const local = hldb::findByName<hldb::Variable>("a", f->getVariables());
  ASSERT_NE(local, nullptr) << "local variable 'a' not found";
  EXPECT_EQ(local->getRandType(), vpiNotRand)
      << "IEEE 1800-2023 Sec 37.17 details 15/22: vpiRandType is one of vpiRand, vpiRandC or vpiNotRand; "
         "a plain local 'a' is vpiNotRand";
}

// Check: F's body is "randcase ... ; return a;" -- two statements, in that order.
TEST_F(RandomWeightedCaseRandcase1Test, FunctionFBodyIsRandcaseThenReturn) {
  const hldb::Function *const f = getF();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getVariables(), nullptr);
  const hldb::Variable *const local = hldb::findByName<hldb::Variable>("a", f->getVariables());
  ASSERT_NE(local, nullptr);
  const std::vector<const hldb::Any *> stmts = topStmts(f->getStmt());
  ASSERT_EQ(stmts.size(), 2u) << "F's body has two statements: the randcase and the return";
  ASSERT_NE(stmts[0], nullptr);
  EXPECT_EQ(stmts[0]->getAnyType(), hldb::AnyType::CaseStmt) << "the first statement is the randcase";
  const hldb::ReturnStmt *const ret = any_cast<hldb::ReturnStmt>(stmts[1]);
  ASSERT_NE(ret, nullptr) << "the last statement is 'return a;'";
  expectRefTo(ret->getCondition(), local);
}

// Check: "randcase 0 : a = 5; 1 : a = 10; endcase".
TEST_F(RandomWeightedCaseRandcase1Test, RandcaseHasZeroAndOneWeightBranches) {
  const hldb::Function *const f = getF();
  ASSERT_NE(f, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", f->getVariables());
  ASSERT_NE(a, nullptr);
  const std::vector<const hldb::CaseStmt *> cases = collectStmts<hldb::CaseStmt>(f->getStmt());
  ASSERT_EQ(cases.size(), 1u) << "F contains exactly one randcase";
  EXPECT_EQ(cases[0]->getCondition(), nullptr) << "randcase has no case expression (Syntax 18-12)";
  ASSERT_NE(cases[0]->getCaseItems(), nullptr);
  ASSERT_EQ(cases[0]->getCaseItems()->size(), 2u);
  expectLiteralWeightBranch(cases[0]->getCaseItems()->at(0), "0", a, "5");
  expectLiteralWeightBranch(cases[0]->getCaseItems()->at(1), "1", a, "10");
}

// Check: env extends uvm_env and declares its members.
TEST_F(RandomWeightedCaseRandcase1Test, EnvExtendsUvmEnvAndDeclaresMembers) {
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
  EXPECT_EQ(env->getVariables()->size(), 1u) << "env declares exactly: x";
  {
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>("x", env->getVariables());
    ASSERT_NE(var, nullptr) << "member 'x' not found";
    EXPECT_EQ(var->getRandType(), vpiNotRand) << "x is not declared rand";
    EXPECT_EQ(typespecKind(var->getTypespec()), hldb::AnyType::IntTypespec) << "x is 'int'";
  }
}

// Check: run_phase assigns "x = F();" and the call binds to the compilation-unit function F.
TEST_F(RandomWeightedCaseRandcase1Test, RunPhaseAssignsXFromCallOfF) {
  const hldb::Task *const runPhase = getRunPhase();
  ASSERT_NE(runPhase, nullptr) << "env::run_phase not found";
  const hldb::Assignment *const assign = findAssignmentTo(collectStmts<hldb::Assignment>(runPhase->getStmt()), "x");
  ASSERT_NE(assign, nullptr) << "'x = F();' not found";
  const hldb::TFCall *const call = any_cast<hldb::TFCall>(assign->getRhs());
  ASSERT_NE(call, nullptr) << "x is assigned the result of a call";
  EXPECT_EQ(call->getName(), "F");
  ASSERT_NE(getF(), nullptr);
  EXPECT_EQ(call->getTaskFunc(), static_cast<const hldb::TaskFunc *>(getF())) << "the call binds to function F";
  EXPECT_EQ(argumentCount(call), 0u) << "F() is called without arguments";
}

// Check: module top declares 'env environment'.
TEST_F(RandomWeightedCaseRandcase1Test, TopDeclaresEnvironmentHandle) {
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
TEST_F(RandomWeightedCaseRandcase1Test, RuntimeZeroWeightBranchIsNeverTaken) {
  if (!m_design->getElaborated()) {
    GTEST_SKIP() << "Design is not elaborated, so no run-time value of x is available. "
                    "IEEE 1800-2023 Sec 18.16: a zero-weight branch is never taken, so F() returns 10: x == 10.";
  }
  const hldb::ClassDefn *const env = getClass("env");
  ASSERT_NE(env, nullptr);
  const hldb::Variable *const x = hldb::findByName<hldb::Variable>("x", env->getVariables());
  ASSERT_NE(x, nullptr);
  ASSERT_NE(x->getValue(), nullptr) << "x should hold its value after run_phase executed";
  const hldb::Constant *const value = x->getValue<hldb::Constant>();
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getDecompile(), "10");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
