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

// Spec-based validation of IEEE 1800-2017 ss.16.9.6:
// sequence 'intersect' operator -- both sequences must match and end together.
// SV: tests/Sequence/sequence7.sv
//
//   module tb;
//     bit clk; always #5 clk = ~clk;
//     bit a, b;
//
//     sequence seq7;
//       a intersect b;
//     endsequence
//
//     assert property(@(posedge clk) seq7);
//
//     initial begin
//       #10 a=1; b=1;
//       #30 $finish;
//     end
//   endmodule
//
// -- ss.16.9.6 rules under test ----
//
// Sequence 'intersect' operator (ss.16.9.6):
//   * 'a intersect b' is the sequence intersection operator.
//   * Both operand sequences start at the same point in time, must each match
//     independently, and must also end at exactly the same clock tick.
//   * This is stricter than 'and' (ss.16.9.5): 'and' allows operands to end
//     at different times; 'intersect' requires them to end together.
//   * The correct VPI opType is vpiIntersectOp (72), defined in sv_vpi_user.h.
//   * Each operand is a RefObj that must resolve to its Variable declaration at
//     compile time: RefObj("a") -> Variable("a"), RefObj("b") -> Variable("b").
//
// Concurrent assert property (ss.16.14):
//   * 'assert property(@(posedge clk) seq7)' uses seq7 as the property body.
//   * The property expression 'seq7' must resolve (vpiActual) to the
//     SequenceDecl for seq7 -- not be treated as an implicit net.
//
// -- Expected HLDB tree (if compiler is correct) ----
//
//   Module name:tb
//   +-- getSequenceDecls() (1 item)
//   |   +-- SequenceDecl name:"seq7"
//   |         vpiExpr: Operation opType:intersect (vpiIntersectOp = 72)
//   |           operand[0]: RefObj name:"a" -> Variable name:"a"
//   |           operand[1]: RefObj name:"b" -> Variable name:"b"
//   +-- getConcurrentAssertions() (1 item)
//       +-- Assert
//             PropertySpec
//               clocking: Operation posedge(clk)
//               propertyExpr: RefObj name:"seq7" -> SequenceDecl name:"seq7"

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assert_stmt.h>
#include <hldb/concurrent_assertions.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/variable.h>
#include <hldb/operation.h>
#include <hldb/property_spec.h>
#include <hldb/ref_obj.h>
#include <hldb/sequence_decl.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

namespace hlc {

class Sequence7Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sequence7.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ----
// Helpers
// ----

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

// ss.16.9.6 + ss.16.14: the SV file is syntactically and semantically valid
// -- no errors expected.  A compiler that fails to resolve the sequence name
// 'seq7' to its SequenceDecl might misidentify it as an undeclared implicit
// net instead, which would surface as a spurious error here.
TEST_F(Sequence7Test, Compiler_NoErrors) {
  ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0) << "ss.16.14: assert property(@(posedge clk) seq7) must not produce "
                                 "errors -- 'seq7' must resolve to its SequenceDecl, not be treated "
                                 "as an implicit net";
}

TEST_F(Sequence7Test, Compiler_NoSyntaxErrors) {
  ErrorContainer::Stats stats = m_compiler->getErrorStats();
  EXPECT_EQ(stats.nbSyntax, 0) << "sequence7.sv is syntactically valid -- no syntax errors expected";
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(Sequence7Test, ModuleExists) { ASSERT_NE(getTb(m_design), nullptr) << "module 'tb' not found"; }

// ===========================================================================
// Sequence declaration (ss.16.8 / ss.16.9.6)
// ===========================================================================

// ss.16.8: exactly one sequence is declared in this module.
TEST_F(Sequence7Test, SequenceDeclCount_IsOne) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getSequenceDecls(), nullptr) << "module has no sequence declarations";
  EXPECT_EQ(m->getSequenceDecls()->size(), 1u) << "ss.16.8: exactly one sequence is declared: seq7";
}

// ss.16.8: 'seq7' must appear in the sequence declaration collection.
TEST_F(Sequence7Test, Seq7_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(getSeqDecl(m, "seq7"), nullptr) << "ss.16.8: sequence 'seq7' must be declared";
}

// ===========================================================================
// seq7 body: 'a intersect b'  (ss.16.9.6)
// ===========================================================================

// ss.16.9.6: seq7 uses the 'intersect' operator -- it must have a non-null
// body expression.
TEST_F(Sequence7Test, Seq7_HasExpression) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s7 = getSeqDecl(m, "seq7");
  ASSERT_NE(s7, nullptr) << "seq7 not found";
  EXPECT_NE(s7->getExpr(), nullptr) << "ss.16.9.6: seq7 must have a body expression";
}

// ss.16.9.6: 'a intersect b' must produce an Operation with opType
// vpiIntersectOp (72).
TEST_F(Sequence7Test, Seq7_Expr_IsIntersectOperation) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s7 = getSeqDecl(m, "seq7");
  ASSERT_NE(s7, nullptr);
  const hldb::Operation *op = s7->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "seq7 body must be an Operation";
  EXPECT_EQ(op->getOpType(), vpiIntersectOp) << "ss.16.9.6: 'a intersect b' must have opType vpiIntersectOp (72)";
}

// ss.16.9.6: 'a intersect b' has two operand sequences.
TEST_F(Sequence7Test, Seq7_Expr_HasTwoOperands) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s7 = getSeqDecl(m, "seq7");
  ASSERT_NE(s7, nullptr);
  const hldb::Operation *op = s7->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u) << "ss.16.9.6: 'a intersect b' must produce exactly 2 operands";
}

// ss.16.9.6: operand[0] is the first sequence 'a'.  It must be a RefObj
// named "a".
TEST_F(Sequence7Test, Seq7_Operand0_IsRefToA) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s7 = getSeqDecl(m, "seq7");
  ASSERT_NE(s7, nullptr);
  const hldb::Operation *op = s7->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);

  const hldb::RefObj *op0 = any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(op0, nullptr) << "operand[0] must be a RefObj";
  EXPECT_EQ(op0->getName(), "a") << "ss.16.9.6: first operand of 'a intersect b' must reference 'a'";
}

// ss.16.9.6: the RefObj for 'a' must resolve (vpiActual) to Variable name:'a'.
// Compile-time name binding -- no elaboration guard needed.
TEST_F(Sequence7Test, Seq7_Operand0_ResolvesToNet) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s7 = getSeqDecl(m, "seq7");
  ASSERT_NE(s7, nullptr);
  const hldb::Operation *op = s7->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);

  const hldb::RefObj *op0 = any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(op0, nullptr);
  EXPECT_NE(op0->getActual<hldb::Variable>(), nullptr) << "ss.16.9.6: RefObj for 'a' must resolve to Variable name:'a' at "
                                                     "compile time";
}

// ss.16.9.6: operand[1] is the second sequence 'b'.  It must be a RefObj
// named "b".
TEST_F(Sequence7Test, Seq7_Operand1_IsRefToB) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s7 = getSeqDecl(m, "seq7");
  ASSERT_NE(s7, nullptr);
  const hldb::Operation *op = s7->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);

  const hldb::RefObj *op1 = any_cast<hldb::RefObj>((*op->getOperands())[1]);
  ASSERT_NE(op1, nullptr) << "operand[1] must be a RefObj";
  EXPECT_EQ(op1->getName(), "b") << "ss.16.9.6: second operand of 'a intersect b' must reference 'b'";
}

// ss.16.9.6: the RefObj for 'b' must resolve (vpiActual) to Variable name:'b'.
// Compile-time name binding -- no elaboration guard needed.
TEST_F(Sequence7Test, Seq7_Operand1_ResolvesToNet) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s7 = getSeqDecl(m, "seq7");
  ASSERT_NE(s7, nullptr);
  const hldb::Operation *op = s7->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);

  const hldb::RefObj *op1 = any_cast<hldb::RefObj>((*op->getOperands())[1]);
  ASSERT_NE(op1, nullptr);
  EXPECT_NE(op1->getActual<hldb::Variable>(), nullptr) << "ss.16.9.6: RefObj for 'b' must resolve to Variable name:'b' at "
                                                     "compile time";
}

// ===========================================================================
// Concurrent assertion: 'assert property(@(posedge clk) seq7)'  (ss.16.14)
// ===========================================================================

// ss.16.14: the module must have at least one concurrent assertion.
TEST_F(Sequence7Test, ConcurrentAssertion_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getConcurrentAssertions(), nullptr) << "module has no concurrent assertions";
  EXPECT_NE(getFirstAssert(m), nullptr) << "ss.16.14: an Assert node must be present";
}

// ss.16.14: the assert must carry an inline PropertySpec.
TEST_F(Sequence7Test, Assert_HasPropertySpec) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getProperty<hldb::PropertySpec>(), nullptr) << "ss.16.14: Assert must have an inline PropertySpec";
}

// ss.16.14: '@(posedge clk)' must be represented as the clocking event on
// the PropertySpec.
TEST_F(Sequence7Test, Assert_PropertySpec_HasClockingEvent) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  EXPECT_NE(spec->getClockingEvent(), nullptr) << "ss.16.14: @(posedge clk) must produce a clocking event on the "
                                                  "PropertySpec";
}

// ss.16.14: the property expression is the reference to 'seq7'.  It must be
// a RefObj named "seq7".
TEST_F(Sequence7Test, Assert_PropertyExpr_ReferencesSeq7) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr) << "property expression must be a RefObj";
  EXPECT_EQ(propExpr->getName(), "seq7") << "ss.16.14: property expression must reference 'seq7'";
}

// ss.16.14: the RefObj for 'seq7' in the concurrent assertion must resolve
// (vpiActual) to the SequenceDecl node, not be treated as an implicit net --
// the same compile-time resolution confirmed in sequence4-6.
TEST_F(Sequence7Test, Assert_PropertyExpr_ResolvedToSeq7Decl) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr);
  EXPECT_NE(propExpr->getActual<hldb::SequenceDecl>(), nullptr)
      << "ss.16.14: 'seq7' in assert property must resolve to SequenceDecl, not be "
         "treated as an implicit net";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
