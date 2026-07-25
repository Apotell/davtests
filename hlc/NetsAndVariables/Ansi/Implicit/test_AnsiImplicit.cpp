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
//   - "implicit_wire" / "implicit_logic" are formally declared despite their
//     names (implicit_wire -> vpiWire, implicit_logic -> LogicTypespec)
//   - implicit_net_a / implicit_net_b (undeclared identifiers used as the LHS
//     of a continuous assignment) are NOT materialized as a Net, matching the
//     documented behavior in 6.10--implicit_continuous_assignment (HLC
//     reports EL0535 "Illegal implicit net" and does not add a graph node for
//     the identifier)
//   - implicit_var_a / implicit_var_b / implicit_var_c (undeclared
//     identifiers assigned inside a procedural block) are likewise NOT
//     materialized as a Net. Per LRM 6.10, implicit declarations are net-only
//     -- there is no implicit variable in SystemVerilog -- so this asserts
//     the same "not declared" outcome as the net case. This has not been
//     confirmed against an actual HLC compile; adjust if it reports an error
//     or materializes a node instead.

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
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "AnsiImplicit.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("work@nets_and_variables_test", m_design->getAllModules());
  }
};

TEST_F(AnsiImplicitTest, ImplicitWireIsFormallyDeclaredWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const w = hldb::findByName<hldb::Net>("implicit_wire", top->getNets());
  ASSERT_NE(w, nullptr);
  EXPECT_EQ(w->getNetType(), vpiWire);
}

TEST_F(AnsiImplicitTest, ImplicitLogicIsFormallyDeclaredVariable) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const l = hldb::findByName<hldb::Net>("implicit_logic", top->getNets());
  ASSERT_NE(l, nullptr);
  const hldb::RefTypespec *const rts = l->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(AnsiImplicitTest, ImplicitNetANotDeclared) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("implicit_net_a", top->getNets()), nullptr)
      << "'implicit_net_a' should not appear in vpiNet -- it was implicitly declared";
}

TEST_F(AnsiImplicitTest, ImplicitNetBNotDeclared) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("implicit_net_b", top->getNets()), nullptr)
      << "'implicit_net_b' should not appear in vpiNet -- it was implicitly declared";
}

// ---------------------------------------------------------------------------
// "Implicit variable" -- per LRM 6.10, implicit declarations are net-only;
// there is no implicit variable in SystemVerilog. implicit_var_a/b/c are
// undeclared identifiers assigned inside a procedural block, so none of them
// should be materialized as a Net (this suite models module-scope variables
// as Net nodes; see VarLogicIsLogicTypespec in the Variables test).
// ---------------------------------------------------------------------------
TEST_F(AnsiImplicitTest, ImplicitVarANotDeclared) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("implicit_var_a", top->getNets()), nullptr)
      << "'implicit_var_a' should not appear in vpiNet -- there is no implicit variable in SystemVerilog";
}

TEST_F(AnsiImplicitTest, ImplicitVarBNotDeclared) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("implicit_var_b", top->getNets()), nullptr)
      << "'implicit_var_b' should not appear in vpiNet -- there is no implicit variable in SystemVerilog";
}

TEST_F(AnsiImplicitTest, ImplicitVarCNotDeclared) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("implicit_var_c", top->getNets()), nullptr)
      << "'implicit_var_c' should not appear in vpiNet -- there is no implicit variable in SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
