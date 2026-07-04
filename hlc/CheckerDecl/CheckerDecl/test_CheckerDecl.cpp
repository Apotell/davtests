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

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/always.h>
#include <hldb/any.h>
#include <hldb/assert_stmt.h>
#include <hldb/assume.h>
#include <hldb/checker_decl.h>
#include <hldb/checker_port.h>
#include <hldb/cont_assign.h>
#include <hldb/cover.h>
#include <hldb/design.h>
#include <hldb/dist_item.h>
#include <hldb/distribution.h>
#include <hldb/final_stmt.h>
#include <hldb/function.h>
#include <hldb/immediate_assert.h>
#include <hldb/initial.h>
#include <hldb/let_decl.h>
#include <hldb/property_decl.h>
#include <hldb/restrict.h>
#include <hldb/sequence_decl.h>
#include <hldb/variable.h>

#include <gtest/gtest.h>

namespace hlc {

// ============================================================================
// Test fixture — compiles tests/CheckerDeclarationAll/dut.sv once for all
// test cases in this file.
// ============================================================================
class CheckerDeclTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "CheckerDecl.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Find a checker declaration by its qualified name (e.g. "work@C1_NoPortsNoBody").
  const hldb::CheckerDecl *findChecker(std::string_view name) const {
    return getByName<hldb::CheckerDecl>(name, m_design->getCheckerDecls());
  }

  // Find a port on a checker by its simple name.
  const hldb::CheckerPort *findPort(const hldb::CheckerDecl *checker, std::string_view portName) const {
    if (checker == nullptr) return nullptr;
    return getByName<hldb::CheckerPort>(portName, checker->getPorts());
  }
};

// ============================================================================
// Count — all 18 top-level checkers must be present in the design.
// C1–C17 (file scope before module) + my_checker_global2 (file scope after module) = 18.
// The nested "Inner" checker inside C12 is parented to C12, not to the design.
// C1–C17 + my_checker_global2 = 18.
// ============================================================================
TEST_F(CheckerDeclTest, AllCheckersPresent) {
  ASSERT_NE(m_design->getCheckerDecls(), nullptr);
  EXPECT_EQ(m_design->getCheckerDecls()->size(), 18u);
}

// ============================================================================
// C1 — minimal checker: no port list, no body
// ============================================================================
TEST_F(CheckerDeclTest, C1_NoPortsNoBody_Exists) {
  const auto *c1 = findChecker("work@C1_NoPortsNoBody");
  ASSERT_NE(c1, nullptr);
  // No ports
  EXPECT_TRUE(c1->getPorts() == nullptr || c1->getPorts()->empty());
  // No variables
  EXPECT_TRUE(c1->getVariables() == nullptr || c1->getVariables()->empty());
}

// ============================================================================
// C2 — all port formal-type variants
// ============================================================================
TEST_F(CheckerDeclTest, C2_PortCount) {
  const auto *c2 = findChecker("work@C2_PortFormalTypes");
  ASSERT_NE(c2, nullptr);
  ASSERT_NE(c2->getPorts(), nullptr);
  // 10 ports: clk, rst_n, data, no_dir_port, seq_in, prop_in,
  //           untyped_in, flag_out, vec_in, dflt_in
  EXPECT_EQ(c2->getPorts()->size(), 10u);
}

TEST_F(CheckerDeclTest, C2_InputPortDirection) {
  const auto *c2 = findChecker("work@C2_PortFormalTypes");
  ASSERT_NE(c2, nullptr);
  const auto *clk = findPort(c2, "clk");
  ASSERT_NE(clk, nullptr);
  EXPECT_EQ(clk->getDirection(), vpiInput);
}

TEST_F(CheckerDeclTest, C2_OutputPortDirection) {
  const auto *c2 = findChecker("work@C2_PortFormalTypes");
  ASSERT_NE(c2, nullptr);
  const auto *flag_out = findPort(c2, "flag_out");
  ASSERT_NE(flag_out, nullptr);
  EXPECT_EQ(flag_out->getDirection(), vpiOutput);
}

TEST_F(CheckerDeclTest, C2_OmittedDirectionDefaultsToInput) {
  const auto *c2 = findChecker("work@C2_PortFormalTypes");
  ASSERT_NE(c2, nullptr);
  // no_dir_port has no direction keyword → must default to vpiInput
  const auto *p = findPort(c2, "no_dir_port");
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getDirection(), vpiInput);
}

TEST_F(CheckerDeclTest, C2_SequenceFormalPortExists) {
  const auto *c2 = findChecker("work@C2_PortFormalTypes");
  ASSERT_NE(c2, nullptr);
  EXPECT_NE(findPort(c2, "seq_in"), nullptr);
}

TEST_F(CheckerDeclTest, C2_PropertyFormalPortExists) {
  const auto *c2 = findChecker("work@C2_PortFormalTypes");
  ASSERT_NE(c2, nullptr);
  EXPECT_NE(findPort(c2, "prop_in"), nullptr);
}

TEST_F(CheckerDeclTest, C2_UntypedFormalPortExists) {
  const auto *c2 = findChecker("work@C2_PortFormalTypes");
  ASSERT_NE(c2, nullptr);
  EXPECT_NE(findPort(c2, "untyped_in"), nullptr);
}

TEST_F(CheckerDeclTest, C2_DataTypedPortHasTypespec) {
  const auto *c2 = findChecker("work@C2_PortFormalTypes");
  ASSERT_NE(c2, nullptr);
  const auto *clk = findPort(c2, "clk");
  ASSERT_NE(clk, nullptr);
  // logic clk → typespec should be set
  EXPECT_NE(clk->getTypespec(), nullptr);
}

// ============================================================================
// C3 — data declarations (plain + rand)
// ============================================================================
TEST_F(CheckerDeclTest, C3_PortCount) {
  const auto *c3 = findChecker("work@C3_DataDecl");
  ASSERT_NE(c3, nullptr);
  ASSERT_NE(c3->getPorts(), nullptr);
  EXPECT_EQ(c3->getPorts()->size(), 2u);  // clk, en
}

TEST_F(CheckerDeclTest, C3_Variables) {
  const auto *c3 = findChecker("work@C3_DataDecl");
  ASSERT_NE(c3, nullptr);
  // state, r_state, counter, byte_val
  ASSERT_NE(c3->getVariables(), nullptr);
  EXPECT_EQ(c3->getVariables()->size(), 4u);
}

TEST_F(CheckerDeclTest, C3_VariableNames) {
  const auto *c3 = findChecker("work@C3_DataDecl");
  ASSERT_NE(c3, nullptr);
  EXPECT_NE(getByName<hldb::Variable>("state", c3->getVariables()), nullptr);
  EXPECT_NE(getByName<hldb::Variable>("r_state", c3->getVariables()), nullptr);
  EXPECT_NE(getByName<hldb::Variable>("counter", c3->getVariables()), nullptr);
  EXPECT_NE(getByName<hldb::Variable>("byte_val", c3->getVariables()), nullptr);
}

// ============================================================================
// C4 — assertion_item_declaration: property, sequence, let + concurrent asserts
// ============================================================================
TEST_F(CheckerDeclTest, C4_PortCount) {
  const auto *c4 = findChecker("work@C4_AssertionItemDecl");
  ASSERT_NE(c4, nullptr);
  ASSERT_NE(c4->getPorts(), nullptr);
  EXPECT_EQ(c4->getPorts()->size(), 3u);  // clk, a, b
}

TEST_F(CheckerDeclTest, C4_SequenceDecl) {
  const auto *c4 = findChecker("work@C4_AssertionItemDecl");
  ASSERT_NE(c4, nullptr);
  ASSERT_NE(c4->getSequenceDecls(), nullptr);
  EXPECT_EQ(c4->getSequenceDecls()->size(), 1u);
  EXPECT_NE(getByName<hldb::SequenceDecl>("s_ab", c4->getSequenceDecls()), nullptr);
}

TEST_F(CheckerDeclTest, C4_PropertyDecl) {
  const auto *c4 = findChecker("work@C4_AssertionItemDecl");
  ASSERT_NE(c4, nullptr);
  ASSERT_NE(c4->getPropertyDecls(), nullptr);
  EXPECT_EQ(c4->getPropertyDecls()->size(), 1u);
  EXPECT_NE(getByName<hldb::PropertyDecl>("p_ab", c4->getPropertyDecls()), nullptr);
}

TEST_F(CheckerDeclTest, C4_LetDecl) {
  const auto *c4 = findChecker("work@C4_AssertionItemDecl");
  ASSERT_NE(c4, nullptr);
  ASSERT_NE(c4->getLetDecls(), nullptr);
  EXPECT_EQ(c4->getLetDecls()->size(), 1u);
  EXPECT_NE(getByName<hldb::LetDecl>("L_and", c4->getLetDecls()), nullptr);
}

TEST_F(CheckerDeclTest, C4_ConcurrentAssertions) {
  const auto *c4 = findChecker("work@C4_AssertionItemDecl");
  ASSERT_NE(c4, nullptr);
  // assert property, assume property, cover property → 3 concurrent assertions
  ASSERT_NE(c4->getConcurrentAssertions(), nullptr);
  EXPECT_EQ(c4->getConcurrentAssertions()->size(), 3u);
}

TEST_F(CheckerDeclTest, C4_AssertLabel) {
  const auto *c4 = findChecker("work@C4_AssertionItemDecl");
  ASSERT_NE(c4, nullptr);
  ASSERT_NE(c4->getConcurrentAssertions(), nullptr);
  // assert_ab label must be preserved on one of the items
  bool found = false;
  for (const hldb::ConcurrentAssertions *ca : *c4->getConcurrentAssertions()) {
    if (ca->getLabel() == "assert_ab") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "assert_ab label not found in concurrent assertions";
}

// ============================================================================
// C5 — restrict_property_statement + cover_sequence_statement
// ============================================================================
TEST_F(CheckerDeclTest, C5_PortCount) {
  const auto *c5 = findChecker("work@C5_RestrictCoverSeq");
  ASSERT_NE(c5, nullptr);
  ASSERT_NE(c5->getPorts(), nullptr);
  EXPECT_EQ(c5->getPorts()->size(), 3u);
}

TEST_F(CheckerDeclTest, C5_SequenceDecl) {
  const auto *c5 = findChecker("work@C5_RestrictCoverSeq");
  ASSERT_NE(c5, nullptr);
  ASSERT_NE(c5->getSequenceDecls(), nullptr);
  EXPECT_NE(getByName<hldb::SequenceDecl>("s_a_then_b", c5->getSequenceDecls()), nullptr);
}

TEST_F(CheckerDeclTest, C5_ConcurrentAssertions) {
  const auto *c5 = findChecker("work@C5_RestrictCoverSeq");
  ASSERT_NE(c5, nullptr);
  // restrict property + cover sequence → 2 concurrent assertions
  ASSERT_NE(c5->getConcurrentAssertions(), nullptr);
  EXPECT_EQ(c5->getConcurrentAssertions()->size(), 2u);
}

// ============================================================================
// C6 — function_declaration + continuous_assign
// ============================================================================
TEST_F(CheckerDeclTest, C6_PortCount) {
  const auto *c6 = findChecker("work@C6_FunctionDecl");
  ASSERT_NE(c6, nullptr);
  ASSERT_NE(c6->getPorts(), nullptr);
  EXPECT_EQ(c6->getPorts()->size(), 2u);  // clk, data
}

TEST_F(CheckerDeclTest, C6_FunctionDecl) {
  const auto *c6 = findChecker("work@C6_FunctionDecl");
  ASSERT_NE(c6, nullptr);
  ASSERT_NE(c6->getTaskFuncs(), nullptr);
  EXPECT_EQ(c6->getTaskFuncs()->size(), 1u);
  // Check the function name
  bool found = false;
  for (const hldb::TaskFunc *tf : *c6->getTaskFuncs()) {
    if (tf->getName() == "invert_byte") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "Function invert_byte not found";
}

TEST_F(CheckerDeclTest, C6_Variable) {
  const auto *c6 = findChecker("work@C6_FunctionDecl");
  ASSERT_NE(c6, nullptr);
  ASSERT_NE(c6->getVariables(), nullptr);
  EXPECT_NE(getByName<hldb::Variable>("inv_data", c6->getVariables()), nullptr);
}

TEST_F(CheckerDeclTest, C6_ContinuousAssign) {
  const auto *c6 = findChecker("work@C6_FunctionDecl");
  ASSERT_NE(c6, nullptr);
  ASSERT_NE(c6->getContAssigns(), nullptr);
  EXPECT_EQ(c6->getContAssigns()->size(), 1u);
}

TEST_F(CheckerDeclTest, C6_ConcurrentAssertion) {
  const auto *c6 = findChecker("work@C6_FunctionDecl");
  ASSERT_NE(c6, nullptr);
  ASSERT_NE(c6->getConcurrentAssertions(), nullptr);
  EXPECT_EQ(c6->getConcurrentAssertions()->size(), 1u);
}

// ============================================================================
// C7 — initial_construct, always_construct, final_construct
// ============================================================================
TEST_F(CheckerDeclTest, C7_PortCount) {
  const auto *c7 = findChecker("work@C7_ProceduralItems");
  ASSERT_NE(c7, nullptr);
  ASSERT_NE(c7->getPorts(), nullptr);
  EXPECT_EQ(c7->getPorts()->size(), 1u);
}

TEST_F(CheckerDeclTest, C7_Variable) {
  const auto *c7 = findChecker("work@C7_ProceduralItems");
  ASSERT_NE(c7, nullptr);
  ASSERT_NE(c7->getVariables(), nullptr);
  EXPECT_NE(getByName<hldb::Variable>("cnt", c7->getVariables()), nullptr);
}

TEST_F(CheckerDeclTest, C7_Processes) {
  const auto *c7 = findChecker("work@C7_ProceduralItems");
  ASSERT_NE(c7, nullptr);
  // initial + always + final → 3 processes
  ASSERT_NE(c7->getProcesses(), nullptr);
  EXPECT_EQ(c7->getProcesses()->size(), 3u);
}

TEST_F(CheckerDeclTest, C7_ProcessTypes) {
  const auto *c7 = findChecker("work@C7_ProceduralItems");
  ASSERT_NE(c7, nullptr);
  ASSERT_NE(c7->getProcesses(), nullptr);
  bool hasInitial = false, hasAlways = false, hasFinal = false;
  for (const hldb::Process *p : *c7->getProcesses()) {
    if (any_cast<hldb::Initial>(p)) hasInitial = true;
    if (any_cast<hldb::Always>(p)) hasAlways = true;
    if (any_cast<hldb::FinalStmt>(p)) hasFinal = true;
  }
  EXPECT_TRUE(hasInitial) << "Initial process not found in C7";
  EXPECT_TRUE(hasAlways) << "Always process not found in C7";
  EXPECT_TRUE(hasFinal) << "Final process not found in C7";
}

// ============================================================================
// C8 — clocking_declaration + default clocking + default disable iff
// ============================================================================
TEST_F(CheckerDeclTest, C8_PortCount) {
  const auto *c8 = findChecker("work@C8_ClockingDefault");
  ASSERT_NE(c8, nullptr);
  ASSERT_NE(c8->getPorts(), nullptr);
  EXPECT_EQ(c8->getPorts()->size(), 4u);  // clk, rst, a, b
}

TEST_F(CheckerDeclTest, C8_ConcurrentAssertion) {
  const auto *c8 = findChecker("work@C8_ClockingDefault");
  ASSERT_NE(c8, nullptr);
  ASSERT_NE(c8->getConcurrentAssertions(), nullptr);
  EXPECT_EQ(c8->getConcurrentAssertions()->size(), 1u);
}

// ============================================================================
// C9 — covergroup_declaration (covergroup handler is a no-op; variable remains)
// ============================================================================
TEST_F(CheckerDeclTest, C9_PortCount) {
  const auto *c9 = findChecker("work@C9_CovergroupDecl");
  ASSERT_NE(c9, nullptr);
  ASSERT_NE(c9->getPorts(), nullptr);
  EXPECT_EQ(c9->getPorts()->size(), 2u);  // clk, mode
}

TEST_F(CheckerDeclTest, C9_CovergroupInstanceVariable) {
  const auto *c9 = findChecker("work@C9_CovergroupDecl");
  ASSERT_NE(c9, nullptr);
  // cg_inst is the covergroup instance variable
  ASSERT_NE(c9->getVariables(), nullptr);
  EXPECT_NE(getByName<hldb::Variable>("cg_inst", c9->getVariables()), nullptr);
}

// ============================================================================
// C10 — genvar_declaration + loop_generate_construct
// ============================================================================
TEST_F(CheckerDeclTest, C10_PortCount) {
  const auto *c10 = findChecker("work@C10_GenvarLoop");
  ASSERT_NE(c10, nullptr);
  ASSERT_NE(c10->getPorts(), nullptr);
  EXPECT_EQ(c10->getPorts()->size(), 2u);  // clk, en
}

TEST_F(CheckerDeclTest, C10_GenvarVariable) {
  const auto *c10 = findChecker("work@C10_GenvarLoop");
  ASSERT_NE(c10, nullptr);
  // genvar gi → Variable model auto-parented via getModelOnStack
  ASSERT_NE(c10->getVariables(), nullptr);
  EXPECT_NE(getByName<hldb::Variable>("gi", c10->getVariables()), nullptr);
}

// ============================================================================
// C11 — conditional_generate_construct
// ============================================================================
TEST_F(CheckerDeclTest, C11_PortCount) {
  const auto *c11 = findChecker("work@C11_ConditionalGen");
  ASSERT_NE(c11, nullptr);
  ASSERT_NE(c11->getPorts(), nullptr);
  EXPECT_EQ(c11->getPorts()->size(), 3u);  // clk, a, b
}

// ============================================================================
// C12 — nested checker_declaration + checker_instantiation
// ============================================================================
TEST_F(CheckerDeclTest, C12_PortCount) {
  const auto *c12 = findChecker("work@C12_NestedChecker");
  ASSERT_NE(c12, nullptr);
  ASSERT_NE(c12->getPorts(), nullptr);
  EXPECT_EQ(c12->getPorts()->size(), 3u);  // clk, a, b
}

TEST_F(CheckerDeclTest, C12_InnerCheckerNotInDesign) {
  // The nested "Inner" checker is parented to C12, not to the Design.
  // It should NOT appear in the design-level getCheckerDecls().
  const auto *inner = findChecker("work@Inner");
  // Inner should not be at design level
  EXPECT_EQ(inner, nullptr) << "Nested Inner checker should not be at design level";
}

// ============================================================================
// C13 — deferred_immediate_assertion_item (#0 and final forms)
// ============================================================================
TEST_F(CheckerDeclTest, C13_PortCount) {
  const auto *c13 = findChecker("work@C13_DeferredImmediate");
  ASSERT_NE(c13, nullptr);
  ASSERT_NE(c13->getPorts(), nullptr);
  EXPECT_EQ(c13->getPorts()->size(), 2u);  // clk, a
}

TEST_F(CheckerDeclTest, C13_DeferredAssertions) {
  const auto *c13 = findChecker("work@C13_DeferredImmediate");
  ASSERT_NE(c13, nullptr);
  // assert #0 and assert final → ImmediateAssert × 2 → getAssertions()
  ASSERT_NE(c13->getAssertions(), nullptr);
  EXPECT_EQ(c13->getAssertions()->size(), 2u);
}

// ============================================================================
// C14 — empty semicolons in body + assertion
// ============================================================================
TEST_F(CheckerDeclTest, C14_PortCount) {
  const auto *c14 = findChecker("work@C14_EmptySemicolons");
  ASSERT_NE(c14, nullptr);
  ASSERT_NE(c14->getPorts(), nullptr);
  EXPECT_EQ(c14->getPorts()->size(), 2u);  // clk, a
}

TEST_F(CheckerDeclTest, C14_ConcurrentAssertion) {
  const auto *c14 = findChecker("work@C14_EmptySemicolons");
  ASSERT_NE(c14, nullptr);
  ASSERT_NE(c14->getConcurrentAssertions(), nullptr);
  EXPECT_EQ(c14->getConcurrentAssertions()->size(), 1u);
}

// ============================================================================
// C15 — attribute_instance on checker_or_generate_item
// ============================================================================
TEST_F(CheckerDeclTest, C15_PortCount) {
  const auto *c15 = findChecker("work@C15_AttributedItem");
  ASSERT_NE(c15, nullptr);
  ASSERT_NE(c15->getPorts(), nullptr);
  EXPECT_EQ(c15->getPorts()->size(), 3u);  // clk, a, b
}

TEST_F(CheckerDeclTest, C15_SequenceDecl) {
  const auto *c15 = findChecker("work@C15_AttributedItem");
  ASSERT_NE(c15, nullptr);
  ASSERT_NE(c15->getSequenceDecls(), nullptr);
  EXPECT_NE(getByName<hldb::SequenceDecl>("s_x", c15->getSequenceDecls()), nullptr);
}

TEST_F(CheckerDeclTest, C15_ConcurrentAssertion) {
  const auto *c15 = findChecker("work@C15_AttributedItem");
  ASSERT_NE(c15, nullptr);
  ASSERT_NE(c15->getConcurrentAssertions(), nullptr);
  EXPECT_EQ(c15->getConcurrentAssertions()->size(), 1u);
}

// ============================================================================
// C16 — empty body, single port
// ============================================================================
TEST_F(CheckerDeclTest, C16_SinglePort) {
  const auto *c16 = findChecker("work@C16_EmptyBody");
  ASSERT_NE(c16, nullptr);
  ASSERT_NE(c16->getPorts(), nullptr);
  EXPECT_EQ(c16->getPorts()->size(), 1u);
  EXPECT_NE(findPort(c16, "clk"), nullptr);
}

TEST_F(CheckerDeclTest, C16_EmptyBody) {
  const auto *c16 = findChecker("work@C16_EmptyBody");
  ASSERT_NE(c16, nullptr);
  EXPECT_TRUE(c16->getVariables() == nullptr || c16->getVariables()->empty());
  EXPECT_TRUE(c16->getConcurrentAssertions() == nullptr || c16->getConcurrentAssertions()->empty());
  EXPECT_TRUE(c16->getProcesses() == nullptr || c16->getProcesses()->empty());
}

// ============================================================================
// C2 — property_formal_type: all four formal types correctly stored
// ============================================================================
TEST_F(CheckerDeclTest, C2_FormalType_Data) {
  const auto *c2 = findChecker("work@C2_PortFormalTypes");
  ASSERT_NE(c2, nullptr);
  const auto *clk = findPort(c2, "clk");
  ASSERT_NE(clk, nullptr);
  EXPECT_EQ(clk->getFormalType(), vpiFormalTypeData);
}

TEST_F(CheckerDeclTest, C2_FormalType_Sequence) {
  const auto *c2 = findChecker("work@C2_PortFormalTypes");
  ASSERT_NE(c2, nullptr);
  const auto *p = findPort(c2, "seq_in");
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getFormalType(), vpiFormalTypeSequence);
}

TEST_F(CheckerDeclTest, C2_FormalType_Property) {
  const auto *c2 = findChecker("work@C2_PortFormalTypes");
  ASSERT_NE(c2, nullptr);
  const auto *p = findPort(c2, "prop_in");
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getFormalType(), vpiFormalTypeProperty);
}

TEST_F(CheckerDeclTest, C2_FormalType_Untyped) {
  const auto *c2 = findChecker("work@C2_PortFormalTypes");
  ASSERT_NE(c2, nullptr);
  const auto *p = findPort(c2, "untyped_in");
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getFormalType(), vpiFormalTypeUntyped);
}

TEST_F(CheckerDeclTest, C2_FormalType_OmittedDirectionIsData) {
  // "logic no_dir_port" has no direction keyword → direction defaults to vpiInput,
  // formal type must be vpiFormalTypeData (data_type_or_implicit path).
  const auto *c2 = findChecker("work@C2_PortFormalTypes");
  ASSERT_NE(c2, nullptr);
  const auto *p = findPort(c2, "no_dir_port");
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getFormalType(), vpiFormalTypeData);
}

// ============================================================================
// C8 — default disable iff with plain expression (no dist)
// ============================================================================
TEST_F(CheckerDeclTest, C8_DefaultDisableIff_PlainExpr) {
  const auto *c8 = findChecker("work@C8_ClockingDefault");
  ASSERT_NE(c8, nullptr);
  // default disable iff rst; → plain Expr (RefObj for signal "rst")
  ASSERT_NE(c8->getDefaultDisableIff(), nullptr);
  // Must be an Expr, not a Distribution
  EXPECT_TRUE(any_cast<hldb::Expr>(c8->getDefaultDisableIff()) != nullptr);
  EXPECT_TRUE(any_cast<hldb::Distribution>(c8->getDefaultDisableIff()) == nullptr);
}

// ============================================================================
// C17 — DEFAULT DISABLE IFF with dist clause
//        Tests dist_item / dist_list in checker body context.
// ============================================================================
TEST_F(CheckerDeclTest, C17_Exists) {
  const auto *c17 = findChecker("work@C17_DisableIffDist");
  ASSERT_NE(c17, nullptr);
  ASSERT_NE(c17->getPorts(), nullptr);
  EXPECT_EQ(c17->getPorts()->size(), 4u);  // clk, mode, a, b
}

TEST_F(CheckerDeclTest, C17_DefaultDisableIff_Distribution) {
  const auto *c17 = findChecker("work@C17_DisableIffDist");
  ASSERT_NE(c17, nullptr);
  // default disable iff (mode dist {2'b01 := 80, 2'b10 := 20})
  // → Distribution model stored as default_disable_iff
  ASSERT_NE(c17->getDefaultDisableIff(), nullptr);
  const auto *dist = any_cast<hldb::Distribution>(c17->getDefaultDisableIff());
  ASSERT_NE(dist, nullptr) << "expected Distribution model for dist clause";
}

TEST_F(CheckerDeclTest, C17_DistributionHasBaseExpr) {
  const auto *c17 = findChecker("work@C17_DisableIffDist");
  ASSERT_NE(c17, nullptr);
  const auto *dist = any_cast<hldb::Distribution>(c17->getDefaultDisableIff());
  ASSERT_NE(dist, nullptr);
  // The base expression (mode signal) must be set
  EXPECT_NE(dist->getExpr(), nullptr);
}

TEST_F(CheckerDeclTest, C17_DistributionHasDistItems) {
  const auto *c17 = findChecker("work@C17_DisableIffDist");
  ASSERT_NE(c17, nullptr);
  const auto *dist = any_cast<hldb::Distribution>(c17->getDefaultDisableIff());
  ASSERT_NE(dist, nullptr);
  // Two dist items: 2'b01 := 80  and  2'b10 := 20
  ASSERT_NE(dist->getDistItems(), nullptr);
  EXPECT_EQ(dist->getDistItems()->size(), 2u);
}

TEST_F(CheckerDeclTest, C17_DistItemHasWeight) {
  const auto *c17 = findChecker("work@C17_DisableIffDist");
  ASSERT_NE(c17, nullptr);
  const auto *dist = any_cast<hldb::Distribution>(c17->getDefaultDisableIff());
  ASSERT_NE(dist, nullptr);
  ASSERT_NE(dist->getDistItems(), nullptr);
  // Each dist_item must have a weight expression set
  for (const hldb::DistItem *di : *dist->getDistItems()) {
    EXPECT_NE(di->getWeight(), nullptr) << "dist_item weight should not be null";
  }
}

TEST_F(CheckerDeclTest, C17_ConcurrentAssertion) {
  const auto *c17 = findChecker("work@C17_DisableIffDist");
  ASSERT_NE(c17, nullptr);
  ASSERT_NE(c17->getConcurrentAssertions(), nullptr);
  EXPECT_EQ(c17->getConcurrentAssertions()->size(), 1u);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
