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

// Tests for 9.2.3--final.sv (tags: 9.2.3)
//   module initial_tb ();
//     reg a = 0;
//     final
//       a = 1;
//   endmodule
//
// IEEE 1800-2017 Sec 9.2.3 "final construct": a final_construct is
// "final" followed by a single statement_or_null (a function_statement,
// which excludes delay/event controls) -- executes exactly once, at the
// end of simulation. Structurally this parallels 9.2.1--initial.sv's
// "initial" construct: no begin/end here, so the process' body is the
// bare blocking assignment itself, not wrapped in a Begin block. The
// process itself is a dedicated FinalStmt node, distinct from Initial.
//
// "reg a = 0" -- same reasoning as 9.2.1--initial.sv: a Variable (no
// net-type keyword), single-bit unsigned LogicTypespec, no declared
// Ranges, in the module's variable collection only. Both "0" and "1" are
// plain-value bare decimal literals -> vpiUIntConst (established
// precedent; see 9.2.1--initial.sv and the project memory on the
// vpiIntConst/vpiUIntConst split for delay/count vs. plain-value
// contexts -- this is a plain-value context).
//
// Checked:
//   - design has module "initial_tb" with exactly 1 variable: "a"
//   - variable "a": LogicTypespec, unsigned, no declared ranges (implicit
//     scalar bit); not duplicated in the net collection; initial value
//     resolves to Constant "0" (vpiUIntConst)
//   - module has exactly 1 process, and it is a FinalStmt
//   - the FinalStmt's body is directly an Assignment (no enclosing Begin,
//     since the source has no begin/end)
//   - that Assignment is blocking ("="); lhs is a RefObj resolving to
//     Variable "a"; rhs is Constant "1" (vpiUIntConst)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/final_stmt.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FinalTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.2.3--final.hlc"}); }
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

  static const hldb::FinalStmt *getFinalProcess() {
    const hldb::Module *const mod = getModule();
    if (mod == nullptr || mod->getProcesses() == nullptr || mod->getProcesses()->empty()) return nullptr;
    return any_cast<hldb::FinalStmt>(mod->getProcesses()->at(0));
  }
};

// --- module / variable "a" ---------------------------------------------------

TEST_F(FinalTest, ModuleExists) { EXPECT_NE(getModule(), nullptr); }

TEST_F(FinalTest, ModuleHasOneVariable) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 1u);
}

TEST_F(FinalTest, VariableAExists) { EXPECT_NE(getVariableA(), nullptr); }

// "reg a" has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8 it
// must not also appear in the module's net collection.
TEST_F(FinalTest, VariableAIsNotDuplicatedAsNet) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  if (mod->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("a", mod->getNets()), nullptr)
        << "'reg a' has no net-type keyword and must not also appear as a Net";
  }
}

TEST_F(FinalTest, VariableATypespecIsLogicTypespec) { EXPECT_NE(getVariableATypespec(), nullptr); }

TEST_F(FinalTest, VariableATypespecIsUnsigned) {
  const hldb::LogicTypespec *const ts = getVariableATypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_FALSE(ts->getSigned()) << "6.8: 'reg' with no 'signed' keyword defaults to unsigned";
}

TEST_F(FinalTest, VariableATypespecHasNoDeclaredRanges) {
  const hldb::LogicTypespec *const ts = getVariableATypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE(ts->getRanges() == nullptr || ts->getRanges()->empty())
      << "'reg a' declares no '[msb:lsb]' -- it is an implicit scalar bit";
}

TEST_F(FinalTest, VariableAInitialValueIsConstantZero) {
  const hldb::Variable *const a = getVariableA();
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const val = a->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "'reg a = 0' must carry an initial value";
  EXPECT_EQ(val->getDecompile(), "0");
  EXPECT_EQ(val->getConstType(), vpiUIntConst) << "bare decimal literal -> constType unsigned int (9)";
}

// --- final process structure --------------------------------------------------

TEST_F(FinalTest, ModuleHasOneProcess) {
  const hldb::Module *const mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  EXPECT_EQ(mod->getProcesses()->size(), 1u);
}

TEST_F(FinalTest, TheOneProcessIsFinalStmt) { EXPECT_NE(getFinalProcess(), nullptr); }

// 9.2.3: "final" with no begin/end wraps a single statement_or_null -- the
// process' stmt must be the Assignment itself, not a Begin block.
TEST_F(FinalTest, FinalStmtIsDirectlyAnAssignment) {
  const hldb::FinalStmt *const fin = getFinalProcess();
  ASSERT_NE(fin, nullptr);
  EXPECT_NE(fin->getStmt<hldb::Assignment>(), nullptr)
      << "9.2.3: a single-statement 'final' body (no begin/end) must not be wrapped in a Begin block";
}

TEST_F(FinalTest, AssignmentIsBlocking) {
  const hldb::FinalStmt *const fin = getFinalProcess();
  ASSERT_NE(fin, nullptr);
  const hldb::Assignment *const assign = fin->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking()) << "'a = 1' uses the blocking assignment operator '='";
}

TEST_F(FinalTest, AssignmentLhsResolvesToVariableA) {
  const hldb::FinalStmt *const fin = getFinalProcess();
  ASSERT_NE(fin, nullptr);
  const hldb::Assignment *const assign = fin->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "assignment lhs should be a RefObj";
  EXPECT_EQ(lhs->getName(), "a");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariableA());
}

TEST_F(FinalTest, AssignmentRhsIsConstantOne) {
  const hldb::FinalStmt *const fin = getFinalProcess();
  ASSERT_NE(fin, nullptr);
  const hldb::Assignment *const assign = fin->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "assignment rhs should be a Constant";
  EXPECT_EQ(rhs->getDecompile(), "1");
  EXPECT_EQ(rhs->getConstType(), vpiUIntConst) << "bare decimal literal -> constType unsigned int (9)";
}

// --- compiler diagnostics ----------------------------------------------------

// The construct with the most real risk of a binding failure is "a"
// resolving back to its declaration.
TEST_F(FinalTest, ReferenceToAIsNotAFailedBind) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr)
      << "'a' in 'a = 1' must bind to the 'reg a' declaration";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
