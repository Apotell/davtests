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

// Tests for 11.4.14.4--dynamic_array_stream-sim.sv (tags: 11.4.14.4)
//   (same declarations as dynamic_array_stream.sv)
//
//   initial begin
//     byte pkt[$];
//     i_header = 12; i_len = 5; i_crc = 42;
//     i_data = new[5];
//     i_data[0] = 1; i_data[1] = 2; i_data[2] = 3; i_data[3] = 4; i_data[4] = 5;
//
//     pkt = {<< 8 {i_header, i_len, i_crc, i_data}};
//     {<< 8 {o_header, o_len, o_crc, o_data}} = pkt;
//
//     $display(":assert: (12 == %d)", o_header);
//     $display(":assert: (5 == %d)", o_len);
//     $display(":assert: (42 == %d)", o_crc);
//   end
//
// This is the runtime-verification sibling of dynamic_array_stream.sv, and
// it deliberately reorders the stream operands to (i_header, i_len, i_crc,
// i_data) instead of that file's (i_header, i_len, i_data, i_crc) -- so this
// file's own operand-order corner must be checked independently, not
// assumed identical to the plain sibling. It also individually populates
// each element of the dynamic array via per-index BitSelect assignments
// (i_data[0..4]) after allocation, a corner the plain sibling never
// exercises (there, i_data is only ever allocated, never element-assigned).
//
// Checked:
//   - module top has the same 8 variables (6 int + 2 dynamic byte array) as
//     dynamic_array_stream.sv, none decl-assigned. All 8 are declared with
//     a plain data type and no net-type keyword (int/byte), so per IEEE
//     1800-2023 Sec 6.7/6.8 they are hldb::Variable, not hldb::Net -- this
//     holds regardless of `default_nettype, and a dynamic array cannot be
//     net-typed at all (nets are restricted to packed/integer types).
//   - module has exactly 1 process: an Initial whose Begin scope has
//       * exactly 1 local Variable "pkt" (queue of byte, same shape as the
//         plain sibling)
//       * exactly 14 statements, in source order:
//         0-2) i_header=12, i_len=5, i_crc=42 (plain scalar Assignments)
//         3) i_data = new[5] -- ordinary Assignment: lhs RefObj "i_data"
//            resolving to the Variable, rhs ArrayExpr[Constant "5"]
//         4-8) i_data[0]=1 .. i_data[4]=5: 5 Assignments whose **lhs is a
//            BitSelect** "i_data[N]" (prefix RefObj "i_data", index
//            Constant "N") and whose rhs is Constant "N+1" -- this is how
//            individual dynamic-array elements are populated once the
//            array has been sized, distinct from the "new[5]" allocation
//            itself
//         9) the pack: Assignment lhs RefObj "pkt", rhs Operation
//            (vpiStreamRLOp, slice size Constant "8") wrapping a
//            vpiConcatOp with exactly 4 operands in **this file's own
//            order**: RefObj i_header, i_len, i_crc, i_data (note: i_crc
//            before i_data, the opposite order from the plain sibling)
//         10) the unpack: Assignment whose lhs is the mirror-image
//            Operation (o_header, o_len, o_crc, o_data) and whose rhs is
//            RefObj "pkt"
//         11-13) three SysTaskCall "$display", each with 2 arguments: a
//            Constant assert-string and a RefObj resolving to o_header,
//            o_len, o_crc respectively (o_data itself is never displayed
//            -- out of scope by the source's own design, not a tooling gap)
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (signed),
//     StringTypespec
//   - compiler emits zero errors
//
// Not checked (GTEST_SKIP, with a real reason, not just "no time"):
//   - Whether o_header/o_len/o_crc actually equal 12/5/42 at runtime after
//     round-tripping through the pack/unpack and the "pkt" queue. HLC is a
//     compiler/elaborator, not a simulator: Variable::getValue<T>() only ever
//     exposes a declaration-time initializer, and o_header/o_len/o_crc are
//     declared bare (no initializer) and only ever assigned via the
//     streaming unpack -- there is no field anywhere that captures the
//     value that round-trip actually produced.

#include <string>

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_expr.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/byte_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class DynamicArrayStreamSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.14.4--dynamic_array_stream-sim.hlc"}); }
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

TEST_F(DynamicArrayStreamSimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(DynamicArrayStreamSimTest, ModuleHasEightVariablesNoneDeclAssigned) {
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

TEST_F(DynamicArrayStreamSimTest, InitialBeginHasOneLocalQueueVariablePkt) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getVariables(), nullptr);
  ASSERT_EQ(blk->getVariables()->size(), 1u);
  EXPECT_EQ(blk->getVariables()->at(0)->getName(), "pkt");
}

TEST_F(DynamicArrayStreamSimTest, InitialBeginHasFourteenStatements) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 14u);
}

TEST_F(DynamicArrayStreamSimTest, FirstThreeStatementsAssignScalarConstants) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);

  struct Expected {
    uint32_t index;
    const char *net;
    const char *value;
  };
  const Expected expected[3] = {{0u, "i_header", "12"}, {1u, "i_len", "5"}, {2u, "i_crc", "42"}};
  for (const Expected &exp : expected) {
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(exp.index));
    ASSERT_NE(assign, nullptr) << "statement " << exp.index;
    ASSERT_NE(assign->getLhs<hldb::RefObj>(), nullptr) << "statement " << exp.index;
    EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), exp.net);
    ASSERT_NE(assign->getRhs<hldb::Constant>(), nullptr) << "statement " << exp.index;
    EXPECT_EQ(assign->getRhs<hldb::Constant>()->getDecompile(), exp.value);
  }
}

TEST_F(DynamicArrayStreamSimTest, FourthStatementAssignsNewFiveToIDataLhs) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(3));
  ASSERT_NE(assign, nullptr);
  // "i_data = new[5];" is an ordinary assignment: the lhs is simply a
  // reference to the variable "i_data", exactly like any other assignment
  // statement -- "new[N]" only affects what the rhs expression is, it has
  // no special effect on how the lhs is captured.
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "i_data");
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr);

  const hldb::ArrayExpr *const rhs = assign->getRhs<hldb::ArrayExpr>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getExprs(), nullptr);
  ASSERT_EQ(rhs->getExprs()->size(), 1u);
  EXPECT_EQ(any_cast<hldb::Constant>(rhs->getExprs()->at(0))->getDecompile(), "5");
}

TEST_F(DynamicArrayStreamSimTest, NextFiveStatementsAssignEachElementViaBitSelect) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);

  for (uint32_t elem = 0; elem < 5u; ++elem) {
    const uint32_t stmtIndex = 4u + elem;
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(stmtIndex));
    ASSERT_NE(assign, nullptr) << "statement " << stmtIndex;

    const hldb::BitSelect *const lhs = assign->getLhs<hldb::BitSelect>();
    ASSERT_NE(lhs, nullptr) << "statement " << stmtIndex << ": lhs should be a BitSelect 'i_data[" << elem << "]'";
    EXPECT_EQ(lhs->getPrefix<hldb::RefObj>()->getName(), "i_data");
    EXPECT_NE(lhs->getPrefix<hldb::RefObj>()->getActual<hldb::Variable>(), nullptr);
    ASSERT_NE(lhs->getIndex<hldb::Constant>(), nullptr);
    EXPECT_EQ(lhs->getIndex<hldb::Constant>()->getDecompile(), std::to_string(elem));

    ASSERT_NE(assign->getRhs<hldb::Constant>(), nullptr) << "statement " << stmtIndex;
    EXPECT_EQ(assign->getRhs<hldb::Constant>()->getDecompile(), std::to_string(elem + 1));
  }
}

// --- pack: pkt = {<<8{i_header, i_len, i_crc, i_data}} ----------------------
// Note the operand order here (header, len, crc, data) is this file's own
// -- it differs from dynamic_array_stream.sv's (header, len, data, crc).

TEST_F(DynamicArrayStreamSimTest, TenthStatementPacksInThisFilesOwnOperandOrder) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(9));
  ASSERT_NE(assign, nullptr);
  ASSERT_NE(assign->getLhs<hldb::RefObj>(), nullptr);
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "pkt");
  EXPECT_NE(assign->getLhs<hldb::RefObj>()->getActual<hldb::Variable>(), nullptr);

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
  const char *const expectedOrder[4] = {"i_header", "i_len", "i_crc", "i_data"};
  for (uint32_t i = 0; i < 4u; ++i) {
    EXPECT_EQ(any_cast<hldb::RefObj>(concat->getOperands()->at(i))->getName(), expectedOrder[i])
        << "concat operand " << i;
  }
}

TEST_F(DynamicArrayStreamSimTest, EleventhStatementUnpacksInTheMirroredOperandOrder) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(10));
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
  const char *const expectedOrder[4] = {"o_header", "o_len", "o_crc", "o_data"};
  for (uint32_t i = 0; i < 4u; ++i) {
    EXPECT_EQ(any_cast<hldb::RefObj>(concat->getOperands()->at(i))->getName(), expectedOrder[i])
        << "concat operand " << i;
  }

  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "pkt");
  EXPECT_NE(rhs->getActual<hldb::Variable>(), nullptr);
}

// --- three $display assertions ----------------------------------------------

TEST_F(DynamicArrayStreamSimTest, LastThreeStatementsDisplayHeaderLenAndCrc) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);

  struct Expected {
    uint32_t index;
    const char *message;
    const char *net;
  };
  const Expected expected[3] = {
      {11u, ":assert: (12 == %d)", "o_header"},
      {12u, ":assert: (5 == %d)", "o_len"},
      {13u, ":assert: (42 == %d)", "o_crc"},
  };
  for (const Expected &exp : expected) {
    const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(exp.index));
    ASSERT_NE(disp, nullptr) << "statement " << exp.index;
    EXPECT_EQ(disp->getName(), "$display");
    ASSERT_NE(disp->getArguments(), nullptr);
    ASSERT_EQ(disp->getArguments()->size(), 2u);
    EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(), exp.message);
    EXPECT_EQ(any_cast<hldb::RefObj>(disp->getArguments()->at(1))->getName(), exp.net);
  }
}

// --- design-level typespecs / compiler diagnostics --------------------------

TEST_F(DynamicArrayStreamSimTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(DynamicArrayStreamSimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

// --- the actual point of the file: runtime round-trip values ---------------

TEST_F(DynamicArrayStreamSimTest, HeaderLenAndCrcRoundTripAtRuntime) {
  GTEST_SKIP() << "The source asserts that after packing i_header/i_len/"
                  "i_crc/i_data into 'pkt' and unpacking 'pkt' back into "
                  "o_header/o_len/o_data/o_crc, o_header == 12, o_len == 5, "
                  "and o_crc == 42. HLC is a static compiler/elaborator: "
                  "Variable::getValue<T>() only ever exposes a declaration-time "
                  "initializer, and o_header/o_len/o_crc are declared bare "
                  "and only ever assigned via the streaming unpack -- there "
                  "is no field capturing the value that round-trip actually "
                  "produced.";
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const oHeader = hldb::findByName<hldb::Variable>("o_header", top->getVariables());
  const hldb::Variable *const oLen = hldb::findByName<hldb::Variable>("o_len", top->getVariables());
  const hldb::Variable *const oCrc = hldb::findByName<hldb::Variable>("o_crc", top->getVariables());
  ASSERT_NE(oHeader, nullptr);
  ASSERT_NE(oLen, nullptr);
  ASSERT_NE(oCrc, nullptr);
  const hldb::Constant *const headerValue = oHeader->getValue<hldb::Constant>();
  const hldb::Constant *const lenValue = oLen->getValue<hldb::Constant>();
  const hldb::Constant *const crcValue = oCrc->getValue<hldb::Constant>();
  ASSERT_NE(headerValue, nullptr) << "no field captures o_header's post-unpack runtime value";
  ASSERT_NE(lenValue, nullptr) << "no field captures o_len's post-unpack runtime value";
  ASSERT_NE(crcValue, nullptr) << "no field captures o_crc's post-unpack runtime value";
  EXPECT_EQ(headerValue->getDecompile(), "12");
  EXPECT_EQ(lenValue->getDecompile(), "5");
  EXPECT_EQ(crcValue->getDecompile(), "42");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
