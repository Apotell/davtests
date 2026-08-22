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

// Tests for 12.5--case.sv (tags: 12.5)
//   module case_tb ();
//     wire [3:0] a = 0;
//     reg [3:0] b = 0;
//     always @* begin
//       case(a)
//         4'h0: b = 12;
//         4'h3: b = 4;
//         4'hf: b = 8;
//         default b = 0;
//       endcase
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 12.5 "Case statement", p.320,
// checked before any test code was written):
//   "case_statement ::= [ unique_priority ] case_keyword ( case_expression
//   ) case_item { case_item } endcase"; "case_keyword ::= case | casez |
//   casex". This is the plain "case" keyword (no do-not-care handling,
//   unlike 12.5.1's casex/casez) with 3 explicit hexadecimal-literal
//   case items plus one default item, and no unique/unique0/priority
//   qualifier keyword.
//
//   Also (IEEE 1800-2023 6.7/6.8): "wire" is a net_type keyword so
//   "wire a" must be a Net; "reg" is a non-net integer_vector_type
//   keyword so "reg b" must be a Variable.
//
//   Per IEEE 1800-2023 vpi_user.h vpiCaseType enumerants (vpiCaseExact=1
//   for plain "case", confirmed against this repo's actual
//   build/include/hldb/vpi_user.h), this file's plain "case" keyword
//   maps to vpiCaseExact.
//
// What is checked:
//   - module case_tb has exactly 1 Net "a" (wire) and 1 Variable "b" (reg)
//   - CaseStmt exists, getCaseType() == vpiCaseExact, getQualifier() ==
//     vpiNoQualifier (no unique/priority prefix), condition is RefObj
//     "a" resolving to the Net
//   - exactly 4 CaseItems
//   - the first 3 CaseItems each have exactly one vpiExpr: a Constant
//     with the exact source literal decompiled text ("4'h0", "4'h3",
//     "4'hf") and vpiConstType vpiHexConst, and a body Assignment to
//     Variable "b" with the corresponding value (12, 4, 8)
//   - the 4th (default) CaseItem has a null getExprs() (per Annex
//     37.72 detail 2) and body Assignment "b = 0"
//   - no continuous assignments exist in the module
//
// What is NOT checked and why:
//   - the runtime exact-match comparison behavior itself is a
//     simulation-time concept, not a static/structural property
//   - the "always @*" implicit sensitivity list is a chapter-9 concept

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/case_item.h>
#include <hldb/case_stmt.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class CaseTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12.5--case.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("case_tb", m_design->getAllModules());
  }

  static const hldb::CaseStmt *getCaseStmt() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Always *const always = any_cast<hldb::Always>(top->getProcesses()->at(0));
    if (always == nullptr) return nullptr;
    const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
    if (ec == nullptr) return nullptr;
    const hldb::Begin *const body = ec->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) return nullptr;
    return any_cast<hldb::CaseStmt>(body->getStmts()->at(0));
  }

  static void checkExplicitItem(const hldb::CaseItem *item, std::string_view decompile, std::string_view rhsValue) {
    ASSERT_NE(item, nullptr);
    ASSERT_NE(item->getExprs(), nullptr) << "explicit case item must have a case_item_expression";
    ASSERT_EQ(item->getExprs()->size(), 1u);
    const hldb::Constant *const expr = any_cast<hldb::Constant>(item->getExprs()->at(0));
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(expr->getConstType(), vpiHexConst);
    EXPECT_EQ(expr->getDecompile(), decompile);
    const hldb::Assignment *const assign = item->getStmt<hldb::Assignment>();
    ASSERT_NE(assign, nullptr);
    const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), "b");
    EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr) << "'b' should resolve to the Variable, not a Net";
    const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->getDecompile(), rhsValue);
  }
};

TEST_F(CaseTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(CaseTest, ModuleHasOneNetAAndOneVariableB) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Net>("a", top->getNets()), nullptr) << "Net 'a' not found";
  ASSERT_NE(top->getVariables(), nullptr) << "'reg b' should be a Variable, not a Net";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Variable>("b", top->getVariables()), nullptr) << "Variable 'b' not found";
}

TEST_F(CaseTest, CaseStmtExistsWithExactTypeAndNoQualifier) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr) << "the single statement in the always body should resolve to CaseStmt";
  EXPECT_EQ(cs->getCaseType(), vpiCaseExact);
  EXPECT_EQ(cs->getQualifier(), vpiNoQualifier);
}

TEST_F(CaseTest, ConditionResolvesToNetANotVariable) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  const hldb::RefObj *const cond = cs->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr) << "case condition is not a RefObj";
  EXPECT_EQ(cond->getName(), "a");
  EXPECT_NE(cond->getActual<hldb::Net>(), nullptr) << "'a' should resolve to the Net, not a Variable";
}

TEST_F(CaseTest, ExactlyFourCaseItems) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  EXPECT_EQ(cs->getCaseItems()->size(), 4u);
}

TEST_F(CaseTest, FirstItemIs4h0AssigningTwelve) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 1u);
  checkExplicitItem(cs->getCaseItems()->at(0), "4'h0", "12");
}

TEST_F(CaseTest, SecondItemIs4h3AssigningFour) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 2u);
  checkExplicitItem(cs->getCaseItems()->at(1), "4'h3", "4");
}

TEST_F(CaseTest, ThirdItemIs4hfAssigningEight) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 3u);
  checkExplicitItem(cs->getCaseItems()->at(2), "4'hf", "8");
}

TEST_F(CaseTest, FourthItemIsDefaultWithNullExprsAssigningZero) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 4u);
  const hldb::CaseItem *const item = cs->getCaseItems()->at(3);
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->getExprs(), nullptr) << "default case item must have no case_item_expression (Annex 37.72)";
  const hldb::Assignment *const assign = item->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "0");
}

TEST_F(CaseTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
