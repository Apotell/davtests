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

// Tests for 6.9.1--logic_vector.sv (tags: 6.9.1)
//   module top;
//     logic [15:0] a;
//   endmodule
//
// Checked:
//   - design has module top
//   - module has exactly 1 variable: "a" (RefTypespec->LogicTypespec)
//   - LogicTypespec: vpiVector=true, 1 Range [15:0] (left=15, right=0)
//   - variable has no initial value (plain declaration, no initializer)
//   - top has no processes, no continuous assignments

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class LogicVector : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.9.1--logic_vector.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(LogicVector, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(LogicVector, ModuleHasOneVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

TEST_F(LogicVector, VariableNameIsA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  const hldb::Variable *const var = top->getVariables()->at(0);
  ASSERT_NE(var, nullptr);
  EXPECT_EQ(var->getName(), "a");
}

TEST_F(LogicVector, VariableHasNoInitialValue) {
  // `logic [15:0] a` -- no initializer, vpiValue is absent
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->at(0)->getValue(), nullptr);
}

TEST_F(LogicVector, VariableIsVector) {
  // `logic [15:0] a` -- explicit packed dimension makes this a vector type
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_NE(top->getVariables()->at(0), nullptr);
  EXPECT_TRUE(top->getVariables()->at(0)->getVector());
}

// IEEE 1800-2023 Sec 6.7/6.8: `logic [15:0] a` has no net-type keyword, so it
// is a Variable, never a Net -- confirm the name is absent from the Net
// collection.
TEST_F(LogicVector, VariableNameIsNotInNets) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || hldb::findByName<hldb::Net>("a", top->getNets()) == nullptr)
      << "'a' has no net-type keyword; it must not appear in the module's Net collection";
}

TEST_F(LogicVector, VariableHasLogicTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  const hldb::Variable *const var = top->getVariables()->at(0);
  ASSERT_NE(var, nullptr);
  const hldb::RefTypespec *const rt = var->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(LogicVector, LogicTypespecIsVector) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  const hldb::Variable *const var = top->getVariables()->at(0);
  ASSERT_NE(var, nullptr);
  const hldb::RefTypespec *const rt = var->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::LogicTypespec *const ls = rt->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_TRUE(ls->getVector());
}

TEST_F(LogicVector, LogicTypespecHasOneRange) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  const hldb::Variable *const var = top->getVariables()->at(0);
  ASSERT_NE(var, nullptr);
  const hldb::RefTypespec *const rt = var->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::LogicTypespec *const ls = rt->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  ASSERT_NE(ls->getRanges(), nullptr);
  EXPECT_EQ(ls->getRanges()->size(), 1u);
}

TEST_F(LogicVector, RangeLeftIs15) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  const hldb::Variable *const var = top->getVariables()->at(0);
  ASSERT_NE(var, nullptr);
  const hldb::RefTypespec *const rt = var->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::LogicTypespec *const ls = rt->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  ASSERT_NE(ls->getRanges(), nullptr);
  const hldb::Range *const range = ls->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const left = range->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "15");
}

TEST_F(LogicVector, RangeRightIs0) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  const hldb::Variable *const var = top->getVariables()->at(0);
  ASSERT_NE(var, nullptr);
  const hldb::RefTypespec *const rt = var->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::LogicTypespec *const ls = rt->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  ASSERT_NE(ls->getRanges(), nullptr);
  const hldb::Range *const range = ls->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const right = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(LogicVector, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

TEST_F(LogicVector, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
