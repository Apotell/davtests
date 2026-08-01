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

// Validates the UHDM graph produced for tests/NetsAndVariables/NonAnsi/Interface.sv,
// split out of the combined NetsAndVariablesNonAnsi.sv suite so the
// file-scope interface testing point stands on its own.
//
// Checked:
//   - nets_and_variables_if_nonansi exists
//   - it has a single modport "mp"
//   - it has one continuous assignment driving a legally-implicit wire net
//     (if_implicit_net, IEEE 1800 clause 6.10), never explicitly declared

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/modport.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class NonAnsiInterfaceTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Interface.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(NonAnsiInterfaceTest, InterfaceExists) {
  ASSERT_NE(m_design->getAllInterfaces(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Interface>("nets_and_variables_if_nonansi", m_design->getAllInterfaces()), nullptr);
}

TEST_F(NonAnsiInterfaceTest, InterfaceHasOneModport) {
  const hldb::Interface *const iface =
      hldb::findByName<hldb::Interface>("nets_and_variables_if_nonansi", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getModports(), nullptr);
  ASSERT_EQ(iface->getModports()->size(), 1u);
  EXPECT_EQ(iface->getModports()->at(0)->getName(), "mp");
}

TEST_F(NonAnsiInterfaceTest, InterfaceHasOneContAssign) {
  const hldb::Interface *const iface =
      hldb::findByName<hldb::Interface>("nets_and_variables_if_nonansi", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getContAssigns(), nullptr);
  EXPECT_EQ(iface->getContAssigns()->size(), 1u);
}

TEST_F(NonAnsiInterfaceTest, ContAssignDrivesImplicitNetWire) {
  const hldb::Interface *const iface =
      hldb::findByName<hldb::Interface>("nets_and_variables_if_nonansi", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getContAssigns(), nullptr);
  ASSERT_FALSE(iface->getContAssigns()->empty());
  const hldb::RefObj *const lhs = iface->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "if_implicit_net");
  EXPECT_EQ(lhs->getActual(), nullptr) << "'if_implicit_net' is a legally-implicit wire net";
}

TEST_F(NonAnsiInterfaceTest, ImplicitNetIsDeclaredWire) {
  const hldb::Interface *const iface =
      hldb::findByName<hldb::Interface>("nets_and_variables_if_nonansi", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getNets(), nullptr);
  const hldb::Net *const net = hldb::findByName<hldb::Net>("if_implicit_net", iface->getNets());
  ASSERT_EQ(net, nullptr) << "'if_implicit_net' is a legally-implicit wire net and should be materialized";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
