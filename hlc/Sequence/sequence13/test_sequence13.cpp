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

// Spec-based validation of IEEE 1800-2017 ss.16.9.2:
// consecutive cycle delay with range -- 'a ##[1:3] b' as the top-level
// sequence body.
// SV: tests/Sequence/sequence13.sv
//
//   module tb;
//     bit clk; always #5 clk = ~clk;
//     bit a, b;
//
//     sequence seq13;
//       a ##[1:3] b; // range delay
//     endsequence
//
//     assert property(@(posedge clk) seq13);
//
//     initial begin
//       #10 a=1;
//       #20 b=1;
//       #50 $finish;
//     end
//   endmodule
//
// -- ss.16.9.2 rules under test ----
//
// Range consecutive cycle delay 'a ##[m:n] b' (ss.16.9.2):
//   * '##[m:n]' is the range form of the consecutive cycle delay: 'b' must
//     become true between m and n clock cycles after 'a' becomes true.
//   * The HLDB represents this as an Operation with opType vpiCycleDelayOp
//     (71), which has exactly three operands:
//       operand[0]: RefObj name:"a" -- start expression, resolves to Variable("a")
//       operand[1]: Range node -- holds the [1:3] bounds:
//                     getLeftExpr<Constant>()  -> Constant decompile:"1"
//                     getRightExpr<Constant>() -> Constant decompile:"3"
//       operand[2]: RefObj name:"b" -- end expression, resolves to Variable("b")
//   * The Range middle operand distinguishes the range form '##[m:n]' from the
//     fixed form '##N', which uses a Constant as the middle operand instead.
//   * Both RefObj name bindings happen at compile time -- no elaboration guard.
//
// Concurrent assert property (ss.16.14):
//   * 'assert property(@(posedge clk) seq13)' supplies the clock externally on
//     the assert, so the PropertySpec carries a clocking event.  This contrasts
//     with sequence12, whose sequence declaration embedded its own clock and
//     left the PropertySpec without a clocking event.
//   * 'seq13' in the assert must resolve (vpiActual) to the SequenceDecl,
//     not be treated as an implicit net.
//
// -- Expected HLDB tree (if compiler is correct) ----
//
//   Module name:tb
//   +-- getSequenceDecls() (1 item)
//   |   +-- SequenceDecl name:"seq13"
//   |         vpiExpr: Operation opType:cycle_delay (vpiCycleDelayOp = 71)
//   |           operand[0]: RefObj name:"a" -> Variable name:"a"
//   |           operand[1]: Range
//   |             getLeftExpr<Constant>()  -> Constant decompile:"1"
//   |             getRightExpr<Constant>() -> Constant decompile:"3"
//   |           operand[2]: RefObj name:"b" -> Variable name:"b"
//   +-- getConcurrentAssertions() (1 item)
//       +-- Assert
//             PropertySpec
//               clocking: Operation posedge(clk)
//               propertyExpr: RefObj name:"seq13" -> SequenceDecl name:"seq13"

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
#include <hldb/variable.h>
#include <hldb/operation.h>
#include <hldb/property_spec.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/sequence_decl.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

namespace hlc {

class Sequence13Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sequence13.hlc"}); }
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

// Returns the top-level cycle-delay Operation for seq13, or nullptr.
static const hldb::Operation *getCycleDelayOp(const hldb::Module *m) {
  const hldb::SequenceDecl *s13 = getSeqDecl(m, "seq13");
  if (!s13) return nullptr;
  return s13->getExpr<hldb::Operation>();
}

// Returns the Range node at operand[1] of the cycle-delay Operation, or nullptr.
static const hldb::Range *getRange(const hldb::Module *m) {
  const hldb::Operation *cd = getCycleDelayOp(m);
  if (!cd || !cd->getOperands() || cd->getOperands()->size() < 2) return nullptr;
  return any_cast<hldb::Range>((*cd->getOperands())[1]);
}

// ===========================================================================
// Compiler diagnostics
// ===========================================================================

// ss.16.9.2 + ss.16.14: the SV file is syntactically and semantically valid
// -- no errors expected.  A compiler that fails to resolve the sequence name
// 'seq13' to its SequenceDecl might misidentify it as an undeclared implicit
// net instead, which would surface as a spurious error here.
TEST_F(Sequence13Test, Compiler_NoErrors) {
  ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0) << "ss.16.14: assert property(@(posedge clk) seq13) must not produce "
                                 "errors -- 'seq13' must resolve to its SequenceDecl, not be treated "
                                 "as an implicit net";
}

TEST_F(Sequence13Test, Compiler_NoSyntaxErrors) {
  ErrorContainer::Stats stats = m_compiler->getErrorStats();
  EXPECT_EQ(stats.nbSyntax, 0) << "sequence13.sv is syntactically valid -- no syntax errors expected";
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(Sequence13Test, ModuleExists) { ASSERT_NE(getTb(m_design), nullptr) << "module 'tb' not found"; }

// ===========================================================================
// Sequence declaration (ss.16.8 / ss.16.9.2)
// ===========================================================================

TEST_F(Sequence13Test, SequenceDeclCount_IsOne) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getSequenceDecls(), nullptr) << "module has no sequence declarations";
  EXPECT_EQ(m->getSequenceDecls()->size(), 1u) << "ss.16.8: exactly one sequence is declared: seq13";
}

TEST_F(Sequence13Test, Seq13_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(getSeqDecl(m, "seq13"), nullptr) << "ss.16.8: sequence 'seq13' must be declared";
}

// ===========================================================================
// seq13 body: range cycle delay 'a ##[1:3] b' (ss.16.9.2)
// ===========================================================================

TEST_F(Sequence13Test, Seq13_HasExpression) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s13 = getSeqDecl(m, "seq13");
  ASSERT_NE(s13, nullptr) << "seq13 not found";
  EXPECT_NE(s13->getExpr(), nullptr) << "ss.16.9.2: seq13 must have a body expression";
}

// ss.16.9.2: 'a ##[1:3] b' must be represented as an Operation with opType
// vpiCycleDelayOp (71).  This is the binary range form -- the middle operand
// is a Range node, not a Constant (which is used for the fixed ##N form).
TEST_F(Sequence13Test, Seq13_Expr_IsCycleDelayOperation) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *op = getCycleDelayOp(m);
  ASSERT_NE(op, nullptr) << "seq13 body must be an Operation";
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp) << "ss.16.9.2: 'a ##[1:3] b' must have opType vpiCycleDelayOp (71)";
}

// ss.16.9.2: the range form '##[m:n]' between two signals has three operands:
// start-expression, Range, end-expression.
TEST_F(Sequence13Test, Seq13_CycleDelay_HasThreeOperands) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *op = getCycleDelayOp(m);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u) << "ss.16.9.2: 'a ##[1:3] b' must produce 3 operands: "
                                              "start-expr, Range, end-expr";
}

// ===========================================================================
// operand[0]: start expression 'a'
// ===========================================================================

// ss.16.9.2: operand[0] is the start expression 'a' -- a RefObj named "a".
TEST_F(Sequence13Test, Seq13_CycleDelay_Operand0_IsRefToA) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *op = getCycleDelayOp(m);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);

  const hldb::RefObj *op0 = any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(op0, nullptr) << "operand[0] must be a RefObj";
  EXPECT_EQ(op0->getName(), "a") << "ss.16.9.2: start operand of 'a ##[1:3] b' must reference signal 'a'";
}

// ss.16.9.2: RefObj for 'a' must resolve to Variable name:'a' at compile time.
TEST_F(Sequence13Test, Seq13_CycleDelay_Operand0_ResolvesToNet) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *op = getCycleDelayOp(m);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);

  const hldb::RefObj *op0 = any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(op0, nullptr);
  EXPECT_NE(op0->getActual<hldb::Variable>(), nullptr) << "ss.16.9.2: RefObj for 'a' must resolve to Variable name:'a' at "
                                                     "compile time";
}

// ===========================================================================
// operand[1]: Range [1:3]
// ===========================================================================

// ss.16.9.2: operand[1] of '##[1:3]' must be a Range node.  This is what
// distinguishes the range form '##[m:n]' from the fixed form '##N', which
// uses a Constant node as operand[1].
TEST_F(Sequence13Test, Seq13_CycleDelay_Operand1_IsRange) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *op = getCycleDelayOp(m);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);

  const hldb::Range *r = any_cast<hldb::Range>((*op->getOperands())[1]);
  EXPECT_NE(r, nullptr) << "ss.16.9.2: operand[1] of '##[1:3]' must be a Range node, "
                           "not a Constant (which is used for fixed ##N delays)";
}

// ss.16.9.2: the left bound of [1:3] is 1.
TEST_F(Sequence13Test, Seq13_CycleDelay_Range_LeftBound_IsOne) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Range *r = getRange(m);
  ASSERT_NE(r, nullptr) << "Range node not found at operand[1]";
  const hldb::Constant *lc = r->getLeftExpr<hldb::Constant>();
  ASSERT_NE(lc, nullptr) << "left bound of Range must be a Constant";
  EXPECT_EQ(lc->getDecompile(), "1") << "ss.16.9.2: left bound of '##[1:3]' must be 1";
}

// ss.16.9.2: the right bound of [1:3] is 3.
TEST_F(Sequence13Test, Seq13_CycleDelay_Range_RightBound_IsThree) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Range *r = getRange(m);
  ASSERT_NE(r, nullptr) << "Range node not found at operand[1]";
  const hldb::Constant *rc = r->getRightExpr<hldb::Constant>();
  ASSERT_NE(rc, nullptr) << "right bound of Range must be a Constant";
  EXPECT_EQ(rc->getDecompile(), "3") << "ss.16.9.2: right bound of '##[1:3]' must be 3";
}

// ===========================================================================
// operand[2]: end expression 'b'
// ===========================================================================

// ss.16.9.2: operand[2] is the end expression 'b' -- a RefObj named "b".
TEST_F(Sequence13Test, Seq13_CycleDelay_Operand2_IsRefToB) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *op = getCycleDelayOp(m);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 3u);

  const hldb::RefObj *op2 = any_cast<hldb::RefObj>((*op->getOperands())[2]);
  ASSERT_NE(op2, nullptr) << "operand[2] must be a RefObj";
  EXPECT_EQ(op2->getName(), "b") << "ss.16.9.2: end operand of 'a ##[1:3] b' must reference signal 'b'";
}

// ss.16.9.2: RefObj for 'b' must resolve to Variable name:'b' at compile time.
TEST_F(Sequence13Test, Seq13_CycleDelay_Operand2_ResolvesToNet) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *op = getCycleDelayOp(m);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 3u);

  const hldb::RefObj *op2 = any_cast<hldb::RefObj>((*op->getOperands())[2]);
  ASSERT_NE(op2, nullptr);
  EXPECT_NE(op2->getActual<hldb::Variable>(), nullptr) << "ss.16.9.2: RefObj for 'b' must resolve to Variable name:'b' at "
                                                     "compile time";
}

// ===========================================================================
// Concurrent assertion: 'assert property(@(posedge clk) seq13)'  (ss.16.14)
// ===========================================================================

TEST_F(Sequence13Test, ConcurrentAssertion_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getConcurrentAssertions(), nullptr) << "module has no concurrent assertions";
  EXPECT_NE(getFirstAssert(m), nullptr) << "ss.16.14: an Assert node must be present";
}

TEST_F(Sequence13Test, Assert_HasPropertySpec) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getProperty<hldb::PropertySpec>(), nullptr) << "ss.16.14: Assert must have an inline PropertySpec";
}

// ss.16.14: the clock is given externally on the assert, so the PropertySpec
// must have a non-null clocking event.  This contrasts with sequence12 where
// the sequence embedded its own clock and the PropertySpec had no clocking event.
TEST_F(Sequence13Test, Assert_PropertySpec_HasClockingEvent) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  EXPECT_NE(spec->getClockingEvent(), nullptr) << "ss.16.14: @(posedge clk) on the assert must produce a clocking "
                                                  "event on the PropertySpec";
}

TEST_F(Sequence13Test, Assert_PropertyExpr_ReferencesSeq13) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr) << "property expression must be a RefObj";
  EXPECT_EQ(propExpr->getName(), "seq13") << "ss.16.14: property expression must reference 'seq13'";
}

// ss.16.14: RefObj for 'seq13' must resolve to SequenceDecl, not be treated
// as an implicit net -- the same compile-time resolution confirmed in
// sequence4-12.
TEST_F(Sequence13Test, Assert_PropertyExpr_ResolvedToSeq13Decl) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr);
  EXPECT_NE(propExpr->getActual<hldb::SequenceDecl>(), nullptr)
      << "ss.16.14: 'seq13' in assert property must resolve to SequenceDecl, not be "
         "treated as an implicit net";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
