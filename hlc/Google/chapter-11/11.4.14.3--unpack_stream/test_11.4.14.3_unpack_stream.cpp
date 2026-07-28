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

// Tests for 11.4.14.3--unpack_stream.sv (tags: 11.4.14.3)
//   int a = 1;
//   int b = 2;
//   int c = 3;
//   initial begin
//     bit [95:0] d = {<<{a, b, c}};
//   end
//
// IEEE 1800-2017 11.4.14.3 covers "unpacking": using a streaming
// concatenation on the RHS to pack several values into one destination
// in a single declaration ("bit [95:0] d = {<<{...}}"), the mirror image
// of "packing" three separate variables via "{>>...}" seen elsewhere in
// this batch. Two corners are unique to this file: first, "d" is
// declared *inside* the initial block, so it is a local hldb::Variable
// scoped to the enclosing Begin, not a module-level Net -- a genuinely
// different object kind with its own accessors. Second, "{<<{a, b, c}}"
// has NO explicit slice size (contrast with every other stream operator
// file in this batch, which all specify one, numeric or "byte") -- the
// question is whether that omission collapses the streaming Operation
// to a single operand (just the inner concatenation) rather than the
// two-operand [slice-size, concatenation] shape used everywhere else.
// The AST confirms it does: exactly 1 operand here, vs. 2 when a slice
// size is given.
//
// Checked:
//   - module work@top has exactly 3 nets, "a", "b", "c", each int with a
//     declaration-time getValue<Constant>() of "1", "2", "3"
//     respectively
//   - the initial block's Begin has exactly 1 entry in its own
//     getVariables(): a local Variable "d", whose getTypespec<RefTypespec>()
//     ->getActual<BitTypespec>() has range [95:0] -- confirming "d" is a
//     block-scoped Variable, not a Net, and that its declared width
//     (96 bits) exactly matches the combined width of a+b+c (3x32)
//   - the Begin's own getTypespecs() also has exactly 1 entry: the same
//     [95:0] BitTypespec, registered in scope alongside the Variable
//     that uses it
//   - "d"'s getValue<Operation>() is an Operation (vpiStreamRLOp) with
//     exactly 1 operand (no leading slice-size operand, unlike every
//     other stream file in this batch): an Operation (vpiConcatOp, 3
//     operands: RefObj "a", RefObj "b", RefObj "c", each resolving via
//     getActual<Net>() back to the corresponding module-level net)
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//     -- no StringTypespec, since this file has no $display
//   - compiler emits zero errors
//
// Not checked:
//   - the actual runtime value packed into "d". This file carries no
//     $display assertion of its own (unlike its "-sim" sibling,
//     11.4.14.3--unpack_stream-sim.sv, which does and is tested
//     separately), so there is no author-declared expected value to
//     check even in principle.

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
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class UnpackStreamTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.14.3--unpack_stream.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()); }
  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }
};

// --- module / nets -----------------------------------------------------

TEST_F(UnpackStreamTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(UnpackStreamTest, ModuleHasThreeIntNetsOneTwoThree) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 3u);
  const char *const names[3] = {"a", "b", "c"};
  const char *const values[3] = {"1", "2", "3"};
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(names[i], top->getNets());
    ASSERT_NE(net, nullptr) << "net " << names[i];
    EXPECT_NE(net->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
    ASSERT_NE(net->getValue<hldb::Constant>(), nullptr);
    EXPECT_EQ(net->getValue<hldb::Constant>()->getDecompile(), values[i]);
  }
}

// --- the point of the file: "d" is a block-scoped Variable, not a Net -----

TEST_F(UnpackStreamTest, InitialBeginDeclaresOneLocalVariableD) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getVariables(), nullptr);
  ASSERT_EQ(blk->getVariables()->size(), 1u);
  const hldb::Variable *const d = hldb::findByName<hldb::Variable>("d", blk->getVariables());
  ASSERT_NE(d, nullptr);
  const hldb::BitTypespec *const bt = d->getTypespec<hldb::RefTypespec>()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  ASSERT_NE(bt->getRanges(), nullptr);
  ASSERT_EQ(bt->getRanges()->size(), 1u);
  EXPECT_EQ(bt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "95");
  EXPECT_EQ(bt->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(UnpackStreamTest, InitialBeginHasOneMatchingBitTypespecInScope) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getTypespecs(), nullptr);
  ASSERT_EQ(blk->getTypespecs()->size(), 1u);
  const hldb::BitTypespec *const bt = any_cast<hldb::BitTypespec>(blk->getTypespecs()->at(0));
  ASSERT_NE(bt, nullptr);
  ASSERT_NE(bt->getRanges(), nullptr);
  EXPECT_EQ(bt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "95");
}

TEST_F(UnpackStreamTest, DValueIsStreamRLWithNoSliceSizeOperand) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::Variable *const d = hldb::findByName<hldb::Variable>("d", blk->getVariables());
  ASSERT_NE(d, nullptr);

  const hldb::Operation *const stream = d->getValue<hldb::Operation>();
  ASSERT_NE(stream, nullptr);
  EXPECT_EQ(stream->getOpType(), vpiStreamRLOp);
  ASSERT_NE(stream->getOperands(), nullptr);
  ASSERT_EQ(stream->getOperands()->size(), 1u)
      << "'{<<{...}}' with no explicit slice size should have only the concatenation operand, "
         "not a [slice-size, concatenation] pair";

  const hldb::Operation *const concat = any_cast<hldb::Operation>(stream->getOperands()->at(0));
  ASSERT_NE(concat, nullptr);
  EXPECT_EQ(concat->getOpType(), vpiConcatOp);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 3u);
  const char *const names[3] = {"a", "b", "c"};
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(concat->getOperands()->at(i));
    ASSERT_NE(ref, nullptr) << "operand index " << i;
    EXPECT_EQ(ref->getName(), names[i]);
    EXPECT_NE(ref->getActual<hldb::Net>(), nullptr);
  }
}

// --- design-level typespecs / compiler diagnostics -------------------------

TEST_F(UnpackStreamTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(UnpackStreamTest, CompilerReportsZeroErrors) {
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
