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

// Tests for 10.4.2--non-blocking-assignment.sv (tags: 10.4.2)
//   module top();
//     logic a;
//     initial begin
//       a <= 2;
//     end
//   endmodule
//
// Checked:
//   - design has module top with exactly 1 variable: "a", RefTypespec ->
//     LogicTypespec, no initializer. Per IEEE 1800-2023 Sec 6.7/6.8: "logic
//     a;" has no net-type keyword and is not a port declaration, so it is a
//     Variable, not a Net; module has no nets (getNets() is null).
//   - Initial process: 1 Begin with 1 stmt (the source uses explicit
//     'begin'/'end' around the single statement, so it IS wrapped in a
//     Begin -- contrast with chapter-10/10.3--proc-assignment--bad.sv,
//     whose single-statement initial has no begin/end in the source and is
//     therefore NOT wrapped)
//   - Stmt[0]: Assignment, getBlocking() == false (non-blocking '<='), lhs
//     RefObj "a" resolving Variable "a", rhs Constant "2". Per IEEE
//     1800-2023 Table 10-1, non-blocking procedural assignment to a
//     variable is legal (the Table 10-1 net-target restriction tested in
//     chapter-10/10.3--proc-assignment--bad.sv does not apply here).
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class NonBlockingAssignmentTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "10.4.2--non-blocking-assignment.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Begin *getInitialBegin() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }
};

// --- module / net ----

TEST_F(NonBlockingAssignmentTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(NonBlockingAssignmentTest, ModuleHasOneLogicVariableWithNoInitializer) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 1u);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>(), nullptr);
  EXPECT_EQ(a->getValue(), nullptr);
}

TEST_F(NonBlockingAssignmentTest, ModuleHasNoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getNets(), nullptr);
}

// --- initial process ----

TEST_F(NonBlockingAssignmentTest, InitialBeginHasOneStmt) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr) << "explicit begin/end around the single statement should still produce a Begin";
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 1u);
}

TEST_F(NonBlockingAssignmentTest, FirstStmtIsNonBlockingAssignmentOfTwoToA) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_FALSE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a");
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "2");
}

// --- design-level typespecs / compiler diagnostics ----

TEST_F(NonBlockingAssignmentTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(NonBlockingAssignmentTest, DesignHasSignedIntTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(NonBlockingAssignmentTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
