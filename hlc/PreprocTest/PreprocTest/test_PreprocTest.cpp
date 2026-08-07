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

#include <hlc/ErrorReporting/ErrorContainer.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/identifier.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/source_file.h>

namespace hlc {
class PreprocTestTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PreprocTest.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.4 + 22.5.1: bp_common_pkg.vh must be a top-level source file.
TEST_F(PreprocTestTest, BpCommonPkgSourceFileRecorded) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("bp_common_pkg.vh", m_design->getSourceFiles());
  EXPECT_NE(sf, nullptr) << "bp_common_pkg.vh must be recorded as a source file";
}

// LRM 22.4 + 22.5.1: bp_fe_icache_pkg.vh must be a top-level source file.
TEST_F(PreprocTestTest, BpFeIcachePkgSourceFileRecorded) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("bp_fe_icache_pkg.vh", m_design->getSourceFiles());
  EXPECT_NE(sf, nullptr) << "bp_fe_icache_pkg.vh must be recorded as a source file";
}

// LRM 22.5.1 + 22.6 (ifdef guards): BP_COMMON_ME_IF_VH is the include guard
// defined in bp_common_me_if.vh to prevent double-inclusion. It must be
// recorded in the file where it is defined (a direct child of
// bp_common_pkg.vh's include tree).
TEST_F(PreprocTestTest, BpCommonMeIfGuardDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("bp_common_pkg.vh", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr) << "bp_common_pkg.vh must include bp_common_me_if.vh";
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("bp_common_me_if.vh", sf->getIncludes());
  ASSERT_NE(inc, nullptr) << "bp_common_me_if.vh not found in bp_common_pkg.vh's includes";
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("BP_COMMON_ME_IF_VH", inc->getPreprocMacroDefinitions());
  EXPECT_NE(macro, nullptr) << "BP_COMMON_ME_IF_VH include guard must be recorded in bp_common_me_if.vh";
}

// LRM 22.5.1 + 22.6: BP_FE_ICACHE_VH is the include guard defined in
// bp_fe_icache.vh, a direct child of bp_fe_icache_pkg.vh's include tree.
// Same reasoning as BP_COMMON_ME_IF_VH above.
TEST_F(PreprocTestTest, BpFeIcacheGuardDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("bp_fe_icache_pkg.vh", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr) << "bp_fe_icache_pkg.vh must include bp_fe_icache.vh";
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("bp_fe_icache.vh", sf->getIncludes());
  ASSERT_NE(inc, nullptr) << "bp_fe_icache.vh not found in bp_fe_icache_pkg.vh's includes";
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("BP_FE_ICACHE_VH", inc->getPreprocMacroDefinitions());
  EXPECT_NE(macro, nullptr) << "BP_FE_ICACHE_VH include guard must be recorded in bp_fe_icache.vh";
}

// ----
// 1. PreprocMacroDefinition arguments and tokens
// ----

// LRM 22.5.1: BP_COMMON_ME_IF_VH is a flag macro (`define BP_COMMON_ME_IF_VH
// with no body and no argument list).
TEST_F(PreprocTestTest, BpCommonMeIfGuardHasNoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("bp_common_pkg.vh", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("bp_common_me_if.vh", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("BP_COMMON_ME_IF_VH", inc->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getArguments() == nullptr || macro->getArguments()->empty())
      << "BP_COMMON_ME_IF_VH is a flag macro with no argument list";
}

TEST_F(PreprocTestTest, BpCommonMeIfGuardHasNoTokens) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("bp_common_pkg.vh", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("bp_common_me_if.vh", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("BP_COMMON_ME_IF_VH", inc->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getTokens() == nullptr || macro->getTokens()->empty())
      << "BP_COMMON_ME_IF_VH is a flag macro with no replacement body";
}

// LRM 22.5.1: BP_FE_ICACHE_VH is a flag macro (`define BP_FE_ICACHE_VH with
// no body and no argument list).
TEST_F(PreprocTestTest, BpFeIcacheGuardHasNoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("bp_fe_icache_pkg.vh", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("bp_fe_icache.vh", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("BP_FE_ICACHE_VH", inc->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getArguments() == nullptr || macro->getArguments()->empty())
      << "BP_FE_ICACHE_VH is a flag macro with no argument list";
}

TEST_F(PreprocTestTest, BpFeIcacheGuardHasNoTokens) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("bp_fe_icache_pkg.vh", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("bp_fe_icache.vh", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("BP_FE_ICACHE_VH", inc->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getTokens() == nullptr || macro->getTokens()->empty())
      << "BP_FE_ICACHE_VH is a flag macro with no replacement body";
}

// ----
// 2. Overall compilation cleanliness
// ----

// Neither file defines anything illegal; the whole compilation (both
// top-level files plus their include trees) must produce no errors.
TEST_F(PreprocTestTest, NoErrorsReported) {
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
