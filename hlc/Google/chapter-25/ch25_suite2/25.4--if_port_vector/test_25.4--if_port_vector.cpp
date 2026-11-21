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

// IEEE 1800-2023 Sec 25.4 with Sec 23.2.2.3: "interface vec_if(input logic [7:0] data);" -- an
// input port with a packed data type keeps its range; with no net type or var it is a wire net.
// Source: tests/Google/chapter-25/ch25_suite2/25.4--if_port_vector.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/logic_typespec.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IfPortVectorTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.4--if_port_vector.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getInterface() {
    return hldb::findByDefName<hldb::Interface>("vec_if", m_design->getAllInterfaces());
  }

  static const hldb::Net *getDataNet() {
    const hldb::Interface *const iface = getInterface();
    if (iface == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>("data", iface->getNets());
  }
};

TEST_F(IfPortVectorTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfPortVectorTest, DataIsTheOnlyInputPort) {
  // Line 23: one port, "input ... data".
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getPorts(), nullptr);
  ASSERT_EQ(iface->getPorts()->size(), 1u);
  const hldb::Port *const p = iface->getPorts()->at(0);
  EXPECT_EQ(p->getName(), "data");
  EXPECT_EQ(p->getDirection(), vpiInput);
}

TEST_F(IfPortVectorTest, DataIsWireNet) {
  // Line 23: no net type or var, direction input -> a wire net (Sec 23.2.2.3).
  const hldb::Net *const data = getDataNet();
  ASSERT_NE(data, nullptr);
  EXPECT_EQ(data->getNetType(), vpiWire);
}

TEST_F(IfPortVectorTest, DataIsLogic7To0) {
  // Line 23: "logic [7:0]".
  const hldb::Net *const data = getDataNet();
  ASSERT_NE(data, nullptr);
  ASSERT_NE(data->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lts = data->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lts, nullptr);
  ASSERT_NE(lts->getRanges(), nullptr);
  ASSERT_EQ(lts->getRanges()->size(), 1u);
  const hldb::Range *const range = lts->getRanges()->at(0);
  ASSERT_NE(range->getLeftExpr<hldb::Constant>(), nullptr);
  ASSERT_NE(range->getRightExpr<hldb::Constant>(), nullptr);
  EXPECT_EQ(range->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(range->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
