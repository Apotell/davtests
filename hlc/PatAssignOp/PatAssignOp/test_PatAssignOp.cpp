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

// Validates the HLDB model built for tests/PatAssignOp/dut.sv:
//
//   module top ();
//     localparam coh_noc_dims_p         = 2;
//     localparam coh_noc_trans_p        = 0;
//     localparam  coh_noc_x_cord_width_p = 10;
//     localparam  coh_noc_y_cord_width_p = 20;
//     localparam int coh_noc_cord_markers_pos_p[coh_noc_dims_p:0] = coh_noc_trans_p ?
//     '{coh_noc_x_cord_width_p+coh_noc_y_cord_width_p, coh_noc_y_cord_width_p, 0}
//         : '{coh_noc_y_cord_width_p+coh_noc_x_cord_width_p, coh_noc_x_cord_width_p, 0};
//     localparam coh_noc_cord_width_p   = coh_noc_cord_markers_pos_p[coh_noc_dims_p];
//     if (coh_noc_cord_width_p != 30) begin
//        BAD bad();
//     end
//    if (coh_noc_cord_width_p == 30) begin
//        GOOD good();
//     end
//   endmodule
//
// The fixture chooses between two array assignment patterns (10.9.1) with a
// conditional operator (11.4.11) to set an unpacked int array local
// parameter, then selects one element of it. coh_noc_trans_p is 0, so the
// second pattern is chosen: '{20+10, 10, 0}. Element [2] is the first item,
// 30, so the '== 30' block is selected and the '!= 30' block is not.
//
// What is checked, and why:
//   - the 6 localparams are local (6.20.4); the 4 scalar ones hold the
//     literals 2, 0, 10 and 20
//   - coh_noc_cord_markers_pos_p is an unpacked array of int (7.4.2) whose
//     value is a conditional operator over coh_noc_trans_p and two 3-item
//     assignment patterns (10.9.1); each pattern's first item is an
//     addition of the two width parameters, in the order written
//   - coh_noc_cord_width_p's value is a bit-select [coh_noc_dims_p] of the
//     array parameter (11.5.1)
//   - top holds 2 'if' generate constructs (27.5): '!= 30' holding BAD bad,
//     then '== 30' holding GOOD good
//   - no error names BAD: its block is not selected, so BAD is not
//     instantiated into the model (27.5)
//   - no fatal or syntax diagnostic
//
// KNOWN COMPILER BUG (errors for modules in unselected generate blocks), not
// a defect in this test: HLC reports an "Unsupported typespec" error for the
// undefined BAD of the '!= 30' block, although 27.5 says "The conditional
// generate constructs, if-generate and case-generate, select at most one
// generate block from a set of alternative generate blocks based on constant
// expressions evaluated during elaboration. The selected generate block, if
// any, is instantiated into the model." UnselectedBadBlockReportsNoError is
// expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// What is NOT checked, and why:
//   - Whether HLC reports the undefined GOOD of the selected block; this file
//     does not pin how that is reported.
//   - The reduced value 30 and the selected block: no .hlc in this suite
//     elaborates.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_block.h>
#include <hldb/gen_if.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PatAssignOpTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PatAssignOp.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Whether a diagnostic of error severity names 'symbol'. Which error
  // identifier HLC picks for it is a tool convention, so any is accepted.
  bool reportsErrorNaming(std::string_view symbol) {
    for (const ErrorDefinition::ErrorMap::value_type &entry : ErrorDefinition::getErrorInfoMap()) {
      if ((entry.second.m_severity == ErrorDefinition::ERROR) && (findError(entry.first, symbol) != nullptr)) {
        return true;
      }
    }
    return false;
  }

  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Parameter *getParam(std::string_view name) {
    return hldb::findByName<hldb::Parameter>(name, getTop()->getParameters());
  }

  static const hldb::ParamAssign *getAssign(std::string_view name) {
    const hldb::Module *const m = getTop();
    if ((m == nullptr) || (m->getParamAssigns() == nullptr)) return nullptr;
    for (const hldb::ParamAssign *const pa : *m->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == name)) return pa;
    }
    return nullptr;
  }

  static void ExpectConstant(const hldb::Any *e, std::string_view text, std::string_view why) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(e);
    ASSERT_NE(c, nullptr) << why << ": a literal";
    EXPECT_EQ(c->getDecompile(), text) << why;
  }

  static void ExpectRef(const hldb::Any *e, std::string_view name) {
    const hldb::RefObj *const r = any_cast<hldb::RefObj>(e);
    ASSERT_NE(r, nullptr) << name << " is a reference";
    EXPECT_EQ(r->getActual(), getParam(name)) << name << " binds to top's localparam";
  }

  // Verifies 'e' is the 3-item pattern '{first + second, third, 0}.
  static void ExpectPattern(const hldb::Any *e, std::string_view first, std::string_view second,
                            std::string_view third) {
    const hldb::Operation *const pat = any_cast<hldb::Operation>(e);
    ASSERT_NE(pat, nullptr);
    EXPECT_EQ(pat->getOpType(), vpiAssignmentPatternOp) << "10.9.1: an assignment pattern";
    ASSERT_NE(pat->getOperands(), nullptr);
    ASSERT_EQ(pat->getOperands()->size(), 3u) << "10.9.1: one item per element of [2:0]";
    const hldb::Operation *const add = any_cast<hldb::Operation>(pat->getOperands()->at(0));
    ASSERT_NE(add, nullptr);
    EXPECT_EQ(add->getOpType(), vpiAddOp) << "11.4.3: '+'";
    ASSERT_NE(add->getOperands(), nullptr);
    ASSERT_EQ(add->getOperands()->size(), 2u);
    ExpectRef(add->getOperands()->at(0), first);
    ExpectRef(add->getOperands()->at(1), second);
    ExpectRef(pat->getOperands()->at(1), third);
    ExpectConstant(pat->getOperands()->at(2), "0", "the last item");
  }

  static const hldb::GenIf *getGenIf(size_t i) {
    const hldb::Module *const m = getTop();
    if ((m == nullptr) || (m->getGenStmts() == nullptr) || (m->getGenStmts()->size() != 2)) return nullptr;
    return any_cast<hldb::GenIf>(m->getGenStmts()->at(i));
  }
};

TEST_F(PatAssignOpTest, ScalarLocalparamsHoldTheirLiterals) {
  ASSERT_NE(getTop(), nullptr);
  const char *const names[] = {"coh_noc_dims_p", "coh_noc_trans_p", "coh_noc_x_cord_width_p", "coh_noc_y_cord_width_p"};
  const char *const values[] = {"2", "0", "10", "20"};
  for (size_t i = 0; i < 4; ++i) {
    const hldb::Parameter *const p = getParam(names[i]);
    ASSERT_NE(p, nullptr) << names[i];
    EXPECT_TRUE(p->getLocalParam()) << "6.20.4: declared 'localparam'";
    const hldb::ParamAssign *const pa = getAssign(names[i]);
    ASSERT_NE(pa, nullptr);
    ExpectConstant(pa->getRhs(), values[i], names[i]);
  }
}

TEST_F(PatAssignOpTest, MarkersIsUnpackedIntArray) {
  ASSERT_NE(getTop(), nullptr);
  const hldb::Parameter *const p = getParam("coh_noc_cord_markers_pos_p");
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getLocalParam()) << "6.20.4";
  ASSERT_NE(p->getTypespec(), nullptr);
  EXPECT_NE(p->getTypespec()->getActual<hldb::ArrayTypespec>(), nullptr) << "7.4.2: an unpacked array";
}

TEST_F(PatAssignOpTest, MarkersValueChoosesBetweenTwoPatterns) {
  ASSERT_NE(getTop(), nullptr);
  const hldb::ParamAssign *const pa = getAssign("coh_noc_cord_markers_pos_p");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const cond = pa->getRhs<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiConditionOp) << "11.4.11: '?:'";
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 3u);
  ExpectRef(cond->getOperands()->at(0), "coh_noc_trans_p");
  ExpectPattern(cond->getOperands()->at(1), "coh_noc_x_cord_width_p", "coh_noc_y_cord_width_p",
                "coh_noc_y_cord_width_p");
  ExpectPattern(cond->getOperands()->at(2), "coh_noc_y_cord_width_p", "coh_noc_x_cord_width_p",
                "coh_noc_x_cord_width_p");
}

TEST_F(PatAssignOpTest, CordWidthSelectsElementDims) {
  ASSERT_NE(getTop(), nullptr);
  const hldb::Parameter *const p = getParam("coh_noc_cord_width_p");
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getLocalParam()) << "6.20.4";
  const hldb::ParamAssign *const pa = getAssign("coh_noc_cord_width_p");
  ASSERT_NE(pa, nullptr);
  const hldb::BitSelect *const bs = pa->getRhs<hldb::BitSelect>();
  ASSERT_NE(bs, nullptr) << "11.5.1: an element select";
  ExpectRef(bs->getPrefix(), "coh_noc_cord_markers_pos_p");
  ExpectRef(bs->getIndex(), "coh_noc_dims_p");
}

TEST_F(PatAssignOpTest, TwoGenerateIfsHoldBadThenGood) {
  const char *const modules[] = {"BAD", "GOOD"};
  const char *const insts[] = {"bad", "good"};
  const int ops[] = {vpiNeqOp, vpiEqOp};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::GenIf *const gi = getGenIf(i);
    ASSERT_NE(gi, nullptr) << "27.5: top holds 2 'if' generate constructs";
    const hldb::Operation *const c = any_cast<hldb::Operation>(gi->getCondition());
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->getOpType(), ops[i]) << "11.4.5";
    ASSERT_NE(c->getOperands(), nullptr);
    ASSERT_EQ(c->getOperands()->size(), 2u);
    ExpectRef(c->getOperands()->at(0), "coh_noc_cord_width_p");
    ExpectConstant(c->getOperands()->at(1), "30", "the compared value");
    const hldb::GenBlock *const b = any_cast<hldb::GenBlock>(gi->getStmt());
    ASSERT_NE(b, nullptr);
    ASSERT_NE(b->getStmts(), nullptr);
    const hldb::RefInstance *inst = nullptr;
    for (const hldb::Any *s : *b->getStmts()) {
      if (const hldb::RefInstance *const ri = any_cast<hldb::RefInstance>(s)) inst = ri;
    }
    ASSERT_NE(inst, nullptr) << "the block holds a module instance";
    EXPECT_EQ(inst->getName(), insts[i]);
    ASSERT_NE(inst->getTypespec(), nullptr);
    EXPECT_EQ(inst->getTypespec()->getName(), modules[i]);
  }
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (errors for
// modules in unselected generate blocks) in the file header.
TEST_F(PatAssignOpTest, UnselectedBadBlockReportsNoError) {
  EXPECT_FALSE(reportsErrorNaming("BAD")) << "27.5: the '!= 30' block is not selected, so BAD is not instantiated "
                                             "into the model";
}

TEST_F(PatAssignOpTest, NoFatalOrSyntaxDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
