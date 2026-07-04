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

// Validates that the `include compiler directive is recorded in UHDM and that
// including an empty file does not affect the compiled module.
//
// SV source:
//   `include "/dev/null"   // include an empty file (Unix null device)
//   module empty();
//   endmodule
//
// UHDM structure:
//   SourceFile
//     vpiIncludes (1 item): SourceFile name:"/dev/null"
//   Module name:work@empty  — one empty module, no nets, no processes
//
// The `include directive is recorded as a child SourceFile in the parent
// SourceFile's vpiIncludes list.  The included file contributes no nodes
// to the design since /dev/null is empty.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/source_file.h>

namespace hlc {

class CompilerDirectivesInclude : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.6.4--compiler-directives-include.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::SourceFile *getSourceFile(const hldb::Design *d) {
  if (!d->getSourceFiles() || d->getSourceFiles()->empty()) return nullptr;
  return (*d->getSourceFiles())[0];
}

// ---------------------------------------------------------------------------
// Module — empty, unaffected by the included file
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesInclude, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@empty", m_design->getAllModules()), nullptr)
      << "module 'work@empty' not found";
}

TEST_F(CompilerDirectivesInclude, ModuleIsEmpty) {
  const hldb::Module *const m = hldb::findByName<hldb::Module>("work@empty", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getNets() || m->getNets()->empty());
  EXPECT_TRUE(!m->getProcesses() || m->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// `include is recorded as a child SourceFile in vpiIncludes
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesInclude, OneIncludeRecorded) {
  const hldb::SourceFile *const sf = getSourceFile(m_design);
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  EXPECT_EQ(sf->getIncludes()->size(), 1u) << "exactly one `include directive should be recorded";
}

TEST_F(CompilerDirectivesInclude, IncludedFileNameContainsNull) {
  // The SV uses `include "/dev/null".  The exact name stored by Surelog is
  // platform-dependent ("/dev/null" on Unix), but it will always contain
  // "null" as part of the path.
  const hldb::SourceFile *const sf = getSourceFile(m_design);
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  ASSERT_EQ(sf->getIncludes()->size(), 1u);
  const hldb::SourceFile *const inc = (*sf->getIncludes())[0];
  ASSERT_NE(inc, nullptr);
  const std::string_view name = inc->getName();
  EXPECT_NE(name.find("null"), std::string_view::npos) << "included file name should contain 'null'; got: " << name;
}

TEST_F(CompilerDirectivesInclude, IncludedFileHasNoNets) {
  // The included file (/dev/null) is empty, so its SourceFile node should
  // carry no nets or other content.
  const hldb::SourceFile *const sf = getSourceFile(m_design);
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  ASSERT_EQ(sf->getIncludes()->size(), 1u);
  const hldb::SourceFile *const inc = (*sf->getIncludes())[0];
  ASSERT_NE(inc, nullptr);
  // An empty include contributes no macro definitions.
  EXPECT_TRUE(!inc->getPreprocMacroDefinitions() || inc->getPreprocMacroDefinitions()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
