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

// Validates that the `resetall compiler directive is accepted without error
// and that the module following it compiles cleanly.
//
// SV source:
//   `resetall
//   module ts();
//   endmodule
//
// `resetall resets all active compiler directives (timescale, celldefine,
// default_nettype, etc.) to their defaults.  It produces no UHDM nodes.
//
// UHDM structure:
//   Module name:work@ts  — empty, defNetType: none (12)
//
// Note: Surelog records defNetType as none (12) after `resetall, which is
// Surelog's internal reset state for this property.

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/design.h>
#include <uhdm/module.h>

namespace SURELOG {

class CompilerDirectivesResetall : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.6.4--compiler-directives-resetall.hlc"});

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
// `resetall is consumed without error; the module compiles cleanly.
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesResetall, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@ts' not found";
}

TEST_F(CompilerDirectivesResetall, ModuleIsEmpty) {
  const uhdm::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getNets() || m->getNets()->empty());
  EXPECT_TRUE(!m->getProcesses() || m->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// After `resetall, Surelog sets defNetType to none (12).
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesResetall, ModuleDefNetTypeIsNone) {
  const uhdm::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  // vpiNone = 12; `resetall yields none as Surelog's reset state.
  EXPECT_EQ(m->getDefNetType(), 12);
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
