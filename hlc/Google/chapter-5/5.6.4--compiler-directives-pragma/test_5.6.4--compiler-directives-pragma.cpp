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
//   Module name:work@ts
//     vpiNet (1 item): Net "protected_wire"  vpiNetType: wire (1)
//
// Key assertion: `pragma protect / `pragma protect end produce no UHDM nodes
// of their own; the wire declaration inside the region is compiled as normal.

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/design.h>
#include <uhdm/module.h>
#include <uhdm/net.h>

namespace SURELOG {

class CompilerDirectivesPragma : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.6.4--compiler-directives-pragma.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

static const uhdm::Module *getTop(const uhdm::Design *d) {
  return uhdm::findByName<uhdm::Module>("work@ts", d->getAllModules());
}

// ---------------------------------------------------------------------------
// Module
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesPragma, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@ts' not found";
}

// ---------------------------------------------------------------------------
// Net inside the `pragma protect region compiles normally
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesPragma, OneNetExists) {
  const uhdm::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 1u)
      << "the wire declaration inside the pragma region should compile normally";
}

TEST_F(CompilerDirectivesPragma, ProtectedWireNetExists) {
  const uhdm::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(uhdm::findByName<uhdm::Net>("protected_wire", m->getNets()),
            nullptr)
      << "net 'protected_wire' not found";
}

TEST_F(CompilerDirectivesPragma, ProtectedWireIsWireType) {
  const uhdm::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Net>("protected_wire", m->getNets());
  ASSERT_NE(net, nullptr);
  // vpiWire = 1
  EXPECT_EQ(net->getNetType(), 1)
      << "protected_wire should have net type wire (1)";
}

// ---------------------------------------------------------------------------
// `pragma produces no processes
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesPragma, NoProcesses) {
  const uhdm::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getProcesses() || m->getProcesses()->empty())
      << "`pragma directives should not produce process nodes";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
