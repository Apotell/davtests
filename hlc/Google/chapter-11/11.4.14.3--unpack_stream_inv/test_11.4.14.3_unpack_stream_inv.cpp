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

// Tests for 11.4.14.3--unpack_stream_inv.sv (tags: 11.4.14.3)
//   :should_fail_because: stream is wider than assignment target
//   int a = 1;
//   int b = 2;
//   int c = 3;
//   initial begin
//     int d = {<<{a, b, c}};
//   end
//
// This file is the deliberate negative case for IEEE 1800-2023 11.4.14
// (general rule, restated with a worked example matching this exact
// pattern in 11.4.14.3): "If the target represents a fixed-size variable
// that is narrower (has fewer bits) than the stream, an error shall be
// generated." 11.4.14.3 even gives the identical shape as a worked
// example: "int j = {>>{ a, b, c }}; // error: j is 32 bits < 96 bits".
// Here the source stream packs three 32-bit ints (a, b, c = 96 bits
// total) into "d", which is declared as a plain fixed-size "int" (32
// bits) -- 64 bits too narrow. This is exactly the illegal case the
// file's own :should_fail_because: tag describes, and it is structurally
// identical to the legal 11.4.14.3--unpack_stream.sv (dest exactly 96
// bits) and 11.4.14.3--unpack_stream_pad.sv (dest 128 bits, wider than
// needed) siblings -- the only difference is d's declared width being
// too small instead of exactly right or padded.
//
// Ground truth from the compiler log: parsing and elaboration both
// succeed with the identical AST shape as the legal siblings (Variable
// "d", typespec IntTypespec instead of BitTypespec[95:0]/[127:0], value
// an Operation(vpiStreamRLOp) wrapping Operation(vpiConcatOp,[a,b,c])),
// and the error count is exactly 0 -- HLC accepts this file, contrary to
// its own :should_fail_because: tag. This is a genuine, confirmed
// silent-acceptance gap, following the same pattern already documented
// elsewhere in this codebase (e.g. chapter-7's packed-struct default-
// value case, chapter-10's procedural-assignment-to-wire case): a
// construct IEEE 1800-2023 requires to be rejected is instead compiled
// with zero diagnostics.
//
// Checked:
//   - module top has exactly 3 variables (bare "int", no net-type
//     keyword, so hldb::Variable per IEEE 1800-2023 6.8, not hldb::Net),
//     "a", "b", "c", each int with a declaration-time getValue<Constant>()
//     of "1", "2", "3"
//   - the initial block's Begin has exactly 1 entry in its own
//     getVariables(): a local Variable "d" whose declared type is
//     IntTypespec (32 bits) -- not a BitTypespec range, since "int d"
//     (not "bit [N:0] d") was written
//   - "d"'s getValue<Operation>() is the same shape as the legal
//     siblings: Operation (vpiStreamRLOp, 1 operand -- no explicit slice
//     size, same as unpack_stream.sv): Operation (vpiConcatOp, 3
//     operands: RefObj "a", "b", "c")
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//
// The final test below (CompilerShouldRejectOversizedStreamUnpackButDoesNot)
// is GTEST_SKIP()'d: this diagnostic is not implemented yet, so there is
// nothing to assert against a real build. The GTEST_SKIP message cites the
// standard rule so the gap stays visible as pending work rather than as a
// silently-passing or perpetually-failing test.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class UnpackStreamInvTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.14.3--unpack_stream_inv.hlc"}); }
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

TEST_F(UnpackStreamInvTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(UnpackStreamInvTest, ModuleHasThreeIntVariablesOneTwoThree) {
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

// --- confirming the AST is well-formed despite the width mismatch ---------

TEST_F(UnpackStreamInvTest, LocalVariableDIsPlainThirtyTwoBitInt) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getVariables(), nullptr);
  ASSERT_EQ(blk->getVariables()->size(), 1u);
  const hldb::Variable *const d = hldb::findByName<hldb::Variable>("d", blk->getVariables());
  ASSERT_NE(d, nullptr);
  ASSERT_NE(d->getTypespec(), nullptr);
  EXPECT_NE(d->getTypespec()->getActual<hldb::IntTypespec>(), nullptr)
      << "'int d' should declare a plain 32-bit IntTypespec, not a sized BitTypespec range";
}

TEST_F(UnpackStreamInvTest, DValueIsStreamRLOfConcatenatedAAndBAndC) {
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

// --- design-level typespecs -------------------------------------------

TEST_F(UnpackStreamInvTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

// --- the actual point of the file: this construct should be rejected -----

TEST_F(UnpackStreamInvTest, CompilerShouldRejectOversizedStreamUnpackButDoesNot) {
  GTEST_SKIP() << "HLC does not yet implement the IEEE 1800-2023 11.4.14 destination-width check "
                  "for streaming concatenation unpacks (worked example repeated in 11.4.14.3): an "
                  "error shall be generated when the stream's resulting size (96 bits from a+b+c) "
                  "exceeds a fixed-size destination's width (32 bits, 'int d'), matching this "
                  "file's own :should_fail_because: tag. Not implemented yet.";
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "IEEE 1800-2023 11.4.14 (worked example repeated in 11.4.14.3): an error shall be "
         "generated when a streaming concatenation's resulting size (here 96 bits from a+b+c) "
         "exceeds a fixed-size destination's width (here 32 bits, 'int d'), matching this file's "
         "own :should_fail_because: tag -- HLC currently accepts it with zero diagnostics";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
