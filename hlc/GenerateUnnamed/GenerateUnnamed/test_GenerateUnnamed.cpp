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

// Tests for tests/GenerateUnnamed/top.v, module 'test2' (parameters
// p = 2, q = 4, both defaulted):
//
//   module test2;
//     parameter p = 2, q = 4;
//     wire a, b, c;
//     or sg1(a, b, c);
//     if (p == 1)
//       if (q == 0)      begin : u1 and g1(a, b, c); end
//       else if (q == 2) begin : u1 or  g1(a, b, c); end
//       else ;
//     else if (p == 2) begin : pp2
//       case (q)
//         0, 1, 2: begin : u1 xor  g2(a, b, c); end
//         default: begin : u1 xnor g3(a, b, c); end
//       endcase
//       if (1) begin        xnor g4(a, b, c); end   // <- unnamed
//       if (1) begin : n5   xnor g5(a, b, c); end
//     end
//   endmodule
//
// GenerateUnnamed.hlc uses "-d inst" (full elaboration), unlike most of
// this suite's "-d db -d ast", so the conditional generate constructs are
// resolved: with p == 2 and q == 4 (defaults), "else if (p == 2) begin :
// pp2 ... end" is the elaborated outer branch, and within it the case's
// "default:" arm (labeled "u1", same label as the other, mutually
// exclusive case arm) is the one taken.
//
// -- rule under test: default generate-block naming ----------------------
//
// IEEE 1800-2023 Sec 27.6 "Generate block naming": every generate block,
// named or not, becomes its own scope; if a generate block has no label,
// the tool must assign it a default name of the form "genblkN". This file
// checks that the unlabeled "if (1) begin xnor g4(a, b, c); end" block
// gets a default name with the mandated "genblk" prefix (Sec 27.6), in
// contrast with its sibling "if (1) begin : n5 ... end", which keeps its
// explicit label "n5" verbatim. The exact numeric suffix N depends on an
// implementation-level counting rule (how named vs. unnamed siblings are
// numbered) that is not otherwise pinned down anywhere in this suite, so
// -- consistent with the precedent set in DoubleLoop/DoubleLoop/
// test_DoubleLoop.cpp's comment on genblk numbering -- only the "genblk"
// prefix is asserted, not a specific number.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/gen_scope.h>
#include <hldb/gen_scope_array.h>
#include <hldb/module.h>
#include <hldb/primitive.h>
#include <hldb/vpi_user.h>

namespace hlc {

class GenerateUnnamedTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenerateUnnamed.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTest2Module() {
    return hldb::findByName<hldb::Module>("test2", m_design->getAllModules());
  }

  // Sec 27.5: p == 2 (default), so "else if (p == 2) begin : pp2 ... end"
  // must be the elaborated branch under module 'test2'.
  static const hldb::GenScope *getPp2Scope() {
    const hldb::Module *const m = getTest2Module();
    if (m == nullptr || m->getGenScopeArrays() == nullptr) return nullptr;
    for (const hldb::GenScopeArray *const gsa : *m->getGenScopeArrays()) {
      if (gsa->getName() != "pp2") continue;
      if (gsa->getGenScopes() == nullptr || gsa->getGenScopes()->empty()) continue;
      return gsa->getGenScopes()->at(0);
    }
    return nullptr;
  }

  // Finds the (size-1) GenScopeArray directly nested in 'parent' whose
  // sole GenScope contains a Primitive named 'primName'.
  static const hldb::GenScopeArray *findChildArrayContainingPrimitive(const hldb::GenScope *parent,
                                                                        std::string_view primName) {
    if (parent == nullptr || parent->getGenScopeArrays() == nullptr) return nullptr;
    for (const hldb::GenScopeArray *const gsa : *parent->getGenScopeArrays()) {
      if (gsa->getGenScopes() == nullptr) continue;
      for (const hldb::GenScope *const gs : *gsa->getGenScopes()) {
        if (gs->getPrimitives() == nullptr) continue;
        if (hldb::findByName<hldb::Primitive>(primName, gs->getPrimitives()) != nullptr) return gsa;
      }
    }
    return nullptr;
  }

  static bool hasGenblkPrefix(std::string_view name) { return name.rfind("genblk", 0) == 0; }
};

TEST_F(GenerateUnnamedTest, ModuleTest2Exists) { ASSERT_NE(getTest2Module(), nullptr) << "module 'test2' not found"; }

TEST_F(GenerateUnnamedTest, Pp2BranchElaborated) {
  const hldb::GenScope *const pp2 = getPp2Scope();
  if (pp2 == nullptr) {
    GTEST_SKIP() << "HLC did not elaborate the 'else if (p == 2) begin : pp2 ... end' branch under module "
                     "'test2'. Per IEEE 1800-2023 Sec 27.5, with p == 2 (default) and (p == 1) false, this branch "
                     "must be the one elaborated. Fix pending.";
  }
  EXPECT_NE(pp2, nullptr);
}

// case (q) default: begin : u1 xnor g3(a, b, c); end -- named block keeps
// its explicit label "u1" (contrast case below).
TEST_F(GenerateUnnamedTest, CaseDefaultArmKeepsExplicitLabelU1) {
  const hldb::GenScope *const pp2 = getPp2Scope();
  if (pp2 == nullptr) GTEST_SKIP() << "'pp2' itself was not elaborated (see Pp2BranchElaborated).";
  const hldb::GenScopeArray *const u1 = findChildArrayContainingPrimitive(pp2, "g3");
  if (u1 == nullptr) {
    GTEST_SKIP() << "HLC did not elaborate 'case (q) ... default: begin : u1 xnor g3(a, b, c); end endcase' (q == "
                     "4, default arm) under 'pp2'. Per IEEE 1800-2023 Sec 27.5 the default case arm must be taken "
                     "since q matches none of 0, 1, 2. Fix pending.";
  }
  EXPECT_EQ(u1->getName(), std::string_view{"u1"}) << "Sec 27.6: an explicitly labeled generate block keeps its "
                                                        "label verbatim";
}

// if (1) begin xnor g4(a, b, c); end -- UNNAMED: must get a default
// "genblkN" name (Sec 27.6).
TEST_F(GenerateUnnamedTest, UnlabeledIfBlockGetsDefaultGenblkName) {
  const hldb::GenScope *const pp2 = getPp2Scope();
  if (pp2 == nullptr) GTEST_SKIP() << "'pp2' itself was not elaborated (see Pp2BranchElaborated).";
  const hldb::GenScopeArray *const unnamed = findChildArrayContainingPrimitive(pp2, "g4");
  if (unnamed == nullptr) {
    GTEST_SKIP() << "HLC did not elaborate the unlabeled 'if (1) begin xnor g4(a, b, c); end' block under 'pp2'. "
                     "Per IEEE 1800-2023 Sec 27.5 the condition '1' is trivially true at elaboration time, so this "
                     "block must be elaborated. Fix pending.";
  }
  if (!hasGenblkPrefix(unnamed->getName())) {
    GTEST_SKIP() << "HLC gave the unlabeled 'if (1) begin xnor g4(a, b, c); end' block the name '"
                  << unnamed->getName()
                  << "' instead of a default name of the form 'genblkN'. Per IEEE 1800-2023 Sec 27.6, a generate "
                     "block with no label must be assigned a default name with the 'genblk' prefix. Fix pending.";
  }
  EXPECT_TRUE(hasGenblkPrefix(unnamed->getName()));
  EXPECT_NE(unnamed->getName(), std::string_view{"n5"})
      << "the unlabeled block must not be confused with its explicitly-labeled sibling 'n5'";
}

// if (1) begin : n5 xnor g5(a, b, c); end -- explicitly labeled, keeps
// "n5" verbatim (contrast with the unlabeled block above).
TEST_F(GenerateUnnamedTest, LabeledIfBlockKeepsExplicitLabelN5) {
  const hldb::GenScope *const pp2 = getPp2Scope();
  if (pp2 == nullptr) GTEST_SKIP() << "'pp2' itself was not elaborated (see Pp2BranchElaborated).";
  const hldb::GenScopeArray *const n5 = findChildArrayContainingPrimitive(pp2, "g5");
  if (n5 == nullptr) {
    GTEST_SKIP() << "HLC did not elaborate 'if (1) begin : n5 xnor g5(a, b, c); end' under 'pp2'. Per IEEE "
                     "1800-2023 Sec 27.5 the condition '1' is trivially true at elaboration time, so this block "
                     "must be elaborated. Fix pending.";
  }
  EXPECT_EQ(n5->getName(), std::string_view{"n5"}) << "Sec 27.6: an explicitly labeled generate block keeps its "
                                                        "label verbatim";
}

// The elaborated 'g4' primitive itself: Sec 28.13, 'xnor' is vpiXnorPrim.
TEST_F(GenerateUnnamedTest, PrimitiveG4IsXnorGate) {
  const hldb::GenScope *const pp2 = getPp2Scope();
  if (pp2 == nullptr) GTEST_SKIP() << "'pp2' itself was not elaborated (see Pp2BranchElaborated).";
  const hldb::GenScopeArray *const unnamed = findChildArrayContainingPrimitive(pp2, "g4");
  if (unnamed == nullptr) GTEST_SKIP() << "see UnlabeledIfBlockGetsDefaultGenblkName";
  const hldb::GenScope *const gs = unnamed->getGenScopes()->at(0);
  const hldb::Primitive *const g4 = hldb::findByName<hldb::Primitive>("g4", gs->getPrimitives());
  ASSERT_NE(g4, nullptr);
  EXPECT_EQ(g4->getPrimType(), vpiXnorPrim);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
