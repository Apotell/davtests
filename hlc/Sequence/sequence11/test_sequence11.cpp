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

// Spec-based validation of IEEE 1800-2017 ss.16.9.10:
// within -- seq1 within seq2 matches if there is a contiguous subsequence of
// seq2 in which seq1 matches.
// SV: tests/Sequence/sequence11.sv
//
//   module tb;
//     bit clk; always #5 clk = ~clk;
//     bit a, b, c;
//
//     sequence seq11;
//       (a ##1 b) within (##3 c);
//     endsequence
//
//     assert property(@(posedge clk) seq11);
//
//     initial begin
//       #10 a=1;
//       #10 b=1;
//       #30 c=1;
//       #60 $finish;
//     end
//   endmodule
//
// -- ss.16.9.10 rules under test -----------------------------------------------
//
// within operator (ss.16.9.10):
//   * 'seq1 within seq2' is true if seq1 matches within a contiguous sub-
//     interval of seq2.
//   * The correct VPI opType for within is vpiWithinOp (75), defined in
//     sv_vpi_user.h.
//   * The operator has two operands:
//       operand[0]: the left sequence '(a ##1 b)' -- an Operation with opType
//                   vpiCycleDelayOp (71) (binary fixed cycle delay ##N).
//       operand[1]: the right sequence '(##3 c)' -- an Operation with opType
//                   vpiUnaryCycleDelayOp (70) (unary cycle delay ##N with no
//                   explicit left-hand sequence).
//
// Binary fixed cycle delay 'a ##1 b' (ss.16.9.2):
//   * vpiCycleDelayOp (71) has three operands when the delay is a fixed ##N:
//       operand[0]: RefObj name:"a" -- must resolve to Net name:"a"
//       operand[1]: Constant decompile:"1" (the delay amount)
//       operand[2]: RefObj name:"b" -- must resolve to Net name:"b"
//   * Note: when the delay is a range ##[m:n], the middle operand is a Range
//     node instead of a Constant.  Here it is a Constant since ##1 is fixed.
//
// Unary cycle delay '##3 c' (ss.16.9.2):
//   * vpiUnaryCycleDelayOp (70) has two operands when used in the unary form:
//       operand[0]: Constant decompile:"3" (the delay amount)
//       operand[1]: RefObj name:"c" -- must resolve to Net name:"c"
//
// Concurrent assert property (ss.16.14):
//   * 'assert property(@(posedge clk) seq11)' uses seq11 as the property body.
//   * The property expression 'seq11' must resolve (vpiActual) to the
//     SequenceDecl for seq11 -- not be treated as an implicit net.
//   * Surelog emits EL0535 ("Illegal implicit net") for 'seq11' here -- the
//     same compile-time name resolution bug confirmed in sequence4 through 10.
//
// -- Expected HLDB tree (if compiler is correct) --------------------------------
//
//   Module name:work@tb
//   +-- getSequenceDecls() (1 item)
//   |   +-- SequenceDecl name:"seq11"
//   |         vpiExpr: Operation opType:within (vpiWithinOp = 75)
//   |           operand[0]: Operation opType:cycle_delay (vpiCycleDelayOp = 71)
//   |             operand[0]: RefObj name:"a" -> Net name:"a"
//   |             operand[1]: Constant decompile:"1"
//   |             operand[2]: RefObj name:"b" -> Net name:"b"
//   |           operand[1]: Operation opType:unary_cycle_delay
//   |                         (vpiUnaryCycleDelayOp = 70)
//   |             operand[0]: Constant decompile:"3"
//   |             operand[1]: RefObj name:"c" -> Net name:"c"
//   +-- getConcurrentAssertions() (1 item)
//       +-- Assert
//             PropertySpec
//               clocking: Operation posedge(clk)
//               propertyExpr: RefObj name:"seq11" -> SequenceDecl name:"seq11"

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

class Sequence11Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sequence11.hlc"}); }
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

// Returns the outer within Operation, or nullptr on failure.
static const hldb::Operation *getWithinOp(const hldb::Module *m) {
  const hldb::SequenceDecl *s11 = getSeqDecl(m, "seq11");
  if (!s11) return nullptr;
  return s11->getExpr<hldb::Operation>();
}

// Returns operand[0] of within: the binary cycle-delay Operation 'a ##1 b'.
static const hldb::Operation *getLeftCycleDelayOp(const hldb::Module *m) {
  const hldb::Operation *w = getWithinOp(m);
  if (!w || !w->getOperands() || w->getOperands()->empty()) return nullptr;
  return any_cast<hldb::Operation>((*w->getOperands())[0]);
}

// Returns operand[1] of within: the unary cycle-delay Operation '##3 c'.
static const hldb::Operation *getRightCycleDelayOp(const hldb::Module *m) {
  const hldb::Operation *w = getWithinOp(m);
  if (!w || !w->getOperands() || w->getOperands()->size() < 2) return nullptr;
  return any_cast<hldb::Operation>((*w->getOperands())[1]);
}

// ===========================================================================
// Compiler diagnostics
// ===========================================================================

// ss.16.9.10 + ss.16.14: the SV file is syntactically and semantically valid
// -- no errors expected.  If this test fails, Surelog emits EL0535 ("Illegal
// implicit net") for 'seq11' in the assert property statement, meaning it
// misidentifies the sequence name as an undeclared net instead of resolving
// it to the SequenceDecl node.
TEST_F(Sequence11Test, Compiler_NoErrors) {
  ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0) << "ss.16.14: assert property(@(posedge clk) seq11) must not produce "
                                 "errors -- EL0535 'Illegal implicit net' means Surelog does not "
                                 "resolve sequence names to SequenceDecl nodes";
}

TEST_F(Sequence11Test, Compiler_NoSyntaxErrors) {
  ErrorContainer::Stats stats = m_compiler->getErrorStats();
  EXPECT_EQ(stats.nbSyntax, 0) << "sequence11.sv is syntactically valid -- no syntax errors expected";
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(Sequence11Test, ModuleExists) { ASSERT_NE(getTb(m_design), nullptr) << "module 'work@tb' not found"; }

// ===========================================================================
// Sequence declaration (ss.16.8 / ss.16.9.10)
// ===========================================================================

// ss.16.8: exactly one sequence is declared in this module.
TEST_F(Sequence11Test, SequenceDeclCount_IsOne) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getSequenceDecls(), nullptr) << "module has no sequence declarations";
  EXPECT_EQ(m->getSequenceDecls()->size(), 1u) << "ss.16.8: exactly one sequence is declared: seq11";
}

// ss.16.8: 'seq11' must appear in the sequence declaration collection.
TEST_F(Sequence11Test, Seq11_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(getSeqDecl(m, "seq11"), nullptr) << "ss.16.8: sequence 'seq11' must be declared";
}

// ===========================================================================
// seq11 body: outer within operation (ss.16.9.10)
// ===========================================================================

// ss.16.9.10: seq11 uses within -- it must have a non-null body.
TEST_F(Sequence11Test, Seq11_HasExpression) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s11 = getSeqDecl(m, "seq11");
  ASSERT_NE(s11, nullptr) << "seq11 not found";
  EXPECT_NE(s11->getExpr(), nullptr) << "ss.16.9.10: seq11 must have a body expression";
}

// ss.16.9.10: '(a ##1 b) within (##3 c)' must be represented as an Operation
// with opType vpiWithinOp (75), as defined in sv_vpi_user.h.
TEST_F(Sequence11Test, Seq11_Expr_IsWithinOperation) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *op = getWithinOp(m);
  ASSERT_NE(op, nullptr) << "seq11 body must be an Operation";
  EXPECT_EQ(op->getOpType(), vpiWithinOp) << "ss.16.9.10: '(a ##1 b) within (##3 c)' must have opType "
                                             "vpiWithinOp (75)";
}

// ss.16.9.10: within takes a left sequence and a right sequence -- exactly
// two operands.
TEST_F(Sequence11Test, Seq11_Within_HasTwoOperands) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *op = getWithinOp(m);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u) << "ss.16.9.10: within must produce exactly 2 operands";
}

// ===========================================================================
// seq11 body: within operand[0] -- 'a ##1 b' (ss.16.9.2)
// ===========================================================================

// ss.16.9.2: operand[0] of within is '(a ##1 b)'.  The fixed binary cycle
// delay ##N is vpiCycleDelayOp (71).
TEST_F(Sequence11Test, Seq11_Within_Operand0_IsBinaryCycleDelay) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getLeftCycleDelayOp(m);
  ASSERT_NE(cd, nullptr) << "operand[0] of within must be a cycle-delay Operation";
  EXPECT_EQ(cd->getOpType(), vpiCycleDelayOp) << "ss.16.9.2: 'a ##1 b' must have opType vpiCycleDelayOp (71)";
}

// ss.16.9.2: 'a ##1 b' with a fixed delay N has three operands: start-expr,
// Constant delay, end-expr.
TEST_F(Sequence11Test, Seq11_BinaryCycleDelay_HasThreeOperands) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getLeftCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  EXPECT_EQ(cd->getOperands()->size(), 3u) << "ss.16.9.2: 'a ##1 b' must produce 3 operands: "
                                              "start-expr, delay-constant, end-expr";
}

// ss.16.9.2: operand[0] of 'a ##1 b' is the start expression 'a'.
// It must be a RefObj named "a".
TEST_F(Sequence11Test, Seq11_BinaryCycleDelay_Operand0_IsRefToA) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getLeftCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 1u);

  const hldb::RefObj *op0 = any_cast<hldb::RefObj>((*cd->getOperands())[0]);
  ASSERT_NE(op0, nullptr) << "operand[0] of 'a ##1 b' must be a RefObj";
  EXPECT_EQ(op0->getName(), "a") << "ss.16.9.2: start of 'a ##1 b' must reference signal 'a'";
}

// ss.16.9.2: RefObj for 'a' must resolve to Net name:'a' at compile time.
TEST_F(Sequence11Test, Seq11_BinaryCycleDelay_Operand0_ResolvesToNet) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getLeftCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 1u);

  const hldb::RefObj *op0 = any_cast<hldb::RefObj>((*cd->getOperands())[0]);
  ASSERT_NE(op0, nullptr);
  EXPECT_NE(op0->getActual<hldb::Net>(), nullptr) << "ss.16.9.2: RefObj for 'a' in 'a ##1 b' must resolve to "
                                                     "Net name:'a' at compile time";
}

// ss.16.9.2: operand[1] of 'a ##1 b' is the fixed delay amount.  For a fixed
// ##N delay it must be a Constant node (not a Range, which is used for ##[m:n]).
TEST_F(Sequence11Test, Seq11_BinaryCycleDelay_Delay_IsConstant) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getLeftCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 2u);

  const hldb::Constant *c = any_cast<hldb::Constant>((*cd->getOperands())[1]);
  EXPECT_NE(c, nullptr) << "ss.16.9.2: fixed delay operand of 'a ##1 b' must be a Constant, "
                           "not a Range (which is used for ##[m:n])";
}

// ss.16.9.2: the fixed delay constant for '##1' must have value "1".
TEST_F(Sequence11Test, Seq11_BinaryCycleDelay_Delay_ValueIsOne) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getLeftCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 2u);

  const hldb::Constant *c = any_cast<hldb::Constant>((*cd->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "1") << "ss.16.9.2: delay constant for '##1' must have value \"1\"";
}

// ss.16.9.2: operand[2] of 'a ##1 b' is the end expression 'b'.
// It must be a RefObj named "b".
TEST_F(Sequence11Test, Seq11_BinaryCycleDelay_Operand2_IsRefToB) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getLeftCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 3u);

  const hldb::RefObj *op2 = any_cast<hldb::RefObj>((*cd->getOperands())[2]);
  ASSERT_NE(op2, nullptr) << "operand[2] of 'a ##1 b' must be a RefObj";
  EXPECT_EQ(op2->getName(), "b") << "ss.16.9.2: end of 'a ##1 b' must reference signal 'b'";
}

// ss.16.9.2: RefObj for 'b' must resolve to Net name:'b' at compile time.
TEST_F(Sequence11Test, Seq11_BinaryCycleDelay_Operand2_ResolvesToNet) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getLeftCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 3u);

  const hldb::RefObj *op2 = any_cast<hldb::RefObj>((*cd->getOperands())[2]);
  ASSERT_NE(op2, nullptr);
  EXPECT_NE(op2->getActual<hldb::Net>(), nullptr) << "ss.16.9.2: RefObj for 'b' in 'a ##1 b' must resolve to "
                                                     "Net name:'b' at compile time";
}

// ===========================================================================
// seq11 body: within operand[1] -- '##3 c' (ss.16.9.2)
// ===========================================================================

// ss.16.9.2: operand[1] of within is '(##3 c)'.  The unary form of the cycle
// delay ##N (no explicit left-hand sequence) is vpiUnaryCycleDelayOp (70).
TEST_F(Sequence11Test, Seq11_Within_Operand1_IsUnaryCycleDelay) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getRightCycleDelayOp(m);
  ASSERT_NE(cd, nullptr) << "operand[1] of within must be a cycle-delay Operation";
  EXPECT_EQ(cd->getOpType(), vpiUnaryCycleDelayOp) << "ss.16.9.2: '##3 c' must have opType vpiUnaryCycleDelayOp (70)";
}

// ss.16.9.2: '##3 c' has two operands: the delay constant and the endpoint.
TEST_F(Sequence11Test, Seq11_UnaryCycleDelay_HasTwoOperands) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getRightCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  EXPECT_EQ(cd->getOperands()->size(), 2u) << "ss.16.9.2: '##3 c' must produce 2 operands: delay constant and "
                                              "endpoint expression";
}

// ss.16.9.2: operand[0] of '##3 c' is the delay amount -- a Constant.
TEST_F(Sequence11Test, Seq11_UnaryCycleDelay_Delay_IsConstant) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getRightCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 1u);

  const hldb::Constant *c = any_cast<hldb::Constant>((*cd->getOperands())[0]);
  EXPECT_NE(c, nullptr) << "ss.16.9.2: operand[0] of '##3 c' must be a Constant";
}

// ss.16.9.2: the delay constant for '##3' must have value "3".
TEST_F(Sequence11Test, Seq11_UnaryCycleDelay_Delay_ValueIsThree) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getRightCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 1u);

  const hldb::Constant *c = any_cast<hldb::Constant>((*cd->getOperands())[0]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "3") << "ss.16.9.2: delay constant for '##3' must have value \"3\"";
}

// ss.16.9.2: operand[1] of '##3 c' is the endpoint expression 'c'.
// It must be a RefObj named "c".
TEST_F(Sequence11Test, Seq11_UnaryCycleDelay_Operand1_IsRefToC) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getRightCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 2u);

  const hldb::RefObj *op1 = any_cast<hldb::RefObj>((*cd->getOperands())[1]);
  ASSERT_NE(op1, nullptr) << "operand[1] of '##3 c' must be a RefObj";
  EXPECT_EQ(op1->getName(), "c") << "ss.16.9.2: endpoint of '##3 c' must reference signal 'c'";
}

// ss.16.9.2: RefObj for 'c' must resolve to Net name:'c' at compile time.
TEST_F(Sequence11Test, Seq11_UnaryCycleDelay_Operand1_ResolvesToNet) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getRightCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 2u);

  const hldb::RefObj *op1 = any_cast<hldb::RefObj>((*cd->getOperands())[1]);
  ASSERT_NE(op1, nullptr);
  EXPECT_NE(op1->getActual<hldb::Net>(), nullptr) << "ss.16.9.2: RefObj for 'c' in '##3 c' must resolve to "
                                                     "Net name:'c' at compile time";
}

// ===========================================================================
// Concurrent assertion: 'assert property(@(posedge clk) seq11)'  (ss.16.14)
// ===========================================================================

// ss.16.14: the module must have at least one concurrent assertion.
TEST_F(Sequence11Test, ConcurrentAssertion_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getConcurrentAssertions(), nullptr) << "module has no concurrent assertions";
  EXPECT_NE(getFirstAssert(m), nullptr) << "ss.16.14: an Assert node must be present";
}

// ss.16.14: the assert must carry an inline PropertySpec.
TEST_F(Sequence11Test, Assert_HasPropertySpec) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getProperty<hldb::PropertySpec>(), nullptr) << "ss.16.14: Assert must have an inline PropertySpec";
}

// ss.16.14: '@(posedge clk)' must be represented as the clocking event on
// the PropertySpec.
TEST_F(Sequence11Test, Assert_PropertySpec_HasClockingEvent) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  EXPECT_NE(spec->getClockingEvent(), nullptr) << "ss.16.14: @(posedge clk) must produce a clocking event on the "
                                                  "PropertySpec";
}

// ss.16.14: the property expression is the reference to 'seq11'.  It must be
// a RefObj named "seq11".
TEST_F(Sequence11Test, Assert_PropertyExpr_ReferencesSeq11) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr) << "property expression must be a RefObj";
  EXPECT_EQ(propExpr->getName(), "seq11") << "ss.16.14: property expression must reference 'seq11'";
}

// ss.16.14: the RefObj for 'seq11' in the concurrent assertion must resolve
// (vpiActual) to the SequenceDecl node.  Surelog emits EL0535 for this
// reference, treating 'seq11' as an implicit net instead of resolving it to
// the SequenceDecl.  Same compile-time name resolution bug as sequence4-10.
TEST_F(Sequence11Test, Assert_PropertyExpr_ResolvedToSeq11Decl) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr);
  EXPECT_NE(propExpr->getActual<hldb::SequenceDecl>(), nullptr)
      << "ss.16.14: 'seq11' in assert property must resolve to SequenceDecl -- "
         "Surelog emits EL0535 treating it as an implicit net instead";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
