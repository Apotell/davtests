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

// Tests for tests/HierPathPackedArrayNet/dut.sv:
//
//   package rvfi_pkg;
//   localparam NRET = 1;
//   typedef struct packed { logic [NRET-1:0] trap; } rvfi_instr_t;
//   endpackage
//
//   module rvfi_tracer #(parameter int unsigned NR_COMMIT_PORTS = 2)(
//     input rvfi_pkg::rvfi_instr_t[NR_COMMIT_PORTS-1:0] rvfi_i
//   );
//   always_ff @(posedge clk_i) begin
//     for (int i = 0; i < NR_COMMIT_PORTS; i++) begin
//       if (rvfi_i[i].trap) begin
//       end
//     end
//   end
//   endmodule
//
// "rvfi_i[i].trap" first bit-selects an element of the unpacked-array port
// "rvfi_i" and then descends via a hierarchical path into the ".trap" field
// of the packed struct element type -- the array-select is applied *before*
// entering the hierarchical path (contrast with HierPathPackedStruct /
// HierPathSelect, where the select applies to a field already reached via a
// hierarchical path).
//
// "rvfi_i" is declared with a data type only ("input rvfi_pkg::rvfi_instr_t
// ... rvfi_i") -- no "wire"/"var" keyword is written. Per IEEE 1800-2023
// Sec 6.7/6.8, a port with no net-type keyword defaults to a variable, not
// a net (the "default nettype" applies only to nets that ARE declared with
// a net keyword or implicit single-bit wires); this is the same standard
// citation already applied elsewhere in this suite for undecorated struct
// port declarations.
//
// Checked:
//   - module "rvfi_tracer" exists and has one Variable-typed port "rvfi_i"
//     (not a Net)
//   - the always_ff / for / if nesting is present
//   - the if-condition is a hierarchical RefObj with 2 path elements:
//     a BitSelect named "rvfi_i" (index "i"), then RefObj "trap"

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/for_stmt.h>
#include <hldb/if_stmt.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>

#include <gtest/gtest.h>

#include <string_view>

namespace hlc {

class HierPathPackedArrayNetTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierPathPackedArrayNet.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTracer() {
    return hldb::findByDefName<hldb::Module>("rvfi_tracer", m_design->getAllModules());
  }

  // Descends always_ff -> event-control -> for -> begin -> if, returning the
  // IfStmt's condition, or nullptr if any expected link is missing.
  static const hldb::Expr *findIfCondition(const hldb::Module *mod) {
    if (mod == nullptr || mod->getProcesses() == nullptr) return nullptr;
    for (const hldb::Any *const proc : *mod->getProcesses()) {
      const hldb::Always *const always = any_cast<hldb::Always>(proc);
      if (always == nullptr) continue;
      const hldb::EventControl *const evc = always->getStmt<hldb::EventControl>();
      if (evc == nullptr) continue;
      const hldb::ForStmt *const forStmt = evc->getStmt<hldb::ForStmt>();
      if (forStmt == nullptr) continue;
      const hldb::Begin *const body = forStmt->getStmt<hldb::Begin>();
      if (body == nullptr || body->getStmts() == nullptr) continue;
      for (const hldb::Any *const s : *body->getStmts()) {
        const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(s);
        if (ifStmt != nullptr) return ifStmt->getCondition();
      }
    }
    return nullptr;
  }
};

TEST_F(HierPathPackedArrayNetTest, ModuleExists) { EXPECT_NE(getTracer(), nullptr); }

TEST_F(HierPathPackedArrayNetTest, RvfiIIsVariableNotNet) {
  // Per IEEE 1800-2023 Sec 6.7/6.8: no net-type keyword on the port
  // declaration means "rvfi_i" must be modeled as a Variable, never a Net,
  // regardless of any `default_nettype.
  const hldb::Module *const mod = getTracer();
  ASSERT_NE(mod, nullptr);
  const hldb::Variable *const asVar = hldb::findByName<hldb::Variable>("rvfi_i", mod->getVariables());
  EXPECT_NE(asVar, nullptr) << "'rvfi_i' has no net-type keyword and must be modeled as a Variable";
  const hldb::Net *const asNet = hldb::findByName<hldb::Net>("rvfi_i", mod->getNets());
  EXPECT_EQ(asNet, nullptr) << "'rvfi_i' must not be modeled as a Net (no net-type keyword given)";
}

TEST_F(HierPathPackedArrayNetTest, IfConditionExists) { EXPECT_NE(findIfCondition(getTracer()), nullptr); }

TEST_F(HierPathPackedArrayNetTest, ConditionIsArraySelectThenHierPathToTrap) {
  const hldb::Expr *const cond = findIfCondition(getTracer());
  ASSERT_NE(cond, nullptr);
  const hldb::RefObj *const hierPath = any_cast<hldb::RefObj>(cond);
  ASSERT_NE(hierPath, nullptr) << "'rvfi_i[i].trap' condition should be a hierarchical RefObj";

  ASSERT_NE(hierPath->getPathElems(), nullptr);
  ASSERT_EQ(hierPath->getPathElems()->size(), 2u);

  const hldb::Any *const first = hierPath->getPathElems()->at(0);
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(first->getName(), std::string_view{"rvfi_i"});
  const hldb::BitSelect *const arraySel = any_cast<hldb::BitSelect>(first);
  ASSERT_NE(arraySel, nullptr) << "first path element 'rvfi_i[i]' should be a BitSelect";
  ASSERT_NE(arraySel->getIndex(), nullptr);
  const hldb::RefObj *const idx = arraySel->getIndex<hldb::RefObj>();
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(idx->getName(), std::string_view{"i"});

  const hldb::Any *const second = hierPath->getPathElems()->at(1);
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(second->getName(), std::string_view{"trap"});
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
