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

// Spec-based validation of IEEE 1800-2017 ss.16.8 / ss.16.9.2:
// consecutive repetition applied to a sequence instance.
// SV: tests/Sequence/sequence4.sv
//
//   module tb;
//     bit clk; always #5 clk = ~clk;
//     bit a;
//
//     sequence base_seq;
//       a;
//     endsequence
//
//     sequence seq4;
//       base_seq[*3];   // repetition on a sequence instance
//     endsequence
//
//     assert property(@(posedge clk) seq4);
//   endmodule
//
// -- ss.16.8 / ss.16.9.2 rules under test ----
//
// Sequence instance as repetition operand (ss.16.9.2):
//   * 'base_seq[*3]' applies consecutive repetition to a sequence instance.
//   * This is distinct from 'a[*n]' where the operand is a signal/expression.
//   * The operand of the repetition must be resolved to the SequenceDecl node
//     for 'base_seq', not treated as a plain net or wire.
//   * The compiler must not emit EL0535 ("Illegal implicit net") for
//     sequence instance references -- that error means the compiler is
//     misidentifying a sequence name as an undeclared net.
//
// base_seq body (ss.16.8):
//   * 'sequence base_seq; a; endsequence' declares a named sequence.
//   * The body expression is a RefObj referencing the bit signal 'a'.
//   * The RefObj must resolve (vpiActual) to the Variable node for 'a'.
//
// Concurrent assert property (ss.16.14):
//   * 'assert property(@(posedge clk) seq4)' is a concurrent assertion.
//   * The property expression 'seq4' must resolve to the SequenceDecl,
//     not be treated as an implicit net.
//
// -- Expected HLDB tree (if compiler is correct) ----
//
//   Module name:tb
//   +-- getSequenceDecls() (2 items)
//   |   +-- SequenceDecl name:"base_seq"
//   |   |     vpiExpr: RefObj name:"a" -> Variable name:"a"
//   |   +-- SequenceDecl name:"seq4"
//   |         vpiExpr: Operation opType:consecutive_repeat (77)
//   |           operand[0]: RefObj name:"base_seq" -> SequenceDecl name:"base_seq"
//   |           operand[1]: Constant { decompile:"3" }
//   +-- getConcurrentAssertions() (1 item)
//       +-- Assert
//             PropertySpec
//               clocking: Operation posedge(clk)
//               propertyExpr: RefObj name:"seq4" -> SequenceDecl name:"seq4"

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
#include <hldb/ref_obj.h>
#include <hldb/sequence_decl.h>
#include <hldb/vpi_user.h>

namespace hlc {

class Sequence4Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sequence4.hlc"}); }
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

// ss.16.8 + ss.16.9.2: both sequence declarations and the assert property
// reference are syntactically and semantically valid -- no errors expected.
// A compiler that fails to resolve sequence instance names to their
// SequenceDecl might misidentify them as undeclared implicit nets instead,
// which would surface as a spurious error here.
TEST_F(Sequence4Test, Compiler_NoErrors) {
  ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0) << "ss.16.8: sequence instance references must not produce errors -- "
                                 "sequence names must resolve to SequenceDecl nodes, not be treated "
                                 "as implicit nets";
}

TEST_F(Sequence4Test, Compiler_NoSyntaxErrors) {
  ErrorContainer::Stats stats = m_compiler->getErrorStats();
  EXPECT_EQ(stats.nbSyntax, 0) << "sequence4.sv is syntactically valid -- no syntax errors expected";
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(Sequence4Test, ModuleExists) { ASSERT_NE(getTb(m_design), nullptr) << "module 'tb' not found"; }

// ===========================================================================
// Sequence declaration collection (ss.16.8)
// ===========================================================================

// ss.16.8: two named sequences are declared in this module.
TEST_F(Sequence4Test, SequenceDeclCount_IsTwo) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getSequenceDecls(), nullptr) << "module has no sequence declarations";
  EXPECT_EQ(m->getSequenceDecls()->size(), 2u) << "ss.16.8: two sequences are declared: base_seq and seq4";
}

// ss.16.8: 'base_seq' must appear in the sequence declaration collection.
TEST_F(Sequence4Test, BaseSeq_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(getSeqDecl(m, "base_seq"), nullptr) << "ss.16.8: sequence 'base_seq' must be declared";
}

// ss.16.8: 'seq4' must appear in the sequence declaration collection.
TEST_F(Sequence4Test, Seq4_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(getSeqDecl(m, "seq4"), nullptr) << "ss.16.8: sequence 'seq4' must be declared";
}

// ===========================================================================
// base_seq body: 'a'  (ss.16.8)
// ===========================================================================

// ss.16.8: 'sequence base_seq; a; endsequence' -- the body is a reference
// to signal 'a'. The sequence must have a non-null expression node.
TEST_F(Sequence4Test, BaseSeq_HasExpression) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *bs = getSeqDecl(m, "base_seq");
  ASSERT_NE(bs, nullptr) << "base_seq not found";
  EXPECT_NE(bs->getExpr(), nullptr) << "ss.16.8: base_seq must have a body expression";
}

// ss.16.8: the body of base_seq is a reference to signal 'a', which must
// produce a RefObj named "a".
TEST_F(Sequence4Test, BaseSeq_ExprIsRefToA) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *bs = getSeqDecl(m, "base_seq");
  ASSERT_NE(bs, nullptr);
  const hldb::RefObj *ref = bs->getExpr<hldb::RefObj>();
  ASSERT_NE(ref, nullptr) << "base_seq body must be a RefObj";
  EXPECT_EQ(ref->getName(), "a") << "ss.16.8: base_seq body must reference signal 'a'";
}

// ss.16.8: the RefObj for 'a' in base_seq must resolve to the Variable node.
// This is name binding done at compile time (not elaboration).
TEST_F(Sequence4Test, BaseSeq_Expr_A_ResolvesToNet) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *bs = getSeqDecl(m, "base_seq");
  ASSERT_NE(bs, nullptr);
  const hldb::RefObj *ref = bs->getExpr<hldb::RefObj>();
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::Variable>(), nullptr) << "ss.16.8: RefObj for 'a' in base_seq must resolve to Variable name:'a'";
}

// ===========================================================================
// seq4 body: 'base_seq[*3]'  (ss.16.9.2)
// ===========================================================================

// ss.16.9.2: seq4 uses consecutive repetition -- it must have an expression.
TEST_F(Sequence4Test, Seq4_HasExpression) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s4 = getSeqDecl(m, "seq4");
  ASSERT_NE(s4, nullptr) << "seq4 not found";
  EXPECT_NE(s4->getExpr(), nullptr) << "ss.16.9.2: seq4 must have a body expression (base_seq[*3])";
}

// ss.16.9.2: 'base_seq[*3]' is a consecutive repetition -- the expression
// must be an Operation with opType consecutive_repeat (77).
TEST_F(Sequence4Test, Seq4_ExprIsConsecutiveRepeat) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s4 = getSeqDecl(m, "seq4");
  ASSERT_NE(s4, nullptr);
  const hldb::Operation *op = s4->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "seq4 body must be an Operation";
  EXPECT_EQ(op->getOpType(), vpiConsecutiveRepeatOp) << "ss.16.9.2: seq4 body must have opType consecutive_repeat";
}

// ss.16.9.2: 'base_seq[*3]' produces two operands: the sequence being
// repeated and the count.
TEST_F(Sequence4Test, Seq4_ExprHasTwoOperands) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s4 = getSeqDecl(m, "seq4");
  ASSERT_NE(s4, nullptr);
  const hldb::Operation *op = s4->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u) << "ss.16.9.2: base_seq[*3] must produce 2 operands: "
                                              "the sequence instance ref and the count";
}

// ss.16.9.2: operand[0] is the sequence being repeated. It must be a RefObj
// named "base_seq" -- a reference to the sequence instance, not a net.
TEST_F(Sequence4Test, Seq4_RepeatOperand_IsRefNamedBaseSeq) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s4 = getSeqDecl(m, "seq4");
  ASSERT_NE(s4, nullptr);
  const hldb::Operation *op = s4->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);

  const hldb::RefObj *op0 = any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(op0, nullptr) << "operand[0] must be a RefObj";
  EXPECT_EQ(op0->getName(), "base_seq") << "ss.16.9.2: operand[0] must reference 'base_seq'";
}

// ss.16.9.2: the RefObj for 'base_seq' in seq4 must resolve to the
// SequenceDecl node, not to a Net or Variable. Name resolution of sequence
// instances happens at compile time (same as resolving a signal reference
// to its Net/Variable node) -- no elaboration is required.
TEST_F(Sequence4Test, Seq4_RepeatOperand_ResolvedToSequenceDecl) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s4 = getSeqDecl(m, "seq4");
  ASSERT_NE(s4, nullptr);
  const hldb::Operation *op = s4->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);

  const hldb::RefObj *op0 = any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(op0, nullptr);
  EXPECT_NE(op0->getActual<hldb::SequenceDecl>(), nullptr)
      << "ss.16.9.2: 'base_seq' in seq4 must resolve to SequenceDecl, not be "
         "treated as an implicit net";
}

// ss.16.9.2: operand[1] is the repetition count. It must be a Constant
// with decompile value "3".
TEST_F(Sequence4Test, Seq4_RepeatCount_IsThree) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s4 = getSeqDecl(m, "seq4");
  ASSERT_NE(s4, nullptr);
  const hldb::Operation *op = s4->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);

  const hldb::Constant *op1 = any_cast<hldb::Constant>((*op->getOperands())[1]);
  ASSERT_NE(op1, nullptr) << "operand[1] must be a Constant";
  EXPECT_EQ(std::string(op1->getDecompile()), "3") << "ss.16.9.2: repetition count must be 3";
}

// ===========================================================================
// Concurrent assertion: 'assert property(@(posedge clk) seq4)'  (ss.16.14)
// ===========================================================================

// ss.16.14: the module must have at least one concurrent assertion.
TEST_F(Sequence4Test, ConcurrentAssertion_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getConcurrentAssertions(), nullptr) << "module has no concurrent assertions";
  EXPECT_NE(getFirstAssert(m), nullptr) << "ss.16.14: an Assert node must be present";
}

// ss.16.14: the assert must carry an inline PropertySpec.
TEST_F(Sequence4Test, Assert_HasPropertySpec) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getProperty<hldb::PropertySpec>(), nullptr) << "ss.16.14: Assert must have an inline PropertySpec";
}

// ss.16.14: '@(posedge clk)' must be represented as the clocking event on
// the PropertySpec.
TEST_F(Sequence4Test, Assert_PropertySpec_HasClockingEvent) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  EXPECT_NE(spec->getClockingEvent(), nullptr) << "ss.16.14: @(posedge clk) must produce a clocking event on the "
                                                  "PropertySpec";
}

// ss.16.14: the property expression is the reference to 'seq4'. It must be
// a RefObj named "seq4".
TEST_F(Sequence4Test, Assert_PropertyExpr_ReferencesSeq4) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr) << "property expression must be a RefObj";
  EXPECT_EQ(propExpr->getName(), "seq4") << "ss.16.14: property expression must reference 'seq4'";
}

// ss.16.14: the RefObj for 'seq4' in the concurrent assertion must resolve
// to the SequenceDecl node, not be treated as an implicit net -- the same
// compile-time resolution as Seq4_RepeatOperand_ResolvedToSequenceDecl above.
TEST_F(Sequence4Test, Assert_PropertyExpr_ResolvedToSeq4Decl) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr);
  EXPECT_NE(propExpr->getActual<hldb::SequenceDecl>(), nullptr)
      << "ss.16.14: 'seq4' in assert property must resolve to SequenceDecl, not be "
         "treated as an implicit net";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
