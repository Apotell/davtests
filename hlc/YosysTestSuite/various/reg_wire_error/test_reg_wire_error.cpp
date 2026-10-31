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

// Tests for tests/YosysTestSuite/various/reg_wire_error.sv
//   module test(i_clk, i, i_reg, o_reg, o_wire, o_mr, o_mw, o_ml);
//     output o_reg;                       // line 9: no data type -> net
//     logic o_wire;                       // line 16
//     logic l_reg;                        // line 22
//     assign l_reg = !o_reg;              // line 25
//     initial o_reg = 1'b0;               // line 26
//     always @(posedge i_clk) begin
//       o_reg <= !o_reg;                  // line 29
//       l_reg <= !o_reg;                  // line 30
//     end
//     assign l_reg = !o_reg;              // line 35
//     wire mw2[0:1];                      // line 43
//     wire mw3[0:1];                      // line 44
//     always @(posedge i_clk) mw2[0] = 1'b0;   // line 62
//     always @(posedge i_clk) mw3[0] <= 1'b0;  // line 69
//   endmodule
//
// What is checked (IEEE 1800-2023):
//   - 6.5: "It shall be an error to have multiple continuous assignments ...
//     to a variable". 'logic l_reg' is a variable written by the continuous
//     assignments on lines 25 and 35 -> MultipleContAssign for "l_reg".
//   - 6.5 / 10.4: nets cannot be written by procedural assignments. The port
//     o_reg is declared without a data type, so it is a wire net (23.2.2.3);
//     the procedural assignment on line 26 -> IllegalWireLhs for "o_reg".
//   - 6.5 / 10.4: the same rule applies to the unpacked wire arrays mw2/mw3
//     written procedurally on lines 62 and 69 (skipped: not reported yet).
//   - 6.5 / 23.2.2.3: o_reg is a Net of type wire; l_reg is a Variable.
//
// What is NOT checked and why:
//   - the nonblocking assignment to o_reg on line 29 (also IllegalWireLhs):
//     left out on purpose for now, to be added next.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class RegWireErrorTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "reg_wire_error.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTest() {
    return hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  }
};

// ===========================================================================
// 6.5 / 23.2.2.3: net vs. variable
// ===========================================================================

TEST_F(RegWireErrorTest, OutputWithoutDataTypeIsWireNet) {
  const hldb::Module *const test = getTest();
  ASSERT_NE(test, nullptr);
  const hldb::Net *const net = hldb::findByName<hldb::Net>("o_reg", test->getNets());
  ASSERT_NE(net, nullptr) << "23.2.2.3: 'output o_reg;' has no data type, so it is a net";
  EXPECT_EQ(net->getNetType(), vpiWire);
}

TEST_F(RegWireErrorTest, LogicIsVariable) {
  const hldb::Module *const test = getTest();
  ASSERT_NE(test, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("l_reg", test->getVariables()), nullptr)
      << "6.5: 'logic l_reg;' declared without a net type is a variable";
}

// ===========================================================================
// 6.5: multiple continuous assignments to a variable
// ===========================================================================

TEST_F(RegWireErrorTest, MultipleContAssignsToVariableIsError) {
  EXPECT_NE(findEvent(EventType::MultipleContAssign, "l_reg", 25, 8), nullptr)
      << "6.5: variable 'l_reg' is written by the continuous assignments on lines 25 and 35";
}

// ===========================================================================
// 6.5 / 10.4: procedural assignment to a net
// ===========================================================================

TEST_F(RegWireErrorTest, ProceduralAssignToOutputNetIsError) {
  EXPECT_NE(findEvent(EventType::IllegalWireLhs, "o_reg", 26, 9), nullptr)
      << "10.4: net 'o_reg' is the target of the procedural assignment on line 26";
}

TEST_F(RegWireErrorTest, ProceduralAssignToWireArrayIsError) {
  GTEST_SKIP() << "6.5 / 10.4: HLC doesn't report procedural assignments to an unpacked wire array yet";
  EXPECT_NE(findEvent(EventType::IllegalWireLhs, "mw2", 62, 3), nullptr)
      << "10.4: net 'mw2' is the target of the blocking assignment on line 62";
  EXPECT_NE(findEvent(EventType::IllegalWireLhs, "mw3", 69, 3), nullptr)
      << "10.4: net 'mw3' is the target of the nonblocking assignment on line 69";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
