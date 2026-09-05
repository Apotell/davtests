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

// Tests for 9.3.2--parallel_block_join.sv (tags: 9.3.2)
//   module parallel_tb ();
//     reg a = 0;
//     reg b = 0;
//     reg c = 0;
//     initial
//       fork
//         a = 1;
//         b = 0;
//         c = 1;
//       join
//   endmodule
//
// IEEE 1800-2017 Sec 9.3.2 "Parallel blocks (fork-join)": a fork-join
// block's statements execute concurrently; "join" specifically means the
// parent process blocks until ALL of the fork's statements complete
// (contrast with join_any/join_none in the sibling files in this group).
// The parser must produce a dedicated ForkStmt (Scope) node whose
// getJoinType() is vpiJoin, distinct from a Begin block, with its 3
// statements in getStmts() (source order does not imply execution order
// here, only declaration order in the collection). "fork...join" is
// itself the single statement following "initial" (no begin/end needed
// around the fork), so it sits directly on the Initial's stmt.
//
// "reg a/b/c = 0" -- same reasoning as 9.3.1--sequential_block.sv: each a
// Variable, single-bit unsigned LogicTypespec, no declared Ranges. All
// bare decimal literals ("0" x3 initializers, "1", "0", "1") are
// plain-value literals -> vpiUIntConst.
//
// Checked:
//   - design has module "parallel_tb" with exactly 3 variables: "a", "b",
//     "c"
//   - each variable: LogicTypespec, unsigned, no declared ranges; not
//     duplicated in the net collection; initial value resolves to
//     Constant "0" (vpiUIntConst)
//   - module has exactly 1 process, and it is an Initial
//   - the Initial's body is directly a ForkStmt (no enclosing Begin,
//     since there is no begin/end around "fork...join" itself)
//   - the ForkStmt's getJoinType() is vpiJoin; it has exactly 3
//     statements
//   - stmt[0] "a = 1": blocking Assignment, lhs RefObj -> Variable "a",
//     rhs Constant "1"
//   - stmt[1] "b = 0": blocking Assignment, lhs RefObj -> Variable "b",
//     rhs Constant "0"
//   - stmt[2] "c = 1": blocking Assignment, lhs RefObj -> Variable "c",
//     rhs Constant "1"

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/constant.h>
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

namespace hlc {

class ParallelBlockJoinTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.3.2--parallel_block_join.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() {
    return hldb::findByName<hldb::Module>("parallel_tb", m_design->getAllModules());
  }

  static const hldb::Variable *getVariable(std::string_view name) {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, mod->getVariables());
  }

  static const hldb::LogicTypespec *getVariableTypespec(std::string_view name) {
    const hldb::Variable *const v = getVariable(name);
    if (v == nullptr || v->getTypespec() == nullptr) return nullptr;
    return v->getTypespec()->getActual<hldb::LogicTypespec>();
  }

  static const hldb::Initial *getInitialProcess() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty()) return nullptr;
    return any_cast<hldb::Initial>(mod->getProcesses()->at(0));
  }

  static const hldb::ForkStmt *getForkStmt() {
    const hldb::Initial *const init = getInitialProcess();
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::ForkStmt>();
  }

  static const hldb::Assignment *getStmt(size_t index) {
    const hldb::ForkStmt *const fork = getForkStmt();
    if (fork == nullptr || fork->getStmts() == nullptr || fork->getStmts()->size() <= index) return nullptr;
    return any_cast<hldb::Assignment>(fork->getStmts()->at(index));
  }
};

// --- module / variables "a", "b", "c" ------------------------------------------

TEST_F(ParallelBlockJoinTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(ParallelBlockJoinTest, ModuleHasThreeVariables) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 3u);
}

TEST_F(ParallelBlockJoinTest, VariableAExists) { EXPECT_NE(getVariable("a"), nullptr); }
TEST_F(ParallelBlockJoinTest, VariableBExists) { EXPECT_NE(getVariable("b"), nullptr); }
TEST_F(ParallelBlockJoinTest, VariableCExists) { EXPECT_NE(getVariable("c"), nullptr); }

// "reg a/b/c" have no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8
// none must also appear in the module's net collection.
TEST_F(ParallelBlockJoinTest, VariablesAreNotDuplicatedAsNets) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  if (mod->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("a", mod->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>("b", mod->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>("c", mod->getNets()), nullptr);
  }
}

TEST_F(ParallelBlockJoinTest, VariableTypespecsAreLogicUnsignedWithNoRanges) {
  for (std::string_view name : {"a", "b", "c"}) {
    const hldb::LogicTypespec *const ts = getVariableTypespec(name);
    ASSERT_NE(ts, nullptr) << "'reg " << name << "' should resolve to LogicTypespec";
    EXPECT_FALSE(ts->getSigned()) << "6.8: 'reg " << name << "' with no 'signed' keyword defaults to unsigned";
    EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty())
        << "'reg " << name << "' declares no '[msb:lsb]' -- it is an implicit scalar bit";
  }
}

TEST_F(ParallelBlockJoinTest, AllVariablesInitialValueIsConstantZero) {
  for (std::string_view name : {"a", "b", "c"}) {
    const hldb::Variable *const v = getVariable(name);
    ASSERT_NE(v, nullptr);
    const hldb::Constant *const val = v->getValue<hldb::Constant>();
    ASSERT_NE(val, nullptr) << "'reg " << name << " = 0' must carry an initial value";
    EXPECT_EQ(val->getDecompile(), "0");
    EXPECT_EQ(val->getConstType(), vpiUIntConst) << "bare decimal literal -> constType unsigned int (9)";
  }
}

// --- initial process / fork structure ------------------------------------------

TEST_F(ParallelBlockJoinTest, ModuleHasOneProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
}

TEST_F(ParallelBlockJoinTest, TheOneProcessIsInitial) { EXPECT_NE(getInitialProcess(), nullptr); }

// 9.3.2: "fork...join" is itself the single statement following
// "initial" -- no enclosing Begin is needed or expected.
TEST_F(ParallelBlockJoinTest, InitialStmtIsDirectlyAForkStmt) {
  EXPECT_NE(getForkStmt(), nullptr) << "9.3.2: 'fork...join' must be modeled as a ForkStmt";
}

TEST_F(ParallelBlockJoinTest, ForkJoinTypeIsJoin) {
  const hldb::ForkStmt *const fork = getForkStmt();
  ASSERT_NE(fork, nullptr);
  EXPECT_EQ(fork->getJoinType(), vpiJoin) << "9.3.2: plain 'join' must have JoinType vpiJoin";
}

TEST_F(ParallelBlockJoinTest, ForkHasThreeStmts) {
  const hldb::ForkStmt *const fork = getForkStmt();
  ASSERT_NE(fork, nullptr);
  ASSERT_NE(fork->getStmts(), nullptr);
  EXPECT_EQ(fork->getStmts()->size(), 3u) << "9.3.2: 'a=1; b=0; c=1;' is exactly 3 statements";
}

// --- fork statements: a = 1, b = 0, c = 1 --------------------------------------

TEST_F(ParallelBlockJoinTest, FirstStmtIsBlockingAssignToA) {
  const hldb::Assignment *const assign = getStmt(0);
  ASSERT_NE(assign, nullptr) << "9.3.2: stmt[0] should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "'a = 1' uses the blocking assignment operator '='";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariable("a"));
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "1");
  EXPECT_EQ(rhs->getConstType(), vpiUIntConst);
}

TEST_F(ParallelBlockJoinTest, SecondStmtIsBlockingAssignToB) {
  const hldb::Assignment *const assign = getStmt(1);
  ASSERT_NE(assign, nullptr) << "9.3.2: stmt[1] should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "'b = 0' uses the blocking assignment operator '='";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "b");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariable("b"));
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "0");
  EXPECT_EQ(rhs->getConstType(), vpiUIntConst);
}

TEST_F(ParallelBlockJoinTest, ThirdStmtIsBlockingAssignToC) {
  const hldb::Assignment *const assign = getStmt(2);
  ASSERT_NE(assign, nullptr) << "9.3.2: stmt[2] should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "'c = 1' uses the blocking assignment operator '='";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "c");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariable("c"));
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "1");
  EXPECT_EQ(rhs->getConstType(), vpiUIntConst);
}

// --- compiler diagnostics ----------------------------------------------------

TEST_F(ParallelBlockJoinTest, ReferencesAreNotFailedBinds) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr)
      << "'a', 'b', and 'c' must all bind to their declarations";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
