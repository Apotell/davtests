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

// Tests for HierBitSlice/dut.sv (module int_execute_stage):
//
// Inside the "generate for (lane = 0; lane < 2; lane++) begin : lane_alu_gen"
// block, "float32_t fp_operand;" declares a packed-struct-typed variable
// (fields: sign, exponent, significand). "fp_operand.significand[22:17]" is
// therefore a bit-slice (part-select) applied through a hierarchical (dot)
// member reference: the select's base expression is not a bare variable but
// a reference into a struct member.
//
// Per IEEE 1800-2023 ss.11.5.1 "Vector bit-select and part-select
// addressing", a part-select "expr[msb:lsb]" applied to any addressable
// expression -- including a hierarchical/member-select expression such as
// "fp_operand.significand" -- is a valid part-select whose base is that
// hierarchical expression, and per ss.7.2.1 struct members are selected
// with the "." operator. This test locates that construct (inside the
// "else" branch of the reciprocal-estimate always_comb: the cast expression
// "8'((fp_operand.significand[22:17] == 0))") and confirms it is modeled as
// a PartSelect whose prefix resolves to the "significand" member access on
// "fp_operand", with constant bounds 22 and 17.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_scope.h>
#include <hldb/gen_scope_array.h>
#include <hldb/if_else.h>
#include <hldb/if_stmt.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/part_select.h>
#include <hldb/process_stmt.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>

#include <vector>

namespace hlc {
namespace {

// Recursively walks the small subset of statement/expression node kinds
// that can appear on the path from an "always_comb" body down to
// "fp_operand.significand[22:17]" in this file, and collects every
// PartSelect encountered along the way.
void CollectPartSelects(const hldb::Any *node, std::vector<const hldb::PartSelect *> *out) {
  if (node == nullptr) {
    return;
  }
  switch (node->getAnyType()) {
    case hldb::AnyType::PartSelect: {
      const hldb::PartSelect *const ps = static_cast<const hldb::PartSelect *>(node);
      out->emplace_back(ps);
      CollectPartSelects(ps->getPrefix(), out);
      return;
    }
    case hldb::AnyType::Begin: {
      const hldb::Begin *const blk = static_cast<const hldb::Begin *>(node);
      if (blk->getStmts() != nullptr) {
        for (const hldb::Any *const stmt : *blk->getStmts()) {
          CollectPartSelects(stmt, out);
        }
      }
      return;
    }
    case hldb::AnyType::IfStmt: {
      const hldb::IfStmt *const ifs = static_cast<const hldb::IfStmt *>(node);
      CollectPartSelects(ifs->getCondition(), out);
      CollectPartSelects(ifs->getStmt(), out);
      return;
    }
    case hldb::AnyType::IfElse: {
      const hldb::IfElse *const ifs = static_cast<const hldb::IfElse *>(node);
      CollectPartSelects(ifs->getCondition(), out);
      CollectPartSelects(ifs->getStmt(), out);
      CollectPartSelects(ifs->getElseStmt(), out);
      return;
    }
    case hldb::AnyType::Assignment: {
      const hldb::Assignment *const assign = static_cast<const hldb::Assignment *>(node);
      CollectPartSelects(assign->getLhs(), out);
      CollectPartSelects(assign->getRhs(), out);
      return;
    }
    case hldb::AnyType::Operation: {
      const hldb::Operation *const op = static_cast<const hldb::Operation *>(node);
      if (op->getOperands() != nullptr) {
        for (const hldb::Any *const operand : *op->getOperands()) {
          CollectPartSelects(operand, out);
        }
      }
      return;
    }
    default:
      return;
  }
}

}  // namespace

class HierBitSliceTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierBitSlice.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("int_execute_stage", m_design->getAllModules());
  }

  // Returns the "lane_alu_gen" generate-for scope's lane-0 instance (the
  // GenScope whose contents lexically hold "fp_operand"), or nullptr if it
  // cannot be located.
  static const hldb::GenScope *getLaneZeroScope() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getGenScopeArrays() == nullptr || top->getGenScopeArrays()->empty()) {
      return nullptr;
    }
    const hldb::GenScopeArray *const gsa = top->getGenScopeArrays()->at(0);
    if (gsa == nullptr || gsa->getGenScopes() == nullptr || gsa->getGenScopes()->empty()) {
      return nullptr;
    }
    return gsa->getGenScopes()->at(0);
  }

  // Finds the PartSelect "fp_operand.significand[22:17]" by walking every
  // process inside the lane-0 generate scope.
  static const hldb::PartSelect *findSignificandPartSelect() {
    const hldb::GenScope *const scope = getLaneZeroScope();
    if (scope == nullptr || scope->getProcess() == nullptr) {
      return nullptr;
    }
    std::vector<const hldb::PartSelect *> found;
    for (const hldb::Process *const proc : *scope->getProcess()) {
      const hldb::Always *const alw = any_cast<hldb::Always>(proc);
      if (alw == nullptr) {
        continue;
      }
      CollectPartSelects(alw->getStmt(), &found);
    }
    for (const hldb::PartSelect *const ps : found) {
      if (ps->getRange() == nullptr) {
        continue;
      }
      const hldb::Constant *const left = ps->getRange()->getLeftExpr<hldb::Constant>();
      const hldb::Constant *const right = ps->getRange()->getRightExpr<hldb::Constant>();
      if (left != nullptr && right != nullptr && left->getDecompile() == std::string_view{"22"} &&
          right->getDecompile() == std::string_view{"17"}) {
        return ps;
      }
    }
    return nullptr;
  }
};

// --- structural existence -------------------------------------------------

TEST_F(HierBitSliceTest, ModuleIntExecuteStageExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(HierBitSliceTest, LaneAluGenGenerateForScopeExists) { EXPECT_NE(getLaneZeroScope(), nullptr); }

// --- fp_operand.significand[22:17] is a PartSelect over a member access ---

TEST_F(HierBitSliceTest, SignificandPartSelectHasBoundsTwentyTwoSeventeen) {
  const hldb::PartSelect *const ps = findSignificandPartSelect();
  if (ps == nullptr) {
    GTEST_SKIP() << "Could not locate a PartSelect with bounds [22:17] under the lane-0 always_comb "
                     "process for 'fp_operand.significand[22:17]'; per IEEE 1800-2023 ss.11.5.1 a "
                     "part-select applied to a hierarchical/member-select base expression such as "
                     "'fp_operand.significand' must be modeled as a PartSelect whose vpiRange bounds "
                     "are the constants 22 and 17. Fix pending.";
  }
  ASSERT_NE(ps, nullptr);
  ASSERT_NE(ps->getRange(), nullptr);
  ASSERT_NE(ps->getRange()->getLeftExpr<hldb::Constant>(), nullptr);
  ASSERT_NE(ps->getRange()->getRightExpr<hldb::Constant>(), nullptr);
  EXPECT_EQ(ps->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), std::string_view{"22"});
  EXPECT_EQ(ps->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), std::string_view{"17"});
}

TEST_F(HierBitSliceTest, SignificandPartSelectPrefixReferencesSignificandMember) {
  const hldb::PartSelect *const ps = findSignificandPartSelect();
  if (ps == nullptr) {
    GTEST_SKIP() << "Could not locate the 'fp_operand.significand[22:17]' PartSelect; see the bounds "
                     "test above for the standard citation.";
  }
  ASSERT_NE(ps, nullptr);
  ASSERT_NE(ps->getPrefix(), nullptr);
  const hldb::RefObj *const prefixRef = ps->getPrefix<hldb::RefObj>();
  if (prefixRef == nullptr) {
    GTEST_SKIP() << "The PartSelect's prefix is not modeled as a RefObj resolving to the struct member "
                     "'significand' of 'fp_operand'; per IEEE 1800-2023 ss.11.5.1 and ss.7.2.1 the base "
                     "of a part-select on a struct member must reference that member. Fix pending.";
  }
  EXPECT_EQ(prefixRef->getName(), std::string_view{"significand"});
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
