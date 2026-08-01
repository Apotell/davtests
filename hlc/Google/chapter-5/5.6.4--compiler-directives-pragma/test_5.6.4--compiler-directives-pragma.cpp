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

// Validates that `pragma directives are accepted without error and that
// declarations inside a pragma-protected region are compiled normally.
//
// SV source:
//   module ts();
//   `pragma protect
//     wire protected_wire;
//   `pragma protect end
//   endmodule
//
// UHDM structure:
//   Module name:ts
//     vpiNet (1 item): Net "protected_wire"  vpiNetType: wire (1)
//
// Key assertion: `pragma protect / `pragma protect end produce no UHDM nodes
// of their own; the wire declaration inside the region is compiled as normal.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/variable.h>

namespace hlc {

class CompilerDirectivesPragma : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.6.4--compiler-directives-pragma.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("ts", d->getAllModules());
}

// ----
// Module
// ----
TEST_F(CompilerDirectivesPragma, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'ts' not found"; }

// ----
// Net inside the `pragma protect region compiles normally
// ----
TEST_F(CompilerDirectivesPragma, OneNetExists) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 1u) << "the wire declaration inside the pragma region should compile normally";
}

TEST_F(CompilerDirectivesPragma, ProtectedWireNetExists) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("protected_wire", m->getNets()), nullptr) << "net 'protected_wire' not found";
}

TEST_F(CompilerDirectivesPragma, ProtectedWireIsWireType) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  const hldb::Net *const net = hldb::findByName<hldb::Net>("protected_wire", m->getNets());
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getNetType(), vpiWire) << "protected_wire should have net type wire";
}

// `wire protected_wire;` is a net-type declaration, so per IEEE 1800-2023 Sec
// 6.7/6.8 it must not also appear in the module's variable collection.
TEST_F(CompilerDirectivesPragma, ProtectedWireIsNotDuplicatedAsVariable) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  if (m->getVariables() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Variable>("protected_wire", m->getVariables()), nullptr)
        << "'protected_wire' is a wire (net) and must not also appear as a Variable";
  }
}

// ----
// `pragma produces no processes
// ----
TEST_F(CompilerDirectivesPragma, NoProcesses) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getProcesses() || m->getProcesses()->empty())
      << "`pragma directives should not produce process nodes";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
