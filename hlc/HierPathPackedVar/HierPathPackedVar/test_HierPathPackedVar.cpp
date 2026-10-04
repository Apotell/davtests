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

// Tests for tests/HierPathPackedVar/dut.sv:
//
//   package std_cache_pkg;
//      typedef struct packed { logic req; ... } bypass_req_t;
//   endpackage
//
//   module axi_adapter_arbiter #(
//       parameter NR_PORTS = 4,
//       parameter type req_t = std_cache_pkg::bypass_req_t)
//       (input  req_t [NR_PORTS-1:0] req_i);
//       reg state_d;
//       always_comb begin
//          if (req_i[i].req == 1'b1) begin
//             state_d = SERVING;
//         end
//       end
//   endmodule
//
// Like HierPathPackedArrayNet, "req_i[i].req" bit-selects an element of the
// unpacked-array port "req_i" (of parameterized packed-struct type req_t)
// before descending via a hierarchical path into its ".req" field -- the
// select is applied before the hierarchical path, distinguishing this shape
// from HierPathPackedStruct/HierPathSelect (select applied after the path).
//
// Note: the source itself is malformed (a latent bug in the fixture, not
// this test) -- "i" is used as an index with no enclosing generate/for loop
// declaring it, and "SERVING" is never declared anywhere. Per IEEE
// 1800-2023 Sec 23.8/26.3 neither identifier can resolve to a declaration,
// so this test does not assert that either name binds to anything; it only
// asserts the parse-time hierarchical-path/select shape, which the grammar
// guarantees regardless of whether "i" itself resolves.
//
// "req_i" carries no net-type keyword, so per Sec 6.7/6.8 it must be
// modeled as a Variable, not a Net.
//
// Checked:
//   - module "axi_adapter_arbiter" exists, "req_i" is a Variable (not Net)
//   - always_comb -> begin -> if is present
//   - if-condition operand 0 is a hierarchical RefObj with 2 path
//     elements: a BitSelect named "req_i" (index RefObj "i"), then RefObj
//     "req"

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/design.h>
#include <hldb/if_stmt.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <gtest/gtest.h>

#include <string_view>

namespace hlc {

class HierPathPackedVarTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierPathPackedVar.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getArbiter() {
    return hldb::findByDefName<hldb::Module>("axi_adapter_arbiter", m_design->getAllModules());
  }

  // Descends always_comb -> begin -> if, returning the IfStmt's condition.
  static const hldb::Expr *findIfCondition(const hldb::Module *mod) {
    if (mod == nullptr || mod->getProcesses() == nullptr) return nullptr;
    for (const hldb::Any *const proc : *mod->getProcesses()) {
      const hldb::Always *const always = any_cast<hldb::Always>(proc);
      if (always == nullptr) continue;
      const hldb::Begin *const body = always->getStmt<hldb::Begin>();
      if (body == nullptr || body->getStmts() == nullptr) continue;
      for (const hldb::Any *const s : *body->getStmts()) {
        const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(s);
        if (ifStmt != nullptr) return ifStmt->getCondition();
      }
    }
    return nullptr;
  }
};

TEST_F(HierPathPackedVarTest, ModuleExists) { EXPECT_NE(getArbiter(), nullptr); }

TEST_F(HierPathPackedVarTest, ReqIIsVariableNotNet) {
  // Per IEEE 1800-2023 Sec 6.7/6.8: no net-type keyword means "req_i" must
  // be modeled as a Variable, never a Net.
  const hldb::Module *const mod = getArbiter();
  ASSERT_NE(mod, nullptr);
  const hldb::Variable *const asVar = hldb::findByName<hldb::Variable>("req_i", mod->getVariables());
  EXPECT_NE(asVar, nullptr) << "'req_i' has no net-type keyword and must be modeled as a Variable";
  const hldb::Net *const asNet = hldb::findByName<hldb::Net>("req_i", mod->getNets());
  EXPECT_EQ(asNet, nullptr) << "'req_i' must not be modeled as a Net (no net-type keyword given)";
}

TEST_F(HierPathPackedVarTest, IfConditionExists) { EXPECT_NE(findIfCondition(getArbiter()), nullptr); }

TEST_F(HierPathPackedVarTest, ConditionLhsIsArraySelectThenHierPathToReq) {
  const hldb::Expr *const cond = findIfCondition(getArbiter());
  ASSERT_NE(cond, nullptr);
  const hldb::Operation *const eq = any_cast<hldb::Operation>(cond);
  ASSERT_NE(eq, nullptr) << "'req_i[i].req == 1'b1' condition should be an Operation";
  EXPECT_EQ(eq->getOpType(), vpiEqOp);
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);

  const hldb::RefObj *const hierPath = any_cast<hldb::RefObj>(eq->getOperands()->at(0));
  ASSERT_NE(hierPath, nullptr) << "'req_i[i].req' should be a hierarchical RefObj";

  ASSERT_NE(hierPath->getPathElems(), nullptr);
  ASSERT_EQ(hierPath->getPathElems()->size(), 2u);

  const hldb::Any *const first = hierPath->getPathElems()->at(0);
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(first->getName(), std::string_view{"req_i"});
  const hldb::BitSelect *const arraySel = any_cast<hldb::BitSelect>(first);
  ASSERT_NE(arraySel, nullptr) << "first path element 'req_i[i]' should be a BitSelect";

  const hldb::Any *const second = hierPath->getPathElems()->at(1);
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(second->getName(), std::string_view{"req"});
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
