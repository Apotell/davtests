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

// Spec-based validation of IEEE 1800-2017 ss.16.9.4:
// sequence match items -- (sequence_expr, variable_assignment, subroutine_call).
// SV: tests/Sequence/sequence5.sv
//
//   module tb;
//     bit clk; always #5 clk = ~clk;
//     int x;
//
//     function void f(); x++; endfunction
//
//     sequence seq5;
//       (1, x=0, f()); // match items
//     endsequence
//
//     assert property(@(posedge clk) seq5);
//
//     initial begin
//       #20 $finish;
//     end
//   endmodule
//
// -- ss.16.10 rules under test -------------------------------------------------
//
// Match item form (ss.16.10):
//   * '(1, x=0, f())' is a sequence expression with match items.
//   * The parenthesized form '(sequence_expr, match_item, ...)' triggers the
//     listed actions when the sequence matches.
//   * The expression '1' is the sequence body (always matches at any clock tick).
//   * 'x=0' is a variable_assignment match item -- a blocking assignment.
//   * 'f()' is a subroutine_call match item -- calls the void function f.
//   * The compiler must represent this as a list Operation
//     (vpiOpType = vpiListOp = 37) with three operands:
//     [0] Constant "1", [1] Assignment (x=0), [2] FuncCall (f).
//
// LRM RESTRICTION (ss.16.10):
//   "It shall be illegal to use a variable other than a local variable of
//    the sequence on the left-hand side of an assignment in a sequence match
//    item or in an inc_or_dec_expression in a sequence match item."
//
//   A local sequence variable must be declared with 'var' inside the sequence
//   body.  In sequence5.sv 'x' is a MODULE-LEVEL variable, not a local
//   sequence variable.  Therefore:
//     'x=0' -- ILLEGAL: x is not a local variable of seq5
//     'f()' -- LEGAL: subroutine calls have no such restriction at call site
//
//   A conforming compiler (e.g., Questa) must reject 'x=0'.
//   Surelog silently accepts it -- that is a Surelog bug.
//
// Concurrent assert property (ss.16.14):
//   * 'assert property(@(posedge clk) seq5)' uses seq5 as the property body.
//   * The property expression 'seq5' must resolve (vpiActual) to the
//     SequenceDecl node for seq5 -- not be treated as an implicit net.
//   * Surelog emits EL0535 ("Illegal implicit net") for 'seq5' here, meaning
//     it misidentifies the sequence name as an undeclared net.  This is the
//     same class of bug confirmed in sequence4 -- Surelog does not resolve
//     sequence names to SequenceDecl in any reference context.
//
// -- Expected HLDB tree (if compiler is correct) --------------------------------
//
//   Module name:work@tb
//   +-- getSequenceDecls() (1 item)
//   |   +-- SequenceDecl name:"seq5"
//   |         vpiExpr: Operation opType:list (vpiListOp = 37)
//   |           operand[0]: Constant { decompile:"1" }
//   |           operand[1]: Assignment (blocking)
//   |             vpiLhs: RefObj name:"x" -> Net name:"x"
//   |             vpiRhs: Constant { decompile:"0" }
//   |           operand[2]: FuncCall name:"f"
//   +-- getConcurrentAssertions() (1 item)
//       +-- Assert
//             PropertySpec
//               clocking: Operation posedge(clk)
//               propertyExpr: RefObj name:"seq5" -> SequenceDecl name:"seq5"

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assert_stmt.h>
#include <hldb/assignment.h>
#include <hldb/concurrent_assertions.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/property_spec.h>
#include <hldb/ref_obj.h>
#include <hldb/sequence_decl.h>
#include <hldb/vpi_user.h>

namespace hlc {

class Sequence5Test : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "sequence5.hlc"});

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

// ===========================================================================
// Compiler diagnostics
// ===========================================================================

// ss.16.10: the LHS of any assignment or inc/dec expression in a sequence
// match item must be a local variable of the sequence (declared with 'var'
// inside the sequence body).
//
// This test first reads seq5's local variable declarations via getVariables():
//   - If there are NO local variables, no assignment or inc/dec in the match
//     item list is legal at all -- a conforming compiler must reject them.
//   - If there ARE local variables, any assignment or inc/dec must target only
//     those locals (LHS must not resolve to a Net, which would indicate a
//     module-level variable slipping through).
//
// seq5 declares no 'var' variables, so getVariables() is null/empty.
// Surelog still places 'x=0' (Assignment) into the HLDB -- this test FAILS,
// exposing that Surelog does not enforce the ss.16.10 LRM restriction.
TEST_F(Sequence5Test, Seq5_MatchItems_OnlyModifyLocalVars) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s5 = getSeqDecl(m, "seq5");
  ASSERT_NE(s5, nullptr);

  bool hasLocalVars = (s5->getVariables() != nullptr &&
                       !s5->getVariables()->empty());

  const hldb::Operation *op = s5->getExpr<hldb::Operation>();
  if (!op || !op->getOperands()) return;

  for (uint32_t i = 0; i < op->getOperands()->size(); ++i) {
    const hldb::Any *operand = (*op->getOperands())[i];

    if (const hldb::Assignment *asgn = any_cast<hldb::Assignment>(operand)) {
      if (!hasLocalVars) {
        ADD_FAILURE()
            << "ss.16.10: operand[" << i << "]: Assignment found in the "
               "match item list but seq5 has no local sequence variables -- "
               "Surelog accepted 'x=0' silently; only 'var'-declared local "
               "variables may be assigned in a sequence match item";
      } else {
        const hldb::RefObj *lhs = asgn->getLhs<hldb::RefObj>();
        if (lhs) {
          EXPECT_EQ(lhs->getActual<hldb::Net>(), nullptr)
              << "ss.16.10: operand[" << i << "]: assignment to '"
              << lhs->getName() << "' targets a module-level variable -- "
                 "only local sequence variables may be assigned";
        }
      }
    }
  }
}

TEST_F(Sequence5Test, Compiler_NoSyntaxErrors) {
  ErrorContainer::Stats stats = m_compiler->getErrorStats();
  EXPECT_EQ(stats.nbSyntax, 0)
      << "sequence5.sv is syntactically valid -- no syntax errors expected";
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(Sequence5Test, ModuleExists) {
  ASSERT_NE(getTb(m_design), nullptr) << "module 'work@tb' not found";
}

// ===========================================================================
// Sequence declaration (ss.16.8 / ss.16.9.4)
// ===========================================================================

// ss.16.8: exactly one sequence is declared in this module.
TEST_F(Sequence5Test, SequenceDeclCount_IsOne) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getSequenceDecls(), nullptr)
      << "module has no sequence declarations";
  EXPECT_EQ(m->getSequenceDecls()->size(), 1u)
      << "ss.16.8: exactly one sequence is declared: seq5";
}

// ss.16.8: 'seq5' must appear in the sequence declaration collection.
TEST_F(Sequence5Test, Seq5_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(getSeqDecl(m, "seq5"), nullptr)
      << "ss.16.8: sequence 'seq5' must be declared";
}

// ===========================================================================
// seq5 body: '(1, x=0, f())'  (ss.16.9.4)
// ===========================================================================

// ss.16.9.4: seq5 uses the match item form -- it must have a non-null body.
TEST_F(Sequence5Test, Seq5_HasExpression) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s5 = getSeqDecl(m, "seq5");
  ASSERT_NE(s5, nullptr) << "seq5 not found";
  EXPECT_NE(s5->getExpr(), nullptr)
      << "ss.16.9.4: seq5 must have a body expression";
}

// ss.16.9.4: '(1, x=0, f())' is a parenthesized sequence_expr with match
// items.  Surelog represents this as a list Operation (vpiListOp = 37).
TEST_F(Sequence5Test, Seq5_Expr_IsMatchItemList) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s5 = getSeqDecl(m, "seq5");
  ASSERT_NE(s5, nullptr);
  const hldb::Operation *op = s5->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "seq5 body must be an Operation";
  EXPECT_EQ(op->getOpType(), vpiListOp)
      << "ss.16.9.4: match item form must have opType list (vpiListOp = 37)";
}

// ss.16.9.4: '(1, x=0, f())' has one sequence expression and two match items,
// giving three operands total:
//   [0] Constant(1), [1] Assignment(x=0), [2] FuncCall(f).
TEST_F(Sequence5Test, Seq5_Expr_HasThreeOperands) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s5 = getSeqDecl(m, "seq5");
  ASSERT_NE(s5, nullptr);
  const hldb::Operation *op = s5->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u)
      << "ss.16.9.4: (1, x=0, f()) must produce 3 operands: "
         "sequence_expr + 2 match items";
}

// ss.16.9.4: operand[0] is the sequence expression '1'.  It must be a
// Constant with decompile value "1".
TEST_F(Sequence5Test, Seq5_MatchExpr_IsConstantOne) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s5 = getSeqDecl(m, "seq5");
  ASSERT_NE(s5, nullptr);
  const hldb::Operation *op = s5->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);

  const hldb::Constant *c0 =
      any_cast<hldb::Constant>((*op->getOperands())[0]);
  ASSERT_NE(c0, nullptr) << "operand[0] must be a Constant";
  EXPECT_EQ(std::string(c0->getDecompile()), "1")
      << "ss.16.9.4: sequence expression '1' must be Constant with value 1";
}


// ss.16.9.4: operand[2] is the subroutine call match item 'f()'.  It must be
// a FuncCall node (ss.16.9.4: subroutine_call).
TEST_F(Sequence5Test, Seq5_MatchItem_FuncCall_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s5 = getSeqDecl(m, "seq5");
  ASSERT_NE(s5, nullptr);
  const hldb::Operation *op = s5->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 3u);

  EXPECT_NE(any_cast<hldb::FuncCall>((*op->getOperands())[2]), nullptr)
      << "ss.16.9.4: subroutine call match item 'f()' must be a FuncCall";
}

// ss.16.9.4: the FuncCall for 'f()' must carry the function name "f".
TEST_F(Sequence5Test, Seq5_MatchItem_FuncCall_NameIsF) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s5 = getSeqDecl(m, "seq5");
  ASSERT_NE(s5, nullptr);
  const hldb::Operation *op = s5->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 3u);

  const hldb::FuncCall *fc =
      any_cast<hldb::FuncCall>((*op->getOperands())[2]);
  ASSERT_NE(fc, nullptr);
  EXPECT_EQ(fc->getName(), "f")
      << "ss.16.9.4: subroutine call match item must name function 'f'";
}

// ss.16.9.4: the FuncCall for 'f()' must resolve (getTaskFunc) to the
// Function declaration node for 'f'.  This is compile-time name binding --
// 'f' is declared in the same module scope, so no elaboration is needed to
// find it.  If this test fails, Surelog does not link subroutine calls in
// sequence match items to their declarations at compile time.
TEST_F(Sequence5Test, Seq5_MatchItem_FuncCall_ResolvesToFunction) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s5 = getSeqDecl(m, "seq5");
  ASSERT_NE(s5, nullptr);
  const hldb::Operation *op = s5->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 3u);

  const hldb::FuncCall *fc =
      any_cast<hldb::FuncCall>((*op->getOperands())[2]);
  ASSERT_NE(fc, nullptr);
  EXPECT_NE(fc->getTaskFunc<hldb::Function>(), nullptr)
      << "ss.16.9.4: FuncCall for 'f()' in the match item must resolve to "
         "the Function declaration -- 'f' is in the same module scope so "
         "this is compile-time name binding, same as RefObj->Net";
}

// ===========================================================================
// Concurrent assertion: 'assert property(@(posedge clk) seq5)'  (ss.16.14)
// ===========================================================================

// ss.16.14: the module must have at least one concurrent assertion.
TEST_F(Sequence5Test, ConcurrentAssertion_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getConcurrentAssertions(), nullptr)
      << "module has no concurrent assertions";
  EXPECT_NE(getFirstAssert(m), nullptr)
      << "ss.16.14: an Assert node must be present";
}

// ss.16.14: the assert must carry an inline PropertySpec.
TEST_F(Sequence5Test, Assert_HasPropertySpec) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getProperty<hldb::PropertySpec>(), nullptr)
      << "ss.16.14: Assert must have an inline PropertySpec";
}

// ss.16.14: '@(posedge clk)' must be represented as the clocking event on
// the PropertySpec.
TEST_F(Sequence5Test, Assert_PropertySpec_HasClockingEvent) {
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

// ss.16.14: the property expression is the reference to 'seq5'.  It must be
// a RefObj named "seq5".
TEST_F(Sequence5Test, Assert_PropertyExpr_ReferencesSeq5) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr) << "property expression must be a RefObj";
  EXPECT_EQ(propExpr->getName(), "seq5")
      << "ss.16.14: property expression must reference 'seq5'";
}

// ss.16.14: the RefObj for 'seq5' in the concurrent assertion must resolve
// (vpiActual) to the SequenceDecl node.  Surelog emits EL0535 for this
// reference, treating 'seq5' as an implicit net instead of a sequence name.
// NOTE: same class of bug as confirmed in sequence4 -- Surelog does not
// resolve sequence names to SequenceDecl in any reference context.
TEST_F(Sequence5Test, Assert_PropertyExpr_ResolvedToSeq5Decl) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr);
  EXPECT_NE(propExpr->getActual<hldb::SequenceDecl>(), nullptr)
      << "ss.16.14: 'seq5' in assert property must resolve to SequenceDecl -- "
         "Surelog emits EL0535 treating it as an implicit net instead";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
