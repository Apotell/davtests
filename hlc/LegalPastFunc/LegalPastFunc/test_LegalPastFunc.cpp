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

// Tests for dut.sv (tags: LegalPastFunc)
//   module top();
//   sequence s_req_bad;
//     @ (posedge clk)
//     $rose(req) ##1 $past(!req,0);
//   endsequence
//
//   sequence s_req_ok;
//     @ (posedge clk)
//     $rose(req) ##1 $past(!req,1);
//   endsequence
//   endmodule
//
// IEEE 1800-2023 16.9.3: "number_of_ticks shall be 1 or greater and shall
// be an elaboration-time constant expression." So '$past(!req,0)' in
// s_req_bad is illegal and '$past(!req,1)' in s_req_ok is legal.
//
// What is checked:
//   - the illegal number_of_ticks (0) is diagnosed at 5:29
//     (HLDB_NON_POSITIVE_VALUE), and the legal one (1) on line 11 is not
//   - 'clk' and 'req' are never declared; neither is in a context where an
//     implicit net is created (6.10), so every reference must fail to bind
//   - module 'top' has exactly two SequenceDecls, s_req_bad and s_req_ok
//     (16.8)
//   - each sequence body is a clocked sequence (16.16 / 16.8): clocking
//     event is '@(posedge clk)' (vpiPosedgeOp on a reference to clk)
//   - the sequence expression is the binary cycle delay 'a ##1 b'
//     (16.9.2): vpiCycleDelayOp with operands ($rose(req), 1, $past(...))
//   - '$rose(req)' is a system function call with one argument 'req'
//     (16.9.3)
//   - '$past(!req,N)' is a system function call whose first argument is
//     vpiNotOp over 'req' and whose second argument is the number of
//     ticks N (0 for s_req_bad, 1 for s_req_ok)
//
// What is NOT checked and why:
//   - the exact message text of the number_of_ticks diagnostic (not
//     standard-mandated).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/clocked_seq.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/sequence_decl.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class LegalPastFuncTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LegalPastFunc.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::SequenceDecl *findSeq(std::string_view name) {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getSequenceDecls() == nullptr) return nullptr;
    for (const hldb::SequenceDecl *const s : *top->getSequenceDecls()) {
      if (s->getName() == name) return s;
    }
    return nullptr;
  }

  static void checkSequence(std::string_view name, std::string_view ticks) {
    const hldb::SequenceDecl *const seq = findSeq(name);
    ASSERT_NE(seq, nullptr) << "sequence '" << name << "' not found";
    const hldb::ClockedSeq *const cs = seq->getExpr<hldb::ClockedSeq>();
    ASSERT_NE(cs, nullptr) << "'@(posedge clk) ...' must be a clocked sequence";

    // @(posedge clk)
    const hldb::Operation *const ev = cs->getClockingEvent<hldb::Operation>();
    ASSERT_NE(ev, nullptr);
    EXPECT_EQ(ev->getOpType(), vpiPosedgeOp);
    ASSERT_NE(ev->getOperands(), nullptr);
    ASSERT_EQ(ev->getOperands()->size(), 1u);
    const hldb::RefObj *const clk = any_cast<hldb::RefObj>(ev->getOperands()->at(0));
    ASSERT_NE(clk, nullptr);
    EXPECT_EQ(clk->getName(), "clk");

    // $rose(req) ##1 $past(!req, N)
    const hldb::Operation *const delay = cs->getSequenceExpr<hldb::Operation>();
    ASSERT_NE(delay, nullptr);
    EXPECT_EQ(delay->getOpType(), vpiCycleDelayOp) << "16.9.2: 'a ##1 b' is a binary cycle delay";
    ASSERT_NE(delay->getOperands(), nullptr);
    ASSERT_EQ(delay->getOperands()->size(), 3u) << "lhs sequence, delay amount, rhs sequence";

    const hldb::SysFuncCall *const rose = any_cast<hldb::SysFuncCall>(delay->getOperands()->at(0));
    ASSERT_NE(rose, nullptr) << "'$rose(req)' must be a system function call";
    EXPECT_EQ(rose->getName(), "$rose");
    ASSERT_NE(rose->getArguments(), nullptr);
    ASSERT_EQ(rose->getArguments()->size(), 1u);
    const hldb::RefObj *const roseArg = any_cast<hldb::RefObj>(rose->getArguments()->at(0));
    ASSERT_NE(roseArg, nullptr);
    EXPECT_EQ(roseArg->getName(), "req");

    const hldb::Constant *const amount = any_cast<hldb::Constant>(delay->getOperands()->at(1));
    ASSERT_NE(amount, nullptr) << "'##1' delay amount must be a Constant";
    EXPECT_EQ(amount->getValue(), "1");

    const hldb::SysFuncCall *const past = any_cast<hldb::SysFuncCall>(delay->getOperands()->at(2));
    ASSERT_NE(past, nullptr) << "'$past(...)' must be a system function call";
    EXPECT_EQ(past->getName(), "$past");
    ASSERT_NE(past->getArguments(), nullptr);
    ASSERT_EQ(past->getArguments()->size(), 2u) << "expression1 and number_of_ticks";
    const hldb::Operation *const notReq = any_cast<hldb::Operation>(past->getArguments()->at(0));
    ASSERT_NE(notReq, nullptr);
    EXPECT_EQ(notReq->getOpType(), vpiNotOp);
    ASSERT_NE(notReq->getOperands(), nullptr);
    ASSERT_EQ(notReq->getOperands()->size(), 1u);
    const hldb::RefObj *const req = any_cast<hldb::RefObj>(notReq->getOperands()->at(0));
    ASSERT_NE(req, nullptr);
    EXPECT_EQ(req->getName(), "req");
    const hldb::Constant *const n = any_cast<hldb::Constant>(past->getArguments()->at(1));
    ASSERT_NE(n, nullptr) << "number_of_ticks must be a Constant";
    EXPECT_EQ(n->getValue(), ticks);
  }
};

// ---------------------------------------------------------------------------
// 16.9.3: number_of_ticks shall be 1 or greater
// ---------------------------------------------------------------------------

TEST_F(LegalPastFuncTest, ZeroTicksIsDiagnosed) {
  EXPECT_NE(findError(ErrorDefinition::HLDB_NON_POSITIVE_VALUE, 5, 29), nullptr)
      << "16.9.3: '$past(!req,0)' -- number_of_ticks shall be 1 or greater";
}

TEST_F(LegalPastFuncTest, OneTickIsNotDiagnosed) {
  EXPECT_EQ(findError(ErrorDefinition::HLDB_NON_POSITIVE_VALUE, 11), nullptr) << "16.9.3: '$past(!req,1)' is legal";
}

// ---------------------------------------------------------------------------
// 6.10: no implicit nets for 'clk' / 'req' in these contexts
// ---------------------------------------------------------------------------

TEST_F(LegalPastFuncTest, UndeclaredReqFailsToBind) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "req", 5, 9), nullptr);
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "req", 5, 25), nullptr);
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "req", 11, 9), nullptr);
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "req", 11, 25), nullptr);
}

TEST_F(LegalPastFuncTest, UndeclaredClkFailsToBind) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "clk", 4, 14), nullptr)
      << "6.10: 'clk' is never declared and a clocking event is not an implicit-net context";
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "clk", 10, 14), nullptr)
      << "6.10: 'clk' is never declared and a clocking event is not an implicit-net context";
}

// ---------------------------------------------------------------------------
// Sequence declarations -- 16.8
// ---------------------------------------------------------------------------

TEST_F(LegalPastFuncTest, TopHasTwoSequenceDecls) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  ASSERT_NE(top->getSequenceDecls(), nullptr);
  EXPECT_EQ(top->getSequenceDecls()->size(), 2u);
  EXPECT_NE(findSeq("s_req_bad"), nullptr);
  EXPECT_NE(findSeq("s_req_ok"), nullptr);
}

TEST_F(LegalPastFuncTest, SequencesHaveNoFormals) {
  for (std::string_view name : {"s_req_bad", "s_req_ok"}) {
    const hldb::SequenceDecl *const seq = findSeq(name);
    ASSERT_NE(seq, nullptr) << name;
    EXPECT_TRUE(seq->getSeqFormalDecls() == nullptr || seq->getSeqFormalDecls()->empty()) << name;
  }
}

TEST_F(LegalPastFuncTest, SReqBadShape) { checkSequence("s_req_bad", "0"); }

TEST_F(LegalPastFuncTest, SReqOkShape) { checkSequence("s_req_ok", "1"); }

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
