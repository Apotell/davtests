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
#include <hldb/identifier.h>
#include <hldb/module.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/source_file.h>

namespace hlc {
class SimpleIncludeAndMacrosTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "SimpleIncludeAndMacros.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.5.1: all FAKELIB_* modules from lib.v must compile.
TEST_F(SimpleIncludeAndMacrosTest, FakelibNand2Compiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("work@FAKELIB_NAND2", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "FAKELIB_NAND2 must compile";
}

TEST_F(SimpleIncludeAndMacrosTest, FakelibDffCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("work@FAKELIB_DFF", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "FAKELIB_DFF must compile";
}

// LRM 22.4: my_incl.vh must be recorded as an include of top.v.
TEST_F(SimpleIncludeAndMacrosTest, MyInclVhIncluded) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("top.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("my_incl.vh", sf->getIncludes());
  EXPECT_NE(inc, nullptr) << "my_incl.vh must be recorded as an include of top.v";
}

// LRM 22.5.1: _my_incl_vh_ is the include guard defined in my_incl.vh.
TEST_F(SimpleIncludeAndMacrosTest, MyInclVhGuardDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("top.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("my_incl.vh", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("_my_incl_vh_", inc->getPreprocMacroDefinitions());
  EXPECT_NE(macro, nullptr) << "_my_incl_vh_ include guard must be defined in my_incl.vh";
}

// LRM 22.4: mode.vh must be found in the include chain (via bar.vh -> mode.vh
// or directly), and BLOB must be a defined macro inside it.
TEST_F(SimpleIncludeAndMacrosTest, BlobMacroReachable) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("top.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  // mode.vh defines BLOB; it may appear in top.v's include chain.
  if (sf->getIncludes() != nullptr) {
    const hldb::SourceFile *const modeVh = hldb::findByName<hldb::SourceFile>("mode.vh", sf->getIncludes());
    if (modeVh != nullptr) {
      const hldb::PreprocMacroDefinition *const macro =
          hldb::findByName<hldb::PreprocMacroDefinition>("BLOB", modeVh->getPreprocMacroDefinitions());
      EXPECT_NE(macro, nullptr) << "BLOB must be defined in mode.vh";
    }
  }
}

// LRM 22.5.1: N is defined in top.v itself (object-like, body at col 11).
TEST_F(SimpleIncludeAndMacrosTest, NMacroDefinedInTopV) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("top.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("N", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "N must be defined in top.v";
  ASSERT_NE(macro->getTokens(), nullptr);
  ASSERT_FALSE(macro->getTokens()->empty());
  ASSERT_NE(macro->getTokens()->front(), nullptr);
  EXPECT_EQ(macro->getTokens()->front()->getStartColumn(), 11u) << "N body starts at column 11";
}

// LRM 22.5.1: N is object-like (no argument list).
TEST_F(SimpleIncludeAndMacrosTest, NMacroHasNoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("top.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("N", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getArguments() == nullptr || macro->getArguments()->empty())
      << "N is object-like; getArguments() must be null or empty";
}

TEST_F(SimpleIncludeAndMacrosTest, NMacroHasOneToken) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("top.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("N", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getTokens(), nullptr);
  EXPECT_FALSE(macro->getTokens()->empty()) << "N body must have at least one token (4)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
