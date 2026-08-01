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

// Validates the UHDM graph produced for tests/NetsAndVariables/Ansi/Ports.sv:
// the ANSI-style module header for nets_and_variables_test, split out of the
// combined NetsAndVariablesAnsi.sv suite so the port/direction testing point
// stands on its own.
//
// Checked:
//   - work@nets_and_variables_test exists with ports clk, a, b (logic, input),
//     c (implicit net port, input), d (explicit wire port, input), y (logic,
//     output), and y2 (implicit net port, output)
//   - c has no type keyword and so defaults to an implicit net (wire) under
//     default_nettype, matching the pattern established by
//     6.10--implicit_port in this suite
//   - d is an explicit net port declared with the 'wire' keyword
//   - y2 has no type keyword and, like an implicit input port, defaults to
//     an implicit net (wire) under default_nettype -- unlike input/inout
//     ports, an output port's kind (net vs variable) is otherwise inferred
//     from how it is driven inside the module, but with no such usage here
//     it falls back to the default net type, matching NonAnsi/Ports.sv's
//     already-established 'output y;' example

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AnsiPortsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "AnsiPorts.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("work@nets_and_variables_test", m_design->getAllModules());
  }
};

TEST_F(AnsiPortsTest, ModuleExists) { ASSERT_NE(getTop(), nullptr); }

TEST_F(AnsiPortsTest, SevenPortsExist) {
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
  const hldb::Port *const y2 = hldb::findByName<hldb::Port>("y2", top->getPorts());
  ASSERT_NE(clk, nullptr);
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(d, nullptr);
  ASSERT_NE(y, nullptr);
  ASSERT_NE(y2, nullptr);

  EXPECT_EQ(clk->getDirection(), vpiInput);
  EXPECT_EQ(a->getDirection(), vpiInput);
  EXPECT_EQ(b->getDirection(), vpiInput);
  EXPECT_EQ(c->getDirection(), vpiInput);
  EXPECT_EQ(d->getDirection(), vpiInput);
  EXPECT_EQ(y->getDirection(), vpiOutput);
  EXPECT_EQ(y2->getDirection(), vpiOutput);
}

// ---------------------------------------------------------------------------
// Port 'c' -- implicit net port (no type keyword; defaults to wire)
// Port 'd' -- explicit net port (declared with the 'wire' keyword)
// ---------------------------------------------------------------------------
TEST_F(AnsiPortsTest, ImplicitNetPortCResolvesToWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);

  const hldb::Port *const c = hldb::findByName<hldb::Port>("c", top->getPorts());
  ASSERT_NE(c, nullptr);
  const hldb::RefObj *const lc = c->getLowConn<hldb::RefObj>();
  ASSERT_NE(lc, nullptr) << "port 'c' has no lowConn RefObj";
  const hldb::Net *const net = lc->getActual<hldb::Net>();
  ASSERT_NE(net, nullptr) << "port 'c' lowConn does not resolve to a Net";
  EXPECT_EQ(net->getNetType(), vpiWire) << "port 'c' has no type keyword and should default to wire";
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
  EXPECT_EQ(net->getNetType(), vpiWire) << "port 'd' is explicitly declared 'wire'";
}

// ---------------------------------------------------------------------------
// Port 'y2' -- implicit net port, output direction (no type keyword)
// ---------------------------------------------------------------------------
TEST_F(AnsiPortsTest, ImplicitNetPortY2ResolvesToWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);

  const hldb::Port *const y2 = hldb::findByName<hldb::Port>("y2", top->getPorts());
  ASSERT_NE(y2, nullptr);
  const hldb::RefObj *const lc = y2->getLowConn<hldb::RefObj>();
  ASSERT_NE(lc, nullptr) << "port 'y2' has no lowConn RefObj";
  const hldb::Net *const net = lc->getActual<hldb::Net>();
  ASSERT_NE(net, nullptr) << "port 'y2' lowConn does not resolve to a Net";
  EXPECT_EQ(net->getNetType(), vpiWire) << "port 'y2' has no type keyword and should default to wire";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
