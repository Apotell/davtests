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

// Spec-based validation of IEEE 1800-2017 ss.16.9.9:
// throughout -- requires an expression to hold true at every clock tick
// throughout the evaluation of a sequence.
// SV: tests/Sequence/sequence10.sv
//
//   module tb;
//     bit clk; always #5 clk = ~clk;
//     bit a, b;
//
//     sequence seq10;
//       a throughout (##2 b);
//     endsequence
//
//     assert property(@(posedge clk) seq10);
//
//     initial begin
//       a=1;
//       #20 b=1;
//       #50 $finish;
//     end
//   endmodule
//
// -- ss.16.9.9 rules under test ------------------------------------------------
//
// throughout operator (ss.16.9.9):
//   * 'expr throughout seq_expr' requires 'expr' to hold true at every clock
//     tick during which 'seq_expr' is being matched.
//   * The correct VPI opType for throughout is vpiThroughoutOp (74), defined
//     in sv_vpi_user.h.
//   * The operator has two operands:
//       operand[0]: the expression 'a' -- a RefObj that must resolve to
//                   Net name:"a" at compile time.
//       operand[1]: the sequence '##2 b' -- an Operation with opType
//                   vpiUnaryCycleDelayOp (70), the fixed cycle delay ##N.
//   * The inner '##2 b' operation has two operands:
//       operand[0]: Constant with value "2" (the delay amount)
//       operand[1]: RefObj name:"b" that must resolve to Net name:"b" at
//                   compile time.
//
// Concurrent assert property (ss.16.14):
//   * 'assert property(@(posedge clk) seq10)' uses seq10 as the property body.
//   * The property expression 'seq10' must resolve (vpiActual) to the
//     SequenceDecl for seq10 -- not be treated as an implicit net.
//   * Surelog emits EL0535 ("Illegal implicit net") for 'seq10' here -- the
//     same compile-time name resolution bug confirmed in sequence4 through 9.
//
// -- Expected HLDB tree (if compiler is correct) --------------------------------
//
//   Module name:work@tb
//   +-- getSequenceDecls() (1 item)
//   |   +-- SequenceDecl name:"seq10"
//   |         vpiExpr: Operation opType:throughout (vpiThroughoutOp = 74)
//   |           operand[0]: RefObj name:"a" -> Net name:"a"
//   |           operand[1]: Operation opType:unary_cycle_delay
//   |                         (vpiUnaryCycleDelayOp = 70)
//   |             operand[0]: Constant decompile:"2"
//   |             operand[1]: RefObj name:"b" -> Net name:"b"
//   +-- getConcurrentAssertions() (1 item)
//       +-- Assert
//             PropertySpec
//               clocking: Operation posedge(clk)
//               propertyExpr: RefObj name:"seq10" -> SequenceDecl name:"seq10"

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assert_stmt.h>
#include <hldb/concurrent_assertions.h>
#include <hldb/constant.h>
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

class Sequence10Test : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "sequence10.hlc"});

    ASSERT_NE(m_session,  nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design,   nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design   = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session  = nullptr;
  }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Module *getTb(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@tb", d->getAllModules());
}

static const hldb::SequenceDecl *getSeqDecl(const hldb::Module *m,
                                             std::string_view name) {
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

// Returns the inner unary-cycle-delay Operation that is operand[1] of the
// throughout Operation, or nullptr on any failure.
static const hldb::Operation *getInnerCycleDelayOp(const hldb::Module *m) {
  const hldb::SequenceDecl *s10 = getSeqDecl(m, "seq10");
  if (!s10) return nullptr;
  const hldb::Operation *th = s10->getExpr<hldb::Operation>();
  if (!th || !th->getOperands() || th->getOperands()->size() < 2) return nullptr;
  return any_cast<hldb::Operation>((*th->getOperands())[1]);
}

// ===========================================================================
// Compiler diagnostics
// ===========================================================================

// ss.16.9.9 + ss.16.14: the SV file is syntactically and semantically valid
// -- no errors expected.  If this test fails, Surelog emits EL0535 ("Illegal
// implicit net") for 'seq10' in the assert property statement, meaning it
// misidentifies the sequence name as an undeclared net instead of resolving
// it to the SequenceDecl node.
TEST_F(Sequence10Test, Compiler_NoErrors) {
  ErrorContainer::Stats stats =
      m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0)
      << "ss.16.14: assert property(@(posedge clk) seq10) must not produce "
         "errors -- EL0535 'Illegal implicit net' means Surelog does not "
         "resolve sequence names to SequenceDecl nodes";
}

TEST_F(Sequence10Test, Compiler_NoSyntaxErrors) {
  ErrorContainer::Stats stats = m_compiler->getErrorStats();
  EXPECT_EQ(stats.nbSyntax, 0)
      << "sequence10.sv is syntactically valid -- no syntax errors expected";
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(Sequence10Test, ModuleExists) {
  ASSERT_NE(getTb(m_design), nullptr) << "module 'work@tb' not found";
}

// ===========================================================================
// Sequence declaration (ss.16.8 / ss.16.9.9)
// ===========================================================================

// ss.16.8: exactly one sequence is declared in this module.
TEST_F(Sequence10Test, SequenceDeclCount_IsOne) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getSequenceDecls(), nullptr)
      << "module has no sequence declarations";
  EXPECT_EQ(m->getSequenceDecls()->size(), 1u)
      << "ss.16.8: exactly one sequence is declared: seq10";
}

// ss.16.8: 'seq10' must appear in the sequence declaration collection.
TEST_F(Sequence10Test, Seq10_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(getSeqDecl(m, "seq10"), nullptr)
      << "ss.16.8: sequence 'seq10' must be declared";
}

// ===========================================================================
// seq10 body: outer throughout operation (ss.16.9.9)
// ===========================================================================

// ss.16.9.9: seq10 uses throughout -- it must have a non-null body.
TEST_F(Sequence10Test, Seq10_HasExpression) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s10 = getSeqDecl(m, "seq10");
  ASSERT_NE(s10, nullptr) << "seq10 not found";
  EXPECT_NE(s10->getExpr(), nullptr)
      << "ss.16.9.9: seq10 must have a body expression";
}

// ss.16.9.9: 'a throughout (##2 b)' must be represented as an Operation with
// opType vpiThroughoutOp (74), as defined in sv_vpi_user.h.
TEST_F(Sequence10Test, Seq10_Expr_IsThroughoutOperation) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s10 = getSeqDecl(m, "seq10");
  ASSERT_NE(s10, nullptr);
  const hldb::Operation *op = s10->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "seq10 body must be an Operation";
  EXPECT_EQ(op->getOpType(), vpiThroughoutOp)
      << "ss.16.9.9: 'a throughout (##2 b)' must have opType "
         "vpiThroughoutOp (74)";
}

// ss.16.9.9: throughout takes an expression on the left and a sequence on the
// right -- exactly two operands.
TEST_F(Sequence10Test, Seq10_Throughout_HasTwoOperands) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s10 = getSeqDecl(m, "seq10");
  ASSERT_NE(s10, nullptr);
  const hldb::Operation *op = s10->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u)
      << "ss.16.9.9: throughout must produce exactly 2 operands: "
         "guard-expression and sequence";
}

// ===========================================================================
// seq10 body: throughout operand[0] -- the guard expression 'a'
// ===========================================================================

// ss.16.9.9: operand[0] of throughout is the guard expression 'a'.
// It must be a RefObj named "a".
TEST_F(Sequence10Test, Seq10_Throughout_Operand0_IsRefToA) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s10 = getSeqDecl(m, "seq10");
  ASSERT_NE(s10, nullptr);
  const hldb::Operation *op = s10->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);

  const hldb::RefObj *op0 =
      any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(op0, nullptr) << "operand[0] of throughout must be a RefObj";
  EXPECT_EQ(op0->getName(), "a")
      << "ss.16.9.9: guard expression of throughout must reference signal 'a'";
}

// ss.16.9.9: the guard RefObj for 'a' must resolve to Net name:'a' at compile
// time.  Name binding happens at compile time -- no elaboration guard needed.
TEST_F(Sequence10Test, Seq10_Throughout_Operand0_ResolvesToNet) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s10 = getSeqDecl(m, "seq10");
  ASSERT_NE(s10, nullptr);
  const hldb::Operation *op = s10->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);

  const hldb::RefObj *op0 =
      any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(op0, nullptr);
  EXPECT_NE(op0->getActual<hldb::Net>(), nullptr)
      << "ss.16.9.9: RefObj for 'a' in throughout guard must resolve to "
         "Net name:'a' at compile time";
}

// ===========================================================================
// seq10 body: throughout operand[1] -- the sequence '##2 b'
// ===========================================================================

// ss.16.9.9: operand[1] of throughout is the sequence '##2 b'.  The fixed
// cycle delay ##N must be represented as an Operation with opType
// vpiUnaryCycleDelayOp (70), as defined in sv_vpi_user.h.
TEST_F(Sequence10Test, Seq10_Throughout_Operand1_IsCycleDelayOperation) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getInnerCycleDelayOp(m);
  ASSERT_NE(cd, nullptr)
      << "operand[1] of throughout must be a cycle-delay Operation";
  EXPECT_EQ(cd->getOpType(), vpiUnaryCycleDelayOp)
      << "ss.16.9.9 / ss.16.9.2: '##2 b' must have opType "
         "vpiUnaryCycleDelayOp (70) for the fixed cycle delay ##N";
}

// ss.16.9.2: '##2 b' has two operands: the delay constant and the endpoint
// sequence expression.
TEST_F(Sequence10Test, Seq10_InnerCycleDelay_HasTwoOperands) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getInnerCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  EXPECT_EQ(cd->getOperands()->size(), 2u)
      << "ss.16.9.2: '##2 b' must produce 2 operands: delay constant and "
         "endpoint expression";
}

// ss.16.9.2: operand[0] of '##2 b' is the delay amount.  It must be a
// Constant node.
TEST_F(Sequence10Test, Seq10_InnerCycleDelay_Delay_IsConstant) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getInnerCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 1u);

  const hldb::Constant *c =
      any_cast<hldb::Constant>((*cd->getOperands())[0]);
  EXPECT_NE(c, nullptr)
      << "ss.16.9.2: operand[0] of '##2 b' must be a Constant";
}

// ss.16.9.2: the delay constant for '##2' must have value "2".
TEST_F(Sequence10Test, Seq10_InnerCycleDelay_Delay_ValueIsTwo) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getInnerCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 1u);

  const hldb::Constant *c =
      any_cast<hldb::Constant>((*cd->getOperands())[0]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "2")
      << "ss.16.9.2: delay constant for '##2' must have value \"2\"";
}

// ss.16.9.2: operand[1] of '##2 b' is the endpoint expression 'b'.
// It must be a RefObj named "b".
TEST_F(Sequence10Test, Seq10_InnerCycleDelay_Operand1_IsRefToB) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getInnerCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 2u);

  const hldb::RefObj *op1 =
      any_cast<hldb::RefObj>((*cd->getOperands())[1]);
  ASSERT_NE(op1, nullptr) << "operand[1] of '##2 b' must be a RefObj";
  EXPECT_EQ(op1->getName(), "b")
      << "ss.16.9.2: endpoint of '##2 b' must reference signal 'b'";
}

// ss.16.9.2: the RefObj for 'b' must resolve to Net name:'b' at compile time.
// Name binding happens at compile time -- no elaboration guard needed.
TEST_F(Sequence10Test, Seq10_InnerCycleDelay_Operand1_ResolvesToNet) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getInnerCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 2u);

  const hldb::RefObj *op1 =
      any_cast<hldb::RefObj>((*cd->getOperands())[1]);
  ASSERT_NE(op1, nullptr);
  EXPECT_NE(op1->getActual<hldb::Net>(), nullptr)
      << "ss.16.9.2: RefObj for 'b' in '##2 b' must resolve to "
         "Net name:'b' at compile time";
}

// ===========================================================================
// Concurrent assertion: 'assert property(@(posedge clk) seq10)'  (ss.16.14)
// ===========================================================================

// ss.16.14: the module must have at least one concurrent assertion.
TEST_F(Sequence10Test, ConcurrentAssertion_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getConcurrentAssertions(), nullptr)
      << "module has no concurrent assertions";
  EXPECT_NE(getFirstAssert(m), nullptr)
      << "ss.16.14: an Assert node must be present";
}

// ss.16.14: the assert must carry an inline PropertySpec.
TEST_F(Sequence10Test, Assert_HasPropertySpec) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getProperty<hldb::PropertySpec>(), nullptr)
      << "ss.16.14: Assert must have an inline PropertySpec";
}

// ss.16.14: '@(posedge clk)' must be represented as the clocking event on
// the PropertySpec.
TEST_F(Sequence10Test, Assert_PropertySpec_HasClockingEvent) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  EXPECT_NE(spec->getClockingEvent(), nullptr)
      << "ss.16.14: @(posedge clk) must produce a clocking event on the "
         "PropertySpec";
}

// ss.16.14: the property expression is the reference to 'seq10'.  It must be
// a RefObj named "seq10".
TEST_F(Sequence10Test, Assert_PropertyExpr_ReferencesSeq10) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr) << "property expression must be a RefObj";
  EXPECT_EQ(propExpr->getName(), "seq10")
      << "ss.16.14: property expression must reference 'seq10'";
}

// ss.16.14: the RefObj for 'seq10' in the concurrent assertion must resolve
// (vpiActual) to the SequenceDecl node.  Surelog emits EL0535 for this
// reference, treating 'seq10' as an implicit net instead of resolving it to
// the SequenceDecl.  Same compile-time name resolution bug as sequence4-9.
TEST_F(Sequence10Test, Assert_PropertyExpr_ResolvedToSeq10Decl) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr);
  EXPECT_NE(propExpr->getActual<hldb::SequenceDecl>(), nullptr)
      << "ss.16.14: 'seq10' in assert property must resolve to SequenceDecl -- "
         "Surelog emits EL0535 treating it as an implicit net instead";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
