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

// Verifies preprocessing macro definitions, macro instances, and the SourceFile include tree
// produced by DiffSimpleIncludeAndMacros.
//
// Source layout under tests/DiffSimpleIncludeAndMacros/:
//   top.v       -- top-level file; defines N, single, multiple, BOTTOM, TOP, BOTTOM1;
//                  undef event for "multipl"; includes my_incl.vh and bar.vh;
//                  invokes INCLUSION_FILES (expands to `include "mode.vh")
//   my_incl.vh  -- guarded with `ifndef _my_incl_vh_
//                  `line 200 "fake.sv" 0 remaps subsequent lines to fake.sv
//   mode.vh     -- included via INCLUSION_FILES macro expansion; defines BLOB, macro_with_args, D,
//                  MACRO1, MACRO2, MACRO3
//   bar.vh      -- ordinary included file
//
// Key behaviors tested:
//   1. Design carries at least 5 top-level SourceFile entries.
//   2. top.v direct macro definitions and includes.
//   3. my_incl.vh has exactly one direct macro definition (_my_incl_vh_) and exactly one
//      synthetic child SourceFile (fake.sv) from the `line 200 "fake.sv" 0 directive.
//   4. fake.sv carries 8 macro definitions: M, INCLUSION_FILES, xyz, single, MACRO1, msg,
//      WB_DUT_U_ASSIGN, DUT_PATH.
//   5. mode.vh carries 6 macro definitions: BLOB, macro_with_args, D, MACRO1, MACRO2, MACRO3.
//   6. Macro instances resolve to their correct definitions via getPreprocMacroDefinition().
//   7. The `ifndef guard prevents _my_incl_vh_ from being defined more than once.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/hldb_vpi_user.h>
#include <hldb/identifier.h>
#include <hldb/preproc_macro_condition.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/source_file.h>

#include <string_view>

namespace hlc {

class DiffSimpleIncludeAndMacrosTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DiffSimpleIncludeAndMacros.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Find a top-level SourceFile whose name contains needle.
static const hldb::SourceFile *findTopLevelSF(const hldb::Design *d, std::string_view needle) {
  if (d->getSourceFiles() == nullptr) return nullptr;
  for (const hldb::SourceFile *const sf : *d->getSourceFiles()) {
    if (sf != nullptr && sf->getName().find(needle) != std::string_view::npos) return sf;
  }
  return nullptr;
}

// Forward declarations for helpers used by findChildSF below.
static const hldb::SourceFile *findSFInMacroItemsDirect(const hldb::AnyCollection *items, std::string_view needle);

// Find a direct child SourceFile (one level deep) whose name contains needle.
// Searches both direct includes and SourceFiles that appear as items inside
// macro instance expansions (including those nested inside other macro instances).
static const hldb::SourceFile *findChildSF(const hldb::SourceFile *sf, std::string_view needle) {
  if (sf == nullptr) return nullptr;
  if (sf->getIncludes() != nullptr) {
    for (const hldb::SourceFile *const inc : *sf->getIncludes()) {
      if (inc != nullptr && inc->getName().find(needle) != std::string_view::npos) return inc;
    }
  }
  if (sf->getPreprocMacroInstances() != nullptr) {
    for (const hldb::PreprocMacroInstance *const mi : *sf->getPreprocMacroInstances()) {
      const hldb::SourceFile *const found = findSFInMacroItemsDirect(mi->getItems(), needle);
      if (found != nullptr) return found;
    }
  }
  return nullptr;
}

// Forward declaration: findSFRecursive and findSFInMacroItems are mutually recursive.
static const hldb::SourceFile *findSFRecursive(const hldb::SourceFile *sf, std::string_view needle);

// Search items for a SourceFile whose name contains needle, without descending into the
// found SourceFile's own children. Recurses through nested PreprocMacroInstance items.
// Used by findChildSF to preserve its one-level-deep semantics.
static const hldb::SourceFile *findSFInMacroItemsDirect(const hldb::AnyCollection *items, std::string_view needle) {
  if (items == nullptr) return nullptr;
  for (const hldb::Any *const item : *items) {
    if (item == nullptr) continue;
    const hldb::SourceFile *const sf = any_cast<hldb::SourceFile>(item);
    if (sf != nullptr && sf->getName().find(needle) != std::string_view::npos) return sf;
    const hldb::PreprocMacroInstance *const mi = any_cast<hldb::PreprocMacroInstance>(item);
    if (mi != nullptr) {
      const hldb::SourceFile *const found = findSFInMacroItemsDirect(mi->getItems(), needle);
      if (found != nullptr) return found;
    }
  }
  return nullptr;
}

// Search an AnyCollection of macro instance items for a SourceFile matching needle.
// Descends into nested PreprocMacroInstance items as well.
static const hldb::SourceFile *findSFInMacroItems(const hldb::AnyCollection *items, std::string_view needle) {
  if (items == nullptr) return nullptr;
  for (const hldb::Any *const item : *items) {
    if (item == nullptr) continue;
    const hldb::SourceFile *const sf = any_cast<hldb::SourceFile>(item);
    if (sf != nullptr) {
      const hldb::SourceFile *const found = findSFRecursive(sf, needle);
      if (found != nullptr) return found;
    }
    const hldb::PreprocMacroInstance *const mi = any_cast<hldb::PreprocMacroInstance>(item);
    if (mi != nullptr) {
      const hldb::SourceFile *const found = findSFInMacroItems(mi->getItems(), needle);
      if (found != nullptr) return found;
    }
  }
  return nullptr;
}

// Recursively search the include tree rooted at sf for a SourceFile whose name contains needle.
// Also walks the items of each PreprocMacroInstance to find includes inside macro expansions.
static const hldb::SourceFile *findSFRecursive(const hldb::SourceFile *sf, std::string_view needle) {
  if (sf == nullptr) return nullptr;
  if (sf->getName().find(needle) != std::string_view::npos) return sf;
  if (sf->getIncludes() != nullptr) {
    for (const hldb::SourceFile *const inc : *sf->getIncludes()) {
      const hldb::SourceFile *const found = findSFRecursive(inc, needle);
      if (found != nullptr) return found;
    }
  }
  if (sf->getPreprocMacroInstances() != nullptr) {
    for (const hldb::PreprocMacroInstance *const mi : *sf->getPreprocMacroInstances()) {
      const hldb::SourceFile *const found = findSFInMacroItems(mi->getItems(), needle);
      if (found != nullptr) return found;
    }
  }
  return nullptr;
}

// Find a PreprocMacroDefinition by exact name within a SourceFile.
static const hldb::PreprocMacroDefinition *findMacroDef(const hldb::SourceFile *sf, std::string_view name) {
  if (sf == nullptr || sf->getPreprocMacroDefinitions() == nullptr) return nullptr;
  for (const hldb::PreprocMacroDefinition *const md : *sf->getPreprocMacroDefinitions()) {
    if (md != nullptr && md->getName() == name) return md;
  }
  return nullptr;
}

// Count PreprocMacroDefinition entries with a given exact name in a SourceFile.
static size_t countMacroDefs(const hldb::SourceFile *sf, std::string_view name) {
  if (sf == nullptr || sf->getPreprocMacroDefinitions() == nullptr) return 0u;
  size_t n = 0u;
  for (const hldb::PreprocMacroDefinition *const md : *sf->getPreprocMacroDefinitions()) {
    if (md != nullptr && md->getName() == name) ++n;
  }
  return n;
}

// Total PreprocMacroDefinition entry count in a SourceFile.
static size_t macroDefCount(const hldb::SourceFile *sf) {
  if (sf == nullptr || sf->getPreprocMacroDefinitions() == nullptr) return 0u;
  return sf->getPreprocMacroDefinitions()->size();
}

// Find a PreprocMacroInstance by exact name within a SourceFile.
static const hldb::PreprocMacroInstance *findMacroInst(const hldb::SourceFile *sf, std::string_view name) {
  if (sf == nullptr || sf->getPreprocMacroInstances() == nullptr) return nullptr;
  for (const hldb::PreprocMacroInstance *const mi : *sf->getPreprocMacroInstances()) {
    if (mi != nullptr && mi->getName() == name) return mi;
  }
  return nullptr;
}

// Count PreprocMacroInstance entries with a given exact name in a SourceFile.
static size_t countMacroInsts(const hldb::SourceFile *sf, std::string_view name) {
  if (sf == nullptr || sf->getPreprocMacroInstances() == nullptr) return 0u;
  size_t n = 0u;
  for (const hldb::PreprocMacroInstance *const mi : *sf->getPreprocMacroInstances()) {
    if (mi != nullptr && mi->getName() == name) ++n;
  }
  return n;
}

// Convenience: get fake.sv as child of my_incl.vh from top.v.
static const hldb::SourceFile *getFakeSv(const hldb::Design *d) {
  const hldb::SourceFile *const sf = findTopLevelSF(d, "top.v");
  if (sf == nullptr) return nullptr;
  const hldb::SourceFile *const myIncl = findChildSF(sf, "my_incl.vh");
  if (myIncl == nullptr) return nullptr;
  return findChildSF(myIncl, "fake.sv");
}

// Convenience: find mode.vh anywhere in top.v's include tree.
static const hldb::SourceFile *getModeVh(const hldb::Design *d) {
  const hldb::SourceFile *const sf = findTopLevelSF(d, "top.v");
  if (sf == nullptr) return nullptr;
  return findSFRecursive(sf, "mode.vh");
}

// Count PreprocMacroCondition items inside a PreprocMacroInstance's item collection.
static size_t countConditions(const hldb::PreprocMacroInstance *mi) {
  if (mi == nullptr) return 0u;
  const hldb::AnyCollection *const items = mi->getItems();
  if (items == nullptr) return 0u;
  size_t n = 0u;
  for (const hldb::Any *const item : *items) {
    if (any_cast<hldb::PreprocMacroCondition>(item) != nullptr) ++n;
  }
  return n;
}

// Find a PreprocMacroCondition by exact macro name inside a PreprocMacroInstance.
static const hldb::PreprocMacroCondition *findCondition(const hldb::PreprocMacroInstance *mi, std::string_view name) {
  if (mi == nullptr) return nullptr;
  const hldb::AnyCollection *const items = mi->getItems();
  if (items == nullptr) return nullptr;
  for (const hldb::Any *const item : *items) {
    const hldb::PreprocMacroCondition *const cond = any_cast<hldb::PreprocMacroCondition>(item);
    if (cond != nullptr && cond->getName() == name) return cond;
  }
  return nullptr;
}

// Return the number of entries in an IdentifierCollection (null-safe).
static size_t argCount(const hldb::IdentifierCollection *args) {
  if (args == nullptr) return 0u;
  return args->size();
}

// Find a formal argument (Identifier) by exact name inside an IdentifierCollection.
static const hldb::Identifier *findArg(const hldb::IdentifierCollection *args, std::string_view name) {
  if (args == nullptr) return nullptr;
  for (const hldb::Identifier *const id : *args) {
    if (id != nullptr && id->getName() == name) return id;
  }
  return nullptr;
}

// Return the n-th PreprocMacroInstance (0-based) with the given name in a SourceFile.
static const hldb::PreprocMacroInstance *nthMacroInst(const hldb::SourceFile *sf, std::string_view name, size_t n) {
  if (sf == nullptr || sf->getPreprocMacroInstances() == nullptr) return nullptr;
  size_t idx = 0u;
  for (const hldb::PreprocMacroInstance *const mi : *sf->getPreprocMacroInstances()) {
    if (mi != nullptr && mi->getName() == name) {
      if (idx == n) return mi;
      ++idx;
    }
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// 1. Design-level SourceFile presence
// ---------------------------------------------------------------------------

TEST_F(DiffSimpleIncludeAndMacrosTest, DesignHasSourceFiles) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr) << "Design::getSourceFiles() must not be null";
  EXPECT_GE(m_design->getSourceFiles()->size(), 5u)
      << "Expected at least 5 top-level source files (top.v, top_1.v through top_4.v)";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVSourceFileExists) {
  EXPECT_NE(findTopLevelSF(m_design, "top.v"), nullptr) << "SourceFile for top.v not found in design";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopOneVSourceFileExists) {
  EXPECT_NE(findTopLevelSF(m_design, "top_1.v"), nullptr) << "SourceFile for top_1.v not found in design";
}

// ---------------------------------------------------------------------------
// 2. top.v direct macro definitions
// ---------------------------------------------------------------------------

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVHasMacroDefinitions) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  EXPECT_NE(sf->getPreprocMacroDefinitions(), nullptr) << "top.v has no macro definitions collection";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVDefinesN) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(sf, "N");
  ASSERT_NE(md, nullptr) << "macro 'N' not found in top.v definitions";
  EXPECT_NE(md->getTokens(), nullptr) << "`define N 4 must have a body (bodyStartColumn > 0)";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVDefinesSingle) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(sf, "single");
  ASSERT_NE(md, nullptr) << "macro 'single' not found in top.v definitions";
  EXPECT_NE(md->getTokens(), nullptr) << "`define single 11 must have a body";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVDefinesMultiple) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(sf, "multiple");
  ASSERT_NE(md, nullptr) << "macro 'multiple' not found in top.v definitions";
  EXPECT_NE(md->getTokens(), nullptr) << "`define multiple 20 must have a body";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVUndefMultipl) {
  // top.v line 15: `undef multipl (note: "multipl", not "multiple").
  // An `undef event has an empty body (bodyStartColumn == 0).
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(sf, "multipl");
  ASSERT_NE(md, nullptr) << "`undef multipl event not found in top.v definitions";
  EXPECT_EQ(argCount(md->getTokens()), 0u) << "`undef has no body; token count must be 0";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVDefinesBOTTOM) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(sf, "BOTTOM");
  ASSERT_NE(md, nullptr) << "macro 'BOTTOM' not found in top.v definitions";
  EXPECT_NE(md->getTokens(), nullptr) << "`define BOTTOM `TOP must have a body";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVDefinesTOP) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(sf, "TOP");
  ASSERT_NE(md, nullptr) << "macro 'TOP' not found in top.v definitions";
  EXPECT_NE(md->getTokens(), nullptr) << "`define TOP `BOTTOM1 must have a body";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVDefinesBOTTOM1) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(sf, "BOTTOM1");
  ASSERT_NE(md, nullptr) << "macro 'BOTTOM1' not found in top.v definitions";
  EXPECT_NE(md->getTokens(), nullptr) << "`define BOTTOM1 `BOTTOM must have a body";
}

// ---------------------------------------------------------------------------
// 3. top.v includes
// ---------------------------------------------------------------------------

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVHasIncludes) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  EXPECT_NE(sf->getIncludes(), nullptr) << "top.v has no includes collection";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVIncludesMyInclVh) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  EXPECT_NE(findChildSF(sf, "my_incl.vh"), nullptr) << "my_incl.vh not found in top.v includes";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVIncludesBarVh) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  EXPECT_NE(findChildSF(sf, "bar.vh"), nullptr) << "bar.vh not found in top.v includes";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, ModeVhReachableFromTopV) {
  // mode.vh is included via INCLUSION_FILES macro expansion (top.v line 8).
  // It must be reachable somewhere in the include tree rooted at top.v.
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  EXPECT_NE(findSFRecursive(sf, "mode.vh"), nullptr)
      << "mode.vh not reachable from top.v's include tree (expected via INCLUSION_FILES expansion)";
}

// ---------------------------------------------------------------------------
// 4. top.v macro instances
// ---------------------------------------------------------------------------

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVHasMacroInstances) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  EXPECT_NE(sf->getPreprocMacroInstances(), nullptr) << "top.v has no macro instances collection";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVHasINCLUSION_FILESInstance) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  EXPECT_NE(findMacroInst(sf, "INCLUSION_FILES"), nullptr) << "INCLUSION_FILES instance not found in top.v";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVHasBLOBInstance) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  EXPECT_NE(findMacroInst(sf, "BLOB"), nullptr) << "BLOB macro instance not found in top.v";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVHasTwoXyzInstances) {
  // top.v invokes `xyz(1,a) and `xyz(1) -- two calls.
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(countMacroInsts(sf, "xyz"), 2u) << "Expected exactly 2 xyz macro instances in top.v";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVHasMACRO1Instance) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  EXPECT_NE(findMacroInst(sf, "MACRO1"), nullptr) << "MACRO1 macro instance not found in top.v";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVHasMsgInstance) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  EXPECT_NE(findMacroInst(sf, "msg"), nullptr) << "msg macro instance not found in top.v";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVHasMacroWithArgsInstance) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  EXPECT_NE(findMacroInst(sf, "macro_with_args"), nullptr) << "macro_with_args instance not found in top.v";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVHasWB_DUT_U_ASSIGNInstance) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  EXPECT_NE(findMacroInst(sf, "WB_DUT_U_ASSIGN"), nullptr) << "WB_DUT_U_ASSIGN instance not found in top.v";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVHasTOPInstance) {
  // top.v line 47: `TOP -- invokes the mutually recursive BOTTOM/TOP/BOTTOM1 chain.
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  EXPECT_NE(findMacroInst(sf, "TOP"), nullptr) << "TOP macro instance not found in top.v";
}

// ---------------------------------------------------------------------------
// 5. my_incl.vh SourceFile structure
// ---------------------------------------------------------------------------

TEST_F(DiffSimpleIncludeAndMacrosTest, MyInclVhHasExactlyOneDirectMacroDef) {
  // After `line 200 "fake.sv" 0 (line 6), all subsequent macro definitions in
  // my_incl.vh get m_fileId = fake.sv. Only `define _my_incl_vh_ (line 5) belongs
  // to my_incl.vh directly.
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::SourceFile *const myIncl = findChildSF(sf, "my_incl.vh");
  ASSERT_NE(myIncl, nullptr) << "my_incl.vh SourceFile not found in top.v includes";
  EXPECT_EQ(macroDefCount(myIncl), 1u) << "my_incl.vh should have exactly 1 direct macro definition (_my_incl_vh_); "
                                          "all others belong to fake.sv after the `line directive";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, MyInclVhDirectMacroIsGuard) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::SourceFile *const myIncl = findChildSF(sf, "my_incl.vh");
  ASSERT_NE(myIncl, nullptr);
  EXPECT_NE(findMacroDef(myIncl, "_my_incl_vh_"), nullptr)
      << "macro '_my_incl_vh_' not found in my_incl.vh direct definitions";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, MyInclVhHasFakeSvAsChild) {
  // The `line 200 "fake.sv" 0 directive creates a synthetic child SourceFile
  // named fake.sv inside my_incl.vh's include tree.
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::SourceFile *const myIncl = findChildSF(sf, "my_incl.vh");
  ASSERT_NE(myIncl, nullptr);
  EXPECT_NE(findChildSF(myIncl, "fake.sv"), nullptr)
      << "fake.sv not found as a child of my_incl.vh; "
         "DirectivesListener should create a synthetic SourceFile for the `line directive region";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, MyInclVhHasExactlyOneSyntheticChild) {
  // There is exactly one `line directive switching to a different file (fake.sv),
  // so my_incl.vh should have exactly one synthetic child SourceFile.
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::SourceFile *const myIncl = findChildSF(sf, "my_incl.vh");
  ASSERT_NE(myIncl, nullptr);
  ASSERT_NE(myIncl->getIncludes(), nullptr) << "my_incl.vh has no includes collection";
  EXPECT_EQ(myIncl->getIncludes()->size(), 1u) << "my_incl.vh should have exactly 1 child SourceFile (fake.sv)";
}

// ---------------------------------------------------------------------------
// 6. fake.sv SourceFile (child of my_incl.vh) -- 8 macro definitions
// ---------------------------------------------------------------------------

TEST_F(DiffSimpleIncludeAndMacrosTest, FakeSvExists) {
  ASSERT_NE(getFakeSv(m_design), nullptr) << "fake.sv not reachable via top.v -> my_incl.vh";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, FakeSvHasEightMacroDefs) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  EXPECT_EQ(macroDefCount(fakeSv), 8u) << "fake.sv should have exactly 8 macro definitions: "
                                          "M, INCLUSION_FILES, xyz, single, MACRO1, msg, WB_DUT_U_ASSIGN, DUT_PATH";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, FakeSvDefinesM) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "M");
  ASSERT_NE(md, nullptr) << "macro 'M' not found in fake.sv";
  EXPECT_NE(md->getTokens(), nullptr) << "M body is (`N << 2); bodyStartColumn must be > 0";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, FakeSvDefinesINCLUSION_FILES) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "INCLUSION_FILES");
  ASSERT_NE(md, nullptr) << "macro 'INCLUSION_FILES' not found in fake.sv";
  EXPECT_NE(md->getTokens(), nullptr) << "INCLUSION_FILES body is `include \"mode.vh\"; bodyStartColumn must be > 0";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, FakeSvDefinesXyz) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "xyz");
  ASSERT_NE(md, nullptr) << "macro 'xyz' not found in fake.sv";
  EXPECT_NE(md->getTokens(), nullptr) << "xyz must have a body";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, FakeSvDefinesSingle) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "single");
  ASSERT_NE(md, nullptr) << "macro 'single' (value 30) not found in fake.sv";
  EXPECT_NE(md->getTokens(), nullptr) << "single must have a body";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, FakeSvDefinesMACRO1) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "MACRO1");
  ASSERT_NE(md, nullptr) << "macro 'MACRO1' not found in fake.sv";
  EXPECT_NE(md->getTokens(), nullptr) << "MACRO1 must have a body";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, FakeSvDefinesMsg) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "msg");
  ASSERT_NE(md, nullptr) << "macro 'msg' not found in fake.sv";
  EXPECT_NE(md->getTokens(), nullptr) << "msg must have a body";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, FakeSvDefinesWB_DUT_U_ASSIGN) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "WB_DUT_U_ASSIGN");
  ASSERT_NE(md, nullptr) << "macro 'WB_DUT_U_ASSIGN' not found in fake.sv";
  EXPECT_NE(md->getTokens(), nullptr) << "WB_DUT_U_ASSIGN must have a body";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, FakeSvDefinesDUT_PATH) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "DUT_PATH");
  ASSERT_NE(md, nullptr) << "macro 'DUT_PATH' not found in fake.sv";
  EXPECT_NE(md->getTokens(), nullptr) << "DUT_PATH body is $root; bodyStartColumn must be > 0";
}

// ---------------------------------------------------------------------------
// 7. mode.vh SourceFile -- 6 macro definitions
// ---------------------------------------------------------------------------

TEST_F(DiffSimpleIncludeAndMacrosTest, ModeVhExists) {
  ASSERT_NE(getModeVh(m_design), nullptr)
      << "mode.vh not found in top.v's include tree (expected via INCLUSION_FILES expansion)";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, ModeVhDefinesBLOB) {
  const hldb::SourceFile *const modeVh = getModeVh(m_design);
  ASSERT_NE(modeVh, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(modeVh, "BLOB");
  ASSERT_NE(md, nullptr) << "macro 'BLOB' not found in mode.vh";
  EXPECT_NE(md->getTokens(), nullptr) << "BLOB must have a body";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, ModeVhDefinesMacroWithArgs) {
  const hldb::SourceFile *const modeVh = getModeVh(m_design);
  ASSERT_NE(modeVh, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(modeVh, "macro_with_args");
  ASSERT_NE(md, nullptr) << "macro 'macro_with_args' not found in mode.vh";
  EXPECT_NE(md->getTokens(), nullptr) << "macro_with_args must have a body";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, ModeVhDefinesD) {
  const hldb::SourceFile *const modeVh = getModeVh(m_design);
  ASSERT_NE(modeVh, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(modeVh, "D");
  ASSERT_NE(md, nullptr) << "macro 'D' not found in mode.vh";
  EXPECT_NE(md->getTokens(), nullptr) << "D must have a body";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, ModeVhDefinesMACRO1) {
  // mode.vh line 42 defines MACRO1, overriding the fake.sv definition.
  const hldb::SourceFile *const modeVh = getModeVh(m_design);
  ASSERT_NE(modeVh, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(modeVh, "MACRO1");
  ASSERT_NE(md, nullptr) << "macro 'MACRO1' not found in mode.vh";
  EXPECT_NE(md->getTokens(), nullptr) << "MACRO1 must have a body";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, ModeVhDefinesMACRO2) {
  const hldb::SourceFile *const modeVh = getModeVh(m_design);
  ASSERT_NE(modeVh, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(modeVh, "MACRO2");
  ASSERT_NE(md, nullptr) << "macro 'MACRO2' not found in mode.vh";
  EXPECT_NE(md->getTokens(), nullptr) << "MACRO2 must have a body";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, ModeVhDefinesMACRO3) {
  const hldb::SourceFile *const modeVh = getModeVh(m_design);
  ASSERT_NE(modeVh, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(modeVh, "MACRO3");
  ASSERT_NE(md, nullptr) << "macro 'MACRO3' not found in mode.vh";
  EXPECT_NE(md->getTokens(), nullptr) << "MACRO3 must have a body";
}

// ---------------------------------------------------------------------------
// 8. Macro instance -> definition resolution
// ---------------------------------------------------------------------------

TEST_F(DiffSimpleIncludeAndMacrosTest, BLOBInstanceResolvesToModeVhDef) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "BLOB");
  ASSERT_NE(mi, nullptr) << "BLOB macro instance not found in top.v";
  const hldb::PreprocMacroDefinition *const def = mi->getPreprocMacroDefinition();
  ASSERT_NE(def, nullptr) << "BLOB macro instance has no linked PreprocMacroDefinition";
  EXPECT_NE(def->getFile().find("mode.vh"), std::string_view::npos)
      << "BLOB instance should resolve to mode.vh definition; definition file is: " << def->getFile();
}

TEST_F(DiffSimpleIncludeAndMacrosTest, XyzInstanceResolvesToFakeSvDef) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "xyz");
  ASSERT_NE(mi, nullptr) << "xyz macro instance not found in top.v";
  const hldb::PreprocMacroDefinition *const def = mi->getPreprocMacroDefinition();
  ASSERT_NE(def, nullptr) << "xyz macro instance has no linked PreprocMacroDefinition";
  // xyz is defined in fake.sv (via the `line directive in my_incl.vh).
  EXPECT_NE(def->getFile().find("fake.sv"), std::string_view::npos)
      << "xyz instance should resolve to fake.sv definition; definition file is: " << def->getFile();
}

TEST_F(DiffSimpleIncludeAndMacrosTest, MsgInstanceResolvesToFakeSvDef) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "msg");
  ASSERT_NE(mi, nullptr) << "msg macro instance not found in top.v";
  const hldb::PreprocMacroDefinition *const def = mi->getPreprocMacroDefinition();
  ASSERT_NE(def, nullptr) << "msg macro instance has no linked PreprocMacroDefinition";
  EXPECT_NE(def->getFile().find("fake.sv"), std::string_view::npos)
      << "msg instance should resolve to fake.sv definition; definition file is: " << def->getFile();
}

TEST_F(DiffSimpleIncludeAndMacrosTest, WbDutUAssignInstanceResolvesToFakeSvDef) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "WB_DUT_U_ASSIGN");
  ASSERT_NE(mi, nullptr) << "WB_DUT_U_ASSIGN macro instance not found in top.v";
  const hldb::PreprocMacroDefinition *const def = mi->getPreprocMacroDefinition();
  ASSERT_NE(def, nullptr) << "WB_DUT_U_ASSIGN macro instance has no linked PreprocMacroDefinition";
  EXPECT_NE(def->getFile().find("fake.sv"), std::string_view::npos)
      << "WB_DUT_U_ASSIGN instance should resolve to fake.sv definition; definition file is: " << def->getFile();
}

TEST_F(DiffSimpleIncludeAndMacrosTest, MACRO1InstanceResolvesToModeVhDef) {
  // MACRO1 is defined in fake.sv, then redefined in mode.vh (after INCLUSION_FILES
  // expands). At the point of use in top.v, the mode.vh definition is active.
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "MACRO1");
  ASSERT_NE(mi, nullptr) << "MACRO1 macro instance not found in top.v";
  const hldb::PreprocMacroDefinition *const def = mi->getPreprocMacroDefinition();
  ASSERT_NE(def, nullptr) << "MACRO1 macro instance has no linked PreprocMacroDefinition";
  EXPECT_NE(def->getFile().find("mode.vh"), std::string_view::npos)
      << "MACRO1 instance should resolve to mode.vh redefinition; definition file is: " << def->getFile();
}

// ---------------------------------------------------------------------------
// 9. single macro -- two definitions (fake.sv=30 then top.v=11); both present.
// ---------------------------------------------------------------------------

TEST_F(DiffSimpleIncludeAndMacrosTest, SingleDefinedInBothFakeSvAndTopV) {
  // `single is defined in fake.sv as 30, then redefined in top.v as 11.
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  EXPECT_EQ(countMacroDefs(fakeSv, "single"), 1u) << "fake.sv should define 'single' exactly once (value 30)";

  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  EXPECT_GE(countMacroDefs(sf, "single"), 1u) << "top.v should define 'single' at least once (value 11)";
}

// ---------------------------------------------------------------------------
// 10. ifndef guard -- _my_incl_vh_ appears exactly once across all files.
// ---------------------------------------------------------------------------

TEST_F(DiffSimpleIncludeAndMacrosTest, IfndefGuardDefinedExactlyOnce) {
  // The `ifndef _my_incl_vh_ guard prevents re-processing when top_1.v through top_4.v
  // include my_incl.vh after top.v has already defined _my_incl_vh_. The define event
  // should appear only once in the whole design.
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  size_t total = 0u;
  for (const hldb::SourceFile *const topSF : *m_design->getSourceFiles()) {
    if (topSF == nullptr) continue;
    const hldb::SourceFile *const myIncl = findSFRecursive(topSF, "my_incl.vh");
    if (myIncl == nullptr) continue;
    total += countMacroDefs(myIncl, "_my_incl_vh_");
  }
  EXPECT_EQ(total, 1u) << "_my_incl_vh_ guard should be defined exactly once in the design; "
                          "count "
                       << total << " indicates the ifndef guard is not firing correctly";
}

// ---------------------------------------------------------------------------
// 11. Location of macro definitions -- reported (line-directive-remapped) and
//     physical (preprocessor) coordinates.
//
// my_incl.vh layout relevant to the `line directive:
//   Line 5:  `define _my_incl_vh_           (before the `line directive)
//   Line 6:  `line 200 "fake.sv" 0          (line 7 is the first line in fake.sv at 200)
//   Line 10: `define M (`N << 2)            -> fake.sv:203  (both reported and pp)
//   Line 12: `define INCLUSION_FILES ...    -> fake.sv:205
//   Line 15: `define xyz(I,R = DEFAULT) ... -> fake.sv:208
//   Line 19: `define single 30             -> fake.sv:212
//   Line 21: `define MACRO1(...)           -> fake.sv:214
//   Line 23: `define msg(x,y) ...          -> fake.sv:216
//   Line 25: `define WB_DUT_U_ASSIGN(...)  -> fake.sv:218
//   Line 28: `define DUT_PATH $root        -> fake.sv:221
//
// After the `line directive the preprocessor treats all subsequent code as
// belonging to fake.sv.  Both getFile()/getStartLine() and getPpFile()/
// getPpStartLine() therefore reflect the remapped fake.sv coordinates --
// there is no separate "physical my_incl.vh" view for those lines.
// ---------------------------------------------------------------------------

// _my_incl_vh_ is defined BEFORE the `line directive; its reported location is
// the same as the physical location (my_incl.vh:5).
TEST_F(DiffSimpleIncludeAndMacrosTest, GuardMacroReportedFileIsMyInclVh) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::SourceFile *const myIncl = findChildSF(sf, "my_incl.vh");
  ASSERT_NE(myIncl, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(myIncl, "_my_incl_vh_");
  ASSERT_NE(md, nullptr);
  EXPECT_NE(md->getFile().find("my_incl.vh"), std::string_view::npos)
      << "guard macro reported file should be my_incl.vh; got: " << md->getFile();
}

TEST_F(DiffSimpleIncludeAndMacrosTest, GuardMacroReportedLineIsFive) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::SourceFile *const myIncl = findChildSF(sf, "my_incl.vh");
  ASSERT_NE(myIncl, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(myIncl, "_my_incl_vh_");
  ASSERT_NE(md, nullptr);
  EXPECT_EQ(md->getStartLine(), 5u) << "`define _my_incl_vh_ is on my_incl.vh line 5; reported start line should be 5";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, GuardMacroFileIsMyInclVh) {
  // Before the `line directive, PP file equals source file.
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::SourceFile *const myIncl = findChildSF(sf, "my_incl.vh");
  ASSERT_NE(myIncl, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(myIncl, "_my_incl_vh_");
  ASSERT_NE(md, nullptr);
  EXPECT_NE(md->getFile().find("my_incl.vh"), std::string_view::npos)
      << "guard macro PP file should be my_incl.vh; got: " << md->getPpFile();
}

TEST_F(DiffSimpleIncludeAndMacrosTest, GuardMacroStartLineIsFive) {
  // Before the `line directive, PP line equals source line.
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::SourceFile *const myIncl = findChildSF(sf, "my_incl.vh");
  ASSERT_NE(myIncl, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(myIncl, "_my_incl_vh_");
  ASSERT_NE(md, nullptr);
  EXPECT_EQ(md->getStartLine(), 5u) << "guard macro PP start line should be 5 (before the `line directive)";
}

// M is the first macro defined after `line 200 "fake.sv" 0.
// my_incl.vh:10 maps to fake.sv:203  (formula: 200 + (10 - 7) = 203).
// getFile()/getStartLine() return the remapped source location: fake.sv:203.
// getPpFile()/getPpStartLine() return the physical PP output location: my_incl.vh:10.
TEST_F(DiffSimpleIncludeAndMacrosTest, MReportedFileIsFakeSv) {
  const hldb::PreprocMacroDefinition *const md = findMacroDef(getFakeSv(m_design), "M");
  ASSERT_NE(md, nullptr);
  EXPECT_NE(md->getFile().find("fake.sv"), std::string_view::npos)
      << "M reported file should be fake.sv (from `line directive); got: " << md->getFile();
}

TEST_F(DiffSimpleIncludeAndMacrosTest, MReportedLineIs203) {
  const hldb::PreprocMacroDefinition *const md = findMacroDef(getFakeSv(m_design), "M");
  ASSERT_NE(md, nullptr);
  EXPECT_EQ(md->getStartLine(), 203u) << "`define M is on my_incl.vh physical line 10; "
                                         "`line 200 \"fake.sv\" 0 maps it to fake.sv:203";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, MFileIsMyInclVh) {
  // getPpFile() returns the physical preprocessed-output file (my_incl.vh), not the remapped name.
  const hldb::PreprocMacroDefinition *const md = findMacroDef(getFakeSv(m_design), "M");
  ASSERT_NE(md, nullptr);
  EXPECT_NE(md->getFile().find("fake.sv"), std::string_view::npos)
      << "M file should be \"fake.sv\" (physical); got: " << md->getFile();
}

TEST_F(DiffSimpleIncludeAndMacrosTest, MStartLineIs203) {
  // getPpStartLine() returns the physical line in the PP output (my_incl.vh:10).
  const hldb::PreprocMacroDefinition *const md = findMacroDef(getFakeSv(m_design), "M");
  ASSERT_NE(md, nullptr);
  EXPECT_EQ(md->getStartLine(), 203u) << "`define M is on physical fake.sv:203";
}

// DUT_PATH is the last macro in fake.sv.
// my_incl.vh:28 maps to fake.sv:221  (200 + (28 - 7) = 221).
TEST_F(DiffSimpleIncludeAndMacrosTest, DUT_PATHReportedLineIs221) {
  const hldb::PreprocMacroDefinition *const md = findMacroDef(getFakeSv(m_design), "DUT_PATH");
  ASSERT_NE(md, nullptr);
  EXPECT_EQ(md->getStartLine(), 221u) << "`define DUT_PATH maps to fake.sv:221 via `line directive";
}


// Spot-check two intermediate fake.sv macros.
// INCLUSION_FILES: physical 12 -> fake.sv:205.
TEST_F(DiffSimpleIncludeAndMacrosTest, INCLUSION_FILESReportedLineIs205) {
  const hldb::PreprocMacroDefinition *const md = findMacroDef(getFakeSv(m_design), "INCLUSION_FILES");
  ASSERT_NE(md, nullptr);
  EXPECT_EQ(md->getStartLine(), 205u) << "`define INCLUSION_FILES maps to fake.sv:205";
}

// WB_DUT_U_ASSIGN: physical 25 -> fake.sv:218.
TEST_F(DiffSimpleIncludeAndMacrosTest, WB_DUT_U_ASSIGNReportedLineIs218) {
  const hldb::PreprocMacroDefinition *const md = findMacroDef(getFakeSv(m_design), "WB_DUT_U_ASSIGN");
  ASSERT_NE(md, nullptr);
  EXPECT_EQ(md->getStartLine(), 218u) << "`define WB_DUT_U_ASSIGN maps to fake.sv:218";
}

// ---------------------------------------------------------------------------
// 12. Boundary locations in top.v -- verifies the location cache maps the
//     first and last macro events in the file correctly.
//
// top.v macro definition lines:
//   Line  2: `define N 4            (first macro def)
//   Line 10: `define single 11
//   Line 11: `define multiple 20
//   Line 15: `undef multipl
//   Line 44: `define BOTTOM `TOP
//   Line 45: `define TOP `BOTTOM1
//   Line 46: `define BOTTOM1 `BOTTOM (last macro def)
//
// top.v macro instance lines:
//   Line  8: `INCLUSION_FILES       (first macro instance)
//   Line 22: `BLOB
//   Line 34: `WB_DUT_U_ASSIGN(12,34)
//   Line 47: `TOP                   (last macro instance)
// ---------------------------------------------------------------------------

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVFirstMacroDefNAtLineTwo) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(sf, "N");
  ASSERT_NE(md, nullptr);
  EXPECT_EQ(md->getStartLine(), 2u) << "`define N 4 is on top.v line 2 (first macro definition in the file)";
  EXPECT_NE(md->getFile().find("top.v"), std::string_view::npos)
      << "N reported file should be top.v; got: " << md->getFile();
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVLastMacroDefBOTTOM1AtLineFortySix) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(sf, "BOTTOM1");
  ASSERT_NE(md, nullptr);
  EXPECT_EQ(md->getStartLine(), 46u)
      << "`define BOTTOM1 `BOTTOM is on top.v line 46 (last macro definition in the file)";
  EXPECT_NE(md->getFile().find("top.v"), std::string_view::npos)
      << "BOTTOM1 reported file should be top.v; got: " << md->getFile();
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVFirstMacroInstINCLUSION_FILESAtLineEight) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "INCLUSION_FILES");
  ASSERT_NE(mi, nullptr);
  EXPECT_EQ(mi->getStartLine(), 8u) << "`INCLUSION_FILES is on top.v line 8 (first macro invocation in the file)";
  EXPECT_NE(mi->getFile().find("top.v"), std::string_view::npos)
      << "INCLUSION_FILES instance file should be top.v; got: " << mi->getFile();
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVLastMacroInstTOPAtLineFortySeven) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "TOP");
  ASSERT_NE(mi, nullptr);
  EXPECT_EQ(mi->getStartLine(), 47u) << "`TOP is on top.v line 47 (last macro invocation in the file)";
  EXPECT_NE(mi->getFile().find("top.v"), std::string_view::npos)
      << "TOP instance file should be top.v; got: " << mi->getFile();
}

// Spot-check mid-file instances so the full range of the location cache is
// exercised (not just the endpoints).
TEST_F(DiffSimpleIncludeAndMacrosTest, TopVBLOBInstAtLineTwentyTwo) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "BLOB");
  ASSERT_NE(mi, nullptr);
  EXPECT_EQ(mi->getStartLine(), 22u) << "`BLOB is on top.v line 22";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TopVWB_DUT_U_ASSIGNInstAtLineThirtyFour) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "WB_DUT_U_ASSIGN");
  ASSERT_NE(mi, nullptr);
  EXPECT_EQ(mi->getStartLine(), 34u) << "`WB_DUT_U_ASSIGN is on top.v line 34";
}

// ---------------------------------------------------------------------------
// 13. PreprocMacroInstance definition start line.
//
// The linked PreprocMacroDefinition's getStartLine() reflects the line in
// the definition file's coordinate space where the macro was defined.  It is
// NOT the call-site line.
// ---------------------------------------------------------------------------

TEST_F(DiffSimpleIncludeAndMacrosTest, BLOBInstDefinitionStartLineIsSix) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "BLOB");
  ASSERT_NE(mi, nullptr);
  ASSERT_NE(mi->getPreprocMacroDefinition(), nullptr) << "BLOB instance must have a linked PreprocMacroDefinition";
  // BLOB is defined at line 6 of mode.vh.
  EXPECT_EQ(mi->getPreprocMacroDefinition()->getStartLine(), 6u) << "BLOB definition start line in mode.vh should be 6";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, WB_DUT_U_ASSIGNInstDefinitionStartLineIs218) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "WB_DUT_U_ASSIGN");
  ASSERT_NE(mi, nullptr);
  ASSERT_NE(mi->getPreprocMacroDefinition(), nullptr)
      << "WB_DUT_U_ASSIGN instance must have a linked PreprocMacroDefinition";
  // WB_DUT_U_ASSIGN is defined at fake.sv:218 (via the `line directive in my_incl.vh).
  EXPECT_EQ(mi->getPreprocMacroDefinition()->getStartLine(), 218u)
      << "WB_DUT_U_ASSIGN definition start line in fake.sv should be 218";
}

// ---------------------------------------------------------------------------
// 14. PreprocMacroCondition items inside the BLOB macro instance.
//
// BLOB is defined in mode.vh with a body containing:
//   `ifdef N
//     `ifdef M1
//     `elsif M2
//     `else
//     `endif
//   `endif
//
// When BLOB is expanded in top.v (line 22) three PreprocMacroCondition
// objects are created: one for `ifdef N, one for `ifdef M1, one for
// `elsif M2. The `else and `endif directives produce no condition objects
// because they carry no macro name.
// ---------------------------------------------------------------------------

TEST_F(DiffSimpleIncludeAndMacrosTest, BlobInstHasItems) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "BLOB");
  ASSERT_NE(mi, nullptr) << "BLOB macro instance not found in top.v";
  EXPECT_NE(mi->getItems(), nullptr) << "BLOB PreprocMacroInstance has no items collection";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, BlobInstHasThreeConditions) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "BLOB");
  ASSERT_NE(mi, nullptr) << "BLOB macro instance not found in top.v";
  EXPECT_EQ(countConditions(mi), 6u)
      << "BLOB body has `ifdef N, `ifdef M1, `elsif M2, `else, `endif, `endif -- expect 6 PreprocMacroCondition items";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, BlobInstHasConditionN) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "BLOB");
  ASSERT_NE(mi, nullptr);
  EXPECT_NE(findCondition(mi, "N"), nullptr) << "PreprocMacroCondition for `ifdef N not found in BLOB items";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, BlobInstHasConditionM1) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "BLOB");
  ASSERT_NE(mi, nullptr);
  EXPECT_NE(findCondition(mi, "M1"), nullptr) << "PreprocMacroCondition for `ifdef M1 not found in BLOB items";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, BlobInstHasConditionM2) {
  // M2 comes from `elsif M2, which also generates a PreprocMacroCondition.
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "BLOB");
  ASSERT_NE(mi, nullptr);
  EXPECT_NE(findCondition(mi, "M2"), nullptr) << "PreprocMacroCondition for `elsif M2 not found in BLOB items";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, BlobConditionNHasNameStartColumn) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "BLOB");
  ASSERT_NE(mi, nullptr);
  const hldb::PreprocMacroCondition *const cond = findCondition(mi, "N");
  ASSERT_NE(cond, nullptr) << "PreprocMacroCondition for N not found in BLOB items";
  ASSERT_NE(cond->getNameObj(), nullptr) << "PreprocMacroCondition for N must have a name Identifier";
  EXPECT_GT(cond->getNameObj()->getStartColumn(), 0u)
      << "PreprocMacroCondition for N must have a non-zero name start column";
}

// ---------------------------------------------------------------------------
// 15. PreprocMacroDefinition formal arguments (getArguments).
//
// Macros with no parameter list have a null or empty arguments collection.
// Parameterized macros carry one Identifier per formal argument.  Default
// values are exposed via Identifier::getBuddy().
//
// top.v / fake.sv macros with args:
//   xyz(I, R = DEFAULT)        -- 2 args; R has a default, I does not
//   MACRO1(a=5, b="B", c)      -- 3 args; a and b have defaults, c does not
//   msg(x, y)                  -- 2 args; no defaults
//   WB_DUT_U_ASSIGN(phy_i,idx) -- 2 args; no defaults
//
// mode.vh macros with args:
//   macro_with_args(A, B)      -- 2 args; no defaults
//   D(x, y)                    -- 2 args; no defaults
//   MACRO1(a=5, b="B", c)      -- 3 args; a and b have defaults
//   MACRO2(a=5, b, c="C")      -- 3 args; a and c have defaults
//   MACRO3(a=5, b=0, c="C")    -- 3 args; all have defaults
// ---------------------------------------------------------------------------

TEST_F(DiffSimpleIncludeAndMacrosTest, NoArgMacro_N_HasNullOrEmptyArgs) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(sf, "N");
  ASSERT_NE(md, nullptr);
  EXPECT_EQ(argCount(md->getArguments()), 0u) << "N is an object-like macro; getArguments() must be null or empty";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, NoArgMacro_BLOB_HasNullOrEmptyArgs) {
  const hldb::SourceFile *const modeVh = getModeVh(m_design);
  ASSERT_NE(modeVh, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(modeVh, "BLOB");
  ASSERT_NE(md, nullptr);
  EXPECT_EQ(argCount(md->getArguments()), 0u) << "BLOB is an object-like macro; getArguments() must be null or empty";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, XyzHasTwoFormalArgs) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "xyz");
  ASSERT_NE(md, nullptr);
  EXPECT_EQ(argCount(md->getArguments()), 2u) << "xyz(I, R = DEFAULT) must have 2 formal arguments";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, XyzFormalArgIPresent) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "xyz");
  ASSERT_NE(md, nullptr);
  EXPECT_NE(findArg(md->getArguments(), "I"), nullptr) << "formal argument 'I' not found in xyz";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, XyzFormalArgRPresent) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "xyz");
  ASSERT_NE(md, nullptr);
  EXPECT_NE(findArg(md->getArguments(), "R"), nullptr) << "formal argument 'R' not found in xyz";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, XyzArgRHasDefault) {
  // xyz(I, R = DEFAULT): R carries a default value.
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "xyz");
  ASSERT_NE(md, nullptr);
  const hldb::Identifier *const argR = findArg(md->getArguments(), "R");
  ASSERT_NE(argR, nullptr) << "formal argument 'R' not found in xyz";
  EXPECT_NE(argR->getBuddy(), nullptr) << "xyz argument R = DEFAULT must have a non-null default";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, XyzArgIHasNoDefault) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "xyz");
  ASSERT_NE(md, nullptr);
  const hldb::Identifier *const argI = findArg(md->getArguments(), "I");
  ASSERT_NE(argI, nullptr) << "formal argument 'I' not found in xyz";
  EXPECT_EQ(argI->getBuddy(), nullptr) << "xyz argument I has no default value";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, FakeSvMACRO1HasThreeFormalArgs) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "MACRO1");
  ASSERT_NE(md, nullptr);
  EXPECT_EQ(argCount(md->getArguments()), 3u) << "MACRO1(a=5, b=\"B\", c) must have 3 formal arguments";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, FakeSvMACRO1ArgCHasNoDefault) {
  // c has no default; MACRO1(a=5,b="B",c) -- c is required.
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "MACRO1");
  ASSERT_NE(md, nullptr);
  const hldb::Identifier *const argC = findArg(md->getArguments(), "c");
  ASSERT_NE(argC, nullptr) << "formal argument 'c' not found in MACRO1";
  EXPECT_EQ(argC->getBuddy(), nullptr) << "MACRO1 argument 'c' has no default";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, FakeSvMACRO1ArgAHasDefault) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "MACRO1");
  ASSERT_NE(md, nullptr);
  const hldb::Identifier *const argA = findArg(md->getArguments(), "a");
  ASSERT_NE(argA, nullptr) << "formal argument 'a' not found in MACRO1";
  EXPECT_NE(argA->getBuddy(), nullptr) << "MACRO1 argument 'a' defaults to 5";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, MsgHasTwoFormalArgs) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "msg");
  ASSERT_NE(md, nullptr);
  EXPECT_EQ(argCount(md->getArguments()), 2u) << "msg(x, y) must have 2 formal arguments";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, MsgFormalArgNames) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "msg");
  ASSERT_NE(md, nullptr);
  EXPECT_NE(findArg(md->getArguments(), "x"), nullptr) << "formal argument 'x' not found in msg";
  EXPECT_NE(findArg(md->getArguments(), "y"), nullptr) << "formal argument 'y' not found in msg";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, WB_DUT_U_ASSIGNHasTwoFormalArgs) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "WB_DUT_U_ASSIGN");
  ASSERT_NE(md, nullptr);
  EXPECT_EQ(argCount(md->getArguments()), 2u) << "WB_DUT_U_ASSIGN(phy_i, idx) must have 2 formal arguments";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, WB_DUT_U_ASSIGNFormalArgNames) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "WB_DUT_U_ASSIGN");
  ASSERT_NE(md, nullptr);
  EXPECT_NE(findArg(md->getArguments(), "phy_i"), nullptr) << "formal argument 'phy_i' not found in WB_DUT_U_ASSIGN";
  EXPECT_NE(findArg(md->getArguments(), "idx"), nullptr) << "formal argument 'idx' not found in WB_DUT_U_ASSIGN";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, ModeVhMacroWithArgsHasTwoFormalArgs) {
  const hldb::SourceFile *const modeVh = getModeVh(m_design);
  ASSERT_NE(modeVh, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(modeVh, "macro_with_args");
  ASSERT_NE(md, nullptr);
  EXPECT_EQ(argCount(md->getArguments()), 2u) << "macro_with_args(A, B) must have 2 formal arguments";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, ModeVhDHasTwoFormalArgs) {
  const hldb::SourceFile *const modeVh = getModeVh(m_design);
  ASSERT_NE(modeVh, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(modeVh, "D");
  ASSERT_NE(md, nullptr);
  EXPECT_EQ(argCount(md->getArguments()), 2u) << "D(x, y) must have 2 formal arguments";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, ModeVhMACRO1HasThreeFormalArgs) {
  const hldb::SourceFile *const modeVh = getModeVh(m_design);
  ASSERT_NE(modeVh, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(modeVh, "MACRO1");
  ASSERT_NE(md, nullptr);
  EXPECT_EQ(argCount(md->getArguments()), 3u) << "mode.vh MACRO1(a=5, b=\"B\", c) must have 3 formal arguments";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, ModeVhMACRO2HasThreeFormalArgs) {
  const hldb::SourceFile *const modeVh = getModeVh(m_design);
  ASSERT_NE(modeVh, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(modeVh, "MACRO2");
  ASSERT_NE(md, nullptr);
  EXPECT_EQ(argCount(md->getArguments()), 3u) << "MACRO2(a=5, b, c=\"C\") must have 3 formal arguments";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, ModeVhMACRO2ArgBHasNoDefault) {
  const hldb::SourceFile *const modeVh = getModeVh(m_design);
  ASSERT_NE(modeVh, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(modeVh, "MACRO2");
  ASSERT_NE(md, nullptr);
  const hldb::Identifier *const argB = findArg(md->getArguments(), "b");
  ASSERT_NE(argB, nullptr) << "formal argument 'b' not found in MACRO2";
  EXPECT_EQ(argB->getBuddy(), nullptr) << "MACRO2 argument 'b' has no default value";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, ModeVhMACRO3HasThreeFormalArgs) {
  const hldb::SourceFile *const modeVh = getModeVh(m_design);
  ASSERT_NE(modeVh, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(modeVh, "MACRO3");
  ASSERT_NE(md, nullptr);
  EXPECT_EQ(argCount(md->getArguments()), 3u) << "MACRO3(a=5, b=0, c=\"C\") must have 3 formal arguments";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, ModeVhMACRO3AllArgsHaveDefaults) {
  // MACRO3(a=5, b=0, c="C"): all three arguments carry default values.
  const hldb::SourceFile *const modeVh = getModeVh(m_design);
  ASSERT_NE(modeVh, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(modeVh, "MACRO3");
  ASSERT_NE(md, nullptr);
  const hldb::IdentifierCollection *const args = md->getArguments();
  ASSERT_NE(args, nullptr);
  for (const hldb::Identifier *const id : *args) {
    ASSERT_NE(id, nullptr);
    EXPECT_NE(id->getBuddy(), nullptr) << "MACRO3 argument '" << id->getName() << "' should have a default value";
  }
}

// ---------------------------------------------------------------------------
// 16. PreprocMacroDefinition body tokens (getTokens).
//
// Macros with non-empty bodies carry body tokens; the token list is non-null
// and non-empty.  An `undef entry has no body and must have a null or empty
// token list.
// ---------------------------------------------------------------------------

TEST_F(DiffSimpleIncludeAndMacrosTest, NMacroHasTokens) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(sf, "N");
  ASSERT_NE(md, nullptr);
  ASSERT_NE(md->getTokens(), nullptr) << "`define N 4 must have body tokens";
  EXPECT_GT(md->getTokens()->size(), 0u) << "`define N 4 must have at least one body token";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, UndefMultiplHasNoTokens) {
  // `undef multipl has no body; token collection must be null or empty.
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(sf, "multipl");
  ASSERT_NE(md, nullptr);
  const hldb::IdentifierCollection *const toks = md->getTokens();
  const size_t count = (toks == nullptr) ? 0u : toks->size();
  EXPECT_EQ(count, 0u) << "`undef multipl has no body; token count must be 0";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, XyzMacroHasTokens) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "xyz");
  ASSERT_NE(md, nullptr);
  ASSERT_NE(md->getTokens(), nullptr) << "xyz must have body tokens";
  EXPECT_GT(md->getTokens()->size(), 0u) << "xyz body token list must be non-empty";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, BLOBMacroHasTokens) {
  const hldb::SourceFile *const modeVh = getModeVh(m_design);
  ASSERT_NE(modeVh, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(modeVh, "BLOB");
  ASSERT_NE(md, nullptr);
  ASSERT_NE(md->getTokens(), nullptr) << "BLOB must have body tokens";
  EXPECT_GT(md->getTokens()->size(), 0u) << "BLOB body token list must be non-empty";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, WB_DUT_U_ASSIGNMacroHasTokens) {
  const hldb::SourceFile *const fakeSv = getFakeSv(m_design);
  ASSERT_NE(fakeSv, nullptr);
  const hldb::PreprocMacroDefinition *const md = findMacroDef(fakeSv, "WB_DUT_U_ASSIGN");
  ASSERT_NE(md, nullptr);
  ASSERT_NE(md->getTokens(), nullptr) << "WB_DUT_U_ASSIGN must have body tokens";
  EXPECT_GT(md->getTokens()->size(), 0u) << "WB_DUT_U_ASSIGN body token list must be non-empty";
}

// ---------------------------------------------------------------------------
// 17. PreprocMacroInstance actual arguments (getArguments).
//
// The argument collection on an instance holds the literal text of each
// argument as passed at the call site.  Object-like macros (no parameter
// list) have a null or empty argument collection.
//
// top.v call sites:
//   `BLOB                       -- 0 actual args
//   `INCLUSION_FILES            -- 0 actual args
//   `TOP                        -- 0 actual args
//   `WB_DUT_U_ASSIGN(12,34)     -- 2 actual args
//   `xyz(1,a)  (first call)     -- 2 actual args
//   `xyz(1)    (second call)    -- 1 actual arg
//   `MACRO1 ( 1 , , 3 )         -- 3 actual args
//   `msg(left side,right side)  -- 2 actual args
//   `macro_with_args( out, in)  -- 2 actual args
// ---------------------------------------------------------------------------

TEST_F(DiffSimpleIncludeAndMacrosTest, BLOBInstanceHasNoActualArgs) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "BLOB");
  ASSERT_NE(mi, nullptr);
  EXPECT_EQ(argCount(mi->getArguments()), 0u) << "BLOB is object-like; instance must have no actual arguments";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, INCLUSION_FILESInstanceHasNoActualArgs) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "INCLUSION_FILES");
  ASSERT_NE(mi, nullptr);
  EXPECT_EQ(argCount(mi->getArguments()), 0u)
      << "INCLUSION_FILES is object-like; instance must have no actual arguments";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, TOPInstanceHasNoActualArgs) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "TOP");
  ASSERT_NE(mi, nullptr);
  EXPECT_EQ(argCount(mi->getArguments()), 0u) << "`TOP is object-like; instance must have no actual arguments";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, WB_DUT_U_ASSIGNInstanceHasTwoActualArgs) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "WB_DUT_U_ASSIGN");
  ASSERT_NE(mi, nullptr);
  EXPECT_EQ(argCount(mi->getArguments()), 2u) << "`WB_DUT_U_ASSIGN(12,34) must have 2 actual arguments";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, XyzFirstInstanceHasTwoActualArgs) {
  // First xyz call: `xyz(1,a) -- two arguments.
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = nthMacroInst(sf, "xyz", 0u);
  ASSERT_NE(mi, nullptr) << "first xyz instance not found in top.v";
  EXPECT_EQ(argCount(mi->getArguments()), 2u) << "`xyz(1,a) must have 2 actual arguments";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, XyzSecondInstanceHasOneActualArg) {
  // Second xyz call: `xyz(1) -- one argument.
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = nthMacroInst(sf, "xyz", 1u);
  ASSERT_NE(mi, nullptr) << "second xyz instance not found in top.v";
  EXPECT_EQ(argCount(mi->getArguments()), 1u) << "`xyz(1) must have 1 actual argument";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, MACRO1InstanceHasThreeActualArgs) {
  // `MACRO1 ( 1 , , 3 ) -- three arguments (middle one is empty/default).
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "MACRO1");
  ASSERT_NE(mi, nullptr);
  EXPECT_EQ(argCount(mi->getArguments()), 3u) << "`MACRO1 ( 1 , , 3 ) must have 3 actual arguments";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, MsgInstanceHasTwoActualArgs) {
  // `msg(left side,right side) -- two arguments.
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "msg");
  ASSERT_NE(mi, nullptr);
  EXPECT_EQ(argCount(mi->getArguments()), 2u) << "`msg(left side,right side) must have 2 actual arguments";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, MacroWithArgsInstanceHasTwoActualArgs) {
  // `macro_with_args( out, in) -- two arguments.
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "macro_with_args");
  ASSERT_NE(mi, nullptr);
  EXPECT_EQ(argCount(mi->getArguments()), 2u) << "`macro_with_args( out, in) must have 2 actual arguments";
}

// ---------------------------------------------------------------------------
// 18. PreprocMacroInstance body text (getBody).
//
// getBody() returns the expanded body text of the macro invocation.
// Function-like macros with a non-trivial body must return a non-empty string.
// ---------------------------------------------------------------------------

TEST_F(DiffSimpleIncludeAndMacrosTest, XyzInstanceBodyIsNonEmpty) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = nthMacroInst(sf, "xyz", 0u);
  ASSERT_NE(mi, nullptr) << "first xyz instance not found in top.v";
  EXPECT_FALSE(mi->getBody().empty()) << "`xyz(1,a) expanded body must be non-empty";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, WB_DUT_U_ASSIGNInstanceBodyIsNonEmpty) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "WB_DUT_U_ASSIGN");
  ASSERT_NE(mi, nullptr);
  EXPECT_FALSE(mi->getBody().empty()) << "`WB_DUT_U_ASSIGN(12,34) expanded body must be non-empty";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, BLOBInstanceBodyIsNonEmpty) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "BLOB");
  ASSERT_NE(mi, nullptr);
  EXPECT_FALSE(mi->getBody().empty()) << "`BLOB expanded body must be non-empty (contains ifdef chain)";
}

// ---------------------------------------------------------------------------
// 19. PreprocMacroCondition type (getType).
//
// getType() returns the VPI constant that identifies the kind of conditional
// directive:
//   vpiPMCIfdef  (1) -- `ifdef
//   vpiPMCIfndef (2) -- `ifndef
//   vpiPMCElsif  (3) -- `elsif
//   vpiPMCElse   (4) -- `else
//   vpiPMCEndif  (5) -- `endif
//
// BLOB body (mode.vh lines 6-18):
//   `ifdef N    -> type = vpiPMCIfdef
//   `ifdef M1   -> type = vpiPMCIfdef
//   `elsif M2   -> type = vpiPMCElsif
// (The `else and `endif directives produce no PreprocMacroCondition object
// because they carry no macro name.)
// ---------------------------------------------------------------------------

TEST_F(DiffSimpleIncludeAndMacrosTest, BLOBCondNTypeIsIfdef) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "BLOB");
  ASSERT_NE(mi, nullptr);
  const hldb::PreprocMacroCondition *const cond = findCondition(mi, "N");
  ASSERT_NE(cond, nullptr) << "PreprocMacroCondition for N not found in BLOB items";
  EXPECT_EQ(cond->getType(), vpiPMCIfdef) << "`ifdef N must have type vpiPMCIfdef (" << vpiPMCIfdef << ")";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, BLOBCondM1TypeIsIfdef) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "BLOB");
  ASSERT_NE(mi, nullptr);
  const hldb::PreprocMacroCondition *const cond = findCondition(mi, "M1");
  ASSERT_NE(cond, nullptr) << "PreprocMacroCondition for M1 not found in BLOB items";
  EXPECT_EQ(cond->getType(), vpiPMCIfdef) << "`ifdef M1 must have type vpiPMCIfdef (" << vpiPMCIfdef << ")";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, BLOBCondM2TypeIsElsif) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "BLOB");
  ASSERT_NE(mi, nullptr);
  const hldb::PreprocMacroCondition *const cond = findCondition(mi, "M2");
  ASSERT_NE(cond, nullptr) << "PreprocMacroCondition for M2 not found in BLOB items";
  EXPECT_EQ(cond->getType(), vpiPMCElsif) << "`elsif M2 must have type vpiPMCElsif (" << vpiPMCElsif << ")";
}

// ---------------------------------------------------------------------------
// 20. PreprocMacroCondition location (getFile, getStartLine).
//
// Conditions inside BLOB belong to mode.vh.  Their source location reflects
// the line number within mode.vh where the conditional directive appears:
//
//   mode.vh line 7:  `ifdef N
//   mode.vh line 8:  `ifdef M1
//   mode.vh line 10: `elsif M2
// ---------------------------------------------------------------------------

TEST_F(DiffSimpleIncludeAndMacrosTest, BLOBCondNFileIsModeVh) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "BLOB");
  ASSERT_NE(mi, nullptr);
  const hldb::PreprocMacroCondition *const cond = findCondition(mi, "N");
  ASSERT_NE(cond, nullptr);
  EXPECT_NE(cond->getFile().find("mode.vh"), std::string_view::npos)
      << "`ifdef N is in BLOB body (mode.vh); condition file should contain 'mode.vh'; got: " << cond->getFile();
}

TEST_F(DiffSimpleIncludeAndMacrosTest, BLOBCondNStartLineIsSeven) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "BLOB");
  ASSERT_NE(mi, nullptr);
  const hldb::PreprocMacroCondition *const cond = findCondition(mi, "N");
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getStartLine(), 7u) << "`ifdef N is at mode.vh line 7";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, BLOBCondM1StartLineIsEight) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "BLOB");
  ASSERT_NE(mi, nullptr);
  const hldb::PreprocMacroCondition *const cond = findCondition(mi, "M1");
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getStartLine(), 8u) << "`ifdef M1 is at mode.vh line 8";
}

TEST_F(DiffSimpleIncludeAndMacrosTest, BLOBCondM2StartLineIsTen) {
  const hldb::SourceFile *const sf = findTopLevelSF(m_design, "top.v");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroInstance *const mi = findMacroInst(sf, "BLOB");
  ASSERT_NE(mi, nullptr);
  const hldb::PreprocMacroCondition *const cond = findCondition(mi, "M2");
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getStartLine(), 10u) << "`elsif M2 is at mode.vh line 10";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
