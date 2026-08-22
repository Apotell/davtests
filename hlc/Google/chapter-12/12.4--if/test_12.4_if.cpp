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

// Tests for 12.4--if.sv (tags: 12.4)
//   module if_tb ();
//     wire a = 0;
//     reg b = 0;
//     always @* begin
//       if(a) b = 1;
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 12.4 "Conditional if-else
// statement", p.315-316, checked before any test code was written):
//   "conditional_statement ::= [ unique_priority ] if ( cond_predicate )
//   statement_or_null { else if (...) statement_or_null } [ else
//   statement_or_null ]" -- this file has no unique_priority prefix, no
//   "else if", and no trailing "else", so the whole construct reduces to
//   a single "if (a) b = 1;" with nothing following. This should build a
//   plain if-statement object with no else branch at all (a different
//   UHDM type than the if-else case, see if_stmt.h vs if_else.h).
//
//   Also (IEEE 1800-2023 6.7/6.8): "wire" is a net_type keyword (6.7) so
//   "wire a" must be a Net; "reg" is an integer_vector_type keyword
//   (6.8, non_net data type) so "reg b" must be a Variable, never a Net,
//   regardless of scope.
//
// What is checked:
//   - module if_tb has exactly 1 Net "a" (wire) and 1 Variable "b" (reg)
//   - always process body is a single-item Begin block containing one
//     statement
//   - that statement is specifically IfStmt (AnyType::IfStmt), not
//     IfElse -- proving no else branch was synthesized where none exists
//     in the source
//   - IfStmt qualifier is vpiNoQualifier (no unique/unique0/priority
//     keyword present)
//   - IfStmt condition is RefObj "a" resolving to the Net "a" (not a
//     Variable)
//   - IfStmt body is Assignment "b = 1" whose LHS resolves to Variable
//     "b" (not a Net) and whose RHS is Constant "1"
//   - no continuous assignments exist in the module
//
// What is NOT checked and why:
//   - the "always @*" implicit event/sensitivity list itself is a
//     chapter-9 concept, out of scope for this chapter-12 conditional-
//     statement file
//   - runtime execution/simulation of the if (whether "a" being 0 means
//     "b" keeps its initial value) is a simulation-time concept, not a
//     static/structural compile-time property

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/if_else.h>
#include <hldb/if_stmt.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IfTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12.4--if.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("if_tb", m_design->getAllModules()); }

  static const hldb::Begin *getBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Always *const always = any_cast<hldb::Always>(top->getProcesses()->at(0));
    if (always == nullptr) return nullptr;
    const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
    if (ec == nullptr) return nullptr;
    return ec->getStmt<hldb::Begin>();
  }
};

TEST_F(IfTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// ---------------------------------------------------------------------------
// Declarations -- wire 'a' is a Net, reg 'b' is a Variable
// ---------------------------------------------------------------------------
TEST_F(IfTest, ModuleHasOneNetAAndOneVariableB) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Net>("a", top->getNets()), nullptr) << "Net 'a' not found";
  ASSERT_NE(top->getVariables(), nullptr) << "'reg b' should be a Variable, not a Net";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Variable>("b", top->getVariables()), nullptr) << "Variable 'b' not found";
}

TEST_F(IfTest, ANetIsWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getNetType(), vpiWire);
}

// ---------------------------------------------------------------------------
// Process body -- always @* begin if(a) b = 1; end
// ---------------------------------------------------------------------------
TEST_F(IfTest, AlwaysProcessExistsAndIsAlwaysType) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Always *const always = any_cast<hldb::Always>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  EXPECT_EQ(always->getAlwaysType(), vpiAlways);
}

TEST_F(IfTest, BeginBlockHasExactlyOneStmt) {
  const hldb::Begin *const body = getBody();
  ASSERT_NE(body, nullptr) << "always body should be EventControl -> Begin";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 1u);
}

// ---------------------------------------------------------------------------
// The lone statement is a plain IfStmt (no else branch)
// ---------------------------------------------------------------------------
TEST_F(IfTest, LoneStmtIsIfStmtNotIfElse) {
  const hldb::Begin *const body = getBody();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u);
  hldb::Any *const stmt = body->getStmts()->at(0);
  ASSERT_NE(stmt, nullptr);
  EXPECT_EQ(stmt->getAnyType(), hldb::AnyType::IfStmt)
      << "no else/else-if is present in the source, so this must be IfStmt, not IfElse";
  EXPECT_EQ(any_cast<hldb::IfElse>(stmt), nullptr) << "must not be modeled as an IfElse when there is no else";
}

TEST_F(IfTest, IfStmtQualifierIsNoQualifier) {
  const hldb::Begin *const body = getBody();
  ASSERT_NE(body, nullptr);
  const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(body->getStmts()->at(0));
  ASSERT_NE(ifStmt, nullptr);
  EXPECT_EQ(ifStmt->getQualifier(), vpiNoQualifier) << "no unique/unique0/priority keyword precedes this if";
}

TEST_F(IfTest, IfStmtConditionResolvesToNetANotVariable) {
  const hldb::Begin *const body = getBody();
  ASSERT_NE(body, nullptr);
  const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(body->getStmts()->at(0));
  ASSERT_NE(ifStmt, nullptr);
  const hldb::RefObj *const cond = ifStmt->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr) << "IfStmt condition is not a RefObj";
  EXPECT_EQ(cond->getName(), "a");
  EXPECT_NE(cond->getActual<hldb::Net>(), nullptr) << "'a' should resolve to the Net, not a Variable";
}

TEST_F(IfTest, IfStmtBodyAssignsOneToVariableB) {
  const hldb::Begin *const body = getBody();
  ASSERT_NE(body, nullptr);
  const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(body->getStmts()->at(0));
  ASSERT_NE(ifStmt, nullptr);
  const hldb::Assignment *const assign = ifStmt->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "IfStmt body is not an Assignment";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "b");
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr) << "'b' should resolve to the Variable, not a Net";
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "1");
}

TEST_F(IfTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
