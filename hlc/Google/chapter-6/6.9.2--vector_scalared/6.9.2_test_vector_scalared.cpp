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
//   - design has module work@top
//   - module has exactly 1 net: "a" (vpiNetType=vpiTri1, RefTypespec→LogicTypespec)
//   - LogicTypespec: vpiVector=true, 1 Range [15:0] (left=15, right=0)
//   - net initial value is Constant "0" (vpiUIntConst)
//   - work@top has no processes, no continuous assignments
//
// Not checked:
//   - vpiScalared flag on the net/typespec (scalared keyword behavior)
//   - const type of the initial value (vpiUIntConst=9)

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/logic_typespec.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/range.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class VectorScalared : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.9.2--vector_scalared.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

TEST_F(VectorScalared, ModuleExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(VectorScalared, ModuleHasOneNet) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(VectorScalared, NetNameIsA) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getName(), "a");
}

TEST_F(VectorScalared, NetTypeIsTri1) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getNetType(), vpiTri1);
}

TEST_F(VectorScalared, NetHasLogicTypespec) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const uhdm::RefTypespec *const rt = net->getTypespec<uhdm::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<uhdm::LogicTypespec>(), nullptr);
}

TEST_F(VectorScalared, LogicTypespecIsVector) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const uhdm::RefTypespec *const rt = net->getTypespec<uhdm::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const uhdm::LogicTypespec *const ls =
      rt->getActual<uhdm::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_TRUE(ls->getVector());
}

TEST_F(VectorScalared, LogicTypespecHasOneRange) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const uhdm::RefTypespec *const rt = net->getTypespec<uhdm::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const uhdm::LogicTypespec *const ls =
      rt->getActual<uhdm::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  ASSERT_NE(ls->getRanges(), nullptr);
  EXPECT_EQ(ls->getRanges()->size(), 1u);
}

TEST_F(VectorScalared, RangeLeftIs15) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const uhdm::RefTypespec *const rt = net->getTypespec<uhdm::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const uhdm::LogicTypespec *const ls =
      rt->getActual<uhdm::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  ASSERT_NE(ls->getRanges(), nullptr);
  const uhdm::Range *const range = ls->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  const uhdm::Constant *const left =
      range->getLeftExpr<uhdm::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "15");
}

TEST_F(VectorScalared, RangeRightIs0) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const uhdm::RefTypespec *const rt = net->getTypespec<uhdm::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const uhdm::LogicTypespec *const ls =
      rt->getActual<uhdm::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  ASSERT_NE(ls->getRanges(), nullptr);
  const uhdm::Range *const range = ls->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  const uhdm::Constant *const right =
      range->getRightExpr<uhdm::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(VectorScalared, NetHasInitialValue) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_NE(net->getValue(), nullptr);
}
 
TEST_F(VectorScalared, NetInitialValueIsZero) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const uhdm::Constant *const val = net->getValue<uhdm::Constant>();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getDecompile(), "0");
}

TEST_F(VectorScalared, NoProcesses) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

TEST_F(VectorScalared, NoContAssigns) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace SURELOG
