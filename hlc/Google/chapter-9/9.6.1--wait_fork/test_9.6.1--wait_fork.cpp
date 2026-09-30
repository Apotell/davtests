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

// Tests for 9.6.1--wait_fork.sv (tags: 9.6.1)
//   module fork_tb ();
//     reg a = 0;
//     reg b = 0;
//     initial begin
//       fork
//         begin
//           #50 a = 1;
//           #50 a = 0;
//           #50 a = 1;
//         end
//         begin
//           #50 b = 1;
//           #50 b = 0;
//           #50 b = 1;
//         end
//       join_none
//       wait fork;
//     end
//   endmodule
//
// IEEE 1800-2023 Sec 9.6.1 "wait fork statement": "wait fork;" blocks the
// calling process until all its descendant processes spawned by a
// "fork...join_none" (or join_any) have completed. It carries no
// condition expression -- unlike the "wait (expr) stmt;" form modeled by
// the same Waits base class -- so it must produce a bare WaitFork node
// with a null controlled statement. "initial begin ... end" wraps two
// sibling statements (the fork/join_none block, then "wait fork;"), so a
// Begin scope sits directly on the Initial, distinct from
// 9.3.2--parallel_block_join.sv where "fork...join" alone is the
// Initial's single statement.
//
// Checked:
//   - design has module "fork_tb" with exactly 2 variables: "a", "b"
//   - each variable: LogicTypespec, unsigned, no declared ranges; not
//     duplicated in the net collection; initial value resolves to
//     Constant "0" (vpiUIntConst)
//   - module has exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (from "initial begin ... end") with
//     exactly 2 statements: the ForkStmt, then the WaitFork
//   - the ForkStmt's getJoinType() is vpiJoinNone; it has exactly 2
//     statements, each a Begin (the "begin ... end" fork branches)
//   - branch 0 has 3 statements, each a DelayControl ("#50") controlling
//     a blocking Assignment to "a", cycling 1, 0, 1
//   - branch 1 has 3 statements, each a DelayControl ("#50") controlling
//     a blocking Assignment to "b", cycling 1, 0, 1
//   - the second statement of the Initial's Begin is a WaitFork with a
//     null controlled statement ("wait fork;" has no condition/body)

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
#include <hldb/wait_fork.h>

namespace hlc {

class WaitForkTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.6.1--wait_fork.hlc"}); }
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

  static const hldb::WaitFork *getWaitFork() {
    const hldb::Begin *const begin = getInitialBegin();
    if (begin == nullptr || begin->getStmts() == nullptr || begin->getStmts()->size() < 2)
      return nullptr;
    return any_cast<hldb::WaitFork>(begin->getStmts()->at(1));
  }

  static const hldb::Begin *getForkBranch(size_t index) {
    const hldb::ForkStmt *const fork = getForkStmt();
    if (fork == nullptr || fork->getStmts() == nullptr || fork->getStmts()->size() <= index)
      return nullptr;
    return any_cast<hldb::Begin>(fork->getStmts()->at(index));
  }

  static const hldb::DelayControl *getBranchDelay(size_t branchIndex, size_t stmtIndex) {
    const hldb::Begin *const branch = getForkBranch(branchIndex);
    if (branch == nullptr || branch->getStmts() == nullptr || branch->getStmts()->size() <= stmtIndex) {
      return nullptr;
    }
    return any_cast<hldb::DelayControl>(branch->getStmts()->at(stmtIndex));
  }
};

// --- module / variables "a", "b" ------------------------------------------

TEST_F(WaitForkTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(WaitForkTest, ModuleHasTwoVariables) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 2u);
}

TEST_F(WaitForkTest, VariableAExists) { EXPECT_NE(getVariable("a"), nullptr); }
TEST_F(WaitForkTest, VariableBExists) { EXPECT_NE(getVariable("b"), nullptr); }

// "reg a/b" have no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8
// neither must also appear in the module's net collection.
TEST_F(WaitForkTest, VariablesAreNotDuplicatedAsNets) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  if (mod->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("a", mod->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>("b", mod->getNets()), nullptr);
  }
}

TEST_F(WaitForkTest, VariableTypespecsAreLogicUnsignedWithNoRanges) {
  for (std::string_view name : {"a", "b"}) {
    const hldb::LogicTypespec *const ts = getVariableTypespec(name);
    ASSERT_NE(ts, nullptr) << "'reg " << name << "' should resolve to LogicTypespec";
    EXPECT_FALSE(ts->getSigned()) << "6.8: 'reg " << name << "' with no 'signed' keyword defaults to unsigned";
    EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty())
        << "'reg " << name << "' declares no '[msb:lsb]' -- it is an implicit scalar bit";
  }
}

TEST_F(WaitForkTest, AllVariablesInitialValueIsConstantZero) {
  for (std::string_view name : {"a", "b"}) {
    const hldb::Variable *const v = getVariable(name);
    ASSERT_NE(v, nullptr);
    const hldb::Constant *const val = v->getValue<hldb::Constant>();
    ASSERT_NE(val, nullptr) << "'reg " << name << " = 0' must carry an initial value";
    EXPECT_EQ(val->getDecompile(), "0");
    EXPECT_EQ(val->getConstType(), vpiUIntConst) << "bare decimal literal -> constType unsigned int (9)";
  }
}

// --- initial process / fork / wait fork structure --------------------------

TEST_F(WaitForkTest, ModuleHasOneProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
}

TEST_F(WaitForkTest, TheOneProcessIsInitial) { EXPECT_NE(getInitialProcess(), nullptr); }

// 9.6.1: "initial begin ... end" wraps two sibling statements (the fork
// block, then "wait fork;"), so the Initial's stmt must be a Begin.
TEST_F(WaitForkTest, InitialStmtIsBegin) {
  EXPECT_NE(getInitialBegin(), nullptr) << "'initial begin ... end' must produce a Begin scope";
}

TEST_F(WaitForkTest, InitialBeginHasTwoStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 2u) << "the fork block and 'wait fork;' are exactly 2 statements";
}

TEST_F(WaitForkTest, FirstStmtIsForkStmt) {
  EXPECT_NE(getForkStmt(), nullptr) << "9.6.1: the first statement must be a ForkStmt";
}

TEST_F(WaitForkTest, ForkJoinTypeIsJoinNone) {
  const hldb::ForkStmt *const fork = getForkStmt();
  ASSERT_NE(fork, nullptr);
  EXPECT_EQ(fork->getJoinType(), vpiJoinNone) << "9.6.1: 'join_none' must have JoinType vpiJoinNone";
}

TEST_F(WaitForkTest, ForkHasTwoBeginBranches) {
  const hldb::ForkStmt *const fork = getForkStmt();
  ASSERT_NE(fork, nullptr);
  ASSERT_NE(fork->getStmts(), nullptr);
  ASSERT_EQ(fork->getStmts()->size(), 2u) << "the two 'begin ... end' branches are exactly 2 statements";
  EXPECT_NE(getForkBranch(0), nullptr) << "branch 0 should be a Begin";
  EXPECT_NE(getForkBranch(1), nullptr) << "branch 1 should be a Begin";
}

// --- branch 0: #50 a=1; #50 a=0; #50 a=1; ----------------------------------

TEST_F(WaitForkTest, Branch0HasThreeDelayControlledStmts) {
  const hldb::Begin *const branch = getForkBranch(0);
  ASSERT_NE(branch, nullptr);
  ASSERT_NE(branch->getStmts(), nullptr);
  EXPECT_EQ(branch->getStmts()->size(), 3u) << "'#50 a=1; #50 a=0; #50 a=1;' is exactly 3 statements";
}

TEST_F(WaitForkTest, Branch0DelaysControlAssignmentsToA) {
  const char *const expected[] = {"1", "0", "1"};
  for (size_t i = 0; i < 3; ++i) {
    const hldb::DelayControl *const delay = getBranchDelay(0, i);
    ASSERT_NE(delay, nullptr) << "branch 0 stmt[" << i << "] should be a DelayControl";
    EXPECT_EQ(delay->getDelay<hldb::Constant>()->getDecompile(), "50") << "'#50' delay value";

    const hldb::Assignment *const assign = delay->getStmt<hldb::Assignment>();
    ASSERT_NE(assign, nullptr) << "'#50 a=...' should control a blocking Assignment";
    EXPECT_TRUE(assign->getBlocking()) << "'a = ...' uses the blocking assignment operator '='";

    const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), "a");
    EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariable("a"));

    const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->getDecompile(), expected[i]);
    EXPECT_EQ(rhs->getConstType(), vpiUIntConst);
  }
}

// --- branch 1: #50 b=1; #50 b=0; #50 b=1; ----------------------------------

TEST_F(WaitForkTest, Branch1HasThreeDelayControlledStmts) {
  const hldb::Begin *const branch = getForkBranch(1);
  ASSERT_NE(branch, nullptr);
  ASSERT_NE(branch->getStmts(), nullptr);
  EXPECT_EQ(branch->getStmts()->size(), 3u) << "'#50 b=1; #50 b=0; #50 b=1;' is exactly 3 statements";
}

TEST_F(WaitForkTest, Branch1DelaysControlAssignmentsToB) {
  const char *const expected[] = {"1", "0", "1"};
  for (size_t i = 0; i < 3; ++i) {
    const hldb::DelayControl *const delay = getBranchDelay(1, i);
    ASSERT_NE(delay, nullptr) << "branch 1 stmt[" << i << "] should be a DelayControl";
    EXPECT_EQ(delay->getDelay<hldb::Constant>()->getDecompile(), "50") << "'#50' delay value";

    const hldb::Assignment *const assign = delay->getStmt<hldb::Assignment>();
    ASSERT_NE(assign, nullptr) << "'#50 b=...' should control a blocking Assignment";
    EXPECT_TRUE(assign->getBlocking()) << "'b = ...' uses the blocking assignment operator '='";

    const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), "b");
    EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariable("b"));

    const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->getDecompile(), expected[i]);
    EXPECT_EQ(rhs->getConstType(), vpiUIntConst);
  }
}

// --- wait fork; -------------------------------------------------------------

TEST_F(WaitForkTest, SecondStmtIsWaitFork) {
  EXPECT_NE(getWaitFork(), nullptr) << "9.6.1: the second statement must be a WaitFork";
}

TEST_F(WaitForkTest, WaitForkHasVpiWaitForkType) {
  const hldb::WaitFork *const waitFork = getWaitFork();
  ASSERT_NE(waitFork, nullptr);
  EXPECT_EQ(waitFork->getVpiType(), vpiWaitFork);
}

TEST_F(WaitForkTest, WaitForkHasNoControlledStmt) {
  const hldb::WaitFork *const waitFork = getWaitFork();
  ASSERT_NE(waitFork, nullptr);
  EXPECT_EQ(waitFork->getStmt(), nullptr) << "bare 'wait fork;' has no condition/body -- getStmt() should be null";
}

// --- compiler diagnostics ----------------------------------------------------

TEST_F(WaitForkTest, ReferencesAreNotFailedBinds) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr)
      << "'a' and 'b' must all bind to their declarations";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
