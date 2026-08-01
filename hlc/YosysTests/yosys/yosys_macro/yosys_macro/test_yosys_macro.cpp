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

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/source_file.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {
class YoysysTestsMacroTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "yosys_macro.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.5.1: the top module that exercises macro expansions must compile.
TEST_F(YoysysTestsMacroTest, TopModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'top' must compile";
}

// LRM 22.5.1: top.v uses macros via expansion but defines no macros of
// its own; the source file must have no macro definition entries.
TEST_F(YoysysTestsMacroTest, NoMacroDefinitionsInTopV) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("top.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(sf->getPreprocMacroDefinitions(), nullptr) << "top.v uses but does not define any macros";
}

// top.v: `timescale 1ns/10ps -> time unit 1ns (-9), time precision 10ps (-11).
TEST_F(YoysysTestsMacroTest, TimescaleIsSetOnModule) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getTimeUnit(), -9);
  EXPECT_EQ(module->getTimePrecision(), -11);
}

// input clk/rst and input [1:0] a have no net-type keyword and no explicit
// data type; per IEEE 1800-2023 Sec 23.2.2.3, input ports always default to
// a net of the default net type (wire), regardless of data type.
TEST_F(YoysysTestsMacroTest, InputPortsAreWireNets) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(module, nullptr);
  for (const char *const name : {"clk", "rst", "a"}) {
    const hldb::Net *const n = hldb::findByName<hldb::Net>(name, module->getNets());
    EXPECT_NE(n, nullptr) << "port '" << name << "' must be modeled as a Net (input port, Sec 23.2.2.3)";
    if (n != nullptr) EXPECT_EQ(n->getNetType(), vpiWire);
    EXPECT_EQ(hldb::findByName<hldb::Variable>(name, module->getVariables()), nullptr)
        << "'" << name << "' must not also appear in getVariables()";
  }
}

// output x; reg x; -- the output port's data type is declared separately
// with the explicit data_type syntax "reg". Per IEEE 1800-2023 Sec 23.2.2.3,
// an output port whose data type is declared with the explicit data_type
// syntax defaults to a variable, not a net.
TEST_F(YoysysTestsMacroTest, OutputPortWithExplicitRegIsVariable) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(module, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("x", module->getVariables());
  EXPECT_NE(v, nullptr) << "output port 'x' has an explicit 'reg' data type and must be a Variable (Sec 23.2.2.3)";
  EXPECT_EQ(hldb::findByName<hldb::Net>("x", module->getNets()), nullptr)
      << "'x' must not also appear in getNets()";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
