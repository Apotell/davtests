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

// Spec-based validation of IEEE 1800-2017 ss.16.9.5:
// sequence 'and' operator -- both operand sequences must match.
// SV: tests/Sequence/sequence6.sv
//
//   module tb;
//     bit clk; always #5 clk = ~clk;
//     bit a, b;
//
//     sequence seq6;
//       a and b;
//     endsequence
//
//     assert property(@(posedge clk) seq6);
//
//     initial begin
//       #10 a=1; b=1;
//       #30 $finish;
//     end
//   endmodule
//
// -- ss.16.9.5 rules under test ------------------------------------------------
//
// Sequence 'and' operator (ss.16.9.5):
//   * 'a and b' is the sequence conjunction operator.
//   * Both operand sequences start at the same point in time and must each
//     match independently; the combined sequence matches when both have matched
//     (possibly ending at different clock ticks).
//   * This is semantically distinct from the expression-level logical '&&'.
//   * The correct VPI opType for the sequence 'and' operator is vpiCompAndOp
//     (sv_vpi_user.h, value 91 -- "Composite and operator").
//   * Surelog incorrectly uses vpiLogAndOp (26, "binary logical AND") instead.
//     This misrepresents the sequence 'and' as a boolean expression operator.
//   * Each operand is a RefObj that must resolve to its Net declaration at
//     compile time: RefObj("a") -> Net("a"), RefObj("b") -> Net("b").
//
// Concurrent assert property (ss.16.14):
//   * 'assert property(@(posedge clk) seq6)' uses seq6 as the property body.
//   * The property expression 'seq6' must resolve (vpiActual) to the
//     SequenceDecl for seq6 -- not be treated as an implicit net.
//   * Surelog emits EL0535 ("Illegal implicit net") for 'seq6' here -- the
//     same compile-time name resolution bug confirmed in sequence4 and sequence5.
//
// -- Expected HLDB tree (if compiler is correct) --------------------------------
//
//   Module name:work@tb
//   +-- getSequenceDecls() (1 item)
//   |   +-- SequenceDecl name:"seq6"
//   |         vpiExpr: Operation opType:logical_and (vpiLogAndOp = 26)
//   |           operand[0]: RefObj name:"a" -> Net name:"a"
//   |           operand[1]: RefObj name:"b" -> Net name:"b"
//   +-- getConcurrentAssertions() (1 item)
//       +-- Assert
//             PropertySpec
//               clocking: Operation posedge(clk)
//               propertyExpr: RefObj name:"seq6" -> SequenceDecl name:"seq6"

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

class Sequence6Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sequence6.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Module *getTb(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@tb", d->getAllModules());
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

// ss.16.9.5 + ss.16.14: the SV file is syntactically and semantically valid
// -- no errors expected.  If this test fails, Surelog emits EL0535 ("Illegal
// implicit net") for 'seq6' in the assert property statement, meaning it
// misidentifies the sequence name as an undeclared net instead of resolving
// it to the SequenceDecl node.
TEST_F(Sequence6Test, Compiler_NoErrors) {
  ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0) << "ss.16.14: assert property(@(posedge clk) seq6) must not produce "
                                 "errors -- EL0535 'Illegal implicit net' means Surelog does not "
                                 "resolve sequence names to SequenceDecl nodes";
}

TEST_F(Sequence6Test, Compiler_NoSyntaxErrors) {
  ErrorContainer::Stats stats = m_compiler->getErrorStats();
  EXPECT_EQ(stats.nbSyntax, 0) << "sequence6.sv is syntactically valid -- no syntax errors expected";
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(Sequence6Test, ModuleExists) { ASSERT_NE(getTb(m_design), nullptr) << "module 'work@tb' not found"; }

// ===========================================================================
// Sequence declaration (ss.16.8 / ss.16.9.5)
// ===========================================================================

// ss.16.8: exactly one sequence is declared in this module.
TEST_F(Sequence6Test, SequenceDeclCount_IsOne) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getSequenceDecls(), nullptr) << "module has no sequence declarations";
  EXPECT_EQ(m->getSequenceDecls()->size(), 1u) << "ss.16.8: exactly one sequence is declared: seq6";
}

// ss.16.8: 'seq6' must appear in the sequence declaration collection.
TEST_F(Sequence6Test, Seq6_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(getSeqDecl(m, "seq6"), nullptr) << "ss.16.8: sequence 'seq6' must be declared";
}

// ===========================================================================
// seq6 body: 'a and b'  (ss.16.9.5)
// ===========================================================================

// ss.16.9.5: seq6 uses the 'and' operator -- it must have a non-null body.
TEST_F(Sequence6Test, Seq6_HasExpression) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s6 = getSeqDecl(m, "seq6");
  ASSERT_NE(s6, nullptr) << "seq6 not found";
  EXPECT_NE(s6->getExpr(), nullptr) << "ss.16.9.5: seq6 must have a body expression";
}

// ss.16.9.5: 'a and b' is the sequence conjunction operator.  The correct
// VPI opType is vpiCompAndOp (91, "Composite and operator") defined in
// sv_vpi_user.h.  Surelog incorrectly emits vpiLogAndOp (26, binary logical
// AND) -- a misrepresentation that conflates the sequence 'and' with the
// expression-level '&&' operator.  If this test fails, Surelog is using the
// wrong opType for the sequence 'and' operator.
TEST_F(Sequence6Test, Seq6_Expr_IsAndOperation) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s6 = getSeqDecl(m, "seq6");
  ASSERT_NE(s6, nullptr);
  const hldb::Operation *op = s6->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "seq6 body must be an Operation";
  EXPECT_EQ(op->getOpType(), vpiCompAndOp) << "ss.16.9.5: 'a and b' must have opType vpiCompAndOp (91) -- "
                                              "Surelog incorrectly uses vpiLogAndOp (26, binary logical &&) "
                                              "instead of the sequence-specific vpiCompAndOp (91)";
}

// ss.16.9.5: 'a and b' has two operand sequences.
TEST_F(Sequence6Test, Seq6_Expr_HasTwoOperands) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s6 = getSeqDecl(m, "seq6");
  ASSERT_NE(s6, nullptr);
  const hldb::Operation *op = s6->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u) << "ss.16.9.5: 'a and b' must produce exactly 2 operands";
}

// ss.16.9.5: operand[0] is the first sequence 'a'.  It must be a RefObj
// named "a".
TEST_F(Sequence6Test, Seq6_Operand0_IsRefToA) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s6 = getSeqDecl(m, "seq6");
  ASSERT_NE(s6, nullptr);
  const hldb::Operation *op = s6->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);

  const hldb::RefObj *op0 = any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(op0, nullptr) << "operand[0] must be a RefObj";
  EXPECT_EQ(op0->getName(), "a") << "ss.16.9.5: first operand of 'a and b' must reference signal 'a'";
}

// ss.16.9.5: the RefObj for 'a' must resolve (vpiActual) to Net name:'a'.
// This is compile-time name binding -- no elaboration guard needed.
TEST_F(Sequence6Test, Seq6_Operand0_ResolvesToNet) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s6 = getSeqDecl(m, "seq6");
  ASSERT_NE(s6, nullptr);
  const hldb::Operation *op = s6->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);

  const hldb::RefObj *op0 = any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(op0, nullptr);
  EXPECT_NE(op0->getActual<hldb::Net>(), nullptr) << "ss.16.9.5: RefObj for 'a' must resolve to Net name:'a' at "
                                                     "compile time";
}

// ss.16.9.5: operand[1] is the second sequence 'b'.  It must be a RefObj
// named "b".
TEST_F(Sequence6Test, Seq6_Operand1_IsRefToB) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s6 = getSeqDecl(m, "seq6");
  ASSERT_NE(s6, nullptr);
  const hldb::Operation *op = s6->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);

  const hldb::RefObj *op1 = any_cast<hldb::RefObj>((*op->getOperands())[1]);
  ASSERT_NE(op1, nullptr) << "operand[1] must be a RefObj";
  EXPECT_EQ(op1->getName(), "b") << "ss.16.9.5: second operand of 'a and b' must reference signal 'b'";
}

// ss.16.9.5: the RefObj for 'b' must resolve (vpiActual) to Net name:'b'.
// This is compile-time name binding -- no elaboration guard needed.
TEST_F(Sequence6Test, Seq6_Operand1_ResolvesToNet) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s6 = getSeqDecl(m, "seq6");
  ASSERT_NE(s6, nullptr);
  const hldb::Operation *op = s6->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);

  const hldb::RefObj *op1 = any_cast<hldb::RefObj>((*op->getOperands())[1]);
  ASSERT_NE(op1, nullptr);
  EXPECT_NE(op1->getActual<hldb::Net>(), nullptr) << "ss.16.9.5: RefObj for 'b' must resolve to Net name:'b' at "
                                                     "compile time";
}

// ===========================================================================
// Concurrent assertion: 'assert property(@(posedge clk) seq6)'  (ss.16.14)
// ===========================================================================

// ss.16.14: the module must have at least one concurrent assertion.
TEST_F(Sequence6Test, ConcurrentAssertion_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getConcurrentAssertions(), nullptr) << "module has no concurrent assertions";
  EXPECT_NE(getFirstAssert(m), nullptr) << "ss.16.14: an Assert node must be present";
}

// ss.16.14: the assert must carry an inline PropertySpec.
TEST_F(Sequence6Test, Assert_HasPropertySpec) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getProperty<hldb::PropertySpec>(), nullptr) << "ss.16.14: Assert must have an inline PropertySpec";
}

// ss.16.14: '@(posedge clk)' must be represented as the clocking event on
// the PropertySpec.
TEST_F(Sequence6Test, Assert_PropertySpec_HasClockingEvent) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  EXPECT_NE(spec->getClockingEvent(), nullptr) << "ss.16.14: @(posedge clk) must produce a clocking event on the "
                                                  "PropertySpec";
}

// ss.16.14: the property expression is the reference to 'seq6'.  It must be
// a RefObj named "seq6".
TEST_F(Sequence6Test, Assert_PropertyExpr_ReferencesSeq6) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr) << "property expression must be a RefObj";
  EXPECT_EQ(propExpr->getName(), "seq6") << "ss.16.14: property expression must reference 'seq6'";
}

// ss.16.14: the RefObj for 'seq6' in the concurrent assertion must resolve
// (vpiActual) to the SequenceDecl node.  Surelog emits EL0535 for this
// reference, treating 'seq6' as an implicit net instead of resolving it to
// the SequenceDecl.  This is the same compile-time name resolution bug
// confirmed in sequence4 and sequence5.
TEST_F(Sequence6Test, Assert_PropertyExpr_ResolvedToSeq6Decl) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr);
  EXPECT_NE(propExpr->getActual<hldb::SequenceDecl>(), nullptr)
      << "ss.16.14: 'seq6' in assert property must resolve to SequenceDecl -- "
         "Surelog emits EL0535 treating it as an implicit net instead";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
