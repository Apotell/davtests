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
// Checked:
//   - design has module top with exactly 1 net: "q"
//   - net "q": ArrayTypespec vpiArrayType=queue(4), unpacked, ElemTypespec
//     -> IntTypespec (signed)
//   - the queue's range has a single left bound Constant "$" with
//     vpiConstType=unbounded; there is no right bound (unsized dimension)
//   - net "q" has no initial value and module "top" has no processes or
//     continuous assignments
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
#include <hldb/net.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>

namespace hlc {

class QueuesBasicTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "basic.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module / net ------------------------------------------------------------

TEST_F(QueuesBasicTest, ModuleExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(QueuesBasicTest, ModuleHasOneNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(QueuesBasicTest, NetQExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("q", top->getNets()), nullptr);
}

// --- net "q": unbounded queue of int -----------------------------------------

TEST_F(QueuesBasicTest, NetQTypespecIsArrayTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const q = hldb::findByName<hldb::Net>("q", top->getNets());
  ASSERT_NE(q, nullptr);
  ASSERT_NE(q->getTypespec(), nullptr) << "net 'q' has no typespec";
  EXPECT_NE(q->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>(), nullptr)
      << "net 'q' typespec does not resolve to an ArrayTypespec";
}

TEST_F(QueuesBasicTest, NetQArrayTypeIsQueue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const q = hldb::findByName<hldb::Net>("q", top->getNets());
  ASSERT_NE(q, nullptr);
  const hldb::ArrayTypespec *const at = q->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiQueueArray) << "7.10: 'int q[$]' must be modeled as a queue array";
}

TEST_F(QueuesBasicTest, NetQArrayIsNotPacked) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const q = hldb::findByName<hldb::Net>("q", top->getNets());
  ASSERT_NE(q, nullptr);
  const hldb::ArrayTypespec *const at = q->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_FALSE(at->getPacked()) << "a queue dimension is an unpacked dimension";
}

TEST_F(QueuesBasicTest, NetQRangeLeftIsUnboundedDollar) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const q = hldb::findByName<hldb::Net>("q", top->getNets());
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

TEST_F(QueuesBasicTest, NetQRangeHasNoRightExpr) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const q = hldb::findByName<hldb::Net>("q", top->getNets());
  ASSERT_NE(q, nullptr);
  const hldb::ArrayTypespec *const at = q->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  EXPECT_EQ(at->getRange()->getRightExpr(), nullptr)
      << "7.10: an unsized dimension has a single '$' bound, no right bound";
}

TEST_F(QueuesBasicTest, NetQElemTypespecIsIntTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const q = hldb::findByName<hldb::Net>("q", top->getNets());
  ASSERT_NE(q, nullptr);
  const hldb::ArrayTypespec *const at = q->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr) << "queue ArrayTypespec has no elemTypespec";
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "element type of 'int q[$]' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned()) << "'int' is a signed 32-bit type";
}

TEST_F(QueuesBasicTest, NetQHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const q = hldb::findByName<hldb::Net>("q", top->getNets());
  ASSERT_NE(q, nullptr);
  EXPECT_EQ(q->getValue(), nullptr) << "'int q[$]' has no initializer";
}

// --- module structural completeness -------------------------------------------

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

// --- design-level typespecs ----------------------------------------------------

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

// --- compiler diagnostics -----------------------------------------------------

TEST_F(QueuesBasicTest, CompilerReportsNoErrors) {
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
