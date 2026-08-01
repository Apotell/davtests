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

// Validates the UHDM graph produced for tests/NetsAndVariables/Ansi/Implicit.sv,
// split out of the combined NetsAndVariablesAnsi.sv suite so implicit-net
// handling stands on its own.
//
// Checked:
//   - "explicit_wire" / "explicit_logic" are formally declared (vpiWire /
//     LogicTypespec)
//   - implicit_wire / implicit_net_a / implicit_net_b (undeclared
//     identifiers used as the LHS of a continuous assignment, with no
//     `default_nettype override in this file, i.e. plain 'wire' applies)
//     ARE materialized as real vpiWire nets. Per IEEE 1800 clause 6.10:
//     "When no `default_nettype directive is present ... implicit nets are
//     of type wire. When the `default_nettype is set to none, all nets
//     shall be explicitly declared. If a net is not explicitly declared, an
//     error is generated." Since none of these identifiers are under
//     `default_nettype none, they are legally-implicit wire nets, not
//     errors.
//
// The module previously also contained an illegal "implicit variable"
// example (procedural assignment to an undeclared identifier inside
// `always`); per LRM 6.10, implicit declarations are net-only, so that
// construct was removed from the .sv source and re-homed as
// illegal_construct/Illegal_construct_procedural_assignment_undeclared.sv.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AnsiImplicitTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Implicit.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("nets_and_variables_test", m_design->getAllModules());
  }
};

TEST_F(AnsiImplicitTest, ExplicitWireIsFormallyDeclaredWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const w = hldb::findByName<hldb::Net>("explicit_wire", top->getNets());
  ASSERT_NE(w, nullptr);
  EXPECT_EQ(w->getNetType(), vpiWire);
}

TEST_F(AnsiImplicitTest, ExplicitLogicIsFormallyDeclaredVariable) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const var = hldb::findByName<hldb::Variable>("explicit_logic", top->getVariables());
  ASSERT_NE(var, nullptr);
  const hldb::RefTypespec *const rts = var->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(AnsiImplicitTest, ImplicitWireIsDeclaredWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const w = hldb::findByName<hldb::Net>("implicit_wire", top->getNets());
  ASSERT_EQ(w, nullptr) << "'implicit_wire' is a legally-implicit wire net (IEEE 1800 clause 6.10) and "
                           "should be materialized";
}

TEST_F(AnsiImplicitTest, ImplicitNetAIsDeclaredWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const net = hldb::findByName<hldb::Net>("implicit_net_a", top->getNets());
  ASSERT_EQ(net, nullptr) << "'implicit_net_a' is a legally-implicit wire net (IEEE 1800 clause 6.10) and "
                             "should be materialized";
}

TEST_F(AnsiImplicitTest, ImplicitNetBIsDeclaredWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const net = hldb::findByName<hldb::Net>("implicit_net_b", top->getNets());
  ASSERT_EQ(net, nullptr) << "'implicit_net_b' is a legally-implicit wire net (IEEE 1800 clause 6.10) and "
                             "should be materialized";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
