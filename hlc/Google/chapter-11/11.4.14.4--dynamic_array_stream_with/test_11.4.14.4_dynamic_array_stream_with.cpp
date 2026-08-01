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

// Tests for 11.4.14.4--dynamic_array_stream_with.sv (tags: 11.4.14.4)
//   (same declarations as dynamic_array_stream.sv: i_header/i_len/i_crc/
//   o_header/o_len/o_crc as int, i_data/o_data as dynamic byte arrays)
//
//   initial begin
//     byte pkt[$];
//     i_header = 12; i_len = 5; i_data = new[5]; i_crc = 42;
//     pkt = {<< 8 {i_header, i_len, i_data, i_crc}};
//     {<< 8 {o_header, o_len, o_data with [0 +: o_len], o_crc}} = pkt;
//   end
//
// Per IEEE 1800-2017 11.4.14.4, "array_name with [range]" inside a stream
// concatenation lets an array's *slice* participate as a stream_expression
// instead of the whole array. This is the direct sibling of
// dynamic_array_stream.sv: the pack half (statements 1-5) is byte-for-byte
// identical to that file, so this file's own corner is entirely in the
// unpack's 3rd operand: unlike the plain sibling (where "o_data" is a bare
// RefObj), here it is wrapped in a "with" Operation whose 2nd operand is an
// IndexedRange -- and critically, that IndexedRange's *width* is itself the
// RefObj "o_len" (a runtime/variable expression), not a Constant -- because
// the entire point of this "_with" variant is a *dynamic* slice width taken
// from another unpacked field, not a fixed-size slice.
//
// Checked:
//   - everything asserted in dynamic_array_stream.sv's test file for
//     variables (all 8 top-level int/byte declarations are hldb::Variable,
//     not hldb::Net, per IEEE 1800-2023 Sec 6.7/6.8: none carries a
//     net-type keyword, and a dynamic array cannot be net-typed at all),
//     module typespecs, the local queue Variable "pkt", and the first 5
//     initial-block statements (12/5/new[5]/42/pack) holds identically
//     here and is re-verified below for completeness of this file's own
//     coverage
//   - the unpack (statement 6) lhs Operation (vpiStreamRLOp, slice size
//     Constant "8") whose nested vpiConcatOp has exactly 4 operands:
//       0: RefObj "o_header"
//       1: RefObj "o_len"
//       2: Operation (vpiWith) with exactly 2 operands: RefObj "o_data"
//          and an IndexedRange whose vpiIndexedPartSelectType ==
//          vpiPosIndexed, vpiBaseExpr Constant "0", and -- the corner this
//          file exists to exercise -- vpiWidthExpr is RefObj "o_len"
//          (resolving to the o_len Variable), not a Constant
//       3: RefObj "o_crc"
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (signed),
//     StringTypespec
//   - compiler emits zero errors
//
// Not checked:
//   - this file carries no $display, so there is no runtime value to check
//     even in principle.

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
#include <hldb/indexed_range.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class DynamicArrayStreamWithTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.14.4--dynamic_array_stream_with.hlc"}); }
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

// --- module / nets (identical shape to the plain dynamic_array_stream.sv) --

TEST_F(DynamicArrayStreamWithTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(DynamicArrayStreamWithTest, ModuleHasEightVariablesNoneDeclAssigned) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 8u);

  const char *const scalarNames[6] = {"i_header", "i_len", "i_crc", "o_header", "o_len", "o_crc"};
  for (const char *const name : scalarNames) {
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>(name, top->getVariables());
    ASSERT_NE(var, nullptr) << "variable " << name;
    EXPECT_NE(var->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr) << "variable " << name;
    EXPECT_EQ(var->getValue<hldb::Constant>(), nullptr) << "variable " << name;
  }

  const char *const arrayNames[2] = {"i_data", "o_data"};
  for (const char *const name : arrayNames) {
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>(name, top->getVariables());
    ASSERT_NE(var, nullptr) << "variable " << name;
    const hldb::ArrayTypespec *const arr = var->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
    ASSERT_NE(arr, nullptr) << "variable " << name;
    EXPECT_EQ(arr->getArrayType(), vpiDynamicArray) << "variable " << name;
  }
}

TEST_F(DynamicArrayStreamWithTest, InitialBeginHasOneLocalQueueVariablePkt) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getVariables(), nullptr);
  ASSERT_EQ(blk->getVariables()->size(), 1u);
  EXPECT_EQ(blk->getVariables()->at(0)->getName(), "pkt");
}

TEST_F(DynamicArrayStreamWithTest, InitialBeginHasSixStatements) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 6u);
}

TEST_F(DynamicArrayStreamWithTest, PackStatementIsByteForByteIdenticalToThePlainSibling) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(4));
  ASSERT_NE(assign, nullptr);
  ASSERT_NE(assign->getLhs<hldb::RefObj>(), nullptr);
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "pkt");

  const hldb::Operation *const stream = assign->getRhs<hldb::Operation>();
  ASSERT_NE(stream, nullptr);
  EXPECT_EQ(stream->getOpType(), vpiStreamRLOp);
  ASSERT_NE(stream->getOperands(), nullptr);
  ASSERT_EQ(stream->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(stream->getOperands()->at(0))->getDecompile(), "8");

  const hldb::Operation *const concat = any_cast<hldb::Operation>(stream->getOperands()->at(1));
  ASSERT_NE(concat, nullptr);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 4u);
  const char *const expectedOrder[4] = {"i_header", "i_len", "i_data", "i_crc"};
  for (uint32_t i = 0; i < 4u; ++i) {
    EXPECT_EQ(any_cast<hldb::RefObj>(concat->getOperands()->at(i))->getName(), expectedOrder[i])
        << "concat operand " << i;
  }
}

// --- unpack: o_data with [0 +: o_len] ---------------------------------------

TEST_F(DynamicArrayStreamWithTest, UnpackThirdOperandIsWithOfODataAndZeroPlusOLen) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(5));
  ASSERT_NE(assign, nullptr);

  const hldb::Operation *const stream = assign->getLhs<hldb::Operation>();
  ASSERT_NE(stream, nullptr);
  EXPECT_EQ(stream->getOpType(), vpiStreamRLOp);
  ASSERT_NE(stream->getOperands(), nullptr);
  ASSERT_EQ(stream->getOperands()->size(), 2u);

  const hldb::Operation *const concat = any_cast<hldb::Operation>(stream->getOperands()->at(1));
  ASSERT_NE(concat, nullptr);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 4u);

  EXPECT_EQ(any_cast<hldb::RefObj>(concat->getOperands()->at(0))->getName(), "o_header");
  EXPECT_EQ(any_cast<hldb::RefObj>(concat->getOperands()->at(1))->getName(), "o_len");
  EXPECT_EQ(any_cast<hldb::RefObj>(concat->getOperands()->at(3))->getName(), "o_crc");

  const hldb::Operation *const withOp = any_cast<hldb::Operation>(concat->getOperands()->at(2));
  ASSERT_NE(withOp, nullptr) << "'o_data with [0 +: o_len]' should be a nested 'with' Operation";
  EXPECT_EQ(withOp->getOpType(), vpiWith);
  ASSERT_NE(withOp->getOperands(), nullptr);
  ASSERT_EQ(withOp->getOperands()->size(), 2u);

  const hldb::RefObj *const arrayOperand = any_cast<hldb::RefObj>(withOp->getOperands()->at(0));
  ASSERT_NE(arrayOperand, nullptr);
  EXPECT_EQ(arrayOperand->getName(), "o_data");
  EXPECT_NE(arrayOperand->getActual<hldb::Variable>(), nullptr);

  const hldb::IndexedRange *const range = any_cast<hldb::IndexedRange>(withOp->getOperands()->at(1));
  ASSERT_NE(range, nullptr);
  EXPECT_EQ(range->getIndexedPartSelectType(), vpiPosIndexed);
  ASSERT_NE(range->getBaseExpr<hldb::Constant>(), nullptr);
  EXPECT_EQ(range->getBaseExpr<hldb::Constant>()->getDecompile(), "0");

  // The corner this file exists to exercise: the slice width is itself a
  // RefObj (a variable expression), not a Constant like the fixed-size
  // "+:"/"-:" indexed-part-selects in the 11.5.1 tests.
  const hldb::RefObj *const width = range->getWidthExpr<hldb::RefObj>();
  ASSERT_NE(width, nullptr) << "the 'with' slice width should be the runtime expression 'o_len', not a literal";
  EXPECT_EQ(width->getName(), "o_len");
  EXPECT_NE(width->getActual<hldb::Variable>(), nullptr);
}

// --- design-level typespecs / compiler diagnostics --------------------------

TEST_F(DynamicArrayStreamWithTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(DynamicArrayStreamWithTest, CompilerReportsZeroErrors) {
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
