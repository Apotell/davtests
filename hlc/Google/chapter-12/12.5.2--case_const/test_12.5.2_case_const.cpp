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

// Tests for 12.5.2--case_const.sv (tags: 12.5.2)
//   module case_tb ();
//     wire [3:0] a = 0;
//     reg [3:0] b = 0;
//     always @* begin
//       case(1)
//         a[0] : b = 1;
//         a[1] : b = 2;
//         a[2] : b = 3;
//         a[3] : b = 4;
//         default b = 0;
//       endcase
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 12.5.2 "Constant expression in
// case statement", p.322, checked before any test code was written):
//   "A constant expression can be used for the case_expression. The
//   value of the constant expression shall be compared against the
//   case_item_expressions... the case_expression is a constant
//   expression (1). The case_items are expressions (bit-selects) and
//   are compared against the constant expression for a match." This is
//   the classic "priority encoder" idiom: the roles of 12.5's normal
//   case are inverted -- the case_expression is the constant "1" and
//   each case_item_expression ("a[0]".."a[3]") is a bit-select that
//   must itself evaluate to 1 to match.
//
//   Also (IEEE 1800-2023 6.7/6.8): "wire" is a net_type keyword so
//   "wire a" must be a Net; "reg" is a non-net integer_vector_type
//   keyword so "reg b" must be a Variable.
//
// What is checked:
//   - module case_tb has exactly 1 Net "a" (wire) and 1 Variable "b" (reg)
//   - CaseStmt exists, getCaseType() == vpiCaseExact (plain "case", not
//     casex/casez), getQualifier() == vpiNoQualifier, condition is
//     Constant "1" (vpiUIntConst) -- not a RefObj, confirming the
//     constant-expression form of 12.5.2
//   - exactly 5 CaseItems
//   - the first 4 CaseItems each have exactly one vpiExpr: a BitSelect
//     ("a[0]".."a[3]") whose prefix RefObj resolves to the Net "a", and
//     a body Assignment to Variable "b" with the corresponding value
//     (1, 2, 3, 4)
//   - the 5th (default) CaseItem has a null getExprs() (per Annex
//     37.72 detail 2) and body Assignment "b = 0"
//   - no continuous assignments exist in the module
//
// What is NOT checked and why:
//   - the runtime priority-encoder matching behavior (which bit-select
//     actually evaluates to 1 during simulation) is a simulation-time
//     concept, not a static/structural compile-time property
//   - the "always @*" implicit sensitivity list is a chapter-9 concept

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
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

class CaseConstTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12.5.2--case_const.hlc"}); }
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

  static void checkBitSelectItem(const hldb::CaseItem *item, std::string_view bitSelectName,
                                  std::string_view rhsValue) {
    ASSERT_NE(item, nullptr);
    ASSERT_NE(item->getExprs(), nullptr) << "explicit case item must have a case_item_expression";
    ASSERT_EQ(item->getExprs()->size(), 1u);
    const hldb::BitSelect *const expr = any_cast<hldb::BitSelect>(item->getExprs()->at(0));
    ASSERT_NE(expr, nullptr) << "case_item_expression should be a BitSelect";
    EXPECT_EQ(expr->getName(), bitSelectName);
    const hldb::RefObj *const prefix = expr->getPrefix<hldb::RefObj>();
    ASSERT_NE(prefix, nullptr);
    EXPECT_NE(prefix->getActual<hldb::Net>(), nullptr) << "'a' should resolve to the Net, not a Variable";
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

TEST_F(CaseConstTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(CaseConstTest, ModuleHasOneNetAAndOneVariableB) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Net>("a", top->getNets()), nullptr) << "Net 'a' not found";
  ASSERT_NE(top->getVariables(), nullptr) << "'reg b' should be a Variable, not a Net";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Variable>("b", top->getVariables()), nullptr) << "Variable 'b' not found";
}

TEST_F(CaseConstTest, CaseStmtExistsWithExactTypeAndNoQualifier) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr) << "the single statement in the always body should resolve to CaseStmt";
  EXPECT_EQ(cs->getCaseType(), vpiCaseExact);
  EXPECT_EQ(cs->getQualifier(), vpiNoQualifier);
}

TEST_F(CaseConstTest, ConditionIsConstantOneNotARefObj) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  const hldb::Constant *const cond = cs->getCondition<hldb::Constant>();
  ASSERT_NE(cond, nullptr) << "case_expression should be the constant '1', per 12.5.2";
  EXPECT_EQ(cond->getConstType(), vpiUIntConst);
  EXPECT_EQ(cond->getDecompile(), "1");
}

TEST_F(CaseConstTest, ExactlyFiveCaseItems) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  EXPECT_EQ(cs->getCaseItems()->size(), 5u);
}

TEST_F(CaseConstTest, FirstItemIsA0AssigningOne) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 1u);
  checkBitSelectItem(cs->getCaseItems()->at(0), "a[0]", "1");
}

TEST_F(CaseConstTest, SecondItemIsA1AssigningTwo) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 2u);
  checkBitSelectItem(cs->getCaseItems()->at(1), "a[1]", "2");
}

TEST_F(CaseConstTest, ThirdItemIsA2AssigningThree) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 3u);
  checkBitSelectItem(cs->getCaseItems()->at(2), "a[2]", "3");
}

TEST_F(CaseConstTest, FourthItemIsA3AssigningFour) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 4u);
  checkBitSelectItem(cs->getCaseItems()->at(3), "a[3]", "4");
}

TEST_F(CaseConstTest, FifthItemIsDefaultWithNullExprsAssigningZero) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 5u);
  const hldb::CaseItem *const item = cs->getCaseItems()->at(4);
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->getExprs(), nullptr) << "default case item must have no case_item_expression (Annex 37.72)";
  const hldb::Assignment *const assign = item->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "0");
}

TEST_F(CaseConstTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
