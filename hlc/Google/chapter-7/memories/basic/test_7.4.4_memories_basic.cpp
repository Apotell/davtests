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

// Tests for basic.sv (tags: 7.4.4)
//   module top ();
//     logic [7:0] mem [0:255];
//   endmodule
//
// Checked:
//   - design has module top with exactly 1 net: "mem"
//   - net "mem": RefTypespec -> ArrayTypespec static(1), unpacked range
//     [0:255], elem -> LogicTypespec
//   - "mem"'s elem LogicTypespec: exactly 1 packed Range [7:0], vpiVector
//     true, vpiSigned false (logic is unsigned by default)
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed) -- no
//     StringTypespec since there is no initial block / $display
//   - module has no processes (pure declaration, no initial/always block)
//   - no continuous assignments
//   - compiler emits zero errors
//
// Not checked:
//   - none -- basic.sv is declaration-only with no runtime behavior to
//     defer to simulation

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class MemoriesBasicTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "basic.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / net --------------------------------------------------------------

TEST_F(MemoriesBasicTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(MemoriesBasicTest, ModuleHasOneNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(MemoriesBasicTest, NetMemIsArrayZeroToTwoFiveFiveOfLogicTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const mem = hldb::findByName<hldb::Net>("mem", top->getNets());
  ASSERT_NE(mem, nullptr);
  const hldb::ArrayTypespec *const at = mem->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 1);  // static = 1
  ASSERT_NE(at->getRange(), nullptr);
  EXPECT_EQ(at->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "0");
  EXPECT_EQ(at->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "255");
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(MemoriesBasicTest, MemElemLogicTypespecHasPackedRangeSevenToZeroAndIsVector) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const mem = hldb::findByName<hldb::Net>("mem", top->getNets());
  ASSERT_NE(mem, nullptr);
  const hldb::ArrayTypespec *const at = mem->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  const hldb::LogicTypespec *const elem = at->getElemTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(elem, nullptr);
  EXPECT_TRUE(elem->getVector());
  EXPECT_FALSE(elem->getSigned());
  ASSERT_NE(elem->getRanges(), nullptr);
  ASSERT_EQ(elem->getRanges()->size(), 1u);
  const hldb::Range *const range = elem->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  EXPECT_EQ(range->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(range->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

// --- design-level typespecs / compiler diagnostics ---------------------------

TEST_F(MemoriesBasicTest, DesignHasTwoTypespecs) {
  // No StringTypespec: basic.sv has no initial block / $display.
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(MemoriesBasicTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(MemoriesBasicTest, DesignHasSignedIntTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(MemoriesBasicTest, ModuleHasNoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

TEST_F(MemoriesBasicTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(MemoriesBasicTest, NoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
