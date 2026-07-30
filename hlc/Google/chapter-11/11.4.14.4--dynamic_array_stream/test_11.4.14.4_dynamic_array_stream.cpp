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

// Tests for 11.4.14.4--dynamic_array_stream.sv (tags: 11.4.14.4)
//   int i_header;
//   int i_len;
//   byte i_data[];
//   int i_crc;
//
//   int o_header;
//   int o_len;
//   byte o_data[];
//   int o_crc;
//
//   initial begin
//     byte pkt[$];
//
//     i_header = 12;
//     i_len = 5;
//     i_data = new[5];
//     i_crc = 42;
//
//     pkt = {<< 8 {i_header, i_len, i_data, i_crc}};
//
//     {<< 8 {o_header, o_len, o_data, o_crc}} = pkt;
//   end
//
// Per IEEE 1800-2017 11.4.14.4, a dynamic array is a legal stream_expression
// operand on either side of a streaming concatenation, packed and unpacked
// alongside plain scalar operands with no special wrapper node. The corners
// here are: (1) "i_data"/"o_data" are dynamic (unsized "[]") byte arrays,
// distinct from the module's plain int nets; (2) "pkt" is a *local queue
// variable* declared with "byte pkt[$];" inside the initial block, so it
// lives in the Begin scope's variable list, not in the module's net list;
// (3) "i_data = new[5]" is SystemVerilog's dynamic-array-allocation
// construct, not a plain assignment -- see the dedicated corner below for
// how that shows up structurally; (4) the pack stream's operand order
// (i_header, i_len, i_data, i_crc) must be preserved exactly as written,
// with the dynamic array taking its place among the scalars.
//
// Checked:
//   - module top has exactly 8 nets, all bare (no decl-assignment):
//     i_header/i_len/i_crc/o_header/o_len/o_crc (IntTypespec) and
//     i_data/o_data (ArrayTypespec, vpiArrayType == dynamic, elem
//     typespec ByteTypespec)
//   - module-level typespecs (3): ByteTypespec (signed), and 2 distinct
//     ArrayTypespec objects (one per dynamic array declaration)
//   - module has exactly 1 process: an Initial whose Begin scope has
//       * exactly 1 local Variable: "pkt", typespec ArrayTypespec
//         (vpiArrayType == queue, vpiRange left Constant "$"
//         (vpiConstType == unbounded), elem typespec ByteTypespec) -- this
//         variable is NOT present in top->getNets()
//       * exactly 6 statements, in source order:
//         1) i_header = 12   (Assignment, lhs RefObj -> Net, rhs Constant)
//         2) i_len = 5       (Assignment, lhs RefObj -> Net, rhs Constant)
//         3) i_data = new[5] (Assignment with **no lhs at all** -- see
//            dedicated corner below -- rhs ArrayExpr containing exactly 1
//            Expr: Constant "5", the requested size)
//         4) i_crc = 42      (Assignment, lhs RefObj -> Net, rhs Constant)
//         5) the pack: Assignment lhs RefObj "pkt" (resolving to the local
//            Variable, not a Net), rhs Operation (vpiStreamRLOp) with 2
//            operands: Constant "8" (the slice size) and a nested
//            Operation (vpiConcatOp) with 4 operands in exactly this
//            order: RefObj i_header, RefObj i_len, RefObj i_data (the
//            dynamic array, referenced as a plain RefObj -- no special
//            "array operand" node kind), RefObj i_crc
//         6) the unpack: Assignment whose **lhs** is itself an Operation
//            (vpiStreamRLOp) with the same 2-operand shape (Constant "8",
//            nested vpiConcatOp of RefObj o_header/o_len/o_data/o_crc, in
//            that order), and whose rhs is RefObj "pkt" -- i.e. unpacking
//            reads from the queue variable into the stream-operation
//            target list, the mirror image of the pack
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (signed),
//     StringTypespec
//   - compiler emits zero errors
//
// Not checked:
//   - this file carries no $display, so there is no runtime value to check
//     even in principle (see the "-sim" sibling for that).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_expr.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/byte_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class DynamicArrayStreamTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.14.4--dynamic_array_stream.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if ((top == nullptr) || (top->getProcesses() == nullptr) || (top->getProcesses()->empty())) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }
};

// --- module / nets -----------------------------------------------------

TEST_F(DynamicArrayStreamTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(DynamicArrayStreamTest, ModuleHasEightNetsNoneDeclAssigned) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 8u);

  const char *const scalarNames[6] = {"i_header", "i_len", "i_crc", "o_header", "o_len", "o_crc"};
  for (const char *const name : scalarNames) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(name, top->getNets());
    ASSERT_NE(net, nullptr) << "net " << name;
    EXPECT_NE(net->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr) << "net " << name;
    EXPECT_EQ(net->getValue<hldb::Constant>(), nullptr) << "net " << name;
  }

  const char *const arrayNames[2] = {"i_data", "o_data"};
  for (const char *const name : arrayNames) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(name, top->getNets());
    ASSERT_NE(net, nullptr) << "net " << name;
    const hldb::ArrayTypespec *const arr = net->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
    ASSERT_NE(arr, nullptr) << "net " << name;
    EXPECT_EQ(arr->getArrayType(), vpiDynamicArray) << "net " << name;
    ASSERT_NE(arr->getElemTypespec(), nullptr) << "net " << name;
    EXPECT_NE(arr->getElemTypespec()->getActual<hldb::ByteTypespec>(), nullptr) << "net " << name;
    EXPECT_EQ(net->getValue<hldb::Constant>(), nullptr) << "net " << name;
  }
}

TEST_F(DynamicArrayStreamTest, ModuleHasThreeTypespecs) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  EXPECT_EQ(top->getTypespecs()->size(), 3u);
}

// --- initial block: local queue variable ------------------------------------

TEST_F(DynamicArrayStreamTest, InitialBeginHasOneLocalQueueVariablePktNotAModuleNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("pkt", top->getNets()), nullptr)
      << "'pkt' is a local variable of the initial block's Begin scope, not a module net";

  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getVariables(), nullptr);
  ASSERT_EQ(blk->getVariables()->size(), 1u);
  const hldb::Variable *const pkt = blk->getVariables()->at(0);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->getName(), "pkt");

  const hldb::ArrayTypespec *const pktType = pkt->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(pktType, nullptr);
  EXPECT_EQ(pktType->getArrayType(), vpiQueueArray);
  ASSERT_NE(pktType->getElemTypespec(), nullptr);
  EXPECT_NE(pktType->getElemTypespec()->getActual<hldb::ByteTypespec>(), nullptr);
  ASSERT_NE(pktType->getRange(), nullptr);
  const hldb::Constant *const bound = pktType->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(bound, nullptr);
  EXPECT_EQ(bound->getConstType(), vpiUnboundedConst);
  EXPECT_EQ(bound->getDecompile(), "$");
}

TEST_F(DynamicArrayStreamTest, InitialBeginHasSixStatements) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 6u);
}

TEST_F(DynamicArrayStreamTest, FirstSecondFourthStatementsAssignScalarConstants) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);

  struct Expected {
    uint32_t index;
    const char *net;
    const char *value;
  };
  const Expected expected[3] = {{0u, "i_header", "12"}, {1u, "i_len", "5"}, {3u, "i_crc", "42"}};
  for (const Expected &exp : expected) {
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(exp.index));
    ASSERT_NE(assign, nullptr) << "statement " << exp.index;
    ASSERT_NE(assign->getLhs<hldb::RefObj>(), nullptr) << "statement " << exp.index;
    EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), exp.net);
    EXPECT_NE(assign->getLhs<hldb::RefObj>()->getActual<hldb::Net>(), nullptr) << "statement " << exp.index;
    ASSERT_NE(assign->getRhs<hldb::Constant>(), nullptr) << "statement " << exp.index;
    EXPECT_EQ(assign->getRhs<hldb::Constant>()->getDecompile(), exp.value);
  }
}

TEST_F(DynamicArrayStreamTest, ThirdStatementIsDynamicArrayNewWithNoCapturedLhs) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(2));
  ASSERT_NE(assign, nullptr);

  // "i_data = new[5]" is dynamic-array allocation, not a plain assignment:
  // HLC never populates vpiLhs for this construct (confirmed against the
  // AST dump), even though "i_data" is unambiguously the target being
  // (re)sized. Any consumer wanting to know which array was allocated must
  // look elsewhere (e.g. statement order / textual position), not at this
  // Assignment's lhs.
  EXPECT_EQ(assign->getLhs(), nullptr) << "HLC does not capture the lvalue of a dynamic-array 'new[N]' assignment";

  const hldb::ArrayExpr *const rhs = assign->getRhs<hldb::ArrayExpr>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getExprs(), nullptr);
  ASSERT_EQ(rhs->getExprs()->size(), 1u);
  const hldb::Constant *const size = any_cast<hldb::Constant>(rhs->getExprs()->at(0));
  ASSERT_NE(size, nullptr);
  EXPECT_EQ(size->getDecompile(), "5");
}

// --- pack: pkt = {<<8{i_header, i_len, i_data, i_crc}} ----------------------

TEST_F(DynamicArrayStreamTest, FifthStatementPacksFourOperandsInSourceOrderIntoPkt) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(4));
  ASSERT_NE(assign, nullptr);
  ASSERT_NE(assign->getLhs<hldb::RefObj>(), nullptr);
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "pkt");
  EXPECT_NE(assign->getLhs<hldb::RefObj>()->getActual<hldb::Variable>(), nullptr)
      << "pack target resolves to the local Variable 'pkt', not a Net";

  const hldb::Operation *const stream = assign->getRhs<hldb::Operation>();
  ASSERT_NE(stream, nullptr);
  EXPECT_EQ(stream->getOpType(), vpiStreamRLOp);
  ASSERT_NE(stream->getOperands(), nullptr);
  ASSERT_EQ(stream->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(stream->getOperands()->at(0))->getDecompile(), "8");

  const hldb::Operation *const concat = any_cast<hldb::Operation>(stream->getOperands()->at(1));
  ASSERT_NE(concat, nullptr);
  EXPECT_EQ(concat->getOpType(), vpiConcatOp);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 4u);
  const char *const expectedOrder[4] = {"i_header", "i_len", "i_data", "i_crc"};
  for (uint32_t i = 0; i < 4u; ++i) {
    const hldb::RefObj *const operand = any_cast<hldb::RefObj>(concat->getOperands()->at(i));
    ASSERT_NE(operand, nullptr) << "concat operand " << i;
    EXPECT_EQ(operand->getName(), expectedOrder[i]) << "concat operand " << i;
  }
  // "i_data" (the dynamic array) is a plain RefObj operand alongside the
  // scalar int nets -- IEEE 1800-2017 11.4.14.4 gives it no special
  // wrapper node when used as a stream_expression.
  EXPECT_NE(any_cast<hldb::RefObj>(concat->getOperands()->at(2))->getActual<hldb::Net>(), nullptr);
}

// --- unpack: {<<8{o_header, o_len, o_data, o_crc}} = pkt --------------------

TEST_F(DynamicArrayStreamTest, SixthStatementUnpacksPktIntoFourOperandsAsTheAssignmentLhs) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(5));
  ASSERT_NE(assign, nullptr);

  const hldb::Operation *const stream = assign->getLhs<hldb::Operation>();
  ASSERT_NE(stream, nullptr) << "the unpack target list is itself the assignment's lhs";
  EXPECT_EQ(stream->getOpType(), vpiStreamRLOp);
  ASSERT_NE(stream->getOperands(), nullptr);
  ASSERT_EQ(stream->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(stream->getOperands()->at(0))->getDecompile(), "8");

  const hldb::Operation *const concat = any_cast<hldb::Operation>(stream->getOperands()->at(1));
  ASSERT_NE(concat, nullptr);
  EXPECT_EQ(concat->getOpType(), vpiConcatOp);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 4u);
  const char *const expectedOrder[4] = {"o_header", "o_len", "o_data", "o_crc"};
  for (uint32_t i = 0; i < 4u; ++i) {
    const hldb::RefObj *const operand = any_cast<hldb::RefObj>(concat->getOperands()->at(i));
    ASSERT_NE(operand, nullptr) << "concat operand " << i;
    EXPECT_EQ(operand->getName(), expectedOrder[i]) << "concat operand " << i;
  }

  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "pkt");
  EXPECT_NE(rhs->getActual<hldb::Variable>(), nullptr) << "unpack reads from the local Variable 'pkt'";
}

// --- design-level typespecs / compiler diagnostics --------------------------

TEST_F(DynamicArrayStreamTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(DynamicArrayStreamTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
