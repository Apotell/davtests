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

// Tests IEEE 1800-2017 ss.16.8 parameterized sequence declaration and
// instantiation with named argument binding for sequence22.sv:
//
//   sequence my_seq(input bit x, input bit y);
//     x ##1 y;
//   endsequence
//
//   sequence seq_named;
//     my_seq(.x(a), .y(b));  // named args
//   endsequence
//
//   assert property(@(posedge clk) seq_named);
//
// ss.16.8 rules:
//   Named argument association uses '.formal(actual)' syntax. Each named
//   binding must be represented with the formal parameter name and the actual
//   expression. This differs from positional association (sequence21) in that
//   the argument carries explicit formal-to-actual mapping.
//
// Compiler bugs exposed:
//
//   PA0207 x2 (input keyword): The parser rejects the 'input' direction
//     keyword in sequence formal parameter lists with "extraneous input
//     'input'". ss.16.8 permits 'input', 'inout', and 'output' directions.
//     Reported as SYNTAX errors in the log but NOT counted in nbError.
//
//   EL0535 x2 (x, y in body): The formal parameters 'x' and 'y' used in
//     the body ('x ##1 y') are treated as illegal implicit nets instead of
//     resolving to their SeqFormalDecl nodes. Tests
//     MySeq_Op_Operand0_X_ResolvesToFormalDecl and
//     MySeq_Op_Operand2_Y_ResolvesToFormalDecl FAIL.
//
//   FuncCall instead of SeqInst: 'my_seq(.x(a),.y(b))' inside seq_named is
//     represented as a FuncCall node rather than a proper sequence
//     instantiation node. Named argument bindings appear as IODecl children
//     of the FuncCall. FuncCall has no getActual() method, so there is no API
//     to verify the back-link to SequenceDecl my_seq -- the link does not
//     exist in the HLDB. The named bindings are accessible via IODecl and
//     are tested below.
//
//   EL0535 x1 (seq_named in assert): 'seq_named' in 'assert property(...)'
//     is treated as an illegal implicit net. Test
//     Assert_PropertyExpr_ResolvedToSeqNamedDecl FAILS.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_typespec.h>
#include <hldb/concurrent_assertions.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/property_spec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/seq_formal_decl.h>
#include <hldb/sequence_decl.h>
#include <hldb/sv_vpi_user.h>

#include <string>

namespace hlc {

class Sequence22Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sequence22.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTb(const hldb::Design *d) {
    return hldb::findByName<hldb::Module>("work@tb", d->getAllModules());
  }

  static const hldb::SequenceDecl *getSeqDecl(const hldb::Module *mod,
                                               std::string_view name) {
    if (!mod || !mod->getSequenceDecls()) return nullptr;
    for (const hldb::SequenceDecl *const s : *mod->getSequenceDecls()) {
      if (s->getName() == name) return s;
    }
    return nullptr;
  }

  static const hldb::SeqFormalDecl *getFormalDecl(
      const hldb::SequenceDecl *seq, std::string_view name) {
    if (!seq || !seq->getSeqFormalDecls()) return nullptr;
    for (const hldb::SeqFormalDecl *const f : *seq->getSeqFormalDecls()) {
      if (f->getName() == name) return f;
    }
    return nullptr;
  }
};

// ===========================================================================
// Module
// ===========================================================================

TEST_F(Sequence22Test, ModuleExists) {
  ASSERT_NE(getTb(m_design), nullptr) << "module 'work@tb' not found";
}

// ===========================================================================
// Nets
// ===========================================================================

TEST_F(Sequence22Test, Net_clk_HasBitTypespec) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getNets(), nullptr);
  const hldb::Net *const clk =
      hldb::findByName<hldb::Net>("clk", tb->getNets());
  ASSERT_NE(clk, nullptr) << "net 'clk' not found";
  ASSERT_NE(clk->getTypespec(), nullptr);
  EXPECT_NE(clk->getTypespec()->getActual<hldb::BitTypespec>(), nullptr)
      << "'bit clk' must produce a BitTypespec";
}

TEST_F(Sequence22Test, Net_a_HasBitTypespec) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getNets(), nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", tb->getNets());
  ASSERT_NE(a, nullptr) << "net 'a' not found";
  ASSERT_NE(a->getTypespec(), nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::BitTypespec>(), nullptr)
      << "'bit a' must produce a BitTypespec";
}

TEST_F(Sequence22Test, Net_b_HasBitTypespec) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getNets(), nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", tb->getNets());
  ASSERT_NE(b, nullptr) << "net 'b' not found";
  ASSERT_NE(b->getTypespec(), nullptr);
  EXPECT_NE(b->getTypespec()->getActual<hldb::BitTypespec>(), nullptr)
      << "'bit b' must produce a BitTypespec";
}

// ===========================================================================
// SequenceDecl collection
// ===========================================================================

TEST_F(Sequence22Test, SeqDeclCollection_HasTwoEntries) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);
  EXPECT_EQ(tb->getSequenceDecls()->size(), 2u)
      << "two sequence declarations expected (my_seq, seq_named)";
}

TEST_F(Sequence22Test, SeqDecl_my_seq_Exists) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  EXPECT_NE(getSeqDecl(tb, "my_seq"), nullptr)
      << "SequenceDecl named 'my_seq' not found";
}

TEST_F(Sequence22Test, SeqDecl_seq_named_Exists) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  EXPECT_NE(getSeqDecl(tb, "seq_named"), nullptr)
      << "SequenceDecl named 'seq_named' not found";
}

// ===========================================================================
// my_seq formal parameters  (ss.16.8)
// ===========================================================================

TEST_F(Sequence22Test, MySeq_HasTwoFormalParams) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "my_seq");
  ASSERT_NE(seq, nullptr);
  ASSERT_NE(seq->getSeqFormalDecls(), nullptr);
  EXPECT_EQ(seq->getSeqFormalDecls()->size(), 2u)
      << "ss.16.8: 'my_seq(input bit x, input bit y)' must have 2 formal "
         "parameters; parser may have dropped them due to PA0207 syntax error";
}

TEST_F(Sequence22Test, MySeq_FormalParam_x_Exists) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "my_seq");
  ASSERT_NE(seq, nullptr);
  EXPECT_NE(getFormalDecl(seq, "x"), nullptr)
      << "SeqFormalDecl named 'x' not found in my_seq";
}

TEST_F(Sequence22Test, MySeq_FormalParam_y_Exists) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "my_seq");
  ASSERT_NE(seq, nullptr);
  EXPECT_NE(getFormalDecl(seq, "y"), nullptr)
      << "SeqFormalDecl named 'y' not found in my_seq";
}

TEST_F(Sequence22Test, MySeq_FormalParam_x_HasBitTypespec) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "my_seq");
  ASSERT_NE(seq, nullptr);
  const hldb::SeqFormalDecl *const x = getFormalDecl(seq, "x");
  ASSERT_NE(x, nullptr);
  ASSERT_NE(x->getTypespec(), nullptr);
  EXPECT_NE(x->getTypespec()->getActual<hldb::BitTypespec>(), nullptr)
      << "ss.16.8: formal parameter 'x' declared as 'bit' must have BitTypespec";
}

TEST_F(Sequence22Test, MySeq_FormalParam_y_HasBitTypespec) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "my_seq");
  ASSERT_NE(seq, nullptr);
  const hldb::SeqFormalDecl *const y = getFormalDecl(seq, "y");
  ASSERT_NE(y, nullptr);
  ASSERT_NE(y->getTypespec(), nullptr);
  EXPECT_NE(y->getTypespec()->getActual<hldb::BitTypespec>(), nullptr)
      << "ss.16.8: formal parameter 'y' declared as 'bit' must have BitTypespec";
}

// ===========================================================================
// my_seq body: x ##1 y
// ===========================================================================

TEST_F(Sequence22Test, MySeq_ExprIsCycleDelayOp) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "my_seq");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp)
      << "ss.16.7: 'x ##1 y' must use vpiCycleDelayOp (71)";
}

TEST_F(Sequence22Test, MySeq_Op_HasThreeOperands) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "my_seq");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u)
      << "ss.16.7: 'x ##1 y' must have 3 operands: [RefObj(x), Constant(1), RefObj(y)]";
}

TEST_F(Sequence22Test, MySeq_Op_Operand0_IsRefObjX) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "my_seq");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "x")
      << "ss.16.8: left operand of 'x ##1 y' must reference formal param 'x'";
}

TEST_F(Sequence22Test, MySeq_Op_Operand0_X_ResolvesToFormalDecl) {
  // ss.16.8: 'x' in the body must resolve to SeqFormalDecl x, not an implicit
  // net. EL0535 bug: the compiler treats 'x' as an illegal implicit net.
  // This test FAILS intentionally to document the bug.
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "my_seq");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::SeqFormalDecl>(), nullptr)
      << "EL0535: 'x' in body of my_seq must resolve to SeqFormalDecl 'x'; "
         "compiler treats it as an illegal implicit net instead";
}

TEST_F(Sequence22Test, MySeq_Op_Operand1_IsConstantDelayOne) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "my_seq");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Constant *const c =
      any_cast<const hldb::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr) << "operands[1] of '##1' delay must be a Constant";
  EXPECT_EQ(std::string(c->getDecompile()), "1")
      << "ss.16.7: '##1' cycle delay constant must decompile to \"1\"";
}

TEST_F(Sequence22Test, MySeq_Op_Operand1_ConstType_IsInt) {
  // '##1' produces vpiIntConst (7), not vpiUIntConst (9).
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "my_seq");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Constant *const c =
      any_cast<const hldb::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiIntConst)
      << "ss.16.7: '##1' delay constant must be vpiIntConst (7)";
}

TEST_F(Sequence22Test, MySeq_Op_Operand2_IsRefObjY) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "my_seq");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 3u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*op->getOperands())[2]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "y")
      << "ss.16.8: right operand of 'x ##1 y' must reference formal param 'y'";
}

TEST_F(Sequence22Test, MySeq_Op_Operand2_Y_ResolvesToFormalDecl) {
  // ss.16.8: 'y' in the body must resolve to SeqFormalDecl y.
  // EL0535 bug: compiler treats 'y' as an illegal implicit net.
  // This test FAILS intentionally to document the bug.
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "my_seq");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 3u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*op->getOperands())[2]);
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::SeqFormalDecl>(), nullptr)
      << "EL0535: 'y' in body of my_seq must resolve to SeqFormalDecl 'y'; "
         "compiler treats it as an illegal implicit net instead";
}

// ===========================================================================
// seq_named body: my_seq(.x(a), .y(b))  (ss.16.8 named argument binding)
// ===========================================================================

TEST_F(Sequence22Test, SeqNamed_InstantiatedSeqName_IsMySeq) {
  // The expression inside seq_named calls my_seq -- verify the name.
  // Note: the compiler stores this as a FuncCall node; a correct
  // implementation would use a dedicated sequence instantiation node.
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_named");
  ASSERT_NE(seq, nullptr);
  const hldb::FuncCall *const fc = seq->getExpr<hldb::FuncCall>();
  ASSERT_NE(fc, nullptr);
  EXPECT_EQ(fc->getName(), "my_seq")
      << "ss.16.8: the instantiated sequence name must be 'my_seq'";
}

TEST_F(Sequence22Test, SeqNamed_HasTwoNamedArguments) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_named");
  ASSERT_NE(seq, nullptr);
  const hldb::FuncCall *const fc = seq->getExpr<hldb::FuncCall>();
  ASSERT_NE(fc, nullptr);
  ASSERT_NE(fc->getArguments(), nullptr);
  EXPECT_EQ(fc->getArguments()->size(), 2u)
      << "ss.16.8: 'my_seq(.x(a),.y(b))' must pass exactly 2 named arguments";
}

TEST_F(Sequence22Test, SeqNamed_NamedArg0_IsIODecl) {
  // ss.16.8: named argument '.x(a)' is represented as an IODecl node carrying
  // the formal parameter name and the actual expression.
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_named");
  ASSERT_NE(seq, nullptr);
  const hldb::FuncCall *const fc = seq->getExpr<hldb::FuncCall>();
  ASSERT_NE(fc, nullptr);
  ASSERT_NE(fc->getArguments(), nullptr);
  ASSERT_GE(fc->getArguments()->size(), 1u);
  const hldb::IODecl *const iod =
      any_cast<const hldb::IODecl *>((*fc->getArguments())[0]);
  EXPECT_NE(iod, nullptr)
      << "ss.16.8: named argument 0 ('.x(a)') must be an IODecl node";
}

TEST_F(Sequence22Test, SeqNamed_NamedArg0_FormalName_IsX) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_named");
  ASSERT_NE(seq, nullptr);
  const hldb::FuncCall *const fc = seq->getExpr<hldb::FuncCall>();
  ASSERT_NE(fc, nullptr);
  ASSERT_NE(fc->getArguments(), nullptr);
  ASSERT_GE(fc->getArguments()->size(), 1u);
  const hldb::IODecl *const iod =
      any_cast<const hldb::IODecl *>((*fc->getArguments())[0]);
  ASSERT_NE(iod, nullptr);
  EXPECT_EQ(iod->getName(), "x")
      << "ss.16.8: named argument '.x(a)' formal name must be 'x'";
}

TEST_F(Sequence22Test, SeqNamed_NamedArg0_ActualExpr_IsRefObjA) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_named");
  ASSERT_NE(seq, nullptr);
  const hldb::FuncCall *const fc = seq->getExpr<hldb::FuncCall>();
  ASSERT_NE(fc, nullptr);
  ASSERT_NE(fc->getArguments(), nullptr);
  ASSERT_GE(fc->getArguments()->size(), 1u);
  const hldb::IODecl *const iod =
      any_cast<const hldb::IODecl *>((*fc->getArguments())[0]);
  ASSERT_NE(iod, nullptr);
  const hldb::RefObj *const ref = iod->getExpr<hldb::RefObj>();
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a")
      << "ss.16.8: named argument '.x(a)' actual expression must be 'a'";
}

TEST_F(Sequence22Test, SeqNamed_NamedArg0_ActualSignal_ResolvesToNetA) {
  // ss.16.8: actual argument 'a' (bound to formal 'x') must resolve to the
  // net declared as 'bit a'.
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_named");
  ASSERT_NE(seq, nullptr);
  const hldb::FuncCall *const fc = seq->getExpr<hldb::FuncCall>();
  ASSERT_NE(fc, nullptr);
  ASSERT_NE(fc->getArguments(), nullptr);
  ASSERT_GE(fc->getArguments()->size(), 1u);
  const hldb::IODecl *const iod =
      any_cast<const hldb::IODecl *>((*fc->getArguments())[0]);
  ASSERT_NE(iod, nullptr);
  const hldb::RefObj *const ref = iod->getExpr<hldb::RefObj>();
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::Net>(), nullptr)
      << "ss.16.8: named actual argument 'a' must resolve to Net 'bit a'";
}

TEST_F(Sequence22Test, SeqNamed_NamedArg1_IsIODecl) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_named");
  ASSERT_NE(seq, nullptr);
  const hldb::FuncCall *const fc = seq->getExpr<hldb::FuncCall>();
  ASSERT_NE(fc, nullptr);
  ASSERT_NE(fc->getArguments(), nullptr);
  ASSERT_GE(fc->getArguments()->size(), 2u);
  const hldb::IODecl *const iod =
      any_cast<const hldb::IODecl *>((*fc->getArguments())[1]);
  EXPECT_NE(iod, nullptr)
      << "ss.16.8: named argument 1 ('.y(b)') must be an IODecl node";
}

TEST_F(Sequence22Test, SeqNamed_NamedArg1_FormalName_IsY) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_named");
  ASSERT_NE(seq, nullptr);
  const hldb::FuncCall *const fc = seq->getExpr<hldb::FuncCall>();
  ASSERT_NE(fc, nullptr);
  ASSERT_NE(fc->getArguments(), nullptr);
  ASSERT_GE(fc->getArguments()->size(), 2u);
  const hldb::IODecl *const iod =
      any_cast<const hldb::IODecl *>((*fc->getArguments())[1]);
  ASSERT_NE(iod, nullptr);
  EXPECT_EQ(iod->getName(), "y")
      << "ss.16.8: named argument '.y(b)' formal name must be 'y'";
}

TEST_F(Sequence22Test, SeqNamed_NamedArg1_ActualExpr_IsRefObjB) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_named");
  ASSERT_NE(seq, nullptr);
  const hldb::FuncCall *const fc = seq->getExpr<hldb::FuncCall>();
  ASSERT_NE(fc, nullptr);
  ASSERT_NE(fc->getArguments(), nullptr);
  ASSERT_GE(fc->getArguments()->size(), 2u);
  const hldb::IODecl *const iod =
      any_cast<const hldb::IODecl *>((*fc->getArguments())[1]);
  ASSERT_NE(iod, nullptr);
  const hldb::RefObj *const ref = iod->getExpr<hldb::RefObj>();
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "b")
      << "ss.16.8: named argument '.y(b)' actual expression must be 'b'";
}

TEST_F(Sequence22Test, SeqNamed_NamedArg1_ActualSignal_ResolvesToNetB) {
  // ss.16.8: actual argument 'b' (bound to formal 'y') must resolve to the
  // net declared as 'bit b'.
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_named");
  ASSERT_NE(seq, nullptr);
  const hldb::FuncCall *const fc = seq->getExpr<hldb::FuncCall>();
  ASSERT_NE(fc, nullptr);
  ASSERT_NE(fc->getArguments(), nullptr);
  ASSERT_GE(fc->getArguments()->size(), 2u);
  const hldb::IODecl *const iod =
      any_cast<const hldb::IODecl *>((*fc->getArguments())[1]);
  ASSERT_NE(iod, nullptr);
  const hldb::RefObj *const ref = iod->getExpr<hldb::RefObj>();
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::Net>(), nullptr)
      << "ss.16.8: named actual argument 'b' must resolve to Net 'bit b'";
}

// ===========================================================================
// Concurrent assertion
// ===========================================================================

TEST_F(Sequence22Test, Assert_PropertyExpr_NameIsSeqNamed) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);
  ASSERT_GE(tb->getConcurrentAssertions()->size(), 1u);
  const hldb::ConcurrentAssertions *const ca =
      (*tb->getConcurrentAssertions())[0];
  ASSERT_NE(ca, nullptr);
  const hldb::PropertySpec *const spec = ca->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *const expr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getName(), "seq_named")
      << "assertion must reference 'seq_named'";
}

TEST_F(Sequence22Test, Assert_PropertyExpr_ResolvedToSeqNamedDecl) {
  // ss.16.8: 'seq_named' in 'assert property(...)' must resolve to
  // SequenceDecl seq_named. EL0535 bug: compiler treats it as implicit net.
  // This test FAILS intentionally to document the bug.
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);
  ASSERT_GE(tb->getConcurrentAssertions()->size(), 1u);
  const hldb::ConcurrentAssertions *const ca =
      (*tb->getConcurrentAssertions())[0];
  ASSERT_NE(ca, nullptr);
  const hldb::PropertySpec *const spec = ca->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *const expr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(expr, nullptr);
  EXPECT_NE(expr->getActual<hldb::SequenceDecl>(), nullptr)
      << "EL0535: 'seq_named' in assert property must resolve to SequenceDecl; "
         "compiler treats it as an illegal implicit net instead";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
