/*
 Copyright 2026 Alain Dargelas

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

// IEEE 1800-2023 Sec 16.13.6: sequence methods 'triggered' / 'matched' used as a
// sequence_expr operand, with and without a repetition.
// SV: tests/SequenceMethodCall/dut.sv
//
//   sequence s1;          @(posedge clk) a ##1 b;               endsequence
//   sequence trig_plain;  s1.triggered ##1 c;                   endsequence
//   sequence trig_rep;    s1.triggered[*2] ##1 c;               endsequence
//   sequence match_goto;  @(posedge clk) s1.matched[->1] ##1 c; endsequence
//
// Annex A.2.10: sequence_expr ::= ... | sequence_method_call [ boolean_abbrev ]
// so the repetition applies to the method call and must not be dropped.
//
// Operand order (Sec 37.54 detail 3):
//   vpiCycleDelayOp:   lhs sequence, rhs sequence, delay
//   repeat operators:  repeated sequence, repeat count
//
// The method call itself is the subject of the repetition. It is checked
// through what it must denote -- the 'triggered'/'matched' method of the
// sequence s1 -- rather than through its exact container shape.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/clocked_seq.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/sequence_decl.h>
#include <hldb/sequence_inst.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <gtest/gtest.h>

#include <string>

namespace hlc {

class SequenceMethodCallTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "SequenceMethodCall.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::Module *getModule() const {
    return hldb::findByName<hldb::Module>("sequence_method_call", m_design->getAllModules());
  }

  const hldb::SequenceDecl *findSeq(std::string_view name) const {
    const hldb::Module *mod = getModule();
    if (mod == nullptr) return nullptr;
    return hldb::findByName<hldb::SequenceDecl>(name, mod->getSequenceDecls());
  }

  // The operand at 'index' of 'op', or nullptr.
  static const hldb::Any *operandAt(const hldb::Operation *op, size_t index) {
    if ((op == nullptr) || (op->getOperands() == nullptr) || (op->getOperands()->size() <= index)) return nullptr;
    return (*op->getOperands())[index];
  }

  // Checks that 'subject' denotes the method 'method' of the sequence declared as 's1':
  // a reference whose actual is the MethodFuncCall of that name, reached through a
  // SequenceInst bound to s1.
  void expectMethodOfS1(const hldb::Any *subject, std::string_view method) const {
    const hldb::RefObj *ref = any_cast<hldb::RefObj>(subject);
    ASSERT_NE(ref, nullptr) << "the repeated subject must reference '" << method << "'";
    const hldb::MethodFuncCall *call = ref->getActual<hldb::MethodFuncCall>();
    ASSERT_NE(call, nullptr) << "'" << method << "' must resolve to a method call";
    EXPECT_EQ(call->getName(), method);

    const hldb::SequenceInst *receiver = nullptr;
    if (ref->getPathElems() != nullptr) {
      for (const hldb::Any *elem : *ref->getPathElems()) {
        if ((receiver = any_cast<hldb::SequenceInst>(elem)) != nullptr) break;
      }
    }
    ASSERT_NE(receiver, nullptr) << "the method receiver must be the sequence instance 's1'";
    EXPECT_EQ(receiver->getName(), "s1");
    EXPECT_EQ(receiver->getSequenceDecl(), findSeq("s1")) << "the receiver must be bound to sequence s1";
  }
};

TEST_F(SequenceMethodCallTest, ModuleExists) { ASSERT_NE(getModule(), nullptr); }

// Nothing in these sequences may be dropped from the model.
TEST_F(SequenceMethodCallTest, NoOrphanNodes) {
  EXPECT_EQ(findError(ErrorDefinition::LINT_GOT_ORPHAN_NODES), nullptr);
}

TEST_F(SequenceMethodCallTest, NoWeakNodes) {
  EXPECT_EQ(findError(ErrorDefinition::LINT_WEAK_REF_NODES), nullptr);
}

// ----------------------------------------------------------------------------
// s1.triggered ##1 c
// ----------------------------------------------------------------------------
TEST_F(SequenceMethodCallTest, TrigPlain_IsCycleDelayOfTriggeredAndC) {
  const hldb::SequenceDecl *sd = findSeq("trig_plain");
  ASSERT_NE(sd, nullptr);
  const hldb::Operation *op = any_cast<hldb::Operation>(sd->getExpr());
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 3u) << "Sec 37.54: [lhs, rhs, delay]";
  expectMethodOfS1(operandAt(op, 0), "triggered");
  const hldb::RefObj *rhs = any_cast<hldb::RefObj>(operandAt(op, 1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "c");
}

// ----------------------------------------------------------------------------
// s1.triggered[*2] ##1 c
// ----------------------------------------------------------------------------
TEST_F(SequenceMethodCallTest, TrigRep_LhsIsConsecutiveRepeat) {
  const hldb::SequenceDecl *sd = findSeq("trig_rep");
  ASSERT_NE(sd, nullptr);
  const hldb::Operation *op = any_cast<hldb::Operation>(sd->getExpr());
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp);
  const hldb::Operation *rep = any_cast<hldb::Operation>(operandAt(op, 0));
  ASSERT_NE(rep, nullptr) << "Annex A.2.10: '[*2]' applies to 's1.triggered' and must be kept";
  EXPECT_EQ(rep->getOpType(), vpiConsecutiveRepeatOp);
}

TEST_F(SequenceMethodCallTest, TrigRep_RepeatOperands) {
  const hldb::SequenceDecl *sd = findSeq("trig_rep");
  ASSERT_NE(sd, nullptr);
  const hldb::Operation *op = any_cast<hldb::Operation>(sd->getExpr());
  ASSERT_NE(op, nullptr);
  const hldb::Operation *rep = any_cast<hldb::Operation>(operandAt(op, 0));
  ASSERT_NE(rep, nullptr);
  ASSERT_NE(rep->getOperands(), nullptr);
  ASSERT_EQ(rep->getOperands()->size(), 2u) << "Sec 37.54: [repeated sequence, count]";
  expectMethodOfS1(operandAt(rep, 0), "triggered");
  const hldb::Constant *cnt = any_cast<hldb::Constant>(operandAt(rep, 1));
  ASSERT_NE(cnt, nullptr);
  EXPECT_EQ(cnt->getValue(), "2");
}

TEST_F(SequenceMethodCallTest, TrigRep_RhsIsCAndDelayIsOne) {
  const hldb::SequenceDecl *sd = findSeq("trig_rep");
  ASSERT_NE(sd, nullptr);
  const hldb::Operation *op = any_cast<hldb::Operation>(sd->getExpr());
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 3u);
  const hldb::RefObj *rhs = any_cast<hldb::RefObj>(operandAt(op, 1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "c");
  const hldb::Constant *delay = any_cast<hldb::Constant>(operandAt(op, 2));
  ASSERT_NE(delay, nullptr);
  EXPECT_EQ(delay->getValue(), "1");
}

// ----------------------------------------------------------------------------
// @(posedge clk) s1.matched[->1] ##1 c
// ----------------------------------------------------------------------------
TEST_F(SequenceMethodCallTest, MatchGoto_RepeatOfMatched) {
  const hldb::SequenceDecl *sd = findSeq("match_goto");
  ASSERT_NE(sd, nullptr);
  const hldb::ClockedSeq *cs = any_cast<hldb::ClockedSeq>(sd->getExpr());
  ASSERT_NE(cs, nullptr);
  const hldb::Operation *op = any_cast<hldb::Operation>(cs->getSequenceExpr());
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp);
  const hldb::Operation *rep = any_cast<hldb::Operation>(operandAt(op, 0));
  ASSERT_NE(rep, nullptr) << "Annex A.2.10: '[->1]' applies to 's1.matched' and must be kept";
  EXPECT_EQ(rep->getOpType(), vpiGotoRepeatOp);
  ASSERT_NE(rep->getOperands(), nullptr);
  ASSERT_EQ(rep->getOperands()->size(), 2u) << "Sec 37.54: [repeated sequence, count]";
  expectMethodOfS1(operandAt(rep, 0), "matched");
  const hldb::Constant *cnt = any_cast<hldb::Constant>(operandAt(rep, 1));
  ASSERT_NE(cnt, nullptr);
  EXPECT_EQ(cnt->getValue(), "1");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
