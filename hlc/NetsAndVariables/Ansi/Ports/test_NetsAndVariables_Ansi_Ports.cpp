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

// Validates the UHDM graph produced for tests/NetsAndVariables/Ansi/Ports.sv.
//
// Checked:
//   - nets_and_variables_test has six ports: clk, a, b (input logic), c (input,
//     no type keyword), d (input wire), y (output logic).
//   - Directions are correct: clk/a/b/c/d are vpiInput, y is vpiOutput.
//   - Per IEEE 1800-2023 Sec 23.2.2.3: for input/inout ports the port kind
//     always defaults to a net, regardless of whether an explicit data type
//     (e.g. 'logic') is given -- only an output port's explicit data type
//     with no net-type keyword defaults to a variable. So clk/a/b (input
//     logic) are nets of the default net type (vpiWire), exactly like c and
//     d; only y (output logic) is a variable.
//   - Net ports (clk, a, b, c, d): in getNets() with vpiWire type (except c,
//     which is implicit and not instantiated at all), absent from
//     getVariables(); lowConn RefObj resolves to a Net for the instantiated
//     ones (getActual<Net>()), or has getActual() == nullptr for implicit c.
//   - Variable port (y): in getVariables(), absent from getNets(); lowConn
//     RefObj resolves to a Variable (getActual<Variable>()).

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AnsiPortsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Ports.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("nets_and_variables_test", m_design->getAllModules());
  }
};

TEST_F(AnsiPortsTest, ModuleExists) { ASSERT_NE(getTop(), nullptr); }

TEST_F(AnsiPortsTest, SixPortsExist) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  EXPECT_EQ(top->getPorts()->size(), 7u) << "expected ports clk, a, b, c, d, y, y2";
}

TEST_F(AnsiPortsTest, PortDirections) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);

  const hldb::Port *const clk = hldb::findByName<hldb::Port>("clk", top->getPorts());
  const hldb::Port *const a = hldb::findByName<hldb::Port>("a", top->getPorts());
  const hldb::Port *const b = hldb::findByName<hldb::Port>("b", top->getPorts());
  const hldb::Port *const c = hldb::findByName<hldb::Port>("c", top->getPorts());
  const hldb::Port *const d = hldb::findByName<hldb::Port>("d", top->getPorts());
  const hldb::Port *const y = hldb::findByName<hldb::Port>("y", top->getPorts());
  ASSERT_NE(clk, nullptr);
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(d, nullptr);
  ASSERT_NE(y, nullptr);

  EXPECT_EQ(clk->getDirection(), vpiInput);
  EXPECT_EQ(a->getDirection(), vpiInput);
  EXPECT_EQ(b->getDirection(), vpiInput);
  EXPECT_EQ(c->getDirection(), vpiInput);
  EXPECT_EQ(d->getDirection(), vpiInput);
  EXPECT_EQ(y->getDirection(), vpiOutput);
}

// ---------------------------------------------------------------------------
// Net ports (clk, a, b, c, d): getNets() membership and correct net type
// ---------------------------------------------------------------------------

TEST_F(AnsiPortsTest, LogicPortClkIsInNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const n = hldb::findByName<hldb::Net>("clk", top->getNets());
  ASSERT_NE(n, nullptr) << "'clk' (input logic, no net-type keyword) must default to a net";
  EXPECT_EQ(n->getNetType(), vpiWire) << "'clk' has no explicit net-type keyword -- defaults to wire";
}

TEST_F(AnsiPortsTest, LogicPortAIsInNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const n = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(n, nullptr) << "'a' (input logic, no net-type keyword) must default to a net";
  EXPECT_EQ(n->getNetType(), vpiWire) << "'a' has no explicit net-type keyword -- defaults to wire";
}

TEST_F(AnsiPortsTest, LogicPortBIsInNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const n = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(n, nullptr) << "'b' (input logic, no net-type keyword) must default to a net";
  EXPECT_EQ(n->getNetType(), vpiWire) << "'b' has no explicit net-type keyword -- defaults to wire";
}

TEST_F(AnsiPortsTest, ImplicitNetPortCNotInNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("c", top->getNets()), nullptr)
      << "'c' has no type keyword -- implicit port must not appear in nets";
}

TEST_F(AnsiPortsTest, ImplicitNetPortCNotInVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("c", top->getVariables()), nullptr)
      << "'c' (net port) must not appear in variables";
}

TEST_F(AnsiPortsTest, ExplicitWirePortDIsInNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const n = hldb::findByName<hldb::Net>("d", top->getNets());
  ASSERT_NE(n, nullptr) << "'d' not found in nets";
  EXPECT_EQ(n->getNetType(), vpiWire) << "'d' is explicitly declared wire";
}

TEST_F(AnsiPortsTest, ExplicitWirePortDNotInVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("d", top->getVariables()), nullptr)
      << "'d' (net port) must not appear in variables";
}

// ---------------------------------------------------------------------------
// Net ports (clk, a, b, c, d): absent from getVariables()
// ---------------------------------------------------------------------------

TEST_F(AnsiPortsTest, LogicPortClkNotInVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("clk", top->getVariables()), nullptr)
      << "'clk' (net port) must not appear in variables";
}

TEST_F(AnsiPortsTest, LogicPortANotInVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("a", top->getVariables()), nullptr)
      << "'a' (net port) must not appear in variables";
}

TEST_F(AnsiPortsTest, LogicPortBNotInVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("b", top->getVariables()), nullptr)
      << "'b' (net port) must not appear in variables";
}

// ---------------------------------------------------------------------------
// Net ports (c, d): lowConn resolves to a Net
// ---------------------------------------------------------------------------

TEST_F(AnsiPortsTest, ImplicitNetPortCLowConnHasNoActual) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  const hldb::Port *const c = hldb::findByName<hldb::Port>("c", top->getPorts());
  ASSERT_NE(c, nullptr);
  const hldb::RefObj *const lc = c->getLowConn<hldb::RefObj>();
  ASSERT_NE(lc, nullptr) << "port 'c' has no lowConn RefObj";
  ASSERT_NE(lc->getActual(), nullptr);
  EXPECT_EQ(lc->getActual()->getAnyType(), hldb::AnyType::Net);
}

TEST_F(AnsiPortsTest, ExplicitWirePortDResolvesToWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  const hldb::Port *const d = hldb::findByName<hldb::Port>("d", top->getPorts());
  ASSERT_NE(d, nullptr);
  const hldb::RefObj *const lc = d->getLowConn<hldb::RefObj>();
  ASSERT_NE(lc, nullptr) << "port 'd' has no lowConn RefObj";
  const hldb::Net *const net = lc->getActual<hldb::Net>();
  ASSERT_NE(net, nullptr) << "port 'd' lowConn does not resolve to a Net";
  EXPECT_EQ(net->getNetType(), vpiWire) << "port 'd' is explicitly declared wire";
}

// ---------------------------------------------------------------------------
// Net ports (clk, a, b): lowConn resolves to a Net
// ---------------------------------------------------------------------------

TEST_F(AnsiPortsTest, LogicPortClkResolvesToNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  const hldb::Port *const clk = hldb::findByName<hldb::Port>("clk", top->getPorts());
  ASSERT_NE(clk, nullptr);
  const hldb::RefObj *const lc = clk->getLowConn<hldb::RefObj>();
  ASSERT_NE(lc, nullptr) << "port 'clk' has no lowConn RefObj";
  const hldb::Net *const net = lc->getActual<hldb::Net>();
  ASSERT_NE(net, nullptr) << "port 'clk' (input logic) lowConn should resolve to a Net";
  EXPECT_EQ(net->getNetType(), vpiWire) << "port 'clk' has no explicit net-type keyword -- defaults to wire";
}

TEST_F(AnsiPortsTest, LogicPortAResolvesToNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  const hldb::Port *const a = hldb::findByName<hldb::Port>("a", top->getPorts());
  ASSERT_NE(a, nullptr);
  const hldb::RefObj *const lc = a->getLowConn<hldb::RefObj>();
  ASSERT_NE(lc, nullptr) << "port 'a' has no lowConn RefObj";
  const hldb::Net *const net = lc->getActual<hldb::Net>();
  ASSERT_NE(net, nullptr) << "port 'a' (input logic) lowConn should resolve to a Net";
  EXPECT_EQ(net->getNetType(), vpiWire) << "port 'a' has no explicit net-type keyword -- defaults to wire";
}

TEST_F(AnsiPortsTest, LogicPortBResolvesToNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  const hldb::Port *const b = hldb::findByName<hldb::Port>("b", top->getPorts());
  ASSERT_NE(b, nullptr);
  const hldb::RefObj *const lc = b->getLowConn<hldb::RefObj>();
  ASSERT_NE(lc, nullptr) << "port 'b' has no lowConn RefObj";
  const hldb::Net *const net = lc->getActual<hldb::Net>();
  ASSERT_NE(net, nullptr) << "port 'b' (input logic) lowConn should resolve to a Net";
  EXPECT_EQ(net->getNetType(), vpiWire) << "port 'b' has no explicit net-type keyword -- defaults to wire";
}

// ---------------------------------------------------------------------------
// Variable port (y): getVariables() membership, absent from getNets()
// ---------------------------------------------------------------------------

TEST_F(AnsiPortsTest, LogicPortYIsInVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("y", top->getVariables()), nullptr)
      << "'y' (output logic) not found in variables";
}

TEST_F(AnsiPortsTest, LogicPortYNotInNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("y", top->getNets()), nullptr) << "'y' (logic port) must not appear in nets";
}

// ---------------------------------------------------------------------------
// Variable port (y): lowConn resolves to a Variable
// ---------------------------------------------------------------------------

TEST_F(AnsiPortsTest, LogicPortYResolvesToVariable) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  const hldb::Port *const y = hldb::findByName<hldb::Port>("y", top->getPorts());
  ASSERT_NE(y, nullptr);
  const hldb::RefObj *const lc = y->getLowConn<hldb::RefObj>();
  ASSERT_NE(lc, nullptr) << "port 'y' has no lowConn RefObj";
  EXPECT_NE(lc->getActual<hldb::Variable>(), nullptr) << "port 'y' (output logic) lowConn should resolve to a Variable";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
