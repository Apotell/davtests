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

// Validates that `celldefine / `endcelldefine compiler directives are accepted
// and that both the wrapped and unwrapped modules compile cleanly.
//
// SV source:
//   `celldefine
//   module cd();    // cell module — used for timing/backannotation
//   endmodule
//   `endcelldefine
//
//   module ncd();   // ordinary module
//   endmodule
//
// UHDM structure:
//   vpiAllModules (2 items):
//     Module name:work@cd   — the celldefine-wrapped module
//     Module name:work@ncd  — the plain module
//
// Note: `celldefine is a compiler hint for SDF timing tools; Surelog accepts
// it without error but the cell-flag distinction is not surfaced in UHDM.
// Both modules appear as structurally identical empty Module nodes.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>

namespace hlc {

class CompilerDirectivesCelldefine : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.6.4--compiler-directives-celldefine.hlc"});

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

// ---------------------------------------------------------------------------
// Both modules present
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesCelldefine, TwoModulesExist) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 2u)
      << "design should contain exactly two modules (cd and ncd)";
}

TEST_F(CompilerDirectivesCelldefine, CelldefineModuleExists) {
  EXPECT_NE(
      hldb::findByName<hldb::Module>("work@cd", m_design->getAllModules()),
      nullptr)
      << "module 'work@cd' (inside `celldefine) not found";
}

TEST_F(CompilerDirectivesCelldefine, NoncelldefineModuleExists) {
  EXPECT_NE(
      hldb::findByName<hldb::Module>("work@ncd", m_design->getAllModules()),
      nullptr)
      << "module 'work@ncd' (outside `celldefine) not found";
}

// ---------------------------------------------------------------------------
// Both modules are empty — no nets, no processes
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesCelldefine, CelldefineModuleHasNoNets) {
  const hldb::Module *const m =
      hldb::findByName<hldb::Module>("work@cd", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getNets() || m->getNets()->empty());
}

TEST_F(CompilerDirectivesCelldefine, NoncelldefineModuleHasNoNets) {
  const hldb::Module *const m =
      hldb::findByName<hldb::Module>("work@ncd", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getNets() || m->getNets()->empty());
}

TEST_F(CompilerDirectivesCelldefine, CelldefineModuleHasNoProcesses) {
  const hldb::Module *const m =
      hldb::findByName<hldb::Module>("work@cd", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getProcesses() || m->getProcesses()->empty());
}

TEST_F(CompilerDirectivesCelldefine, NoncelldefineModuleHasNoProcesses) {
  const hldb::Module *const m =
      hldb::findByName<hldb::Module>("work@ncd", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getProcesses() || m->getProcesses()->empty());
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
