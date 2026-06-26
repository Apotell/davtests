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
//   - design has module work@top
//   - module has exactly 1 net: "a" (RefTypespec→LogicTypespec)
//   - LogicTypespec: vpiVector=true, 1 Range [15:0] (left=15, right=0)
//   - net has no initial value (plain declaration, no initializer)
//   - work@top has no processes, no continuous assignments
//
// Not checked:
//   - vpiNetType not set (logic keyword — Surelog does not assign vpiWire; getNetType() returns 0)
//   - vpiScalared flag (not set)

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

class LogicVector : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.9.1--logic_vector.hlc"});

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

TEST_F(LogicVector, ModuleExists) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(LogicVector, ModuleHasOneNet) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(LogicVector, NetNameIsA) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getName(), "a");
}

TEST_F(LogicVector, NetHasNoInitialValue) {
  // `logic [15:0] a` — no initializer, vpiValue is absent
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->at(0)->getValue(), nullptr);
}

TEST_F(LogicVector, NetHasLogicTypespec) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(LogicVector, LogicTypespecIsVector) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::LogicTypespec *const ls =
      rt->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_TRUE(ls->getVector());
}

TEST_F(LogicVector, LogicTypespecHasOneRange) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::LogicTypespec *const ls =
      rt->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  ASSERT_NE(ls->getRanges(), nullptr);
  EXPECT_EQ(ls->getRanges()->size(), 1u);
}

TEST_F(LogicVector, RangeLeftIs15) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::LogicTypespec *const ls =
      rt->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  ASSERT_NE(ls->getRanges(), nullptr);
  const hldb::Range *const range = ls->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const left =
      range->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "15");
}

TEST_F(LogicVector, RangeRightIs0) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::LogicTypespec *const ls =
      rt->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  ASSERT_NE(ls->getRanges(), nullptr);
  const hldb::Range *const range = ls->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const right =
      range->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(LogicVector, NoProcesses) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

TEST_F(LogicVector, NoContAssigns) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

}  // namespace hlc
