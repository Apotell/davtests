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

// Tests for tests/GenerateBlock2/dut.sv. Like GenerateBlock, GenerateBlock2.hlc
// compiles with "-d db -d ast" only (no "-d inst"), so this file checks the
// unelaborated parse-time shape of two independent generate constructs, per
// IEEE 1800-2023 Sec 27.3-27.5, rather than an elaborated GenScope/
// GenScopeArray tree:
//
//   module sub (output reg int_status);
//   endmodule
//
//   module dut ();
//     parameter p = 1'b1;
//     generate
//       if (p == 1'b1) begin : blk
//       end
//     endgenerate
//     generate
//       genvar   loop_int;
//       wire  [2:0]  int_status;
//       for (loop_int = 0; loop_int < 2; loop_int = loop_int + 1)
//       begin : gen_blk
//           sub sub_i (
//             .int_status(int_status[loop_int])
//           );
//       end
//     endgenerate
//   endmodule
//
// The difference from GenerateBlock (which nests plain, unconditional named
// blocks) is that GenerateBlock2 exercises the two *other* generate-construct
// kinds from Sec 27.4/27.5: a conditional generate-if (with no else, Sec
// 27.5) and a loop generate-for (Sec 27.4) that instantiates a module inside
// its body.
//
// Per Sec 27.3, a generate region with more than one item ('genvar
// loop_int;', 'wire [2:0] int_status;', and the for-loop) does not introduce
// a new scope by itself, so 'int_status' is expected directly on module
// 'dut', not nested inside a wrapper scope. As in GenerateBlock, this file
// unwraps any GenRegion it encounters before checking the underlying
// GenIf/GenFor, so it stays valid whichever way HLC represents these single-
// or multi-item generate regions.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/design.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if.h>
#include <hldb/gen_region.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class GenerateBlock2Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenerateBlock2.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  // Sec 27.3: unwrap a possible GenRegion wrapper around a single generate
  // item.
  static const hldb::Any *unwrap(const hldb::Any *item) {
    while (item != nullptr) {
      const hldb::GenRegion *const region = any_cast<hldb::GenRegion>(item);
      if (region == nullptr) break;
      item = region->getStmt();
    }
    return item;
  }

  static const hldb::GenIf *findGenIf(const hldb::AnyCollection *stmts) {
    if (stmts == nullptr) return nullptr;
    for (const hldb::Any *const item : *stmts) {
      if (const hldb::GenIf *const gi = any_cast<hldb::GenIf>(unwrap(item))) return gi;
    }
    return nullptr;
  }

  static const hldb::GenFor *findGenFor(const hldb::AnyCollection *stmts) {
    if (stmts == nullptr) return nullptr;
    for (const hldb::Any *const item : *stmts) {
      if (const hldb::GenFor *const gf = any_cast<hldb::GenFor>(unwrap(item))) return gf;
    }
    return nullptr;
  }
};

// ---------------------------------------------------------------------------
// Module existence
// ---------------------------------------------------------------------------

TEST_F(GenerateBlock2Test, ModulesExist) {
  EXPECT_NE(getModule("dut"), nullptr) << "module 'dut' not found";
  EXPECT_NE(getModule("sub"), nullptr) << "module 'sub' not found";
}

// ---------------------------------------------------------------------------
// Sec 27.3: a multi-item generate region does not create a new scope, so
// 'wire [2:0] int_status;' is a net directly on 'dut'.
// ---------------------------------------------------------------------------

TEST_F(GenerateBlock2Test, DutHasNetIntStatus) {
  const hldb::Module *const m = getModule("dut");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr) << "'wire [2:0] int_status;' -- module must have at least one net";
  EXPECT_NE(hldb::findByName<hldb::Net>("int_status", m->getNets()), nullptr)
      << "net 'int_status' not found on 'dut'";
}

// ---------------------------------------------------------------------------
// Sec 27.5: "if (p == 1'b1) begin : blk end" -- a generate-if with no else.
// ---------------------------------------------------------------------------

TEST_F(GenerateBlock2Test, GenIfExists) {
  const hldb::Module *const m = getModule("dut");
  ASSERT_NE(m, nullptr);
  const hldb::GenIf *const gi = findGenIf(m->getGenStmts());
  ASSERT_NE(gi, nullptr) << "'if (p == 1'b1) begin : blk end' -- GenIf not found on 'dut'";

  ASSERT_NE(gi->getCondition(), nullptr) << "GenIf has no condition";
  const hldb::Operation *const cond = gi->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "'p == 1'b1': condition must be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiEqOp) << "Sec 11.4.5: '==' is vpiEqOp";

  ASSERT_NE(gi->getStmt(), nullptr) << "GenIf has no body";
  const hldb::Begin *const blk = gi->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr) << "'begin : blk end' body must be a Begin";
  EXPECT_EQ(blk->getEndLabel(), std::string_view{"blk"});
}

// ---------------------------------------------------------------------------
// Sec 27.4: "for (loop_int = 0; loop_int < 2; loop_int = loop_int + 1)
// begin : gen_blk sub sub_i (...); end" -- a loop generate construct whose
// body instantiates module 'sub'.
// ---------------------------------------------------------------------------

TEST_F(GenerateBlock2Test, GenForExists) {
  const hldb::Module *const m = getModule("dut");
  ASSERT_NE(m, nullptr);
  const hldb::GenFor *const gf = findGenFor(m->getGenStmts());
  ASSERT_NE(gf, nullptr) << "loop generate construct not found on 'dut'";

  EXPECT_NE(gf->getForInitStmts(), nullptr) << "'loop_int = 0' init statement missing";
  EXPECT_NE(gf->getForIncStmts(), nullptr) << "'loop_int = loop_int + 1' increment statement missing";

  ASSERT_NE(gf->getCondition(), nullptr) << "GenFor has no condition";
  const hldb::Operation *const cond = gf->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "'loop_int < 2': condition must be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiLtOp) << "Sec 11.4.4: '<' is vpiLtOp";
}

TEST_F(GenerateBlock2Test, GenForBodyIsGenBlkInstantiatingSub) {
  const hldb::Module *const m = getModule("dut");
  ASSERT_NE(m, nullptr);
  const hldb::GenFor *const gf = findGenFor(m->getGenStmts());
  ASSERT_NE(gf, nullptr);

  ASSERT_NE(gf->getStmt(), nullptr) << "GenFor has no body";
  const hldb::Begin *const genBlk = gf->getStmt<hldb::Begin>();
  ASSERT_NE(genBlk, nullptr) << "'begin : gen_blk ... end' body must be a Begin";
  EXPECT_EQ(genBlk->getEndLabel(), std::string_view{"gen_blk"});

  ASSERT_NE(genBlk->getStmts(), nullptr) << "'sub sub_i (...);' -- gen_blk must have at least one statement";
  const hldb::RefInstance *subI = nullptr;
  for (const hldb::Any *const item : *genBlk->getStmts()) {
    if (const hldb::RefInstance *const ri = any_cast<hldb::RefInstance>(item)) {
      if (ri->getName() == "sub_i") {
        subI = ri;
        break;
      }
    }
  }
  ASSERT_NE(subI, nullptr) << "'sub sub_i (...)' instance not found inside gen_blk";

  ASSERT_NE(subI->getTypespec(), nullptr) << "'sub_i' has no typespec";
  const hldb::ModuleTypespec *const mt = subI->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr) << "'sub_i's typespec is not a ModuleTypespec";
  EXPECT_EQ(mt->getName(), std::string_view{"sub"});
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
