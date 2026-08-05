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

// Validates the UHDM graph produced for tests/NetsAndVariables/Ansi/Interface.sv,
// split out of the combined NetsAndVariablesAnsi.sv suite so the file-scope
// interface testing point stands on its own.
//
// Checked:
//   - nets_and_variables_if exists
//   - if_logic is a hldb::Variable (never duplicated as a Net) -- interface
//     items follow module_common_item (grammar/SV3_1aParser.g4,
//     interface_or_generate_item -> module_common_item), so both nets and
//     variables are legal interface-scoped declarations, same as a module
//   - if_wire is a hldb::Net with vpiNetType == vpiWire (never duplicated
//     as a Variable)
//   - it has a single modport "mp"

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/modport.h>
#include <hldb/net.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AnsiInterfaceTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Interface.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(AnsiInterfaceTest, InterfaceExists) {
  ASSERT_NE(m_design->getAllInterfaces(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Interface>("nets_and_variables_if", m_design->getAllInterfaces()), nullptr);
}

TEST_F(AnsiInterfaceTest, IfLogicIsVariableNoNetDuplicate) {
  const hldb::Interface *const iface =
      hldb::findByName<hldb::Interface>("nets_and_variables_if", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("if_logic", iface->getVariables()), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("if_logic", iface->getNets()), nullptr)
      << "'if_logic' is variable-declared -- it must not also appear in vpiNet";
}

TEST_F(AnsiInterfaceTest, IfWireIsNetWithWireTypeNoVariableDuplicate) {
  const hldb::Interface *const iface =
      hldb::findByName<hldb::Interface>("nets_and_variables_if", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  const hldb::Net *const n = hldb::findByName<hldb::Net>("if_wire", iface->getNets());
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->getNetType(), vpiWire);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("if_wire", iface->getVariables()), nullptr)
      << "'if_wire' is net-declared -- it must not also appear in vpiVariables";
}

TEST_F(AnsiInterfaceTest, InterfaceHasOneModport) {
  const hldb::Interface *const iface =
      hldb::findByName<hldb::Interface>("nets_and_variables_if", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getModports(), nullptr);
  ASSERT_EQ(iface->getModports()->size(), 1u);
  EXPECT_EQ(iface->getModports()->at(0)->getName(), "mp");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
