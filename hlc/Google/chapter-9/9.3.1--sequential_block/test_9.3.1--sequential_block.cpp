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

// Tests for 9.3.1--sequential_block.sv (tags: 9.3.1)
//   module sequential_tb ();
//     reg a = 0;
//     reg b = 0;
//     reg c = 0;
//     initial begin
//       a = 1;
//       b = a;
//       c = b;
//     end
//   endmodule
//
// IEEE 1800-2017 Sec 9.3.1 "Sequential blocks": a begin-end block's
// statements execute in the order given -- unlike 9.2.1--initial.sv's
// single-statement "initial" (no begin/end), this source explicitly
// wraps 3 statements in begin/end, so the Initial process' body must be a
// Begin node (Scope) whose getStmts() holds all 3 Assignments in source
// order.
//
// "reg a/b/c = 0" -- same reasoning as 9.2.1--initial.sv: each a Variable
// (no net-type keyword), single-bit unsigned LogicTypespec, no declared
// Ranges. All bare decimal literals here ("0" x3 initializers, "1") are
// plain-value literals -> vpiUIntConst.
//
// The 2nd and 3rd statements ("b = a;", "c = b;") differ from every prior
// file in this chapter's assignments: their rhs is a plain variable
// reference (RefObj), not a Constant.
//
// Checked:
//   - design has module "sequential_tb" with exactly 3 variables: "a",
//     "b", "c"
//   - each variable: LogicTypespec, unsigned, no declared ranges; not
//     duplicated in the net collection; initial value resolves to
//     Constant "0" (vpiUIntConst)
//   - module has exactly 1 process, and it is an Initial
//   - the Initial's body is a Begin (since source uses begin/end) with
//     exactly 3 statements, in source order
//   - stmt[0] "a = 1": blocking Assignment, lhs RefObj -> Variable "a",
//     rhs Constant "1" (vpiUIntConst)
//   - stmt[1] "b = a": blocking Assignment, lhs RefObj -> Variable "b",
//     rhs RefObj -> Variable "a"
//   - stmt[2] "c = b": blocking Assignment, lhs RefObj -> Variable "c",
//     rhs RefObj -> Variable "b"

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class SequentialBlockTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.3.1--sequential_block.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() {
    return hldb::findByName<hldb::Module>("sequential_tb", m_design->getAllModules());
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

  static const hldb::Begin *getBegin() {
    const hldb::Initial *const init = getInitialProcess();
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }

  static const hldb::Assignment *getStmt(size_t index) {
    const hldb::Begin *const begin = getBegin();
    if (begin == nullptr || begin->getStmts() == nullptr || begin->getStmts()->size() <= index) return nullptr;
    return any_cast<hldb::Assignment>(begin->getStmts()->at(index));
  }
};

// --- module / variables "a", "b", "c" ------------------------------------------

TEST_F(SequentialBlockTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(SequentialBlockTest, ModuleHasThreeVariables) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 3u);
}

TEST_F(SequentialBlockTest, VariableAExists) { EXPECT_NE(getVariable("a"), nullptr); }
TEST_F(SequentialBlockTest, VariableBExists) { EXPECT_NE(getVariable("b"), nullptr); }
TEST_F(SequentialBlockTest, VariableCExists) { EXPECT_NE(getVariable("c"), nullptr); }

// "reg a/b/c" have no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8
// none must also appear in the module's net collection.
TEST_F(SequentialBlockTest, VariablesAreNotDuplicatedAsNets) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  if (mod->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("a", mod->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>("b", mod->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>("c", mod->getNets()), nullptr);
  }
}

TEST_F(SequentialBlockTest, VariableATypespecIsLogicTypespec) { EXPECT_NE(getVariableTypespec("a"), nullptr); }
TEST_F(SequentialBlockTest, VariableBTypespecIsLogicTypespec) { EXPECT_NE(getVariableTypespec("b"), nullptr); }
TEST_F(SequentialBlockTest, VariableCTypespecIsLogicTypespec) { EXPECT_NE(getVariableTypespec("c"), nullptr); }

TEST_F(SequentialBlockTest, VariableTypespecsAreUnsignedWithNoRanges) {
  for (std::string_view name : {"a", "b", "c"}) {
    const hldb::LogicTypespec *const ts = getVariableTypespec(name);
    ASSERT_NE(ts, nullptr);
    EXPECT_FALSE(ts->getSigned()) << "6.8: 'reg " << name << "' with no 'signed' keyword defaults to unsigned";
    EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty())
        << "'reg " << name << "' declares no '[msb:lsb]' -- it is an implicit scalar bit";
  }
}

TEST_F(SequentialBlockTest, AllVariablesInitialValueIsConstantZero) {
  for (std::string_view name : {"a", "b", "c"}) {
    const hldb::Variable *const v = getVariable(name);
    ASSERT_NE(v, nullptr);
    const hldb::Constant *const val = v->getValue<hldb::Constant>();
    ASSERT_NE(val, nullptr) << "'reg " << name << " = 0' must carry an initial value";
    EXPECT_EQ(val->getDecompile(), "0");
    EXPECT_EQ(val->getConstType(), vpiUIntConst) << "bare decimal literal -> constType unsigned int (9)";
  }
}

// --- initial process structure -------------------------------------------------

TEST_F(SequentialBlockTest, ModuleHasOneProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
}

TEST_F(SequentialBlockTest, TheOneProcessIsInitial) { EXPECT_NE(getInitialProcess(), nullptr); }

// 9.3.1: "initial begin ... end" must produce a Begin node, unlike a
// single-statement "initial" body (see 9.2.1--initial.sv).
TEST_F(SequentialBlockTest, InitialStmtIsABegin) {
  EXPECT_NE(getBegin(), nullptr) << "9.3.1: 'begin...end' must be modeled as a Begin block";
}

TEST_F(SequentialBlockTest, BeginHasThreeStmts) {
  const hldb::Begin *const begin = getBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 3u) << "9.3.1: 'a=1; b=a; c=b;' is exactly 3 statements";
}

// --- stmt[0]: a = 1 -----------------------------------------------------------

TEST_F(SequentialBlockTest, FirstStmtIsBlockingAssignment) {
  const hldb::Assignment *const assign = getStmt(0);
  ASSERT_NE(assign, nullptr) << "9.3.1: stmt[0] should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "'a = 1' uses the blocking assignment operator '='";
}

TEST_F(SequentialBlockTest, FirstStmtLhsResolvesToVariableA) {
  const hldb::Assignment *const assign = getStmt(0);
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariable("a"));
}

TEST_F(SequentialBlockTest, FirstStmtRhsIsConstantOne) {
  const hldb::Assignment *const assign = getStmt(0);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'a = 1' rhs should be a Constant";
  EXPECT_EQ(rhs->getDecompile(), "1");
  EXPECT_EQ(rhs->getConstType(), vpiUIntConst) << "bare decimal literal -> constType unsigned int (9)";
}

// --- stmt[1]: b = a -----------------------------------------------------------

TEST_F(SequentialBlockTest, SecondStmtIsBlockingAssignment) {
  const hldb::Assignment *const assign = getStmt(1);
  ASSERT_NE(assign, nullptr) << "9.3.1: stmt[1] should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "'b = a' uses the blocking assignment operator '='";
}

TEST_F(SequentialBlockTest, SecondStmtLhsResolvesToVariableB) {
  const hldb::Assignment *const assign = getStmt(1);
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "b");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariable("b"));
}

TEST_F(SequentialBlockTest, SecondStmtRhsResolvesToVariableA) {
  const hldb::Assignment *const assign = getStmt(1);
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "'b = a' rhs should be a RefObj (unlike stmt[0], not a Constant)";
  EXPECT_EQ(rhs->getName(), "a");
  EXPECT_EQ(rhs->getActual<hldb::Variable>(), getVariable("a"));
}

// --- stmt[2]: c = b -----------------------------------------------------------

TEST_F(SequentialBlockTest, ThirdStmtIsBlockingAssignment) {
  const hldb::Assignment *const assign = getStmt(2);
  ASSERT_NE(assign, nullptr) << "9.3.1: stmt[2] should be an Assignment";
  EXPECT_TRUE(assign->getBlocking()) << "'c = b' uses the blocking assignment operator '='";
}

TEST_F(SequentialBlockTest, ThirdStmtLhsResolvesToVariableC) {
  const hldb::Assignment *const assign = getStmt(2);
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "c");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariable("c"));
}

TEST_F(SequentialBlockTest, ThirdStmtRhsResolvesToVariableB) {
  const hldb::Assignment *const assign = getStmt(2);
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "'c = b' rhs should be a RefObj";
  EXPECT_EQ(rhs->getName(), "b");
  EXPECT_EQ(rhs->getActual<hldb::Variable>(), getVariable("b"));
}

// --- compiler diagnostics ----------------------------------------------------

TEST_F(SequentialBlockTest, ReferencesAreNotFailedBinds) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr)
      << "'a', 'b', and 'c' must all bind to their declarations";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
