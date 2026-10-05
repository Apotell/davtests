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

// Tests for tests/MultContAssign/dut.sv (tags: MultContAssign)
//   module top();
//     int v;
//     assign (highz0, weak1)    inout_i1 = pu;          // line 6
//     assign (weak0, highz1)    inout_i1 = ~pd;         // line 7
//     assign v = 12;                                    // line 9
//     assign v = 13;                                    // line 10
//     assign (weak0, weak1)    inout_i2 = pu;           // line 13
//     assign (weak1, weak0)     inout_i2 = ~pd;         // line 14
//     if (0) assign (weak0, weak1)    inout_i3 = pu;    // line 17
//     else assign (weak1, weak0)     inout_i3 = ~pd;    // line 18
//     assign    out = (sel == 2'b00)? v0 : 2'bz;        // line 21
//     assign    out = (sel == 2'b01)? v1 : 2'bz;        // line 22
//     assign     (pull1, pull0) pull_up_1 = 1'b1;       // line 24
//     assign     pull_up_1 = (driver_1_en) ? driver_1 : 1'bz;  // line 26
//     parameter NUM_WAYS = 1;
//     generate case (NUM_WAYS)
//       1: begin assign fill_way = 0; end
//       2: begin assign fill_way = !lru_flags[0]; end
//     endcase endgenerate
//   endmodule
//
//   module wandwor_test0 (A, B, X);
//     input A, B;
//     output wor X;
//     assign X = A, X = B;
//   endmodule
//
//   module top2;
//     uwire two;
//     assign two = 1'b1;
//     assign two = 1'b0;
//     initial $display("Failed: this should be a compile time error!");
//   endmodule
//
// What is checked (IEEE 1800-2023):
//   - 6.5: "Variables can be written by ... one continuous assignment" and
//     "it shall be an error to have multiple continuous assignments ...
//     writing to any term in the expansion of the longest static prefix of
//     a variable". 'int v' is a variable written by two continuous
//     assignments -> HLDB_MULTIPLE_CONT_ASSIGN for "v".
//   - 6.5: "A net can be written by one or more continuous assignments".
//     inout_i1, inout_i2, out and pull_up_1 are implicit nets (6.10: an
//     undeclared identifier on the LHS of a continuous assignment is an
//     implicit scalar net of the default net type), and X is a declared wor
//     net -- none of them may be flagged HLDB_MULTIPLE_CONT_ASSIGN.
//   - 6.10: those implicit-net LHS identifiers (and inout_i3, fill_way inside
//     generate blocks) are legal and must not fail to bind.
//   - 6.10 / 6.3: the RHS identifiers pu, pd, sel, v0, v1, driver_1_en and
//     driver_1 are not in any position where an implicit net is assumed, and
//     are never declared -> each must fail to bind (COMP_FAILED_TO_BIND).
//   - 6.6.2: "It shall be an error to connect any bit of a uwire net to more
//     than one driver." 'two' has two continuous assignments ->
//     HLDB_MULTIPLE_DRIVERS_ON_UWIRE for "two".
//   - 10.3.2 / 10.3.4 / 37.46 (vpiStrength0/vpiStrength1 on cont assign):
//     drive strengths are recorded per assignment, regardless of the order
//     they are written in -- (highz0, weak1), (weak0, highz1),
//     (weak0, weak1), (weak1, weak0), (pull1, pull0). Values use the VPI
//     strength encoding (vpiHiZ, vpiWeakDrive, vpiPullDrive).
//   - 10.3.2: module top has exactly 10 module-level continuous assignments;
//     "assign X = A, X = B;" (list_of_net_assignments) produces two
//     continuous assignments in wandwor_test0.
//   - 6.6.3 / 6.6.2: X is a wor net, two is a uwire net; neither declaration
//     has a net_decl_assignment.
//   - 27.5: the "if (0) ... else ..." generate construct keeps its condition
//     and both branches, each a single continuous assignment to inout_i3;
//     the case generate has two case items, each a begin-end block holding
//     one continuous assignment to fill_way.
//   - 11.4.11: line 21's RHS is a conditional operator (vpiConditionOp).
//
// What is NOT checked and why:
//   - default strength for assignments without a drive_strength (10.3.4
//     says strong0/strong1 by default): whether the model should store an
//     explicit vpiStrongDrive or leave it 0 is not specified by 37.46.
//   - lru_flags (only referenced in the unselected case branch 2): whether
//     names in an unselected generate branch must be resolved is not
//     settled by 27.5 for a non-elaborated model.
//   - the $display string in top2: unrelated to continuous assignments.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/case_item.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/gen_case.h>
#include <hldb/gen_if_else.h>
#include <hldb/gen_region.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class MultContAssignTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "MultContAssign.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  // Returns the module-level continuous assignment of 'module' starting on
  // source line 'line'.
  static const hldb::ContAssign *getAssignAtLine(std::string_view module, uint32_t line) {
    const hldb::Module *const m = getModule(module);
    if ((m == nullptr) || (m->getContAssigns() == nullptr)) return nullptr;
    for (const hldb::ContAssign *const ca : *m->getContAssigns()) {
      if (ca->getStartLine() == line) return ca;
    }
    return nullptr;
  }

  static void expectStrengths(uint32_t line, int32_t s0, int32_t s1) {
    const hldb::ContAssign *const ca = getAssignAtLine("top", line);
    ASSERT_NE(ca, nullptr) << "line " << line;
    EXPECT_EQ(ca->getStrength0(), s0) << "line " << line << ": 0-strength";
    EXPECT_EQ(ca->getStrength1(), s1) << "line " << line << ": 1-strength";
  }

  // A generate branch with a single item (27.5) is still a generate block;
  // accept the continuous assignment either directly or wrapped in a Begin
  // that holds exactly that one item.
  static void expectLhsName(const hldb::Any *stmt, std::string_view name) {
    if (const hldb::Begin *const blk = any_cast<hldb::Begin>(stmt)) {
      ASSERT_NE(blk->getStmts(), nullptr) << name;
      ASSERT_EQ(blk->getStmts()->size(), 1u) << name;
      stmt = blk->getStmts()->at(0);
    }
    const hldb::ContAssign *const ca = any_cast<hldb::ContAssign>(stmt);
    ASSERT_NE(ca, nullptr) << name;
    const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr) << name;
    EXPECT_EQ(lhs->getName(), name);
  }
};

// ===========================================================================
// 6.5: multiple continuous assignments to a variable vs. to nets
// ===========================================================================

TEST_F(MultContAssignTest, VIsAVariable) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("v", top->getVariables()), nullptr);
}

TEST_F(MultContAssignTest, MultipleContAssignsToVariableIsError) {
  EXPECT_NE(findError(ErrorDefinition::HLDB_MULTIPLE_CONT_ASSIGN, "v"), nullptr)
      << "6.5: variable 'v' is written by two continuous assignments";
}

TEST_F(MultContAssignTest, MultipleContAssignsToNetsAreLegal) {
  for (std::string_view name : {"inout_i1", "inout_i2", "out", "pull_up_1", "X"}) {
    EXPECT_EQ(findError(ErrorDefinition::HLDB_MULTIPLE_CONT_ASSIGN, name), nullptr)
        << "6.5: '" << name << "' is a net; nets can be written by multiple continuous assignments";
  }
}

TEST_F(MultContAssignTest, MultipleDriversOnUwireIsError) {
  EXPECT_NE(findError(ErrorDefinition::HLDB_MULTIPLE_DRIVERS_ON_UWIRE, "two"), nullptr)
      << "6.6.2: uwire 'two' is connected to two drivers";
}

// ===========================================================================
// 6.10: implicit nets on LHS vs. undeclared identifiers on RHS
// ===========================================================================

TEST_F(MultContAssignTest, ImplicitNetsOnLhsBind) {
  for (std::string_view name : {"inout_i1", "inout_i2", "inout_i3", "out", "pull_up_1", "fill_way"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, name), nullptr)
        << "6.10: '" << name << "' on a continuous assignment LHS is an implicit net";
  }
}

TEST_F(MultContAssignTest, UndeclaredRhsIdentifiersFailToBind) {
  for (std::string_view name : {"pu", "pd", "sel", "v0", "v1", "driver_1_en", "driver_1"}) {
    EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, name), nullptr)
        << "6.10: '" << name << "' is used on a RHS without being declared";
  }
}

// ===========================================================================
// 10.3.2: continuous assignment structure
// ===========================================================================

TEST_F(MultContAssignTest, TopHasTenModuleLevelContAssigns) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 10u);
}

TEST_F(MultContAssignTest, ContAssignLhsNames) {
  const std::pair<uint32_t, std::string_view> expected[] = {
      {6, "inout_i1"},  {7, "inout_i1"}, {9, "v"},    {10, "v"},         {13, "inout_i2"},
      {14, "inout_i2"}, {21, "out"},     {22, "out"}, {24, "pull_up_1"}, {26, "pull_up_1"}};
  for (const std::pair<uint32_t, std::string_view> &e : expected) {
    const hldb::ContAssign *const ca = getAssignAtLine("top", e.first);
    ASSERT_NE(ca, nullptr) << "line " << e.first;
    expectLhsName(ca, e.second);
  }
}

TEST_F(MultContAssignTest, VAssignmentsBindToVariable) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("v", top->getVariables());
  ASSERT_NE(v, nullptr);
  for (uint32_t line : {9u, 10u}) {
    const hldb::ContAssign *const ca = getAssignAtLine("top", line);
    ASSERT_NE(ca, nullptr) << "line " << line;
    const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getActual(), v) << "line " << line;
  }
}

TEST_F(MultContAssignTest, OutRhsIsConditionalOperator) {
  const hldb::ContAssign *const ca = getAssignAtLine("top", 21);
  ASSERT_NE(ca, nullptr);
  const hldb::Operation *const op = ca->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiConditionOp);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u);
}

// ===========================================================================
// 10.3.4: drive strengths
// ===========================================================================

TEST_F(MultContAssignTest, StrengthHighz0Weak1) { expectStrengths(6, vpiHiZ, vpiWeakDrive); }

TEST_F(MultContAssignTest, StrengthWeak0Highz1) { expectStrengths(7, vpiWeakDrive, vpiHiZ); }

TEST_F(MultContAssignTest, StrengthWeak0Weak1) { expectStrengths(13, vpiWeakDrive, vpiWeakDrive); }

TEST_F(MultContAssignTest, StrengthWeak1Weak0ReversedOrder) { expectStrengths(14, vpiWeakDrive, vpiWeakDrive); }

TEST_F(MultContAssignTest, StrengthPull1Pull0ReversedOrder) { expectStrengths(24, vpiPullDrive, vpiPullDrive); }

// ===========================================================================
// 27.5: generate constructs
// ===========================================================================

TEST_F(MultContAssignTest, IfElseGenerateKeepsBothBranches) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getGenStmts(), nullptr);
  const hldb::GenIfElse *gie = nullptr;
  for (const hldb::Any *const g : *top->getGenStmts()) {
    if ((gie = any_cast<hldb::GenIfElse>(g)) != nullptr) break;
  }
  ASSERT_NE(gie, nullptr) << "'if (0) ... else ...' should be a GenIfElse";
  EXPECT_NE(gie->getCondition(), nullptr);
  ASSERT_NE(gie->getStmt(), nullptr);
  ASSERT_NE(gie->getElseStmt(), nullptr);
  expectLhsName(gie->getStmt(), "inout_i3");
  expectLhsName(gie->getElseStmt(), "inout_i3");
}

TEST_F(MultContAssignTest, CaseGenerateHasTwoItems) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getGenStmts(), nullptr);
  const hldb::GenCase *gc = nullptr;
  for (const hldb::Any *g : *top->getGenStmts()) {
    // "generate ... endgenerate" (27.3) may wrap the case in a GenRegion.
    if (const hldb::GenRegion *const rg = any_cast<hldb::GenRegion>(g)) g = rg->getStmt();
    if ((gc = any_cast<hldb::GenCase>(g)) != nullptr) break;
  }
  ASSERT_NE(gc, nullptr) << "'case (NUM_WAYS)' generate should be a GenCase";
  const hldb::RefObj *const cond = gc->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getName(), "NUM_WAYS");
  ASSERT_NE(gc->getCaseItems(), nullptr);
  ASSERT_EQ(gc->getCaseItems()->size(), 2u);
  for (const hldb::CaseItem *const item : *gc->getCaseItems()) {
    const hldb::Begin *const blk = item->getStmt<hldb::Begin>();
    ASSERT_NE(blk, nullptr);
    ASSERT_NE(blk->getStmts(), nullptr);
    ASSERT_EQ(blk->getStmts()->size(), 1u);
    expectLhsName(blk->getStmts()->at(0), "fill_way");
  }
}

// ===========================================================================
// wandwor_test0 / top2
// ===========================================================================

TEST_F(MultContAssignTest, WandworXIsWorWithTwoAssigns) {
  const hldb::Module *const m = getModule("wandwor_test0");
  ASSERT_NE(m, nullptr);
  const hldb::Net *const x = hldb::findByName<hldb::Net>("X", m->getNets());
  ASSERT_NE(x, nullptr);
  EXPECT_EQ(x->getNetType(), vpiWor);
  EXPECT_FALSE(x->getNetDeclAssign());
  ASSERT_NE(m->getContAssigns(), nullptr);
  ASSERT_EQ(m->getContAssigns()->size(), 2u) << "10.3.2: 'assign X = A, X = B;' is two assignments";
  for (const hldb::ContAssign *const ca : *m->getContAssigns()) {
    const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getActual(), x);
  }
}

TEST_F(MultContAssignTest, Top2TwoIsUwireWithTwoAssigns) {
  const hldb::Module *const m = getModule("top2");
  ASSERT_NE(m, nullptr);
  const hldb::Net *const two = hldb::findByName<hldb::Net>("two", m->getNets());
  ASSERT_NE(two, nullptr);
  EXPECT_EQ(two->getNetType(), vpiUwire);
  EXPECT_FALSE(two->getNetDeclAssign()) << "'uwire two;' has no net_decl_assignment";
  ASSERT_NE(m->getContAssigns(), nullptr);
  EXPECT_EQ(m->getContAssigns()->size(), 2u);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
