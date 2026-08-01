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

#include <string_view>

namespace hlc {
class FpuRptrMacrosTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "fpu_rptr_macros.v.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // fpu_rptr_macros.v contains no `define directives or macro usages at all
  // (plain Verilog-2001 ANSI-style modules); the ports "in"/"out" have a
  // direction and a packed range but no net-type keyword and no explicit
  // data type. Per IEEE 1800-2023 Sec 23.2.2.3, a port with neither a
  // net_port_type nor a var_port_type defaults to a net of the module's
  // default_nettype (wire, since none is overridden here) for both input
  // and output directions -- there is no "output defaults to variable"
  // exception here because that exception only applies when the port has
  // an explicit data type (e.g. "output logic out").
  static const hldb::Net *getPortNet(const hldb::Module *mod, std::string_view name) {
    const hldb::Net *const n = hldb::findByName<hldb::Net>(name, mod->getNets());
    EXPECT_NE(n, nullptr) << "port '" << name << "' must be modeled as a Net (no net-type keyword -> "
                           << "default_nettype wire per Sec 23.2.2.3)";
    EXPECT_EQ(hldb::findByName<hldb::Variable>(name, mod->getVariables()), nullptr)
        << "'" << name << "' must not also appear in getVariables()";
    return n;
  }
};

// Both modules in fpu_rptr_macros.v must compile cleanly.
TEST_F(FpuRptrMacrosTest, FpuBufrptGrp64Compiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("fpu_bufrpt_grp64", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'fpu_bufrpt_grp64' must compile";
}

TEST_F(FpuRptrMacrosTest, FpuBufrptGrp32Compiles) {
  const hldb::Module *const module =
      hldb::findByName<hldb::Module>("fpu_bufrpt_grp32", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'fpu_bufrpt_grp32' must compile";
}

// fpu_rptr_macros.v contains no `define directives; the source file itself
// does not produce macro definition records.
TEST_F(FpuRptrMacrosTest, NoMacroDefinitionsInSourceFile) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("fpu_rptr_macros.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(sf->getPreprocMacroDefinitions(), nullptr)
      << "fpu_rptr_macros.v does not define any macros at source-file level";
}

// fpu_bufrpt_grp64: "input [63:0] in; output [63:0] out;" -- neither port
// has a net-type keyword or an explicit data type, so both default to
// vpiWire nets (Sec 23.2.2.3), not variables.
TEST_F(FpuRptrMacrosTest, FpuBufrptGrp64PortsAreWireNets) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("fpu_bufrpt_grp64", m_design->getAllModules());
  ASSERT_NE(module, nullptr);
  const hldb::Net *const in = getPortNet(module, "in");
  if (in != nullptr) EXPECT_EQ(in->getNetType(), vpiWire);
  const hldb::Net *const out = getPortNet(module, "out");
  if (out != nullptr) EXPECT_EQ(out->getNetType(), vpiWire);
}

// fpu_bufrpt_grp32: same shape as fpu_bufrpt_grp64, at a 32-bit width.
TEST_F(FpuRptrMacrosTest, FpuBufrptGrp32PortsAreWireNets) {
  const hldb::Module *const module =
      hldb::findByName<hldb::Module>("fpu_bufrpt_grp32", m_design->getAllModules());
  ASSERT_NE(module, nullptr);
  const hldb::Net *const in = getPortNet(module, "in");
  if (in != nullptr) EXPECT_EQ(in->getNetType(), vpiWire);
  const hldb::Net *const out = getPortNet(module, "out");
  if (out != nullptr) EXPECT_EQ(out->getNetType(), vpiWire);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
