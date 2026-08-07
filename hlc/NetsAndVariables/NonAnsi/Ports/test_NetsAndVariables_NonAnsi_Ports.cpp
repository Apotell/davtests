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

// Validates the UHDM graph produced for tests/NetsAndVariables/NonAnsi/Ports.sv,
// split out of the combined NetsAndVariablesNonAnsi.sv suite so the
// non-ANSI port testing point stands on its own.
//
// Checked:
//   - nets_and_variables_nonansi exists with 3 ports (a, b input; y
//     output), each an implicit wire under `default_nettype wire, matching
//     the pattern established by 6.10--implicit_port in this suite
//   - each port's lowConn resolves to a Net, and no Variable of the same
//     name exists (no duplicate across the net/variable containers)

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

class NonAnsiPortsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Ports.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getMod() {
    return hldb::findByName<hldb::Module>("nets_and_variables_nonansi", m_design->getAllModules());
  }
};

TEST_F(NonAnsiPortsTest, ModuleExists) { ASSERT_NE(getMod(), nullptr); }

TEST_F(NonAnsiPortsTest, ThreePortsExist) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getPorts(), nullptr);
  EXPECT_EQ(mod->getPorts()->size(), 3u) << "expected ports a, b, y";
}

TEST_F(NonAnsiPortsTest, PortDirections) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getPorts(), nullptr);

  const hldb::Port *const a = hldb::findByName<hldb::Port>("a", mod->getPorts());
  const hldb::Port *const b = hldb::findByName<hldb::Port>("b", mod->getPorts());
  const hldb::Port *const y = hldb::findByName<hldb::Port>("y", mod->getPorts());
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(y, nullptr);

  EXPECT_EQ(a->getDirection(), vpiInput);
  EXPECT_EQ(b->getDirection(), vpiInput);
  EXPECT_EQ(y->getDirection(), vpiOutput);
}

TEST_F(NonAnsiPortsTest, PortsResolveToWireNets) {
  // Matches 6.10--implicit_port: non-ANSI ports with no type keyword default
  // to a net of `default_nettype wire, for both input and output ports.
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getPorts(), nullptr);

  for (const char *const name : {"a", "b", "y"}) {
    const hldb::Port *const p = hldb::findByName<hldb::Port>(name, mod->getPorts());
    ASSERT_NE(p, nullptr) << "port '" << name << "' not found";
    const hldb::RefObj *const lc = p->getLowConn<hldb::RefObj>();
    ASSERT_NE(lc, nullptr) << "port '" << name << "' has no lowConn RefObj";
    const hldb::Net *const net = lc->getActual<hldb::Net>();
    ASSERT_NE(net, nullptr) << "port '" << name << "' lowConn does not resolve to a Net";
    EXPECT_EQ(net->getNetType(), vpiWire) << "port '" << name << "' should default to wire";
  }
}

TEST_F(NonAnsiPortsTest, PortsNotDuplicatedAsVariables) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);

  for (const char *const name : {"a", "b", "y"}) {
    EXPECT_EQ(hldb::findByName<hldb::Variable>(name, mod->getVariables()), nullptr)
        << "port '" << name << "' is net-typed -- it must not also appear in variables";
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
