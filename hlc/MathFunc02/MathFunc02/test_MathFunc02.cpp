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

// Tests for tests/MathFunc02/dut.sv (tags: MathFunc02)
//
//   module tb;
//     real res = 0.0;
//     initial begin
//       res = $clog2(1);
//       res = $ln(1.0);
//       res = $log10(1.0);
//       res = $exp(1.0);
//       res = $sqrt(1.0);
//       res = $floor(1.1);
//       res = $ceil(1.1);
//       $finish;
//     end
//   endmodule
//
// What is checked:
//   - module "tb" declares the module-level variable "res" of type "real"
//     (Sec 6.12). Sec 6.21: at module level the "static" keyword is
//     optional for an initialized variable ("int svar1 = 1; // static
//     keyword optional"), so COMP_MISSING_LIFETIME_KEYWORD must NOT be
//     reported for "res".
//   - "tb" has exactly one initial procedure whose body is a begin-end
//     block with 8 statements (Sec 9.2.1)
//   - Sec 20.8: the first 7 statements are blocking assignments
//     (Sec 10.4.1) "res = <SysFuncCall>" in source order:
//     $clog2 $ln $log10 $exp $sqrt $floor $ceil, every LHS bound to the
//     module-level Variable "res"
//   - every math call takes exactly one argument (Sec 20.8.1, Table 20-4)
//   - Sec 20.8.1: $clog2's argument is an integer literal ("1"), while the
//     Table 20-4 functions take real literals (vpiRealConst, Sec 5.7.2)
//   - return types: Table 20-4 functions "return a real result type"
//     (vpiRealFunc); $clog2 returns the ceiling of log2 "rounded up to an
//     integer value" (vpiIntFunc)
//   - Sec 20.2: the 8th statement "$finish;" is a system task call
//     (SysTaskCall) with no arguments
//   - no COMP_UNDEFINED_SYSTEM_FUNCTION for any standard name used
//
// What is NOT checked and why:
//   - Numeric results ($clog2(1) == 0, $floor(1.1) == 1.0, ...): runtime
//     evaluation, not part of the compiled object model.
//   - The shape of the module-level "= 0.0" initializer node: a modeling
//     choice not mandated by the standard.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string>
#include <vector>

namespace hlc {

class MathFunc02Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "MathFunc02.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTb() { return hldb::findByDefName<hldb::Module>("tb", m_design->getAllModules()); }

  static const hldb::Variable *getRes() {
    const hldb::Module *const tb = getTb();
    if (tb == nullptr || tb->getVariables() == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("res", tb->getVariables());
  }

  static const hldb::Begin *getBlock() {
    const hldb::Module *const tb = getTb();
    if (tb == nullptr || tb->getProcesses() == nullptr) return nullptr;
    for (const hldb::Process *const p : *tb->getProcesses()) {
      if (const hldb::Initial *const init = any_cast<hldb::Initial>(p)) return init->getStmt<hldb::Begin>();
    }
    return nullptr;
  }

  static std::vector<const hldb::Assignment *> getCallAssignments() {
    std::vector<const hldb::Assignment *> result;
    const hldb::Begin *const blk = getBlock();
    if (blk == nullptr || blk->getStmts() == nullptr) return result;
    for (const hldb::Any *const s : *blk->getStmts()) {
      const hldb::Assignment *const a = any_cast<hldb::Assignment>(s);
      if (a != nullptr && a->getRhs() != nullptr && a->getRhs()->getAnyType() == hldb::AnyType::SysFuncCall) {
        result.emplace_back(a);
      }
    }
    return result;
  }

  static const hldb::SysFuncCall *findCall(std::string_view name) {
    for (const hldb::Assignment *const a : getCallAssignments()) {
      const hldb::SysFuncCall *const call = a->getRhs<hldb::SysFuncCall>();
      if (call != nullptr && call->getName() == name) return call;
    }
    return nullptr;
  }

  static const std::vector<std::string> &expectedNames() {
    static const std::vector<std::string> kNames = {"$clog2", "$ln", "$log10", "$exp", "$sqrt", "$floor", "$ceil"};
    return kNames;
  }
};

// ---------------------------------------------------------------------------
// Sec 6.12 / 6.21: module-level real res = 0.0;
// ---------------------------------------------------------------------------

TEST_F(MathFunc02Test, ModuleDeclaresRealRes) {
  ASSERT_NE(getTb(), nullptr) << "module 'tb' not found";
  const hldb::Variable *const res = getRes();
  ASSERT_NE(res, nullptr) << "module-level 'real res' not found";
  ASSERT_NE(res->getTypespec(), nullptr);
  ASSERT_NE(res->getTypespec()->getActual(), nullptr);
  EXPECT_EQ(res->getTypespec()->getActual()->getAnyType(), hldb::AnyType::RealTypespec);
}

TEST_F(MathFunc02Test, ModuleLevelInitializerNeedsNoLifetimeKeyword) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_MISSING_LIFETIME_KEYWORD, "res"), nullptr)
      << "Sec 6.21: the static keyword is optional for a module-level initialized variable";
}

// ---------------------------------------------------------------------------
// Sec 9.2.1: initial begin ... end with 8 statements
// ---------------------------------------------------------------------------

TEST_F(MathFunc02Test, InitialBlockHasEightStatements) {
  const hldb::Module *const tb = getTb();
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getProcesses(), nullptr);
  EXPECT_EQ(tb->getProcesses()->size(), 1u);
  const hldb::Begin *const blk = getBlock();
  ASSERT_NE(blk, nullptr) << "initial body 'begin ... end' not found";
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 8u) << "7 assignments plus '$finish;'";
}

// ---------------------------------------------------------------------------
// Sec 20.8: the 7 math calls in source order
// ---------------------------------------------------------------------------

TEST_F(MathFunc02Test, SevenMathCallsInSourceOrder) {
  const hldb::Variable *const res = getRes();
  ASSERT_NE(res, nullptr);
  const std::vector<const hldb::Assignment *> assigns = getCallAssignments();
  const std::vector<std::string> &names = expectedNames();
  ASSERT_EQ(assigns.size(), names.size());
  for (size_t i = 0; i < names.size(); ++i) {
    EXPECT_TRUE(assigns[i]->getBlocking()) << names[i];
    const hldb::SysFuncCall *const call = assigns[i]->getRhs<hldb::SysFuncCall>();
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->getName(), names[i]) << "statement #" << i;
    const hldb::RefObj *const lhs = assigns[i]->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), "res");
    EXPECT_EQ(lhs->getActual(), res) << names[i] << ": LHS must bind to module-level 'res'";
  }
}

TEST_F(MathFunc02Test, EachCallTakesOneArgument) {
  for (const std::string &name : expectedNames()) {
    const hldb::SysFuncCall *const call = findCall(name);
    ASSERT_NE(call, nullptr) << name << " not found";
    ASSERT_NE(call->getArguments(), nullptr) << name;
    EXPECT_EQ(call->getArguments()->size(), 1u) << name;
  }
}

TEST_F(MathFunc02Test, Clog2ArgumentIsIntegerLiteral) {
  const hldb::SysFuncCall *const call = findCall("$clog2");
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::Constant *const c = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(c, nullptr);
  EXPECT_NE(c->getConstType(), vpiRealConst) << "Sec 20.8.1: '$clog2(1)' takes an integer argument";
  EXPECT_EQ(c->getDecompile(), "1");
}

TEST_F(MathFunc02Test, RealFunctionArgumentsAreRealLiterals) {
  for (const std::string &name : expectedNames()) {
    if (name == "$clog2") continue;
    const hldb::SysFuncCall *const call = findCall(name);
    ASSERT_NE(call, nullptr) << name << " not found";
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u);
    const hldb::Constant *const c = any_cast<hldb::Constant>(call->getArguments()->at(0));
    ASSERT_NE(c, nullptr) << name;
    EXPECT_EQ(c->getConstType(), vpiRealConst) << name << ": Sec 5.7.2 real literal";
  }
}

TEST_F(MathFunc02Test, ReturnTypes) {
  for (const std::string &name : expectedNames()) {
    const hldb::SysFuncCall *const call = findCall(name);
    ASSERT_NE(call, nullptr) << name << " not found";
    if (name == "$clog2") {
      EXPECT_EQ(call->getFuncType(), vpiIntFunc) << "Sec 20.8.1: $clog2 returns an integer value";
    } else {
      EXPECT_EQ(call->getFuncType(), vpiRealFunc) << name << ": Table 20-4 functions return a real result";
    }
  }
}

// ---------------------------------------------------------------------------
// Sec 20.2: $finish;
// ---------------------------------------------------------------------------

TEST_F(MathFunc02Test, LastStatementIsFinishTask) {
  const hldb::Begin *const blk = getBlock();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_FALSE(blk->getStmts()->empty());
  const hldb::SysTaskCall *const fin = any_cast<hldb::SysTaskCall>(blk->getStmts()->back());
  ASSERT_NE(fin, nullptr) << "'$finish;' is a system task call statement";
  EXPECT_EQ(fin->getName(), "$finish");
  EXPECT_TRUE(fin->getArguments() == nullptr || fin->getArguments()->empty()) << "'$finish' has no arguments";
}

TEST_F(MathFunc02Test, NoUndefinedSystemFunctionReported) {
  for (const std::string &name : expectedNames()) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_SYSTEM_FUNCTION, name), nullptr) << name;
  }
  EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_SYSTEM_FUNCTION, "$finish"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
