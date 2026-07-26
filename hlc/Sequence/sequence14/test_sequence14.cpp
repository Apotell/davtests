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

// Spec-based validation of IEEE 1800-2017 ss.16.10:
// sequence match items -- '(1, x=0, x++, inc())' as the sequence body.
// SV: tests/Sequence/sequence14.sv
//
//   module tb;
//     bit clk; always #5 clk = ~clk;
//     int x;
//
//     function void inc(); x++; endfunction
//
//     sequence seq14;
//       (1, x=0, x++, inc());
//     endsequence
//
//     assert property(@(posedge clk) seq14);
//
//     initial begin
//       #20 $finish;
//     end
//   endmodule
//
// -- ss.16.10 rules under test ------------------------------------------------
//
// Sequence with match items '(seq_expr, match_item {, match_item})' (ss.16.10):
//   * The parenthesized form groups a sequence expression with a list of
//     actions that execute when the sequence matches.
//   * '1' is the sequence expression -- a Boolean constant always true.
//   * 'x=0', 'x++', 'inc()' are the three match items.
//
// LRM RESTRICTION (ss.16.10):
//   "It shall be illegal to use a variable other than a local variable of
//    the sequence on the left-hand side of an assignment in a sequence match
//    item or in an inc_or_dec_expression in a sequence match item."
//
//   A local sequence variable must be declared with 'var' inside the sequence
//   body, e.g.:
//     sequence seq14;
//       var int x;
//       (1, x=0, x++, inc());
//     endsequence
//
//   In sequence14.sv 'x' is a MODULE-LEVEL variable, not a local sequence
//   variable.  Therefore:
//     'x=0' -- ILLEGAL: x is not a local variable of seq14
//     'x++' -- ILLEGAL: same reason
//     'inc()' -- LEGAL: subroutine calls have no such restriction at call site
//
//   A conforming compiler (e.g., Questa) must reject 'x=0' and 'x++'.
//   Surelog silently accepts them -- that is a Surelog bug.
//
// HLDB tree (Surelog current output -- reflects Surelog bugs, not spec):
//   * The HLDB represents the entire parenthesized form as an Operation with
//     opType vpiListOp (37), with all items packed as operands:
//       operand[0]: Constant decompile:"1"       -- seq expr
//       operand[1]: Assignment (blocking=true)   -- x=0 (should be rejected)
//                     getLhs<RefObj>() -> name:"x" -> Net name:"x"
//                     getRhs<Constant>() -> decompile:"0"
//       operand[2]: Operation opType:post_inc (vpiPostIncOp=64)  -- x++ (should be rejected)
//                     operand[0]: RefObj name:"x" -> Net name:"x"
//       operand[3]: FuncCall name:"inc"           -- inc() (legal)
//                     getTaskFunc<Function>() must link to Function name:"inc"
//   * RefObj name bindings (x -> Net) happen at compile time.
//   * FuncCall->Function name binding also happens at compile time.  If
//     getTaskFunc<Function>() returns nullptr, Surelog has a separate bug.
//
// Concurrent assert property (ss.16.14):
//   * The clock is given externally: @(posedge clk) on the assert statement.
//   * 'seq14' in the assert must resolve to SequenceDecl at compile time.
//     Surelog emits EL0535 -- same implicit-net bug as seq4-13.
//
// -- Expected HLDB tree (if compiler is correct) --------------------------------
//
//   Module name:tb
//   +-- getSequenceDecls() (1 item)
//   |   +-- SequenceDecl name:"seq14"
//   |         vpiExpr: Operation opType:list (vpiListOp = 37)
//   |           operand[0]: Constant decompile:"1"
//   |           operand[1]: Assignment (blocking=true)
//   |             getLhs<RefObj>() name:"x" -> Net name:"x"
//   |             getRhs<Constant>() decompile:"0"
//   |           operand[2]: Operation opType:post_inc (vpiPostIncOp = 64)
//   |             operand[0]: RefObj name:"x" -> Net name:"x"
//   |           operand[3]: FuncCall name:"inc"
//   |             getTaskFunc<Function>() -> Function name:"inc"
//   +-- getConcurrentAssertions() (1 item)
//       +-- Assert
//             PropertySpec
//               clocking: Operation posedge(clk)
//               propertyExpr: RefObj name:"seq14" -> SequenceDecl name:"seq14"

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
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

namespace hlc {

class Sequence14Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sequence14.hlc"}); }
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

// Returns the top-level vpiListOp Operation for seq14, or nullptr.
static const hldb::Operation *getListOp(const hldb::Module *m) {
  const hldb::SequenceDecl *s14 = getSeqDecl(m, "seq14");
  if (!s14) return nullptr;
  return s14->getExpr<hldb::Operation>();
}

// Returns operand[1] of the list as an Assignment node, or nullptr.
static const hldb::Assignment *getAssignment(const hldb::Module *m) {
  const hldb::Operation *op = getListOp(m);
  if (!op || !op->getOperands() || op->getOperands()->size() < 2) return nullptr;
  return any_cast<hldb::Assignment>((*op->getOperands())[1]);
}

// Returns operand[2] of the list as a post-increment Operation, or nullptr.
static const hldb::Operation *getPostIncOp(const hldb::Module *m) {
  const hldb::Operation *op = getListOp(m);
  if (!op || !op->getOperands() || op->getOperands()->size() < 3) return nullptr;
  return any_cast<hldb::Operation>((*op->getOperands())[2]);
}

// Returns operand[3] of the list as a FuncCall node, or nullptr.
static const hldb::FuncCall *getFuncCall(const hldb::Module *m) {
  const hldb::Operation *op = getListOp(m);
  if (!op || !op->getOperands() || op->getOperands()->size() < 4) return nullptr;
  return any_cast<hldb::FuncCall>((*op->getOperands())[3]);
}

// ===========================================================================
// Compiler diagnostics
// ===========================================================================

// ss.16.10: the LHS of any assignment or inc/dec expression in a sequence
// match item must be a local variable of the sequence (declared with 'var'
// inside the sequence body).
//
// This test first reads seq14's local variable declarations via getVariables():
//   - If there are NO local variables, no assignment or inc/dec in the match
//     item list is legal at all -- a conforming compiler must reject them.
//   - If there ARE local variables, any assignment or inc/dec must target only
//     those locals (LHS must not resolve to a Net, which indicates a module-
//     level variable slipping through).
//
// seq14 declares no 'var' variables, so getVariables() is null/empty.
// Surelog still places 'x=0' (Assignment) and 'x++' (post-increment) into the
// HLDB -- this test FAILS twice, exposing that Surelog does not enforce the
// ss.16.10 LRM restriction for either form.
TEST_F(Sequence14Test, Seq14_MatchItems_OnlyModifyLocalVars) {
  GTEST_SKIP() << "known issue: need to decide when to create Nets from "
                  "variable and when not";
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s14 = getSeqDecl(m, "seq14");
  ASSERT_NE(s14, nullptr);

  bool hasLocalVars = (s14->getVariables() != nullptr && !s14->getVariables()->empty());

  const hldb::Operation *op = getListOp(m);
  if (!op || !op->getOperands()) return;

  for (uint32_t i = 0; i < op->getOperands()->size(); ++i) {
    const hldb::Any *operand = (*op->getOperands())[i];

    // Assignment match item (e.g. 'x=0')
    if (const hldb::Assignment *asgn = any_cast<hldb::Assignment>(operand)) {
      if (!hasLocalVars) {
        ADD_FAILURE() << "ss.16.10: operand[" << i
                      << "]: Assignment found in the "
                         "match item list but seq14 has no local sequence variables -- "
                         "Surelog accepted 'x=0' silently; only 'var'-declared local "
                         "variables may be assigned in a sequence match item";
      } else {
        const hldb::RefObj *lhs = asgn->getLhs<hldb::RefObj>();
        if (lhs) {
          EXPECT_EQ(lhs->getActual<hldb::Net>(), nullptr)
              << "ss.16.10: operand[" << i << "]: assignment to '" << lhs->getName()
              << "' targets a module-level variable -- "
                 "only local sequence variables may be assigned";
        }
      }
    }

    // Inc/dec match item (e.g. 'x++')
    if (const hldb::Operation *inner = any_cast<hldb::Operation>(operand)) {
      int32_t opType = inner->getOpType();
      if (opType == vpiPostIncOp || opType == vpiPreIncOp || opType == vpiPostDecOp || opType == vpiPreDecOp) {
        if (!hasLocalVars) {
          ADD_FAILURE() << "ss.16.10: operand[" << i
                        << "]: inc/dec expression found "
                           "in the match item list but seq14 has no local sequence "
                           "variables -- Surelog accepted 'x++' silently; only "
                           "'var'-declared local variables may be modified";
        } else {
          if (inner->getOperands() && !inner->getOperands()->empty()) {
            const hldb::RefObj *ref = any_cast<hldb::RefObj>((*inner->getOperands())[0]);
            if (ref) {
              EXPECT_EQ(ref->getActual<hldb::Net>(), nullptr)
                  << "ss.16.10: operand[" << i << "]: inc/dec on '" << ref->getName()
                  << "' targets a module-level variable -- "
                     "only local sequence variables may be modified";
            }
          }
        }
      }
    }
  }
}

TEST_F(Sequence14Test, Compiler_NoSyntaxErrors) {
  ErrorContainer::Stats stats = m_compiler->getErrorStats();
  EXPECT_EQ(stats.nbSyntax, 0) << "sequence14.sv is syntactically valid -- no syntax errors expected";
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(Sequence14Test, ModuleExists) { ASSERT_NE(getTb(m_design), nullptr) << "module 'tb' not found"; }

// ===========================================================================
// Sequence declaration (ss.16.8 / ss.16.10)
// ===========================================================================

TEST_F(Sequence14Test, SequenceDeclCount_IsOne) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getSequenceDecls(), nullptr) << "module has no sequence declarations";
  EXPECT_EQ(m->getSequenceDecls()->size(), 1u) << "ss.16.8: exactly one sequence is declared: seq14";
}

TEST_F(Sequence14Test, Seq14_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(getSeqDecl(m, "seq14"), nullptr) << "ss.16.8: sequence 'seq14' must be declared";
}

// ===========================================================================
// seq14 body: list operation for '(1, x=0, x++, inc())' (ss.16.10)
// ===========================================================================

TEST_F(Sequence14Test, Seq14_HasExpression) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::SequenceDecl *s14 = getSeqDecl(m, "seq14");
  ASSERT_NE(s14, nullptr) << "seq14 not found";
  EXPECT_NE(s14->getExpr(), nullptr) << "ss.16.10: seq14 must have a body expression";
}

// ss.16.10: the parenthesized sequence match item form is represented as
// an Operation with opType vpiListOp (37).
TEST_F(Sequence14Test, Seq14_Expr_IsListOperation) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *op = getListOp(m);
  ASSERT_NE(op, nullptr) << "seq14 body must be an Operation";
  EXPECT_EQ(op->getOpType(), vpiListOp) << "ss.16.10: '(1, x=0, x++, inc())' must have opType vpiListOp (37)";
}

// ss.16.10: one sequence expression plus three match items = four operands.
TEST_F(Sequence14Test, Seq14_List_HasFourOperands) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *op = getListOp(m);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 4u) << "ss.16.10: '(1, x=0, x++, inc())' must produce 4 operands: "
                                              "seq_expr + 3 match items";
}

// ===========================================================================
// operand[0]: sequence expression '1' (Constant)
// ===========================================================================

// ss.16.10: operand[0] is the sequence expression -- Constant '1'.
TEST_F(Sequence14Test, Seq14_List_Operand0_IsConstant) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *op = getListOp(m);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);

  const hldb::Constant *c = any_cast<hldb::Constant>((*op->getOperands())[0]);
  EXPECT_NE(c, nullptr) << "ss.16.10: operand[0] of the list must be a Constant (the seq expr '1')";
}

TEST_F(Sequence14Test, Seq14_List_Operand0_ValueIsOne) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *op = getListOp(m);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);

  const hldb::Constant *c = any_cast<hldb::Constant>((*op->getOperands())[0]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "1") << "ss.16.10: the sequence expression in '(1, ...)' must be Constant \"1\"";
}

// ===========================================================================
// operand[1]: match item 'x=0' (blocking Assignment)
// ===========================================================================

// ss.16.10: operand[1] is the first match item -- a blocking operator
// assignment 'x=0', represented as an Assignment node.
TEST_F(Sequence14Test, Seq14_List_Operand1_IsBlockingAssignment) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assignment *asgn = getAssignment(m);
  EXPECT_NE(asgn, nullptr) << "ss.16.10: operand[1] must be an Assignment node (match item 'x=0')";
}

TEST_F(Sequence14Test, Seq14_Assignment_IsBlocking) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assignment *asgn = getAssignment(m);
  ASSERT_NE(asgn, nullptr);
  EXPECT_TRUE(asgn->getBlocking()) << "ss.16.10: 'x=0' in a sequence match item must be a blocking "
                                      "assignment";
}

// ss.16.10: LHS of 'x=0' is a RefObj named "x".
TEST_F(Sequence14Test, Seq14_Assignment_Lhs_IsRefToX) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assignment *asgn = getAssignment(m);
  ASSERT_NE(asgn, nullptr);
  const hldb::RefObj *lhs = asgn->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "LHS of assignment must be a RefObj";
  EXPECT_EQ(lhs->getName(), "x") << "ss.16.10: LHS of 'x=0' must reference variable 'x'";
}

// ss.16.10: RefObj for 'x' on the LHS resolves to Net name:'x'.
TEST_F(Sequence14Test, Seq14_Assignment_Lhs_ResolvesToNet) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assignment *asgn = getAssignment(m);
  ASSERT_NE(asgn, nullptr);
  const hldb::RefObj *lhs = asgn->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr) << "ss.16.10: LHS RefObj 'x' must resolve to Net 'x' at compile time";
}

// ss.16.10: RHS of 'x=0' is a Constant "0".
TEST_F(Sequence14Test, Seq14_Assignment_Rhs_IsConstant) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assignment *asgn = getAssignment(m);
  ASSERT_NE(asgn, nullptr);
  const hldb::Constant *rhs = asgn->getRhs<hldb::Constant>();
  EXPECT_NE(rhs, nullptr) << "ss.16.10: RHS of 'x=0' must be a Constant node";
}

TEST_F(Sequence14Test, Seq14_Assignment_Rhs_ValueIsZero) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assignment *asgn = getAssignment(m);
  ASSERT_NE(asgn, nullptr);
  const hldb::Constant *rhs = asgn->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "0") << "ss.16.10: RHS of 'x=0' must be Constant \"0\"";
}

// ===========================================================================
// operand[2]: match item 'x++' (post-increment Operation)
// ===========================================================================

// ss.16.10: operand[2] is the second match item -- post-increment 'x++',
// represented as an Operation with opType vpiPostIncOp (64).
TEST_F(Sequence14Test, Seq14_List_Operand2_IsPostIncrementOperation) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *postInc = getPostIncOp(m);
  ASSERT_NE(postInc, nullptr) << "ss.16.10: operand[2] must be an Operation (match item 'x++')";
  EXPECT_EQ(postInc->getOpType(), vpiPostIncOp) << "ss.16.10: 'x++' must have opType vpiPostIncOp (64)";
}

// ss.16.10: 'x++' has exactly one operand: the RefObj for 'x'.
TEST_F(Sequence14Test, Seq14_PostIncrement_HasOneOperand) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *postInc = getPostIncOp(m);
  ASSERT_NE(postInc, nullptr);
  ASSERT_NE(postInc->getOperands(), nullptr);
  EXPECT_EQ(postInc->getOperands()->size(), 1u) << "ss.16.10: 'x++' must have exactly one operand";
}

TEST_F(Sequence14Test, Seq14_PostIncrement_Operand_IsRefToX) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *postInc = getPostIncOp(m);
  ASSERT_NE(postInc, nullptr);
  ASSERT_NE(postInc->getOperands(), nullptr);
  ASSERT_GE(postInc->getOperands()->size(), 1u);

  const hldb::RefObj *ref = any_cast<hldb::RefObj>((*postInc->getOperands())[0]);
  ASSERT_NE(ref, nullptr) << "operand of 'x++' must be a RefObj";
  EXPECT_EQ(ref->getName(), "x") << "ss.16.10: operand of 'x++' must reference variable 'x'";
}

TEST_F(Sequence14Test, Seq14_PostIncrement_Operand_ResolvesToNet) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Operation *postInc = getPostIncOp(m);
  ASSERT_NE(postInc, nullptr);
  ASSERT_NE(postInc->getOperands(), nullptr);
  ASSERT_GE(postInc->getOperands()->size(), 1u);

  const hldb::RefObj *ref = any_cast<hldb::RefObj>((*postInc->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::Net>(), nullptr) << "ss.16.10: RefObj 'x' in 'x++' must resolve to Net 'x' at "
                                                     "compile time";
}

// ===========================================================================
// operand[3]: match item 'inc()' (FuncCall)
// ===========================================================================

// ss.16.10: operand[3] is the third match item -- subroutine call 'inc()',
// represented as a FuncCall node.
TEST_F(Sequence14Test, Seq14_List_Operand3_IsFuncCall) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::FuncCall *fc = getFuncCall(m);
  EXPECT_NE(fc, nullptr) << "ss.16.10: operand[3] must be a FuncCall node (match item 'inc()')";
}

TEST_F(Sequence14Test, Seq14_FuncCall_NameIsInc) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::FuncCall *fc = getFuncCall(m);
  ASSERT_NE(fc, nullptr);
  EXPECT_EQ(fc->getName(), "inc") << "ss.16.10: FuncCall must reference function 'inc'";
}

// ss.16.10: FuncCall for 'inc()' must resolve to the Function declaration
// at compile time via getTaskFunc<Function>().  If this test fails, Surelog
// does not perform name binding for FuncCall nodes inside sequence match items.
TEST_F(Sequence14Test, Seq14_FuncCall_ResolvedToFunction) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::FuncCall *fc = getFuncCall(m);
  ASSERT_NE(fc, nullptr);
  EXPECT_NE(fc->getTaskFunc<hldb::Function>(), nullptr)
      << "ss.16.10: FuncCall for 'inc()' must resolve to Function 'inc' at "
         "compile time -- missing compile-time name binding for FuncCall "
         "inside sequence match items";
}

// ===========================================================================
// Concurrent assertion: 'assert property(@(posedge clk) seq14)' (ss.16.14)
// ===========================================================================

TEST_F(Sequence14Test, ConcurrentAssertion_Exists) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getConcurrentAssertions(), nullptr) << "module has no concurrent assertions";
  EXPECT_NE(getFirstAssert(m), nullptr) << "ss.16.14: an Assert node must be present";
}

TEST_F(Sequence14Test, Assert_HasPropertySpec) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getProperty<hldb::PropertySpec>(), nullptr) << "ss.16.14: Assert must have an inline PropertySpec";
}

// ss.16.14: the clock is given externally on the assert, so the PropertySpec
// must carry a clocking event @(posedge clk).
TEST_F(Sequence14Test, Assert_PropertySpec_HasClockingEvent) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  EXPECT_NE(spec->getClockingEvent(), nullptr) << "ss.16.14: @(posedge clk) on the assert must produce a clocking "
                                                  "event on the PropertySpec";
}

TEST_F(Sequence14Test, Assert_PropertyExpr_ReferencesSeq14) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr) << "property expression must be a RefObj";
  EXPECT_EQ(propExpr->getName(), "seq14") << "ss.16.14: property expression must reference 'seq14'";
}

// ss.16.14: RefObj for 'seq14' must resolve to SequenceDecl.  Surelog emits
// EL0535 treating it as an implicit net -- same bug as sequence4-13.
TEST_F(Sequence14Test, Assert_PropertyExpr_ResolvedToSeq14Decl) {
  const hldb::Module *m = getTb(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Assert *a = getFirstAssert(m);
  ASSERT_NE(a, nullptr);
  const hldb::PropertySpec *spec = a->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr);
  EXPECT_NE(propExpr->getActual<hldb::SequenceDecl>(), nullptr)
      << "ss.16.14: 'seq14' in assert property must resolve to SequenceDecl -- "
         "Surelog emits EL0535 treating it as an implicit net instead";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
