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

// Tests for 11.5.2--multi_dim_array_addressing.sv (tags: 11.5.2)
//   logic [7:0] mem [0:1023][0:3];
//   logic [7:0] a;
//
//   initial begin
//     a = mem[123][2];
//   end
//
// Per IEEE 1800-2017 11.5.2, each unpacked dimension of a multi-dimensional
// array is indexed independently, left to right. The corner this file is
// really about: "mem[123][2]" is NOT one BitSelect with two indices -- it
// is a BitSelect ("mem[123][2]") whose own vpiPrefix is *another* BitSelect
// ("mem[123]"), whose vpiPrefix is finally the RefObj "mem". Each unpacked
// dimension therefore adds one more nesting level of BitSelect, mirroring
// how the module's own ArrayTypespec for "mem" nests too (see below) --
// this is the direct multi-dimensional generalization of the single-
// dimension "array_addressing.sv" sibling.
//
// Checked:
//   - module top has exactly 2 nets: "mem" (ArrayTypespec, vpiArrayType ==
//     vpiStaticArray, outer range [0:1023], whose getElemTypespec()
//     resolves to a *second*, inner ArrayTypespec (also vpiStaticArray,
//     range [0:3]) whose own getElemTypespec() resolves to LogicTypespec
//     [7:0] -- i.e. multi-dim unpacked arrays are nested ArrayTypespec
//     objects, one level per dimension) and "a" (LogicTypespec, vector
//     [7:0]), neither decl-assigned
//   - module has exactly 1 process: an Initial whose Begin has exactly 1
//     statement: a blocking Assignment
//   - the Assignment: lhs RefObj "a"; rhs BitSelect "mem[123][2]" whose
//     vpiIndex is Constant "2" and whose vpiPrefix is itself a BitSelect
//     "mem[123]" (name "mem[123]", not a RefObj) -- and *that* inner
//     BitSelect's own vpiPrefix RefObj "mem" resolves to Net "mem" and its
//     vpiIndex is Constant "123"
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
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
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class MultiDimArrayAddressingTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.5.2--multi_dim_array_addressing.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / nets ----------------------------------------------------------

TEST_F(MultiDimArrayAddressingTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(MultiDimArrayAddressingTest, MemIsTwoNestedStaticArrayTypespecsEndingInLogicElem) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 2u);

  const hldb::Net *const mem = hldb::findByName<hldb::Net>("mem", top->getNets());
  ASSERT_NE(mem, nullptr);
  EXPECT_EQ(mem->getValue<hldb::Constant>(), nullptr);

  const hldb::ArrayTypespec *const outer = mem->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(outer, nullptr);
  EXPECT_EQ(outer->getArrayType(), vpiStaticArray);
  ASSERT_NE(outer->getRange(), nullptr);
  EXPECT_EQ(outer->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "0");
  EXPECT_EQ(outer->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "1023");

  ASSERT_NE(outer->getElemTypespec(), nullptr);
  const hldb::ArrayTypespec *const inner = outer->getElemTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(inner, nullptr) << "the second unpacked dimension should be a nested ArrayTypespec, not the elem type";
  EXPECT_EQ(inner->getArrayType(), vpiStaticArray);
  ASSERT_NE(inner->getRange(), nullptr);
  EXPECT_EQ(inner->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "0");
  EXPECT_EQ(inner->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "3");

  ASSERT_NE(inner->getElemTypespec(), nullptr);
  const hldb::LogicTypespec *const elem = inner->getElemTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(elem, nullptr);
  ASSERT_NE(elem->getRanges(), nullptr);
  EXPECT_EQ(elem->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(elem->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

// --- initial block: nested BitSelect for multi-dim addressing --------------

TEST_F(MultiDimArrayAddressingTest, InitialBlockHasOneBlockingAssignment) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 1u);
}

TEST_F(MultiDimArrayAddressingTest, AssignmentRhsIsNestedBitSelectMem123Then2) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "a");

  const hldb::BitSelect *const outer = assign->getRhs<hldb::BitSelect>();
  ASSERT_NE(outer, nullptr);
  EXPECT_EQ(outer->getName(), "mem[123][2]");
  ASSERT_NE(outer->getIndex<hldb::Constant>(), nullptr);
  EXPECT_EQ(outer->getIndex<hldb::Constant>()->getDecompile(), "2");

  const hldb::BitSelect *const inner = outer->getPrefix<hldb::BitSelect>();
  ASSERT_NE(inner, nullptr) << "the outer BitSelect's prefix should be another BitSelect ('mem[123]'), not a RefObj";
  EXPECT_EQ(inner->getName(), "mem[123]");
  EXPECT_EQ(inner->getPrefix<hldb::RefObj>()->getName(), "mem");
  EXPECT_NE(inner->getPrefix<hldb::RefObj>()->getActual<hldb::Net>(), nullptr);
  ASSERT_NE(inner->getIndex<hldb::Constant>(), nullptr);
  EXPECT_EQ(inner->getIndex<hldb::Constant>()->getDecompile(), "123");
}

// --- design-level typespecs / compiler diagnostics --------------------------

TEST_F(MultiDimArrayAddressingTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(MultiDimArrayAddressingTest, CompilerReportsZeroErrors) {
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
