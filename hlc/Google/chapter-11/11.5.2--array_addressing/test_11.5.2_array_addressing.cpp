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

// Tests for 11.5.2--array_addressing.sv (tags: 11.5.2)
//   logic [7:0] mem [0:1023];
//   logic [7:0] a;
//
//   initial begin
//     a = mem[123];
//   end
//
// Per IEEE 1800-2017 11.5.2, "mem[123]" indexes into the *unpacked*
// dimension of a statically-sized array, distinct from the packed
// bit-select corners exercised in the 11.5.1 files: here the indexed
// object ("mem") is itself an ArrayTypespec variable, not a plain vector
// variable.
//
// IEEE 1800-2023 6.7/6.8: "logic [7:0] mem [0:1023]" and "logic [7:0] a"
// have no net-type keyword (wire/tri/.../nettype), so per the standard
// they are variable_declarations, not net_declarations. Both must be
// modeled as hldb::Variable, found via Module::getVariables(), not as
// hldb::Net / Module::getNets().
//
// Checked:
//   - module top has exactly 2 variables: "mem" (ArrayTypespec, vpiArrayType
//     == vpiStaticArray, unpacked range [0:1023], elem typespec LogicTypespec
//     [7:0]) and "a" (LogicTypespec, vector [7:0]), neither decl-assigned
//   - module has exactly 1 process: an Initial whose Begin has exactly 1
//     statement: a blocking Assignment
//   - the Assignment: lhs RefObj "a" resolving to Variable "a"; rhs BitSelect
//     "mem[123]" whose vpiPrefix RefObj "mem" resolves to Variable "mem" and
//     whose vpiIndex is Constant "123" -- indexing a static array uses the
//     same BitSelect node as indexing a packed bit-vector
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
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ArrayAddressingTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.5.2--array_addressing.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / variables -------------------------------------------------------

TEST_F(ArrayAddressingTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ArrayAddressingTest, ModuleHasStaticArrayMemAndScalarVariableANeitherDeclAssigned) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 2u);

  const hldb::Variable *const mem = hldb::findByName<hldb::Variable>("mem", top->getVariables());
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(mem, nullptr);
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(mem->getValue<hldb::Constant>(), nullptr);
  EXPECT_EQ(a->getValue<hldb::Constant>(), nullptr);

  const hldb::ArrayTypespec *const memType = mem->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(memType, nullptr);
  EXPECT_EQ(memType->getArrayType(), vpiStaticArray);
  ASSERT_NE(memType->getRange(), nullptr);
  EXPECT_EQ(memType->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "0");
  EXPECT_EQ(memType->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "1023");
  ASSERT_NE(memType->getElemTypespec(), nullptr);
  const hldb::LogicTypespec *const elem = memType->getElemTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(elem, nullptr);
  ASSERT_NE(elem->getRanges(), nullptr);
  EXPECT_EQ(elem->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(elem->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");

  const hldb::LogicTypespec *const aType = a->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(aType, nullptr);
  ASSERT_NE(aType->getRanges(), nullptr);
  EXPECT_EQ(aType->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(aType->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

// --- initial block: static-array addressing ---------------------------------

TEST_F(ArrayAddressingTest, InitialBlockHasOneBlockingAssignment) {
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

  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "a");
}

TEST_F(ArrayAddressingTest, AssignmentRhsIsBitSelectMemOfOneTwentyThree) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);

  const hldb::BitSelect *const sel = assign->getRhs<hldb::BitSelect>();
  ASSERT_NE(sel, nullptr);
  EXPECT_EQ(sel->getName(), "mem[123]");
  EXPECT_EQ(sel->getPrefix<hldb::RefObj>()->getName(), "mem");
  EXPECT_NE(sel->getPrefix<hldb::RefObj>()->getActual<hldb::Variable>(), nullptr);
  ASSERT_NE(sel->getIndex<hldb::Constant>(), nullptr);
  EXPECT_EQ(sel->getIndex<hldb::Constant>()->getDecompile(), "123");
}

// --- design-level typespecs / compiler diagnostics --------------------------

TEST_F(ArrayAddressingTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(ArrayAddressingTest, CompilerReportsZeroErrors) {
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
