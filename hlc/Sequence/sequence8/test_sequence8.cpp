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

// Spec-based validation of IEEE 1800-2017 ss.16.9.7:
// sequence 'or' operator -- the combined sequence matches when either operand
// sequence matches.
// SV: tests/Sequence/sequence8.sv
//
//   module tb;
//     bit clk; always #5 clk = ~clk;
//     bit a, b;
//
//     sequence seq8;
//       a or b;
//     endsequence
//
//     assert property(@(posedge clk) seq8);
//
//     initial begin
//       #10 a=1;
//       #20 b=1;
//       #40 $finish;
//     end
//   endmodule
//
// -- ss.16.9.7 rules under test ------------------------------------------------
//
// Sequence 'or' operator (ss.16.9.7):
//   * 'a or b' is the sequence disjunction operator.
//   * The combined sequence matches if either operand sequence matches; both
//     sequences start at the same clock tick.
//   * This is semantically distinct from the expression-level logical '||'.
//   * The correct VPI opType is vpiCompOrOp (92), defined in sv_vpi_user.h
//     ("Composite or operator").
//   * Surelog incorrectly uses vpiLogOrOp (27, "binary logical OR") instead.
//     This is the same class of wrong-optype bug as 'a and b' in sequence6,
//     where Surelog used vpiLogAndOp (26) instead of vpiCompAndOp (91).
//   * Each operand is a RefObj that must resolve to its Net declaration at
//     compile time: RefObj("a") -> Net("a"), RefObj("b") -> Net("b").
//
// Concurrent assert property (ss.16.14):
//   * 'assert property(@(posedge clk) seq8)' uses seq8 as the property body.
//   * The property expression 'seq8' must resolve (vpiActual) to the
//     SequenceDecl for seq8 -- not be treated as an implicit net.
//   * Surelog emits EL0535 ("Illegal implicit net") for 'seq8' here -- the
//     same compile-time name resolution bug confirmed in sequence4 through 7.
//
// -- Expected HLDB tree (if compiler is correct) --------------------------------
//
//   Module name:tb
//   +-- getSequenceDecls() (1 item)
//   |   +-- SequenceDecl name:"seq8"
//   |         vpiExpr: Operation opType:composite_or (vpiCompOrOp = 92)
//   |           operand[0]: RefObj name:"a" -> Net name:"a"
//   |           operand[1]: RefObj name:"b" -> Net name:"b"
//   +-- getConcurrentAssertions() (1 item)
//       +-- Assert
//             PropertySpec
//               clocking: Operation posedge(clk)
//               propertyExpr: RefObj name:"seq8" -> SequenceDecl name:"seq8"

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assert_stmt.h>
#include <hldb/concurrent_assertions.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/property_spec.h>
#include <hldb/ref_obj.h>
#include <hldb/sequence_decl.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

namespace hlc {

class Sequence8Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sequence8.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Module *getTb(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("tb", d->getAllModules());
}

static const hldb::SequenceDecl *getSeqDecl(const hldb::Module *m, std::string_view name) {
  if (!m || !m->getSequenceDecls()) return nullptr;
  for (const hldb::SequenceDecl *s : *m->getSequenceDecls()) {
    if (s && s->getName() == name) return s;
  }
  return nullptr;
}

static const hldb::Assert *getFirstAssert(const hldb::Module *m) {
  if (!m || !m->getConcurrentAssertions()) return nullptr;
  for (const hldb::ConcurrentAssertions *ca : *m->getConcurrentAssertions()) {
    if (const hldb::Assert *a = any_cast<hldb::Assert>(ca)) return a;
  }
  return nullptr;
}

// ===========================================================================
// Compiler diagnostics
// ===========================================================================

// ss.16.9.7 + ss.16.14: the SV file is syntactically and semantically valid
// -- no errors expected.  If this test fails, Surelog emits EL0535 ("Illegal
// implicit net") for 'seq8' in the assert property statement, meaning it
// misidentifies the sequence name as an undeclared net instead of resolving
// it to the SequenceDecl node.
TEST_F(Sequence8Test, Compiler_NoErrors) {
  ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0) << "ss.16.14: assert property(@(posedge clk) seq8) must not produce "
                                 "errors -- EL0535 'Illegal implicit net' means Surelog does not "
                                 "resolve sequence names to SequenceDecl nodes";
}

TEST_F(Sequence8Test, Compiler_NoSyntaxErrors) {
  ErrorContainer::Stats stats = m_compiler->getErrorStats();
  EXPECT_EQ(stats.nbSyntax, 0) << "sequence8.sv is syntactically valid -- no syntax errors expected";
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(Sequence8Test, ModuleExists) { ASSERT_NE(getTb(m_design), nullptr) << "module 'tb' not found"; }

// ===========================================================================
// Sequence declaration (ss.16.8 / ss.16.9.7)
// ===========================================================================

// ss.16.8: exactly one sequence is declared in this module.
TEST_F(Sequence8Test, SequenceDeclCount_IsOne) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getSequenceDecls(), nullptr) << "module has no sequence declarations";
  EXPECT_EQ(m->getSequenceDecls()->size(), 1u) << "ss.16.8: exactly one sequence is declared: seq8";
}

// ss.16.8: 'seq8' must appear in the sequence declaration collection.
TEST_F(Sequence8Test, Seq8_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(getSeqDecl(m, "seq8"), nullptr) << "ss.16.8: sequence 'seq8' must be declared";
}

// ===========================================================================
// seq8 body: 'a or b'  (ss.16.9.7)
// ===========================================================================

// ss.16.9.7: seq8 uses the 'or' operator -- it must have a non-null body.
TEST_F(Sequence8Test, Seq8_HasExpression) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s8 = getSeqDecl(m, "seq8");
  ASSERT_NE(s8, nullptr) << "seq8 not found";
  EXPECT_NE(s8->getExpr(), nullptr) << "ss.16.9.7: seq8 must have a body expression";
}

// ss.16.9.7: 'a or b' is the sequence disjunction operator.  The correct
// VPI opType is vpiCompOrOp (92, "Composite or operator") from sv_vpi_user.h.
// Surelog incorrectly uses vpiLogOrOp (27, "binary logical OR") -- the same
// wrong-optype pattern as 'a and b' in sequence6, where Surelog used
// vpiLogAndOp (26) instead of the correct vpiCompAndOp (91).
// If this test fails, Surelog is using the wrong opType for sequence 'or'.
TEST_F(Sequence8Test, Seq8_Expr_IsOrOperation) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s8 = getSeqDecl(m, "seq8");
  ASSERT_NE(s8, nullptr);
  const hldb::Operation *op = s8->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "seq8 body must be an Operation";
  EXPECT_EQ(op->getOpType(), vpiCompOrOp) << "ss.16.9.7: 'a or b' must have opType vpiCompOrOp (92) -- "
                                             "Surelog incorrectly uses vpiLogOrOp (27, binary logical ||) "
                                             "instead of the sequence-specific vpiCompOrOp (92)";
}

// ss.16.9.7: 'a or b' has two operand sequences.
TEST_F(Sequence8Test, Seq8_Expr_HasTwoOperands) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s8 = getSeqDecl(m, "seq8");
  ASSERT_NE(s8, nullptr);
  const hldb::Operation *op = s8->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u) << "ss.16.9.7: 'a or b' must produce exactly 2 operands";
}

// ss.16.9.7: operand[0] is the first sequence 'a'.  It must be a RefObj
// named "a".
TEST_F(Sequence8Test, Seq8_Operand0_IsRefToA) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s8 = getSeqDecl(m, "seq8");
  ASSERT_NE(s8, nullptr);
  const hldb::Operation *op = s8->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);

  const hldb::RefObj *op0 = any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(op0, nullptr) << "operand[0] must be a RefObj";
  EXPECT_EQ(op0->getName(), "a") << "ss.16.9.7: first operand of 'a or b' must reference signal 'a'";
}

// ss.16.9.7: the RefObj for 'a' must resolve (vpiActual) to Net name:'a'.
// Compile-time name binding -- no elaboration guard needed.
TEST_F(Sequence8Test, Seq8_Operand0_ResolvesToNet) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s8 = getSeqDecl(m, "seq8");
  ASSERT_NE(s8, nullptr);
  const hldb::Operation *op = s8->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);

  const hldb::RefObj *op0 = any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(op0, nullptr);
  EXPECT_NE(op0->getActual<hldb::Net>(), nullptr) << "ss.16.9.7: RefObj for 'a' must resolve to Net name:'a' at "
                                                     "compile time";
}

// ss.16.9.7: operand[1] is the second sequence 'b'.  It must be a RefObj
// named "b".
TEST_F(Sequence8Test, Seq8_Operand1_IsRefToB) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s8 = getSeqDecl(m, "seq8");
  ASSERT_NE(s8, nullptr);
  const hldb::Operation *op = s8->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);

  const hldb::RefObj *op1 = any_cast<hldb::RefObj>((*op->getOperands())[1]);
  ASSERT_NE(op1, nullptr) << "operand[1] must be a RefObj";
  EXPECT_EQ(op1->getName(), "b") << "ss.16.9.7: second operand of 'a or b' must reference signal 'b'";
}

// ss.16.9.7: the RefObj for 'b' must resolve (vpiActual) to Net name:'b'.
// Compile-time name binding -- no elaboration guard needed.
TEST_F(Sequence8Test, Seq8_Operand1_ResolvesToNet) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s8 = getSeqDecl(m, "seq8");
  ASSERT_NE(s8, nullptr);
  const hldb::Operation *op = s8->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);

  const hldb::RefObj *op1 = any_cast<hldb::RefObj>((*op->getOperands())[1]);
  ASSERT_NE(op1, nullptr);
  EXPECT_NE(op1->getActual<hldb::Net>(), nullptr) << "ss.16.9.7: RefObj for 'b' must resolve to Net name:'b' at "
                                                     "compile time";
}

// ===========================================================================
// Concurrent assertion: 'assert property(@(posedge clk) seq8)'  (ss.16.14)
// ===========================================================================

// ss.16.14: the module must have at least one concurrent assertion.
TEST_F(Sequence8Test, ConcurrentAssertion_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getConcurrentAssertions(), nullptr) << "module has no concurrent assertions";
  EXPECT_NE(getFirstAssert(m), nullptr) << "ss.16.14: an Assert node must be present";
}

// ss.16.14: the assert must carry an inline PropertySpec.
TEST_F(Sequence8Test, Assert_HasPropertySpec) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getProperty<hldb::PropertySpec>(), nullptr) << "ss.16.14: Assert must have an inline PropertySpec";
}

// ss.16.14: '@(posedge clk)' must be represented as the clocking event on
// the PropertySpec.
TEST_F(Sequence8Test, Assert_PropertySpec_HasClockingEvent) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  EXPECT_NE(spec->getClockingEvent(), nullptr) << "ss.16.14: @(posedge clk) must produce a clocking event on the "
                                                  "PropertySpec";
}

// ss.16.14: the property expression is the reference to 'seq8'.  It must be
// a RefObj named "seq8".
TEST_F(Sequence8Test, Assert_PropertyExpr_ReferencesSeq8) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr) << "property expression must be a RefObj";
  EXPECT_EQ(propExpr->getName(), "seq8") << "ss.16.14: property expression must reference 'seq8'";
}

// ss.16.14: the RefObj for 'seq8' in the concurrent assertion must resolve
// (vpiActual) to the SequenceDecl node.  Surelog emits EL0535 for this
// reference, treating 'seq8' as an implicit net instead of resolving it to
// the SequenceDecl.  Same compile-time name resolution bug as sequence4-7.
TEST_F(Sequence8Test, Assert_PropertyExpr_ResolvedToSeq8Decl) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr);
  EXPECT_NE(propExpr->getActual<hldb::SequenceDecl>(), nullptr)
      << "ss.16.14: 'seq8' in assert property must resolve to SequenceDecl -- "
         "Surelog emits EL0535 treating it as an implicit net instead";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
