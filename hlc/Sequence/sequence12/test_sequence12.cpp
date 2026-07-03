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

// Spec-based validation of IEEE 1800-2017 ss.16.6:
// clocked sequence expression -- a sequence declaration that embeds its own
// clocking event via '@(posedge clk) sequence_expr' syntax.
// SV: tests/Sequence/sequence12.sv
//
//   module tb;
//     bit clk; always #5 clk = ~clk;
//     bit a;
//
//     sequence seq12;
//       @(posedge clk) a;
//     endsequence
//
//     assert property(seq12);
//
//     initial begin
//       #10 a=1;
//       #30 $finish;
//     end
//   endmodule
//
// -- ss.16.6 rules under test --------------------------------------------------
//
// Clocked sequence expression (ss.16.6):
//   * '@(event) sequence_expr' embeds a clocking event directly inside the
//     sequence body.  The HLDB represents this as a ClockedSeq node, which is
//     distinct from the Operation nodes used by other sequence operators.
//   * ClockedSeq carries:
//       getClockingEvent(): the clocking event -- an Operation with the edge
//                           type as opType.  Here opType is posedge (39) and
//                           the single operand is RefObj("clk") -> Net("clk").
//       getSequenceExpr(): the sequence body -- here RefObj("a") -> Net("a").
//   * Both name bindings (clk to Net, a to Net) occur at compile time.
//
// Concurrent assert property without external clocking event (ss.16.14):
//   * 'assert property(seq12)' has no '@(clk)' in the assertion itself because
//     the clocking event is already embedded in seq12.
//   * Consequently, the PropertySpec has no clocking event node (getClockingEvent()
//     returns null on the PropertySpec).
//   * The property expression 'seq12' must resolve (vpiActual) to the
//     SequenceDecl for seq12.  Surelog emits EL0535 ("Illegal implicit net")
//     for 'seq12' -- the same compile-time name resolution bug confirmed in
//     sequence4 through 11.
//
// -- Expected HLDB tree (if compiler is correct) --------------------------------
//
//   Module name:work@tb
//   +-- getSequenceDecls() (1 item)
//   |   +-- SequenceDecl name:"seq12"
//   |         vpiExpr: ClockedSeq
//   |           getClockingEvent(): Operation opType:posedge(39)
//   |             operand[0]: RefObj name:"clk" -> Net name:"clk"
//   |           getSequenceExpr(): RefObj name:"a" -> Net name:"a"
//   +-- getConcurrentAssertions() (1 item)
//       +-- Assert
//             PropertySpec  (no clocking event -- clock is inside seq12)
//               propertyExpr: RefObj name:"seq12" -> SequenceDecl name:"seq12"

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assert_stmt.h>
#include <hldb/clocked_seq.h>
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

class Sequence12Test : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "sequence12.hlc"});

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

// Returns the ClockedSeq node that is the body of seq12, or nullptr.
static const hldb::ClockedSeq *getClockedSeq(const hldb::Module *m) {
  const hldb::SequenceDecl *s12 = getSeqDecl(m, "seq12");
  if (!s12) return nullptr;
  return s12->getExpr<hldb::ClockedSeq>();
}

// ===========================================================================
// Compiler diagnostics
// ===========================================================================

// ss.16.6 + ss.16.14: the SV file is syntactically and semantically valid --
// no errors expected.  If this test fails, Surelog emits EL0535 ("Illegal
// implicit net") for 'seq12' in the assert property statement, meaning it
// misidentifies the sequence name as an undeclared net instead of resolving
// it to the SequenceDecl node.
TEST_F(Sequence12Test, Compiler_NoErrors) {
  ErrorContainer::Stats stats =
      m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0)
      << "ss.16.14: assert property(seq12) must not produce errors -- "
         "EL0535 'Illegal implicit net' means Surelog does not resolve "
         "sequence names to SequenceDecl nodes";
}

TEST_F(Sequence12Test, Compiler_NoSyntaxErrors) {
  ErrorContainer::Stats stats = m_compiler->getErrorStats();
  EXPECT_EQ(stats.nbSyntax, 0)
      << "sequence12.sv is syntactically valid -- no syntax errors expected";
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(Sequence12Test, ModuleExists) {
  ASSERT_NE(getTb(m_design), nullptr) << "module 'work@tb' not found";
}

// ===========================================================================
// Sequence declaration (ss.16.5 / ss.16.6)
// ===========================================================================

// ss.16.5: exactly one sequence is declared in this module.
TEST_F(Sequence12Test, SequenceDeclCount_IsOne) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getSequenceDecls(), nullptr)
      << "module has no sequence declarations";
  EXPECT_EQ(m->getSequenceDecls()->size(), 1u)
      << "ss.16.5: exactly one sequence is declared: seq12";
}

// ss.16.5: 'seq12' must appear in the sequence declaration collection.
TEST_F(Sequence12Test, Seq12_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(getSeqDecl(m, "seq12"), nullptr)
      << "ss.16.5: sequence 'seq12' must be declared";
}

// ===========================================================================
// seq12 body: ClockedSeq node (ss.16.6)
// ===========================================================================

// ss.16.6: seq12 has a body expression.
TEST_F(Sequence12Test, Seq12_HasExpression) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s12 = getSeqDecl(m, "seq12");
  ASSERT_NE(s12, nullptr) << "seq12 not found";
  EXPECT_NE(s12->getExpr(), nullptr)
      << "ss.16.6: seq12 must have a body expression";
}

// ss.16.6: '@(posedge clk) a' inside a sequence declaration is represented as
// a ClockedSeq node, not an Operation.  This distinguishes it from all
// sequence operators (and, or, throughout, within, etc.) which use Operation.
TEST_F(Sequence12Test, Seq12_Expr_IsClockedSeq) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s12 = getSeqDecl(m, "seq12");
  ASSERT_NE(s12, nullptr);
  EXPECT_NE(s12->getExpr<hldb::ClockedSeq>(), nullptr)
      << "ss.16.6: '@(posedge clk) a' in a sequence body must produce a "
         "ClockedSeq node, not an Operation";
}

// ===========================================================================
// ClockedSeq: embedded clocking event (ss.16.6)
// ===========================================================================

// ss.16.6: the ClockedSeq must carry a non-null clocking event.
TEST_F(Sequence12Test, Seq12_ClockedSeq_HasClockingEvent) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::ClockedSeq *cs = getClockedSeq(m);
  ASSERT_NE(cs, nullptr) << "seq12 body must be a ClockedSeq";
  EXPECT_NE(cs->getClockingEvent(), nullptr)
      << "ss.16.6: '@(posedge clk) a' must embed a clocking event in the "
         "ClockedSeq node";
}

// ss.16.6: the clocking event must be an Operation (the posedge edge applied
// to clk is encoded as an Operation node with a single operand).
TEST_F(Sequence12Test, Seq12_ClockedSeq_ClockingEvent_IsOperation) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::ClockedSeq *cs = getClockedSeq(m);
  ASSERT_NE(cs, nullptr);
  EXPECT_NE(cs->getClockingEvent<hldb::Operation>(), nullptr)
      << "ss.16.6: the clocking event of a ClockedSeq must be an Operation";
}

// ss.16.6: the clocking event Operation must have exactly one operand -- the
// signal 'clk' on which the posedge is detected.
TEST_F(Sequence12Test, Seq12_ClockedSeq_ClockingEvent_HasOneOperand) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::ClockedSeq *cs = getClockedSeq(m);
  ASSERT_NE(cs, nullptr);
  const hldb::Operation *op = cs->getClockingEvent<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 1u)
      << "ss.16.6: posedge clocking event must have exactly one operand (clk)";
}

// ss.16.6: the operand of the posedge clocking event is a RefObj named "clk".
TEST_F(Sequence12Test, Seq12_ClockedSeq_ClockingEvent_OperandIsRefToClk) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::ClockedSeq *cs = getClockedSeq(m);
  ASSERT_NE(cs, nullptr);
  const hldb::Operation *op = cs->getClockingEvent<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);

  const hldb::RefObj *clkRef =
      any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(clkRef, nullptr) << "clocking event operand must be a RefObj";
  EXPECT_EQ(clkRef->getName(), "clk")
      << "ss.16.6: clocking event operand must reference signal 'clk'";
}

// ss.16.6: RefObj for 'clk' in the clocking event must resolve to Net name:
// 'clk' at compile time.
TEST_F(Sequence12Test, Seq12_ClockedSeq_ClockingEvent_ClkResolvesToNet) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::ClockedSeq *cs = getClockedSeq(m);
  ASSERT_NE(cs, nullptr);
  const hldb::Operation *op = cs->getClockingEvent<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);

  const hldb::RefObj *clkRef =
      any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(clkRef, nullptr);
  EXPECT_NE(clkRef->getActual<hldb::Net>(), nullptr)
      << "ss.16.6: RefObj for 'clk' in the clocking event must resolve to "
         "Net name:'clk' at compile time";
}

// ===========================================================================
// ClockedSeq: sequence body expression (ss.16.6)
// ===========================================================================

// ss.16.6: the ClockedSeq must carry a non-null sequence body expression.
TEST_F(Sequence12Test, Seq12_ClockedSeq_HasSequenceExpr) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::ClockedSeq *cs = getClockedSeq(m);
  ASSERT_NE(cs, nullptr);
  EXPECT_NE(cs->getSequenceExpr(), nullptr)
      << "ss.16.6: ClockedSeq must have a sequence body expression";
}

// ss.16.6: the sequence body expression is 'a' -- a RefObj named "a".
TEST_F(Sequence12Test, Seq12_ClockedSeq_SequenceExpr_IsRefToA) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::ClockedSeq *cs = getClockedSeq(m);
  ASSERT_NE(cs, nullptr);
  const hldb::RefObj *aRef = cs->getSequenceExpr<hldb::RefObj>();
  ASSERT_NE(aRef, nullptr)
      << "ss.16.6: ClockedSeq sequence body must be a RefObj";
  EXPECT_EQ(aRef->getName(), "a")
      << "ss.16.6: sequence body of '@(posedge clk) a' must reference "
         "signal 'a'";
}

// ss.16.6: RefObj for 'a' in the sequence body must resolve to Net name:'a'
// at compile time.
TEST_F(Sequence12Test, Seq12_ClockedSeq_SequenceExpr_ResolvesToNet) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::ClockedSeq *cs = getClockedSeq(m);
  ASSERT_NE(cs, nullptr);
  const hldb::RefObj *aRef = cs->getSequenceExpr<hldb::RefObj>();
  ASSERT_NE(aRef, nullptr);
  EXPECT_NE(aRef->getActual<hldb::Net>(), nullptr)
      << "ss.16.6: RefObj for 'a' in the ClockedSeq body must resolve to "
         "Net name:'a' at compile time";
}

// ===========================================================================
// Concurrent assertion: 'assert property(seq12)'  (ss.16.14)
// ===========================================================================

// ss.16.14: the module must have at least one concurrent assertion.
TEST_F(Sequence12Test, ConcurrentAssertion_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getConcurrentAssertions(), nullptr)
      << "module has no concurrent assertions";
  EXPECT_NE(getFirstAssert(m), nullptr)
      << "ss.16.14: an Assert node must be present";
}

// ss.16.14: the assert must carry an inline PropertySpec.
TEST_F(Sequence12Test, Assert_HasPropertySpec) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getProperty<hldb::PropertySpec>(), nullptr)
      << "ss.16.14: Assert must have an inline PropertySpec";
}

// ss.16.6 + ss.16.14: 'assert property(seq12)' has no explicit clocking event
// because the clock is embedded inside seq12's ClockedSeq body.  The
// PropertySpec must therefore have a null clocking event.
TEST_F(Sequence12Test, Assert_PropertySpec_HasNoClockingEvent) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  EXPECT_EQ(spec->getClockingEvent(), nullptr)
      << "ss.16.6: when the sequence carries its own embedded clock, "
         "the PropertySpec of 'assert property(seq12)' must have no "
         "external clocking event";
}

// ss.16.14: the property expression is the reference to 'seq12'.  It must be
// a RefObj named "seq12".
TEST_F(Sequence12Test, Assert_PropertyExpr_ReferencesSeq12) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr) << "property expression must be a RefObj";
  EXPECT_EQ(propExpr->getName(), "seq12")
      << "ss.16.14: property expression must reference 'seq12'";
}

// ss.16.14: the RefObj for 'seq12' in the concurrent assertion must resolve
// (vpiActual) to the SequenceDecl node.  Surelog emits EL0535 for this
// reference, treating 'seq12' as an implicit net instead of resolving it to
// the SequenceDecl.  Same compile-time name resolution bug as sequence4-11.
TEST_F(Sequence12Test, Assert_PropertyExpr_ResolvedToSeq12Decl) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr);
  EXPECT_NE(propExpr->getActual<hldb::SequenceDecl>(), nullptr)
      << "ss.16.14: 'seq12' in assert property must resolve to SequenceDecl -- "
         "Surelog emits EL0535 treating it as an implicit net instead";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
