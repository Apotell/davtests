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

// ============================================================================
// SystemVerilog source under test:
// tests/Google/chapter-9/9.6.3--disable_fork.sv
// ----------------------------------------------------------------------------
// // Copyright (C) 2019-2021  The SymbiFlow Authors.
// //
// // Use of this source code is governed by a ISC-style
// // license that can be found in the LICENSE file or at
// // https://opensource.org/licenses/ISC
// //
// // SPDX-License-Identifier: ISC
//
// /*
// :name: disable_fork
// :description: disable fork
// :tags: 9.6.3
// */
// module fork_tb ();
// 	reg a = 0;
// 	reg b = 0;
// 	reg c = 0;
// 	initial begin
// 		fork
// 			#50 a = 1;
// 			#100 b = 1;
// 			#150 c = 1;
// 		join_any
// 		disable fork;
// 	end
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test (Sec 9.6.3, "Disabling fork-join"):
// "disable fork;" immediately terminates all descendant processes of the
// calling process that were spawned by any "fork ... join_any"/"join_none"
// still active, without terminating the calling process itself. Unlike
// "disable block_identifier;" (Sec 9.6.2, modeled by the Disable class),
// "disable fork;" names no target -- it is modeled by the distinct
// DisableFork class, a bare marker statement with no expr/stmt payload.
// "initial begin ... end" wraps two sibling statements (the fork/join_any
// block, then "disable fork;"), so a Begin scope sits directly on the
// Initial -- as in 9.6.1--wait_fork.sv, but here each fork branch is a
// single delay-controlled assignment rather than a "begin ... end" block.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - design has module "fork_tb" with exactly 3 variables: "a", "b", "c"
//   - each variable: LogicTypespec, unsigned, no declared ranges; not
//     duplicated in the net collection; initial value resolves to
//     Constant "0" (vpiUIntConst)
//   - module has exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (from "initial begin ... end") with
//     exactly 2 statements: the ForkStmt, then the DisableFork
//   - the ForkStmt's getJoinType() is vpiJoinAny -- "join_any", not
//     join/join_none
//   - the ForkStmt has exactly 3 statements, each a DelayControl directly
//     (no wrapping Begin, since each fork branch is a single statement):
//     "#50 a = 1;", "#100 b = 1;", "#150 c = 1;"
//   - the second statement of the Initial's Begin is a DisableFork with
//     getVpiType() == vpiDisableFork
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - Runtime effect of "disable fork;" (that any still-pending branch of
//     the "fork ... join_any" is terminated): HLC is a compiler/elaborator
//     with no simulation, so no execution ever happens for this test to
//     observe.
// ============================================================================

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/disable_fork.h>
#include <hldb/fork_stmt.h>
#include <hldb/initial.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class DisableForkTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.6.3--disable_fork.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() {
    return hldb::findByName<hldb::Module>("fork_tb", m_design->getAllModules());
  }

  static const hldb::Variable *getVariable(std::string_view name) {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr)
      return nullptr;
    return hldb::findByName<hldb::Variable>(name, mod->getVariables());
  }

  static const hldb::LogicTypespec *getVariableTypespec(std::string_view name) {
    const hldb::Variable *const v = getVariable(name);
    if (v == nullptr || v->getTypespec() == nullptr)
      return nullptr;
    return v->getTypespec()->getActual<hldb::LogicTypespec>();
  }

  static const hldb::Initial *getInitialProcess() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty())
      return nullptr;
    return any_cast<hldb::Initial>(mod->getProcesses()->at(0));
  }

  static const hldb::Begin *getInitialBegin() {
    const hldb::Initial *const init = getInitialProcess();
    if (init == nullptr)
      return nullptr;
    return init->getStmt<hldb::Begin>();
  }

  static const hldb::ForkStmt *getForkStmt() {
    const hldb::Begin *const begin = getInitialBegin();
    if (begin == nullptr || begin->getStmts() == nullptr || begin->getStmts()->empty())
      return nullptr;
    return any_cast<hldb::ForkStmt>(begin->getStmts()->at(0));
  }

  static const hldb::DisableFork *getDisableFork() {
    const hldb::Begin *const begin = getInitialBegin();
    if (begin == nullptr || begin->getStmts() == nullptr || begin->getStmts()->size() < 2)
      return nullptr;
    return any_cast<hldb::DisableFork>(begin->getStmts()->at(1));
  }

  static const hldb::DelayControl *getForkBranch(size_t index) {
    const hldb::ForkStmt *const fork = getForkStmt();
    if (fork == nullptr || fork->getStmts() == nullptr || fork->getStmts()->size() <= index)
      return nullptr;
    return any_cast<hldb::DelayControl>(fork->getStmts()->at(index));
  }
};

// --- module / variables "a", "b", "c" ---------------------------------------

TEST_F(DisableForkTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(DisableForkTest, ModuleHasThreeVariables) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 3u);
}

TEST_F(DisableForkTest, VariableAExists) { EXPECT_NE(getVariable("a"), nullptr); }
TEST_F(DisableForkTest, VariableBExists) { EXPECT_NE(getVariable("b"), nullptr); }
TEST_F(DisableForkTest, VariableCExists) { EXPECT_NE(getVariable("c"), nullptr); }

// "reg a/b/c" have no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8
// none must also appear in the module's net collection.
TEST_F(DisableForkTest, VariablesAreNotDuplicatedAsNets) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  if (mod->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("a", mod->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>("b", mod->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>("c", mod->getNets()), nullptr);
  }
}

TEST_F(DisableForkTest, VariableTypespecsAreLogicUnsignedWithNoRanges) {
  for (std::string_view name : {"a", "b", "c"}) {
    const hldb::LogicTypespec *const ts = getVariableTypespec(name);
    ASSERT_NE(ts, nullptr) << "'reg " << name << "' should resolve to LogicTypespec";
    EXPECT_FALSE(ts->getSigned()) << "6.8: 'reg " << name << "' with no 'signed' keyword defaults to unsigned";
    EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty())
        << "'reg " << name << "' declares no '[msb:lsb]' -- it is an implicit scalar bit";
  }
}

TEST_F(DisableForkTest, AllVariablesInitialValueIsConstantZero) {
  for (std::string_view name : {"a", "b", "c"}) {
    const hldb::Variable *const v = getVariable(name);
    ASSERT_NE(v, nullptr);
    const hldb::Constant *const val = v->getValue<hldb::Constant>();
    ASSERT_NE(val, nullptr) << "'reg " << name << " = 0' must carry an initial value";
    EXPECT_EQ(val->getDecompile(), "0");
    EXPECT_EQ(val->getConstType(), vpiUIntConst) << "bare decimal literal -> constType unsigned int (9)";
  }
}

// --- initial process / fork / disable fork structure -------------------------

TEST_F(DisableForkTest, ModuleHasOneProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
}

TEST_F(DisableForkTest, TheOneProcessIsInitial) { EXPECT_NE(getInitialProcess(), nullptr); }

// 9.6.3: "initial begin ... end" wraps two sibling statements (the fork
// block, then "disable fork;"), so the Initial's stmt must be a Begin.
TEST_F(DisableForkTest, InitialStmtIsBegin) {
  EXPECT_NE(getInitialBegin(), nullptr) << "'initial begin ... end' must produce a Begin scope";
}

TEST_F(DisableForkTest, InitialBeginHasTwoStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 2u) << "the fork block and 'disable fork;' are exactly 2 statements";
}

TEST_F(DisableForkTest, FirstStmtIsForkStmt) {
  EXPECT_NE(getForkStmt(), nullptr) << "9.6.3: the first statement must be a ForkStmt";
}

TEST_F(DisableForkTest, ForkJoinTypeIsJoinAny) {
  const hldb::ForkStmt *const fork = getForkStmt();
  ASSERT_NE(fork, nullptr);
  EXPECT_EQ(fork->getJoinType(), vpiJoinAny) << "9.6.3: 'join_any' must have JoinType vpiJoinAny";
}

TEST_F(DisableForkTest, ForkHasThreeStmts) {
  const hldb::ForkStmt *const fork = getForkStmt();
  ASSERT_NE(fork, nullptr);
  ASSERT_NE(fork->getStmts(), nullptr);
  EXPECT_EQ(fork->getStmts()->size(), 3u)
      << "'#50 a = 1;', '#100 b = 1;' and '#150 c = 1;' are exactly 3 statements";
}

// --- fork branches: #50 a=1; #100 b=1; #150 c=1; -----------------------------

TEST_F(DisableForkTest, Branch0DelayControlsAssignmentToA) {
  const hldb::DelayControl *const delay = getForkBranch(0);
  ASSERT_NE(delay, nullptr) << "'#50 a = 1;' should be a DelayControl directly bound to the fork branch";
  EXPECT_EQ(delay->getDelay<hldb::Constant>()->getDecompile(), "50") << "'#50' delay value";

  const hldb::Assignment *const assign = delay->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "'#50 a = ...' should control a blocking Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "'a = ...' uses the blocking assignment operator '='";

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariable("a"));

  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "1");
  EXPECT_EQ(rhs->getConstType(), vpiUIntConst);
}

TEST_F(DisableForkTest, Branch1DelayControlsAssignmentToB) {
  const hldb::DelayControl *const delay = getForkBranch(1);
  ASSERT_NE(delay, nullptr) << "'#100 b = 1;' should be a DelayControl directly bound to the fork branch";
  EXPECT_EQ(delay->getDelay<hldb::Constant>()->getDecompile(), "100") << "'#100' delay value";

  const hldb::Assignment *const assign = delay->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "'#100 b = ...' should control a blocking Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "'b = ...' uses the blocking assignment operator '='";

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "b");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariable("b"));

  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "1");
  EXPECT_EQ(rhs->getConstType(), vpiUIntConst);
}

TEST_F(DisableForkTest, Branch2DelayControlsAssignmentToC) {
  const hldb::DelayControl *const delay = getForkBranch(2);
  ASSERT_NE(delay, nullptr) << "'#150 c = 1;' should be a DelayControl directly bound to the fork branch";
  EXPECT_EQ(delay->getDelay<hldb::Constant>()->getDecompile(), "150") << "'#150' delay value";

  const hldb::Assignment *const assign = delay->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "'#150 c = ...' should control a blocking Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "'c = ...' uses the blocking assignment operator '='";

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "c");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariable("c"));

  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "1");
  EXPECT_EQ(rhs->getConstType(), vpiUIntConst);
}

// --- disable fork; ------------------------------------------------------------

TEST_F(DisableForkTest, SecondStmtIsDisableFork) {
  EXPECT_NE(getDisableFork(), nullptr) << "9.6.3: the second statement must be a DisableFork";
}

TEST_F(DisableForkTest, DisableForkHasVpiDisableForkType) {
  const hldb::DisableFork *const disableFork = getDisableFork();
  ASSERT_NE(disableFork, nullptr);
  EXPECT_EQ(disableFork->getVpiType(), vpiDisableFork);
}

// --- compiler diagnostics ----------------------------------------------------

TEST_F(DisableForkTest, ReferencesAreNotFailedBinds) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr) << "'a', 'b' and 'c' must all bind to their declarations";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
