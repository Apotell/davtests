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

// Tests for tests/DecValue/dut.sv:
//
//   module top(output int o);
//      parameter A = 'x;
//      parameter B = 'x;
//      localparam int C = A / B;
//      assign o = int'(C);
//   endmodule
//
// DecValue.hlc passes -PA="'d7_200" -PB="'d1_800": HLC's command-line
// parameter override option, which supplies decimal-radix value literals
// with underscore digit separators (IEEE 1800-2023 Sec 5.7.1: "The
// underscore character... shall be ignored... may be used to provide
// readability") to override A's and B's in-source defaults ('x, an
// unbased unsized value per Sec 5.7.1).
//
// Checked:
//   - module 'top' exists with output port 'o' of direction vpiOutput
//   - 'A' and 'B' are (non-local) module parameters
//   - 'C' is a localparam ('getLocalParam() == true')
//   - 'C's initializer expression is a division Operation (Sec 11.4.4,
//     vpiDivOp) over RefObj 'A' and RefObj 'B', each resolving to the
//     corresponding Parameter -- this holds regardless of the concrete
//     value the -P command-line override ultimately assigns to A/B, so it
//     does not depend on guessing HLC's specific override representation
//   - compiler reports zero errors

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class DecValueTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DecValue.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Parameter *getParam(std::string_view name) {
    const hldb::Module *const top = getTop();
    return (top == nullptr) ? nullptr : hldb::findByName<hldb::Parameter>(name, top->getParameters());
  }

  static const hldb::ParamAssign *getParamAssign(std::string_view name) {
    const hldb::Module *const top = getTop();
    return (top == nullptr) ? nullptr : hldb::findByName(name, top->getParamAssigns());
  }
};

// ---------------------------------------------------------------------------
// Module and port
// ---------------------------------------------------------------------------

TEST_F(DecValueTest, ModuleExists) { ASSERT_NE(getTop(), nullptr) << "module 'top' not found"; }

TEST_F(DecValueTest, PortOIsOutput) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  const hldb::Port *const o = hldb::findByName<hldb::Port>("o", top->getPorts());
  ASSERT_NE(o, nullptr) << "'output int o' not found";
  EXPECT_EQ(o->getDirection(), vpiOutput);
}

// ---------------------------------------------------------------------------
// parameter A / B ('x default, overridden via -PA/-PB on the command line)
// ---------------------------------------------------------------------------

TEST_F(DecValueTest, AAndBAreModuleParametersNotLocalParams) {
  const hldb::Parameter *const a = getParam("A");
  const hldb::Parameter *const b = getParam("B");
  ASSERT_NE(a, nullptr) << "'parameter A' not found";
  ASSERT_NE(b, nullptr) << "'parameter B' not found";
  EXPECT_FALSE(a->getLocalParam());
  EXPECT_FALSE(b->getLocalParam());
}

// ---------------------------------------------------------------------------
// localparam int C = A / B;
// ---------------------------------------------------------------------------

TEST_F(DecValueTest, CIsLocalParam) {
  const hldb::Parameter *const c = getParam("C");
  ASSERT_NE(c, nullptr) << "'localparam int C' not found";
  EXPECT_TRUE(c->getLocalParam());
}

TEST_F(DecValueTest, CInitializerIsDivisionOfAAndB) {
  const hldb::ParamAssign *const c = getParamAssign("C");
  ASSERT_NE(c, nullptr);
  const hldb::Operation *const div = c->getRhs<hldb::Operation>();
  ASSERT_NE(div, nullptr) << "Sec 11.4.4: 'A / B' must be a division Operation";
  EXPECT_EQ(div->getOpType(), vpiDivOp);
  ASSERT_NE(div->getOperands(), nullptr);
  ASSERT_EQ(div->getOperands()->size(), 2u);

  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(div->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view("A"));
  EXPECT_NE(lhs->getActual(), nullptr) << "'A' in 'A / B' must resolve to the Parameter 'A'";

  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(div->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view("B"));
  EXPECT_NE(rhs->getActual(), nullptr) << "'B' in 'A / B' must resolve to the Parameter 'B'";
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(DecValueTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
