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

// Tests for 11.4.14.3--unpack_stream_pad.sv (tags: 11.4.14.3)
//   int a = 1;
//   int b = 2;
//   int c = 3;
//   initial begin
//     bit [127:0] d = {<<{a, b, c}};
//   end
//
// This is the "destination wider than source" corner of IEEE 1800-2023
// 11.4.14 (general rule, restated in 11.4.14.3), deliberately contrasted
// against its two siblings in this batch: 11.4.14.3--unpack_stream.sv
// (destination exactly matches the 96-bit source width) and
// 11.4.14.3--unpack_stream_inv.sv (destination narrower than the source
// -- illegal, and a confirmed compiler gap). Here "d" is 128 bits, 32
// bits *more* than a+b+c's combined 96 bits; per IEEE 1800-2023 11.4.14,
// "If the target represents a fixed-size variable that is wider than
// the stream, the stream shall be widened to match it by filling with
// zero bits on the right" -- so this is legal and the extra bits are
// zero-padded (unlike a too-narrow destination, which must error). The
// AST shape is otherwise identical to unpack_stream.sv's -- only the
// destination's declared width differs -- so this test is the direct
// structural counterpoint proving the batch's "should_fail_because" file
// really is the odd one out.
//
// Checked:
//   - module top has exactly 3 variables (bare "int", no net-type
//     keyword, so hldb::Variable per IEEE 1800-2023 6.8, not hldb::Net),
//     "a", "b", "c", each int with a declaration-time getValue<Constant>()
//     of "1", "2", "3"
//   - the initial block's Begin has exactly 1 entry in its own
//     getVariables(): a local Variable "d" whose typespec is a
//     BitTypespec with range [127:0] -- 32 bits wider than the 96-bit
//     source, confirming this is the "padded" (legal, over-sized)
//     destination case
//   - the Begin's own getTypespecs() also has exactly 1 matching
//     [127:0] BitTypespec entry
//   - "d"'s getValue<Operation>() has the same shape as
//     unpack_stream.sv: Operation (vpiStreamRLOp, 1 operand -- no
//     explicit slice size): Operation (vpiConcatOp, 3 operands: RefObj
//     "a", "b", "c")
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors -- and here that IS the IEEE-correct
//     outcome, since a wider-than-needed fixed-size destination is
//     legal (unlike the narrower-than-needed unpack_stream_inv.sv case)
//
// Not checked:
//   - the actual runtime value packed into "d" (that a,b,c occupy 96 of
//     its 128 bits and the remaining 32 bits are zero-padded per IEEE
//     1800-2023 11.4.14). This file has no $display assertion of its
//     own (unlike its "-sim" sibling, 11.4.14.3--unpack_stream_pad-sim.sv,
//     tested separately), so there is no author-declared expected value
//     to check even in principle.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
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

class UnpackStreamPadTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.14.3--unpack_stream_pad.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }
};

// --- module / variables -------------------------------------------------

TEST_F(UnpackStreamPadTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(UnpackStreamPadTest, ModuleHasThreeIntVariablesOneTwoThree) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 3u);
  const char *const names[3] = {"a", "b", "c"};
  const char *const values[3] = {"1", "2", "3"};
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>(names[i], top->getVariables());
    ASSERT_NE(var, nullptr) << "variable " << names[i];
    ASSERT_NE(var->getValue<hldb::Constant>(), nullptr);
    EXPECT_EQ(var->getValue<hldb::Constant>()->getDecompile(), values[i]);
  }
}

// --- the point of the file: destination wider than the source is legal ---

TEST_F(UnpackStreamPadTest, LocalVariableDIsOneHundredTwentyEightBitBit) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getVariables(), nullptr);
  ASSERT_EQ(blk->getVariables()->size(), 1u);
  const hldb::Variable *const d = hldb::findByName<hldb::Variable>("d", blk->getVariables());
  ASSERT_NE(d, nullptr);
  ASSERT_NE(d->getTypespec(), nullptr);
  const hldb::BitTypespec *const bt = d->getTypespec()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  ASSERT_NE(bt->getRanges(), nullptr);
  ASSERT_EQ(bt->getRanges()->size(), 1u);
  EXPECT_EQ(bt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "127")
      << "128 bits is 32 bits wider than a+b+c's combined 96 bits -- the legal 'padded' case";
  EXPECT_EQ(bt->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(UnpackStreamPadTest, InitialBeginHasOneMatchingBitTypespecInScope) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getTypespecs(), nullptr);
  ASSERT_EQ(blk->getTypespecs()->size(), 1u);
  const hldb::BitTypespec *const bt = any_cast<hldb::BitTypespec>(blk->getTypespecs()->at(0));
  ASSERT_NE(bt, nullptr);
  EXPECT_EQ(bt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "127");
}

TEST_F(UnpackStreamPadTest, DValueIsStreamRLWithNoSliceSizeOperand) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::Variable *const d = hldb::findByName<hldb::Variable>("d", blk->getVariables());
  ASSERT_NE(d, nullptr);

  const hldb::Operation *const stream = d->getValue<hldb::Operation>();
  ASSERT_NE(stream, nullptr);
  EXPECT_EQ(stream->getOpType(), vpiStreamRLOp);
  ASSERT_NE(stream->getOperands(), nullptr);
  ASSERT_EQ(stream->getOperands()->size(), 1u);

  const hldb::Operation *const concat = any_cast<hldb::Operation>(stream->getOperands()->at(0));
  ASSERT_NE(concat, nullptr);
  EXPECT_EQ(concat->getOpType(), vpiConcatOp);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 3u);
  const char *const names[3] = {"a", "b", "c"};
  for (uint32_t i = 0; i < 3u; ++i) {
    EXPECT_EQ(any_cast<hldb::RefObj>(concat->getOperands()->at(i))->getName(), names[i]);
  }
}

// --- design-level typespecs / compiler diagnostics -------------------------

TEST_F(UnpackStreamPadTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(UnpackStreamPadTest, CompilerReportsZeroErrors) {
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
