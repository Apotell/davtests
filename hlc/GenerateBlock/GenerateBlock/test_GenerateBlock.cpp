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

// Tests for tests/GenerateBlock/dut.sv (module 'gen_test9'). GenerateBlock.hlc
// compiles with "-d db -d ast" only (no "-d inst"), so this file exercises the
// *unelaborated* parse-time representation of generate constructs, per IEEE
// 1800-2023 Sec 27.3 "Generate block", rather than the elaborated GenScope/
// GenScopeArray shapes that a "-d inst" run would produce (see e.g.
// hlc/ForElab/ForElab/test_ForElab.cpp for that elaborated style).
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
// Sec 27.3: a "generate ... endgenerate" region whose body is a single named
// block ("begin : A ... end") is a generate block; nested "generate ...
// endgenerate" regions (B, C) inside it are themselves nested generate
// blocks. This file does not assume a specific wrapper node (e.g. a
// standalone "GenRegion" around each block) -- it unwraps any GenRegion it
// encounters before checking the underlying Begin/GenIf/GenFor, so the test
// stays valid whichever way HLC represents an unconditional single-item
// generate region.

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
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class GenerateBlockTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenerateBlock.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() {
    return hldb::findByName<hldb::Module>("gen_test9", m_design->getAllModules());
  }

  // Sec 27.3: unwrap a possible GenRegion wrapper around a single generate
  // item, so callers can check the underlying node regardless of whether
  // HLC materializes a distinct GenRegion for an unconditional single-item
  // region.
  static const hldb::Any *unwrap(const hldb::Any *item) {
    while (item != nullptr) {
      const hldb::GenRegion *const region = any_cast<hldb::GenRegion>(item);
      if (region == nullptr) break;
      item = region->getStmt();
    }
    return item;
  }

  // Finds a Begin (possibly wrapped in a GenRegion) with the given label
  // among the elements of an AnyCollection (a module's getGenStmts(), or a
  // Begin's own getStmts()).
  static const hldb::Begin *findBegin(const hldb::AnyCollection *stmts, std::string_view label) {
    if (stmts == nullptr) return nullptr;
    for (const hldb::Any *const item : *stmts) {
      const hldb::Begin *const begin = any_cast<hldb::Begin>(unwrap(item));
      if (begin != nullptr && begin->getEndLabel() == label) return begin;
    }
    return nullptr;
  }

  static const hldb::ContAssign *findContAssign(const hldb::AnyCollection *stmts) {
    if (stmts == nullptr) return nullptr;
    for (const hldb::Any *const item : *stmts) {
      if (const hldb::ContAssign *const ca = any_cast<hldb::ContAssign>(unwrap(item))) return ca;
    }
    return nullptr;
  }
};

// ---------------------------------------------------------------------------
// Module existence
// ---------------------------------------------------------------------------

TEST_F(GenerateBlockTest, ModuleExists) { ASSERT_NE(getModule(), nullptr) << "module 'gen_test9' not found"; }

TEST_F(GenerateBlockTest, ModuleHasNetW) {
  const hldb::Module *const m = getModule();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr) << "'wire [1:0] w = 2'b11;' -- module must have at least one net";
  EXPECT_NE(hldb::findByName<hldb::Net>("w", m->getNets()), nullptr) << "net 'w' not found on 'gen_test9'";
}

// ---------------------------------------------------------------------------
// Sec 27.3: "generate begin : A ... end endgenerate" -- a single named
// generate block directly in the module.
// ---------------------------------------------------------------------------

TEST_F(GenerateBlockTest, GenBlockAExists) {
  const hldb::Module *const m = getModule();
  ASSERT_NE(m, nullptr);
  const hldb::Begin *const a = findBegin(m->getGenStmts(), "A");
  ASSERT_NE(a, nullptr) << "generate block 'A' not found among the module's generate statements";
  EXPECT_EQ(a->getEndLabel(), std::string_view{"A"});
}

TEST_F(GenerateBlockTest, GenBlockAHasNetX) {
  const hldb::Module *const m = getModule();
  ASSERT_NE(m, nullptr);
  const hldb::Begin *const a = findBegin(m->getGenStmts(), "A");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getNets(), nullptr) << "'wire [1:0] x;' -- block A must have at least one net";
  EXPECT_NE(hldb::findByName<hldb::Net>("x", a->getNets()), nullptr) << "net 'x' not found in block A";
}

// ---------------------------------------------------------------------------
// Sec 27.3: block A nests two further "generate begin : <label> ... end
// endgenerate" regions, B and C, each declaring its own net.
// ---------------------------------------------------------------------------

TEST_F(GenerateBlockTest, NestedGenBlockBExistsWithNetY) {
  const hldb::Module *const m = getModule();
  ASSERT_NE(m, nullptr);
  const hldb::Begin *const a = findBegin(m->getGenStmts(), "A");
  ASSERT_NE(a, nullptr);
  const hldb::Begin *const b = findBegin(a->getStmts(), "B");
  ASSERT_NE(b, nullptr) << "nested generate block 'B' not found inside block A";
  ASSERT_NE(b->getNets(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("y", b->getNets()), nullptr) << "net 'y' not found in block B";
}

TEST_F(GenerateBlockTest, NestedGenBlockCExistsWithNetZ) {
  const hldb::Module *const m = getModule();
  ASSERT_NE(m, nullptr);
  const hldb::Begin *const a = findBegin(m->getGenStmts(), "A");
  ASSERT_NE(a, nullptr);
  const hldb::Begin *const c = findBegin(a->getStmts(), "C");
  ASSERT_NE(c, nullptr) << "nested generate block 'C' not found inside block A";
  ASSERT_NE(c->getNets(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("z", c->getNets()), nullptr) << "net 'z' not found in block C";
}

// ---------------------------------------------------------------------------
// Sec 11.3.2: "assign x = B.y ^ 2'b11 ^ C.z;" -- bitwise XOR is associative,
// so this file only pins down the LHS and the top-level operator, not
// whether the three XOR operands are folded into one flattened Operation or
// kept as nested binary Operations (Operation exposes a 'Flattened' flag,
// so either representation is plausible without being a standard
// violation).
// ---------------------------------------------------------------------------

TEST_F(GenerateBlockTest, GenBlockAHasContAssignToX) {
  const hldb::Module *const m = getModule();
  ASSERT_NE(m, nullptr);
  const hldb::Begin *const a = findBegin(m->getGenStmts(), "A");
  ASSERT_NE(a, nullptr);
  const hldb::ContAssign *const ca = findContAssign(a->getStmts());
  ASSERT_NE(ca, nullptr) << "'assign x = B.y ^ 2'b11 ^ C.z;' -- ContAssign not found in block A";

  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(ca->getLhs(), nullptr) << "ContAssign has no LHS";
  ASSERT_NE(lhs, nullptr) << "'assign x = ...': LHS must be a RefObj (reference to x)";
  EXPECT_EQ(lhs->getName(), std::string_view{"x"});

  ASSERT_NE(ca->getRhs(), nullptr) << "ContAssign has no RHS";
  const hldb::Operation *const rhs = ca->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'B.y ^ 2'b11 ^ C.z': RHS must be an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiBitXorOp) << "Sec 11.3.2: '^' is vpiBitXorOp";
  ASSERT_NE(rhs->getOperands(), nullptr);
  EXPECT_GE(rhs->getOperands()->size(), 2u) << "'^' must have at least two operands";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
