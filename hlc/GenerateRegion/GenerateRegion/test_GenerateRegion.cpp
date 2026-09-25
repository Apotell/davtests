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

// Tests for tests/GenerateRegion/dut.sv, module 'gen_test9' -- a bare
// generate/endgenerate region (IEEE 1800-2023 Sec 27.3 "generate_region ::=
// generate { generate_item } endgenerate") whose single generate_item is a
// named generate_block, itself containing two more nested bare generate
// regions:
//
//   module gen_test9;
//     wire [1:0] w = 2'b11;
//     generate
//       begin : A
//         wire [1:0] x;
//         generate
//           begin : B
//             wire [1:0] y = 2'b00;
//           end
//         endgenerate
//         generate
//           begin : C
//             wire [1:0] z = 2'b01;
//           end
//         endgenerate
//         assign x = B.y ^ 2'b11 ^ C.z;
//       end
//     endgenerate
//   endmodule
//
// -- rules under test ---------------------------------------------------
//
// Sec 27.3: a generate_region introduces no additional hierarchy by
// itself -- it is purely a syntactic wrapper around one or more
// generate_items. Its single item here is a labeled generate_block
// ("begin : A ... end"), so HLC's parse/db-time model of the region is a
// GenRegion whose getStmt() is that Begin, named "A" (Sec 27.3: "each
// generate block ... shall be treated as if it were the only item ...").
// The two further "generate begin : B/C ... end endgenerate" pairs nested
// inside A are themselves independent bare generate regions -- each is
// modeled the same way, one level deeper, as items of A's own statement
// list.
//
// This file only compiles at parse/db time (GenerateRegion.hlc uses
// "-d db -d ast", no "-d inst"), so no elaboration/unrolling is checked --
// this is the single generate_region template as built by the parser.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/cont_assign.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_region.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class GenerateRegionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenerateRegion.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  // module gen_test9's single top-level generate region: getGenStmts() ->
  // GenRegion -> getStmt<Begin>() named "A".
  static const hldb::Begin *getBlockA() {
    const hldb::Module *const m = getModule("gen_test9");
    if (m == nullptr || m->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      const hldb::GenRegion *const region = any_cast<hldb::GenRegion>(stmt);
      if (region == nullptr) continue;
      const hldb::Begin *const a = region->getStmt<hldb::Begin>();
      if (a != nullptr) return a;
    }
    return nullptr;
  }

  // Finds a nested "generate begin : <label> ... end endgenerate" region
  // by label among 'parent's own statement list.
  static const hldb::Begin *findNestedRegion(const hldb::Begin *parent, std::string_view label) {
    if (parent == nullptr || parent->getStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *parent->getStmts()) {
      const hldb::GenRegion *const region = any_cast<hldb::GenRegion>(stmt);
      if (region == nullptr) continue;
      const hldb::Begin *const blk = region->getStmt<hldb::Begin>();
      if (blk != nullptr && blk->getName() == label) return blk;
    }
    return nullptr;
  }
};

// ---------------------------------------------------------------------------
// Module and top-level bare generate region
// ---------------------------------------------------------------------------

TEST_F(GenerateRegionTest, ModuleExists) { ASSERT_NE(getModule("gen_test9"), nullptr) << "module 'gen_test9' not found"; }

TEST_F(GenerateRegionTest, TopLevelGenerateRegionWrapsNamedBlockA) {
  const hldb::Begin *const a = getBlockA();
  ASSERT_NE(a, nullptr) << "'generate begin : A ... end endgenerate' not found as a bare GenRegion";
  EXPECT_EQ(a->getName(), std::string_view{"A"});
}

TEST_F(GenerateRegionTest, ModuleTopWireExists) {
  const hldb::Module *const m = getModule("gen_test9");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr) << "'wire [1:0] w = 2'b11;' should produce a Net at module scope";
  const hldb::Net *const w = hldb::findByName<hldb::Net>("w", m->getNets());
  ASSERT_NE(w, nullptr) << "'w' not found at module scope";
  EXPECT_EQ(w->getNetType(), vpiWire);
}

// ---------------------------------------------------------------------------
// Block A: 'wire [1:0] x;' plus two nested bare generate regions (B, C)
// plus the continuous assignment using their hierarchical members.
// ---------------------------------------------------------------------------

TEST_F(GenerateRegionTest, BlockA_HasNetX) {
  const hldb::Begin *const a = getBlockA();
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getNets(), nullptr) << "'wire [1:0] x;' inside 'begin : A' should produce a Net";
  const hldb::Net *const x = hldb::findByName<hldb::Net>("x", a->getNets());
  ASSERT_NE(x, nullptr) << "'x' not found inside block A";
  EXPECT_EQ(x->getNetType(), vpiWire);
}

TEST_F(GenerateRegionTest, BlockA_ContainsNestedRegionB) {
  const hldb::Begin *const a = getBlockA();
  ASSERT_NE(a, nullptr);
  const hldb::Begin *const b = findNestedRegion(a, "B");
  ASSERT_NE(b, nullptr) << "nested 'generate begin : B ... end endgenerate' not found inside block A";
}

TEST_F(GenerateRegionTest, BlockA_ContainsNestedRegionC) {
  const hldb::Begin *const a = getBlockA();
  ASSERT_NE(a, nullptr);
  const hldb::Begin *const c = findNestedRegion(a, "C");
  ASSERT_NE(c, nullptr) << "nested 'generate begin : C ... end endgenerate' not found inside block A";
}

TEST_F(GenerateRegionTest, BlockB_HasWireYWithInitialValueZero) {
  const hldb::Begin *const a = getBlockA();
  ASSERT_NE(a, nullptr);
  const hldb::Begin *const b = findNestedRegion(a, "B");
  ASSERT_NE(b, nullptr);
  ASSERT_NE(b->getNets(), nullptr) << "'wire [1:0] y = 2'b00;' inside block B should produce a Net";
  const hldb::Net *const y = hldb::findByName<hldb::Net>("y", b->getNets());
  ASSERT_NE(y, nullptr) << "'y' not found inside block B";
  EXPECT_EQ(y->getNetType(), vpiWire);
  const hldb::Constant *const val = y->getValue<hldb::Constant>();
  ASSERT_NE(y->getValue(), nullptr) << "'wire y = 2'b00;': net-decl-assign initial value must be present";
  ASSERT_NE(val, nullptr) << "'y's initial value must be a Constant";
  EXPECT_EQ(val->getDecompile(), "0");
}

TEST_F(GenerateRegionTest, BlockC_HasWireZWithInitialValueOne) {
  const hldb::Begin *const a = getBlockA();
  ASSERT_NE(a, nullptr);
  const hldb::Begin *const c = findNestedRegion(a, "C");
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getNets(), nullptr) << "'wire [1:0] z = 2'b01;' inside block C should produce a Net";
  const hldb::Net *const z = hldb::findByName<hldb::Net>("z", c->getNets());
  ASSERT_NE(z, nullptr) << "'z' not found inside block C";
  EXPECT_EQ(z->getNetType(), vpiWire);
  const hldb::Constant *const val = z->getValue<hldb::Constant>();
  ASSERT_NE(z->getValue(), nullptr) << "'wire z = 2'b01;': net-decl-assign initial value must be present";
  ASSERT_NE(val, nullptr) << "'z's initial value must be a Constant";
  EXPECT_EQ(val->getDecompile(), "1");
}

// 'assign x = B.y ^ 2'b11 ^ C.z;' -- only the LHS shape is asserted here;
// the RHS's hierarchical references (B.y, C.z) into nested generate-block
// scopes are not pinned down by any other test in this suite, so their
// exact representation is left unchecked to avoid guessing.
TEST_F(GenerateRegionTest, BlockA_HasContAssignToX) {
  const hldb::Begin *const a = getBlockA();
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getStmts(), nullptr);
  const hldb::ContAssign *found = nullptr;
  for (const hldb::Any *const stmt : *a->getStmts()) {
    const hldb::ContAssign *const ca = any_cast<hldb::ContAssign>(stmt);
    if (ca != nullptr && ca->getLhs() != nullptr && ca->getLhs()->getName() == "x") {
      found = ca;
      break;
    }
  }
  ASSERT_NE(found, nullptr) << "'assign x = B.y ^ 2'b11 ^ C.z;' not found inside block A";
  const hldb::RefObj *const lhs = found->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "'assign x = ...': LHS must be a RefObj";
  EXPECT_EQ(lhs->getName(), std::string_view{"x"});
  EXPECT_NE(found->getRhs(), nullptr) << "'assign x = B.y ^ 2'b11 ^ C.z;' must have a non-null RHS";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
