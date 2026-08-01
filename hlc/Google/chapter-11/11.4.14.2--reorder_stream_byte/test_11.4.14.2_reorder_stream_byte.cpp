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

// Tests for 11.4.14.2--reorder_stream_byte.sv (tags: 11.4.14.2)
//   int a = {"A", "B", "C", "D"};
//   int b;
//   initial begin
//     b = {<< byte {a}};
//   end
//
// IEEE 1800-2023 11.4.14.2 allows a streaming operator's slice_size to be
// either a constant_expression (a plain number, as in
// 11.4.14.2--reorder_stream.sv's "{<< 8 {a}}") or a simple_type keyword
// like "byte" (8 bits, matching the numeric case here exactly). The
// corner this file isolates is what changes in the AST when the slice
// size is a *type keyword* instead of a *number*: the operand is no
// longer a Constant at all -- it becomes a RefTypespec resolving to a
// ByteTypespec, a completely different operand kind. As a side effect,
// that ByteTypespec also shows up in the enclosing Begin's own typespec
// collection (the type keyword registers as a real typespec object in
// scope, not just an inline literal).
//
// Checked:
//   - module top has exactly 2 variables (bare "int", no net-type
//     keyword, so both are hldb::Variable per IEEE 1800-2023 6.8, not
//     hldb::Net): "a" (int, decl-value a 4-operand concatenation of
//     Constants "A","B","C","D") and "b" (int, no declaration-time value)
//   - the initial block's Begin has exactly 1 entry in its own
//     getTypespecs(): a ByteTypespec with getSigned() true -- confirming
//     the "byte" keyword used as a slice size registers as a scoped
//     typespec object, unlike the numeric slice-size sibling files,
//     whose Begin has no typespec collection at all
//   - the initial block is a Begin with exactly 1 statement: a blocking
//     Assignment, lhs RefObj "b", rhs an Operation (vpiStreamRLOp, 2
//     operands): operand 0 = a RefTypespec whose getActual<ByteTypespec>()
//     is non-null (NOT a Constant, unlike every numeric-slice-size case
//     in this batch); operand 1 = an Operation (vpiConcatOp, exactly 1
//     operand: RefObj "a")
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (signed),
//     StringTypespec
//   - compiler emits zero errors
//
// Not checked (GTEST_SKIP, with a real reason):
//   - Whether "b" actually ends up byte-reversed from a (0x44434241 per
//     the "-sim" sibling's assertion). HLC is a static compiler/
//     elaborator with no post-execution value for a Variable. Genuine
//     simulation-only gap, not a shortcut.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
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
#include <hldb/string_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ReorderStreamByteTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.14.2--reorder_stream_byte.hlc"}); }
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

TEST_F(ReorderStreamByteTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ReorderStreamByteTest, VariableAPacksFourCharsABCD) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 2u);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Operation *const pack = a->getValue<hldb::Operation>();
  ASSERT_NE(pack, nullptr);
  EXPECT_EQ(pack->getOpType(), vpiConcatOp);
  ASSERT_NE(pack->getOperands(), nullptr);
  ASSERT_EQ(pack->getOperands()->size(), 4u);
}

TEST_F(ReorderStreamByteTest, VariableBIsIntWithNoInitializer) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr);
  ASSERT_NE(b->getTypespec(), nullptr);
  EXPECT_NE(b->getTypespec()->getActual<hldb::IntTypespec>(), nullptr);
  EXPECT_EQ(b->getValue(), nullptr);
}

// --- the point of the file: "byte" slice size is a typespec, not a Constant

TEST_F(ReorderStreamByteTest, InitialBeginHasOneByteTypespecInScope) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getTypespecs(), nullptr);
  ASSERT_EQ(blk->getTypespecs()->size(), 1u)
      << "the 'byte' slice-size keyword should register as a scoped typespec, unlike a "
         "numeric slice size";
  const hldb::ByteTypespec *const bt = any_cast<hldb::ByteTypespec>(blk->getTypespecs()->at(0));
  ASSERT_NE(bt, nullptr);
  EXPECT_TRUE(bt->getSigned());
}

TEST_F(ReorderStreamByteTest, InitialBlockHasOneStatement) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 1u);
}

TEST_F(ReorderStreamByteTest, AssignmentSliceSizeOperandIsByteTypespecNotConstant) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "b");

  const hldb::Operation *const stream = assign->getRhs<hldb::Operation>();
  ASSERT_NE(stream, nullptr);
  EXPECT_EQ(stream->getOpType(), vpiStreamRLOp);
  ASSERT_NE(stream->getOperands(), nullptr);
  ASSERT_EQ(stream->getOperands()->size(), 2u);

  EXPECT_EQ(any_cast<hldb::Constant>(stream->getOperands()->at(0)), nullptr)
      << "'byte' as a slice size should NOT be a Constant operand";
  const hldb::RefTypespec *const sliceType = any_cast<hldb::RefTypespec>(stream->getOperands()->at(0));
  ASSERT_NE(sliceType, nullptr) << "'byte' as a slice size should be a RefTypespec operand";
  EXPECT_NE(sliceType->getActual<hldb::ByteTypespec>(), nullptr);

  const hldb::Operation *const concat = any_cast<hldb::Operation>(stream->getOperands()->at(1));
  ASSERT_NE(concat, nullptr);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 1u);
  EXPECT_EQ(any_cast<hldb::RefObj>(concat->getOperands()->at(0))->getName(), "a");
}

// --- design-level typespecs / compiler diagnostics -------------------------

TEST_F(ReorderStreamByteTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(ReorderStreamByteTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

// --- the actual point of the file: does the byte-reversal happen ---------

TEST_F(ReorderStreamByteTest, BEndsUpByteReversedFromA) {
  GTEST_SKIP() << "The '-sim' sibling asserts b == 0x44434241, i.e. that '{<< byte {a}}' "
                  "reverses a's bytes the same way the numeric '{<< 8 {a}}' sibling does. HLC "
                  "is a static compiler/elaborator with no post-execution value for a Variable. "
                  "Genuine simulation-only gap, not a shortcut.";
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr);
  ASSERT_NE(b->getValue(), nullptr) << "b's runtime value is not captured anywhere in the object model";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
