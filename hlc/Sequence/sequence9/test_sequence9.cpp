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

// Spec-based validation of IEEE 1800-2017 ss.16.9.8:
// first_match -- matches only the first of all sequences that match the
// argument, eliminating multiple match threads.
// SV: tests/Sequence/sequence9.sv
//
//   module tb;
//     bit clk; always #5 clk = ~clk;
//     bit a, b;
//
//     sequence seq9;
//       first_match(a ##[1:3] b);
//     endsequence
//
//     assert property(@(posedge clk) seq9);
//
//     initial begin
//       #10 a=1;
//       #20 b=1;
//       #50 $finish;
//     end
//   endmodule
//
// -- ss.16.9.8 rules under test ------------------------------------------------
//
// first_match(seq_expr) (ss.16.9.8):
//   * first_match evaluates seq_expr and retains only the first matching
//     subsequence, pruning all remaining matches.
//   * The correct VPI opType for the first_match operator is vpiFirstMatchOp
//     (73), defined in sv_vpi_user.h.
//   * The argument is the sequence 'a ##[1:3] b'.  The cycle delay range ##
//     operator has opType vpiCycleDelayOp (71), with three operands:
//       operand[0]: RefObj name:"a" (the start expression)
//       operand[1]: Range (the [1:3] bounds: left=1, right=3)
//       operand[2]: RefObj name:"b" (the end expression)
//   * Both RefObj nodes must resolve to their Net declarations at compile time:
//       RefObj("a") -> Net("a"), RefObj("b") -> Net("b").
//
// Concurrent assert property (ss.16.14):
//   * 'assert property(@(posedge clk) seq9)' uses seq9 as the property body.
//   * The property expression 'seq9' must resolve (vpiActual) to the
//     SequenceDecl for seq9 -- not be treated as an implicit net.
//   * Surelog emits EL0535 ("Illegal implicit net") for 'seq9' here -- the
//     same compile-time name resolution bug confirmed in sequence4 through 8.
//
// -- Expected HLDB tree (if compiler is correct) --------------------------------
//
//   Module name:work@tb
//   +-- getSequenceDecls() (1 item)
//   |   +-- SequenceDecl name:"seq9"
//   |         vpiExpr: Operation opType:first_match (vpiFirstMatchOp = 73)
//   |           operand[0]: Operation opType:cycle_delay (vpiCycleDelayOp = 71)
//   |             operand[0]: RefObj name:"a" -> Net name:"a"
//   |             operand[1]: Range leftExpr:Constant("1") rightExpr:Constant("3")
//   |             operand[2]: RefObj name:"b" -> Net name:"b"
//   +-- getConcurrentAssertions() (1 item)
//       +-- Assert
//             PropertySpec
//               clocking: Operation posedge(clk)
//               propertyExpr: RefObj name:"seq9" -> SequenceDecl name:"seq9"

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
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/sequence_decl.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

namespace hlc {

class Sequence9Test : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "sequence9.hlc"});

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

// Returns the inner cycle-delay Operation that is the sole operand of the
// first_match Operation, or nullptr on any failure.
static const hldb::Operation *getCycleDelayOp(const hldb::Module *m) {
  const hldb::SequenceDecl *s9 = getSeqDecl(m, "seq9");
  if (!s9) return nullptr;
  const hldb::Operation *fm = s9->getExpr<hldb::Operation>();
  if (!fm || !fm->getOperands() || fm->getOperands()->empty()) return nullptr;
  return any_cast<hldb::Operation>((*fm->getOperands())[0]);
}

// ===========================================================================
// Compiler diagnostics
// ===========================================================================

// ss.16.9.8 + ss.16.14: the SV file is syntactically and semantically valid
// -- no errors expected.  If this test fails, Surelog emits EL0535 ("Illegal
// implicit net") for 'seq9' in the assert property statement, meaning it
// misidentifies the sequence name as an undeclared net instead of resolving
// it to the SequenceDecl node.
TEST_F(Sequence9Test, Compiler_NoErrors) {
  ErrorContainer::Stats stats =
      m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0)
      << "ss.16.14: assert property(@(posedge clk) seq9) must not produce "
         "errors -- EL0535 'Illegal implicit net' means Surelog does not "
         "resolve sequence names to SequenceDecl nodes";
}

TEST_F(Sequence9Test, Compiler_NoSyntaxErrors) {
  ErrorContainer::Stats stats = m_compiler->getErrorStats();
  EXPECT_EQ(stats.nbSyntax, 0)
      << "sequence9.sv is syntactically valid -- no syntax errors expected";
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(Sequence9Test, ModuleExists) {
  ASSERT_NE(getTb(m_design), nullptr) << "module 'work@tb' not found";
}

// ===========================================================================
// Sequence declaration (ss.16.8 / ss.16.9.8)
// ===========================================================================

// ss.16.8: exactly one sequence is declared in this module.
TEST_F(Sequence9Test, SequenceDeclCount_IsOne) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getSequenceDecls(), nullptr)
      << "module has no sequence declarations";
  EXPECT_EQ(m->getSequenceDecls()->size(), 1u)
      << "ss.16.8: exactly one sequence is declared: seq9";
}

// ss.16.8: 'seq9' must appear in the sequence declaration collection.
TEST_F(Sequence9Test, Seq9_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(getSeqDecl(m, "seq9"), nullptr)
      << "ss.16.8: sequence 'seq9' must be declared";
}

// ===========================================================================
// seq9 body: outer first_match operation (ss.16.9.8)
// ===========================================================================

// ss.16.9.8: seq9 uses first_match -- it must have a non-null body.
TEST_F(Sequence9Test, Seq9_HasExpression) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s9 = getSeqDecl(m, "seq9");
  ASSERT_NE(s9, nullptr) << "seq9 not found";
  EXPECT_NE(s9->getExpr(), nullptr)
      << "ss.16.9.8: seq9 must have a body expression";
}

// ss.16.9.8: first_match(seq_expr) must be represented as an Operation with
// opType vpiFirstMatchOp (73), as defined in sv_vpi_user.h.
TEST_F(Sequence9Test, Seq9_Expr_IsFirstMatchOperation) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s9 = getSeqDecl(m, "seq9");
  ASSERT_NE(s9, nullptr);
  const hldb::Operation *op = s9->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "seq9 body must be an Operation";
  EXPECT_EQ(op->getOpType(), vpiFirstMatchOp)
      << "ss.16.9.8: first_match must have opType vpiFirstMatchOp (73)";
}

// ss.16.9.8: first_match takes a single sequence expression -- the operand
// list must contain exactly one element.
TEST_F(Sequence9Test, Seq9_FirstMatch_HasOneOperand) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s9 = getSeqDecl(m, "seq9");
  ASSERT_NE(s9, nullptr);
  const hldb::Operation *op = s9->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 1u)
      << "ss.16.9.8: first_match takes exactly one sequence expression";
}

// ===========================================================================
// seq9 body: inner cycle-delay operation 'a ##[1:3] b'
// ===========================================================================

// ss.16.9.2: the argument to first_match is 'a ##[1:3] b'.  The range cycle
// delay '##[m:n]' must be represented as an Operation with opType
// vpiCycleDelayOp (71).
TEST_F(Sequence9Test, Seq9_InnerExpr_IsCycleDelayOperation) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getCycleDelayOp(m);
  ASSERT_NE(cd, nullptr)
      << "operand of first_match must be a cycle-delay Operation";
  EXPECT_EQ(cd->getOpType(), vpiCycleDelayOp)
      << "ss.16.9.2: 'a ##[1:3] b' must have opType vpiCycleDelayOp (71)";
}

// ss.16.9.2: '##[1:3]' is a range form -- the cycle-delay Operation must have
// exactly three operands: start-expression, Range, end-expression.
TEST_F(Sequence9Test, Seq9_CycleDelay_HasThreeOperands) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  EXPECT_EQ(cd->getOperands()->size(), 3u)
      << "ss.16.9.2: 'a ##[1:3] b' must produce 3 operands: "
         "start-expr, Range, end-expr";
}

// ss.16.9.2: operand[0] of the cycle delay is the start expression 'a'.
// It must be a RefObj named "a".
TEST_F(Sequence9Test, Seq9_CycleDelay_Operand0_IsRefToA) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 1u);

  const hldb::RefObj *op0 =
      any_cast<hldb::RefObj>((*cd->getOperands())[0]);
  ASSERT_NE(op0, nullptr) << "operand[0] of cycle delay must be a RefObj";
  EXPECT_EQ(op0->getName(), "a")
      << "ss.16.9.2: first operand of 'a ##[1:3] b' must reference signal 'a'";
}

// ss.16.9.2: RefObj for 'a' must resolve to Net name:'a' at compile time.
// Name binding happens at compile time -- no elaboration guard needed.
TEST_F(Sequence9Test, Seq9_CycleDelay_Operand0_ResolvesToNet) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 1u);

  const hldb::RefObj *op0 =
      any_cast<hldb::RefObj>((*cd->getOperands())[0]);
  ASSERT_NE(op0, nullptr);
  EXPECT_NE(op0->getActual<hldb::Net>(), nullptr)
      << "ss.16.9.2: RefObj for 'a' must resolve to Net name:'a' at "
         "compile time";
}

// ss.16.9.2: operand[1] of the cycle delay is the range '[1:3]'.
// It must be a Range node.
TEST_F(Sequence9Test, Seq9_CycleDelay_Operand1_IsRange) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 2u);

  const hldb::Range *r =
      any_cast<hldb::Range>((*cd->getOperands())[1]);
  EXPECT_NE(r, nullptr)
      << "ss.16.9.2: operand[1] of '##[1:3]' must be a Range node";
}

// ss.16.9.2: the left bound of '##[1:3]' is 1.  The Range's left expression
// must be a Constant with decompile value "1".
TEST_F(Sequence9Test, Seq9_CycleDelay_Range_LeftBound_IsOne) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 2u);

  const hldb::Range *r =
      any_cast<hldb::Range>((*cd->getOperands())[1]);
  ASSERT_NE(r, nullptr);
  const hldb::Constant *lc = r->getLeftExpr<hldb::Constant>();
  ASSERT_NE(lc, nullptr) << "left bound of Range must be a Constant";
  EXPECT_EQ(lc->getDecompile(), "1")
      << "ss.16.9.2: left bound of '##[1:3]' must be 1";
}

// ss.16.9.2: the right bound of '##[1:3]' is 3.  The Range's right expression
// must be a Constant with decompile value "3".
TEST_F(Sequence9Test, Seq9_CycleDelay_Range_RightBound_IsThree) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 2u);

  const hldb::Range *r =
      any_cast<hldb::Range>((*cd->getOperands())[1]);
  ASSERT_NE(r, nullptr);
  const hldb::Constant *rc = r->getRightExpr<hldb::Constant>();
  ASSERT_NE(rc, nullptr) << "right bound of Range must be a Constant";
  EXPECT_EQ(rc->getDecompile(), "3")
      << "ss.16.9.2: right bound of '##[1:3]' must be 3";
}

// ss.16.9.2: operand[2] of the cycle delay is the end expression 'b'.
// It must be a RefObj named "b".
TEST_F(Sequence9Test, Seq9_CycleDelay_Operand2_IsRefToB) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 3u);

  const hldb::RefObj *op2 =
      any_cast<hldb::RefObj>((*cd->getOperands())[2]);
  ASSERT_NE(op2, nullptr) << "operand[2] of cycle delay must be a RefObj";
  EXPECT_EQ(op2->getName(), "b")
      << "ss.16.9.2: last operand of 'a ##[1:3] b' must reference signal 'b'";
}

// ss.16.9.2: RefObj for 'b' must resolve to Net name:'b' at compile time.
// Name binding happens at compile time -- no elaboration guard needed.
TEST_F(Sequence9Test, Seq9_CycleDelay_Operand2_ResolvesToNet) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *cd = getCycleDelayOp(m);
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getOperands(), nullptr);
  ASSERT_GE(cd->getOperands()->size(), 3u);

  const hldb::RefObj *op2 =
      any_cast<hldb::RefObj>((*cd->getOperands())[2]);
  ASSERT_NE(op2, nullptr);
  EXPECT_NE(op2->getActual<hldb::Net>(), nullptr)
      << "ss.16.9.2: RefObj for 'b' must resolve to Net name:'b' at "
         "compile time";
}

// ===========================================================================
// Concurrent assertion: 'assert property(@(posedge clk) seq9)'  (ss.16.14)
// ===========================================================================

// ss.16.14: the module must have at least one concurrent assertion.
TEST_F(Sequence9Test, ConcurrentAssertion_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getConcurrentAssertions(), nullptr)
      << "module has no concurrent assertions";
  EXPECT_NE(getFirstAssert(m), nullptr)
      << "ss.16.14: an Assert node must be present";
}

// ss.16.14: the assert must carry an inline PropertySpec.
TEST_F(Sequence9Test, Assert_HasPropertySpec) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getProperty<hldb::PropertySpec>(), nullptr)
      << "ss.16.14: Assert must have an inline PropertySpec";
}

// ss.16.14: '@(posedge clk)' must be represented as the clocking event on
// the PropertySpec.
TEST_F(Sequence9Test, Assert_PropertySpec_HasClockingEvent) {
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

// ss.16.14: the property expression is the reference to 'seq9'.  It must be
// a RefObj named "seq9".
TEST_F(Sequence9Test, Assert_PropertyExpr_ReferencesSeq9) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr) << "property expression must be a RefObj";
  EXPECT_EQ(propExpr->getName(), "seq9")
      << "ss.16.14: property expression must reference 'seq9'";
}

// ss.16.14: the RefObj for 'seq9' in the concurrent assertion must resolve
// (vpiActual) to the SequenceDecl node.  Surelog emits EL0535 for this
// reference, treating 'seq9' as an implicit net instead of resolving it to
// the SequenceDecl.  Same compile-time name resolution bug as sequence4-8.
TEST_F(Sequence9Test, Assert_PropertyExpr_ResolvedToSeq9Decl) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr);
  EXPECT_NE(propExpr->getActual<hldb::SequenceDecl>(), nullptr)
      << "ss.16.14: 'seq9' in assert property must resolve to SequenceDecl -- "
         "Surelog emits EL0535 treating it as an implicit net instead";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
