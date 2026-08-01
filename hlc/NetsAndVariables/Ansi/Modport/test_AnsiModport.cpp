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

// Validates the UHDM graph produced for tests/NetsAndVariables/Ansi/Modport.sv.
//
// Checked:
//   - work@nets_and_variables_modport_if exists, with net mp_net and
//     variable mp_var declared
//   - one continuous assignment drives a legally-implicit wire net
//     (mp_implicit, IEEE 1800 clause 6.10), never explicitly declared
//   - it has a single modport "mp_basic" exposing all three: mp_net (input),
//     mp_var (output), mp_implicit (input)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/io_decl.h>
#include <hldb/modport.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AnsiModportTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "AnsiModport.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getIface() {
    return hldb::findByName<hldb::Interface>("work@nets_and_variables_modport_if", m_design->getAllInterfaces());
  }
};

TEST_F(AnsiModportTest, InterfaceExists) { ASSERT_NE(getIface(), nullptr); }

TEST_F(AnsiModportTest, InterfaceHasNetAndVariable) {
  const hldb::Interface *const iface = getIface();
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getNets(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("mp_net", iface->getNets()), nullptr);
  ASSERT_NE(iface->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("mp_var", iface->getVariables()), nullptr);
}

TEST_F(AnsiModportTest, InterfaceHasOneContAssignDrivingImplicitNet) {
  const hldb::Interface *const iface = getIface();
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getContAssigns(), nullptr);
  ASSERT_EQ(iface->getContAssigns()->size(), 1u);
  const hldb::RefObj *const lhs = iface->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "mp_implicit");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr) << "'mp_implicit' is a legally-implicit wire net";
}

TEST_F(AnsiModportTest, ImplicitNetIsDeclaredWire) {
  const hldb::Interface *const iface = getIface();
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getNets(), nullptr);
  const hldb::Net *const net = hldb::findByName<hldb::Net>("mp_implicit", iface->getNets());
  ASSERT_NE(net, nullptr) << "'mp_implicit' is a legally-implicit wire net and should be materialized";
  EXPECT_EQ(net->getNetType(), vpiWire);
}

TEST_F(AnsiModportTest, InterfaceHasOneModportNamedMpBasic) {
  const hldb::Interface *const iface = getIface();
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getModports(), nullptr);
  ASSERT_EQ(iface->getModports()->size(), 1u);
  EXPECT_EQ(iface->getModports()->at(0)->getName(), "mp_basic");
}

TEST_F(AnsiModportTest, ModportHasThreeIODecls) {
  const hldb::Interface *const iface = getIface();
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getModports(), nullptr);
  ASSERT_FALSE(iface->getModports()->empty());
  const hldb::Modport *const mp = iface->getModports()->at(0);
  ASSERT_NE(mp, nullptr);
  ASSERT_NE(mp->getIODecls(), nullptr);
  ASSERT_EQ(mp->getIODecls()->size(), 3u);

  const hldb::IODecl *const netPort = hldb::findByName<hldb::IODecl>("mp_net", mp->getIODecls());
  ASSERT_NE(netPort, nullptr);
  EXPECT_EQ(netPort->getDirection(), vpiInput);

  const hldb::IODecl *const varPort = hldb::findByName<hldb::IODecl>("mp_var", mp->getIODecls());
  ASSERT_NE(varPort, nullptr);
  EXPECT_EQ(varPort->getDirection(), vpiOutput);

  const hldb::IODecl *const implicitPort = hldb::findByName<hldb::IODecl>("mp_implicit", mp->getIODecls());
  ASSERT_NE(implicitPort, nullptr);
  EXPECT_EQ(implicitPort->getDirection(), vpiInput);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
