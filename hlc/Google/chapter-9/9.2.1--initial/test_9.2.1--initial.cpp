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

// Tests for 9.2.1--initial.sv (tags: 9.2.1)
//   module initial_tb ();
//     reg a = 0;
//     initial
//       a = 1;
//   endmodule
//
// IEEE 1800-2017 Sec 9.2.1 "Initial construct": an initial_construct is
// "initial" followed by a single statement_or_null -- no begin/end here, so
// the initial process' body is the bare blocking assignment itself, not
// wrapped in a Begin block.
//
// "reg a = 0" declares a variable, never a net: per IEEE 1800-2017 Sec 6.8,
// "reg" is a 4-state variable data type identical to "logic" (retained for
// legacy compatibility), so it produces a LogicTypespec, not a dedicated
// Reg-flavored typespec. With no range and no "signed" keyword, it is a
// single-bit, unsigned LogicTypespec with no declared Ranges. Per Sec
// 6.7/6.8, having no net-type keyword means "a" must appear in the
// module's variable collection, never its net collection.
//
// "= 0" and "= 1" are unsized, unbased decimal literals; per Sec 5.7.1
// their *numeric value* defaults to a signed 32-bit interpretation, but
// that is separate from which vpiConstType tag models the literal node --
// this tool consistently classifies a bare decimal literal as
// vpiUIntConst, matching the established precedent in
// chapter-5/5.7.1--integers-unsized.sv ("659: bare decimal -> constType
// unsigned int (9)").
//
// Checked:
//   - design has module "initial_tb" with exactly 1 variable: "a"
//   - variable "a": LogicTypespec, unsigned, no declared ranges (implicit
//     scalar bit); not duplicated in the net collection; initial value
//     resolves to Constant "0" (vpiUIntConst)
//   - module has exactly 1 process, and it is an Initial
//   - the Initial's body is directly an Assignment (no enclosing Begin,
//     since the source has no begin/end)
//   - that Assignment is blocking ("=", not "<="); lhs is a RefObj
//     resolving to Variable "a"; rhs is Constant "1" (vpiUIntConst)
//   - compiler reports zero errors

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
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

class InitialTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.2.1--initial.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() {
    return hldb::findByName<hldb::Module>("initial_tb", m_design->getAllModules());
  }

  static const hldb::Variable *getVariableA() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("a", mod->getVariables());
  }

  static const hldb::LogicTypespec *getVariableATypespec() {
    const hldb::Variable *const a = getVariableA();
    if (a == nullptr || a->getTypespec() == nullptr) return nullptr;
    return a->getTypespec()->getActual<hldb::LogicTypespec>();
  }

  static const hldb::Initial *getInitialProcess() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty()) return nullptr;
    return any_cast<hldb::Initial>(mod->getProcesses()->at(0));
  }
};

// --- module / variable "a" ---------------------------------------------------

TEST_F(InitialTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(InitialTest, ModuleHasOneVariable) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 1u);
}

TEST_F(InitialTest, VariableAExists) { EXPECT_NE(getVariableA(), nullptr); }

// "reg a" has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8 it
// must not also appear in the module's net collection.
TEST_F(InitialTest, VariableAIsNotDuplicatedAsNet) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  if (mod->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("a", mod->getNets()), nullptr)
        << "'reg a' has no net-type keyword and must not also appear as a Net";
  }
}

TEST_F(InitialTest, VariableATypespecIsLogicTypespec) { EXPECT_NE(getVariableATypespec(), nullptr); }

TEST_F(InitialTest, VariableATypespecIsUnsigned) {
  const hldb::LogicTypespec *const ts = getVariableATypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_FALSE(ts->getSigned()) << "6.8: 'reg'/'logic' with no 'signed' keyword defaults to unsigned";
}

TEST_F(InitialTest, VariableATypespecHasNoDeclaredRanges) {
  const hldb::LogicTypespec *const ts = getVariableATypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty())
      << "'reg a' declares no '[msb:lsb]' -- it is an implicit scalar bit";
}

TEST_F(InitialTest, VariableAInitialValueIsConstantZero) {
  const hldb::Variable *const a = getVariableA();
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const val = a->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "'reg a = 0' must carry an initial value";
  EXPECT_EQ(val->getDecompile(), "0");
  EXPECT_EQ(val->getConstType(), vpiUIntConst) << "bare decimal literal -> constType unsigned int (9)";
}

// --- initial process structure -----------------------------------------------

TEST_F(InitialTest, ModuleHasOneProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
}

TEST_F(InitialTest, TheOneProcessIsInitial) { EXPECT_NE(getInitialProcess(), nullptr); }

// 9.2.1: "initial" with no begin/end wraps a single statement_or_null --
// the process' stmt must be the Assignment itself, not a Begin block.
TEST_F(InitialTest, InitialStmtIsDirectlyAnAssignment) {
  const hldb::Initial *const init = getInitialProcess();
  ASSERT_NE(init, nullptr);
  EXPECT_NE(init->getStmt<hldb::Assignment>(), nullptr)
      << "9.2.1: a single-statement 'initial' body (no begin/end) must not be wrapped in a Begin block";
}

TEST_F(InitialTest, AssignmentIsBlocking) {
  const hldb::Initial *const init = getInitialProcess();
  ASSERT_NE(init, nullptr);
  const hldb::Assignment *const assign = init->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking()) << "'a = 1' uses the blocking assignment operator '='";
}

TEST_F(InitialTest, AssignmentLhsResolvesToVariableA) {
  const hldb::Initial *const init = getInitialProcess();
  ASSERT_NE(init, nullptr);
  const hldb::Assignment *const assign = init->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "assignment lhs should be a RefObj";
  EXPECT_EQ(lhs->getName(), "a");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariableA());
}

TEST_F(InitialTest, AssignmentRhsIsConstantOne) {
  const hldb::Initial *const init = getInitialProcess();
  ASSERT_NE(init, nullptr);
  const hldb::Assignment *const assign = init->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "assignment rhs should be a Constant";
  EXPECT_EQ(rhs->getDecompile(), "1");
  EXPECT_EQ(rhs->getConstType(), vpiUIntConst) << "bare decimal literal -> constType unsigned int (9)";
}

// --- compiler diagnostics ----------------------------------------------------

// The one construct here with any real risk of a binding failure is "a" in
// "a = 1" resolving back to the "reg a" declaration -- check that specific
// error type directly, rather than asserting a blanket error count.
TEST_F(InitialTest, ReferenceToAIsNotAFailedBind) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr)
      << "'a' in 'a = 1' must bind to the 'reg a' declaration";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
