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

// Validates that `unconnected_drive and `nounconnected_drive compiler
// directives are accepted without error and produce no UHDM nodes.
//
// SV source:
//   `unconnected_drive pull1   // set pull-up on unconnected input ports
//   module ts();
//   endmodule
//   `nounconnected_drive       // cancel the pull
//
// `unconnected_drive specifies the pull strength applied to unconnected
// input port nets.  `nounconnected_drive cancels it.  Both are compiler
// hints with no structural UHDM representation — they produce no nodes
// beyond the clean empty module.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>

namespace hlc {

class CompilerDirectivesUnconnectedDrive : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__,
            {"-f", "5.6.4--compiler-directives-unconnected-drive.hlc"});

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

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@ts", d->getAllModules());
}

// ---------------------------------------------------------------------------
// Both directives are consumed without error; the module compiles cleanly.
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesUnconnectedDrive, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@ts' not found";
}

TEST_F(CompilerDirectivesUnconnectedDrive, ModuleIsEmpty) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getNets() || m->getNets()->empty());
  EXPECT_TRUE(!m->getProcesses() || m->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// The directives produce no attributes on the module.
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesUnconnectedDrive, ModuleHasNoAttributes) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getAttributes() || m->getAttributes()->empty())
      << "`unconnected_drive should not produce attribute nodes";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
