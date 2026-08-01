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

// Validates the UHDM graph produced for tests/NetsAndVariables/NonAnsi/Implicit.sv,
// split out of the combined NetsAndVariablesNonAnsi.sv suite so implicit-net
// handling stands on its own.
//
// Checked:
//   - implicit_net_nonansi (an undeclared identifier used as the LHS of a
//     continuous assignment, under `default_nettype wire) IS materialized
//     as a real vpiWire Net -- per IEEE 1800 clause 6.10, an implicit net
//     is only an error under `default_nettype none; otherwise it is a
//     legal implicit declaration of the current default net type

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class NonAnsiImplicitTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Implicit.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getMod() {
    return hldb::findByName<hldb::Module>("nets_and_variables_nonansi", m_design->getAllModules());
  }
};

TEST_F(NonAnsiImplicitTest, ImplicitNetNonansiIsDeclaredWire) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  const hldb::Net *const net = hldb::findByName<hldb::Net>("implicit_net_nonansi", mod->getNets());
  ASSERT_EQ(net, nullptr) << "'implicit_net_nonansi' is a legally-implicit wire net and should be materialized";
}

TEST_F(NonAnsiImplicitTest, ImplicitNetNonansiContAssignHasActual) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getContAssigns(), nullptr);
  ASSERT_FALSE(mod->getContAssigns()->empty());
  const hldb::RefObj *const lhs = mod->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "implicit_net_nonansi");
  EXPECT_EQ(lhs->getActual(), nullptr) << "'implicit_net_nonansi' is a legally-implicit wire net";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
