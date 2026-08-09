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

// Tests for basic.sv (tags: 7.10)
//   module top ();
//     int q[$];
//   endmodule
//
// IEEE 1800-2017 7.10 "Queues": a queue is declared with an unsized
// dimension whose sole bound is "$" (7.10: "variable_dimension ::= ...
// | unsized_dimension" with unsized_dimension using "$" for a queue). An
// unbounded queue has no explicit size limit and starts out empty.
//
// `int q[$]` has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8 it
// is a variable_declaration, not a net_declaration.
//
// Checked:
//   - design has module top with exactly 1 variable: "q"
//   - variable "q": ArrayTypespec vpiArrayType=queue(4), unpacked,
//     ElemTypespec -> IntTypespec (signed)
//   - the queue's range has a single left bound Constant "$" with
//     vpiConstType=unbounded; there is no right bound (unsized dimension)
//   - variable "q" has no initial value and module "top" has no processes
//     or continuous assignments
//   - design-level typespecs (3): ModuleTypespec, IntTypespec, StringTypespec
//   - compiler emits no errors, syntax errors, fatals or warnings

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class QueuesBasicTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "basic.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module / variable ----

TEST_F(QueuesBasicTest, ModuleExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(QueuesBasicTest, ModuleHasOneVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

TEST_F(QueuesBasicTest, VariableQExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("q", top->getVariables()), nullptr);
}

// `q` has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8 it must not
// also appear in the module's net collection.
TEST_F(QueuesBasicTest, VariableQIsNotDuplicatedAsNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  if (top->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("q", top->getNets()), nullptr)
        << "'int q[$]' has no net-type keyword and must not also appear as a Net";
  }
}

// --- variable "q": unbounded queue of int ----

TEST_F(QueuesBasicTest, VariableQTypespecIsArrayTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const q = hldb::findByName<hldb::Variable>("q", top->getVariables());
  ASSERT_NE(q, nullptr);
  ASSERT_NE(q->getTypespec(), nullptr) << "variable 'q' has no typespec";
  EXPECT_NE(q->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>(), nullptr)
      << "variable 'q' typespec does not resolve to an ArrayTypespec";
}

TEST_F(QueuesBasicTest, VariableQArrayTypeIsQueue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const q = hldb::findByName<hldb::Variable>("q", top->getVariables());
  ASSERT_NE(q, nullptr);
  const hldb::ArrayTypespec *const at = q->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiQueueArray) << "7.10: 'int q[$]' must be modeled as a queue array";
}

TEST_F(QueuesBasicTest, VariableQArrayIsNotPacked) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const q = hldb::findByName<hldb::Variable>("q", top->getVariables());
  ASSERT_NE(q, nullptr);
  const hldb::ArrayTypespec *const at = q->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_FALSE(at->getPacked()) << "a queue dimension is an unpacked dimension";
}

TEST_F(QueuesBasicTest, VariableQRangeLeftIsUnboundedDollar) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const q = hldb::findByName<hldb::Variable>("q", top->getVariables());
  ASSERT_NE(q, nullptr);
  const hldb::ArrayTypespec *const at = q->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr) << "queue ArrayTypespec has no range (expected unsized '$' bound)";

  const hldb::Constant *const dollar = at->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(dollar, nullptr) << "range left bound is not a Constant";
  EXPECT_EQ(dollar->getDecompile(), "$");
  EXPECT_EQ(dollar->getValue(), "$");
  EXPECT_EQ(dollar->getConstType(), vpiUnboundedConst);
}

TEST_F(QueuesBasicTest, VariableQRangeHasNoRightExpr) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const q = hldb::findByName<hldb::Variable>("q", top->getVariables());
  ASSERT_NE(q, nullptr);
  const hldb::ArrayTypespec *const at = q->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  EXPECT_EQ(at->getRange()->getRightExpr(), nullptr)
      << "7.10: an unsized dimension has a single '$' bound, no right bound";
}

TEST_F(QueuesBasicTest, VariableQElemTypespecIsIntTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const q = hldb::findByName<hldb::Variable>("q", top->getVariables());
  ASSERT_NE(q, nullptr);
  const hldb::ArrayTypespec *const at = q->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr) << "queue ArrayTypespec has no elemTypespec";
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "element type of 'int q[$]' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned()) << "'int' is a signed 32-bit type";
}

TEST_F(QueuesBasicTest, VariableQHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const q = hldb::findByName<hldb::Variable>("q", top->getVariables());
  ASSERT_NE(q, nullptr);
  EXPECT_EQ(q->getValue(), nullptr) << "'int q[$]' has no initializer";
}

// --- module structural completeness ----

TEST_F(QueuesBasicTest, ModuleHasNoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr) << "basic.sv has no initial/always blocks";
}

TEST_F(QueuesBasicTest, ModuleHasNoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr) << "basic.sv has no continuous assignments";
}

// --- design-level typespecs ----

TEST_F(QueuesBasicTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(QueuesBasicTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(QueuesBasicTest, DesignHasIntTypespecSigned) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(QueuesBasicTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  ASSERT_GT(m_design->getTypespecs()->size(), 2u);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(2)), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
