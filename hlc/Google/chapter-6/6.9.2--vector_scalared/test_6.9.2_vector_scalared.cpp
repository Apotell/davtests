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

// Tests for 6.9.2--vector_scalared.sv (tags: 6.9.2)
//   module top;
//     tri1 scalared [15:0] a = 0;
//   endmodule
//
// Checked:
//   - design has module top
//   - module has exactly 1 net: "a" (vpiNetType=vpiTri1, RefTypespec→LogicTypespec)
//   - LogicTypespec: vpiVector=true, 1 Range [15:0] (left=15, right=0)
//   - net initial value is Constant "0" (vpiUIntConst)
//   - top has no processes, no continuous assignments
//   - COMPILER BEHAVIOR: the `scalared` keyword is not stored — the UHDM dump
//     has no vpiScalared property, so getExplicitScalared() returns false
//     (mirrors the `vectored` modifier being silently dropped in vector_vectored.sv)
//   - const type of the initial value is vpiUIntConst (9)

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
#include <hldb/vpi_user.h>

namespace hlc {

class VectorScalared : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.9.2--vector_scalared.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(VectorScalared, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(VectorScalared, ModuleHasOneNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(VectorScalared, NetNameIsA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getName(), "a");
}

TEST_F(VectorScalared, NetTypeIsTri1) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getNetType(), vpiTri1);
}

TEST_F(VectorScalared, NetHasLogicTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(VectorScalared, LogicTypespecIsVector) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::LogicTypespec *const ls = rt->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_TRUE(ls->getVector());
}

TEST_F(VectorScalared, LogicTypespecHasOneRange) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::LogicTypespec *const ls = rt->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  ASSERT_NE(ls->getRanges(), nullptr);
  EXPECT_EQ(ls->getRanges()->size(), 1u);
}

TEST_F(VectorScalared, RangeLeftIs15) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
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

TEST_F(VectorScalared, RangeRightIs0) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
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

TEST_F(VectorScalared, NetHasInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_NE(net->getValue(), nullptr);
}

TEST_F(VectorScalared, NetInitialValueIsZero) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::Constant *const val = net->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getDecompile(), "0");
}

TEST_F(VectorScalared, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

TEST_F(VectorScalared, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

TEST_F(VectorScalared, NetIsNotExplicitlyScalared) {
  // COMPILER BEHAVIOR: HLC parses `scalared` without error but does not call
  // setExplicitScalared(true) -- the modifier is silently dropped in UHDM.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getExplicitScalared());
}

TEST_F(VectorScalared, NetInitialValueConstTypeIsUInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::Constant *const val = net->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getConstType(), vpiUIntConst);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
