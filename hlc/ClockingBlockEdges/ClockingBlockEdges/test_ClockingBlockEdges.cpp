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

// Tests for dut.sv (tags: ClockingBlockEdges)
//
// What is checked (IEEE 1800-2023):
//   - 37.48 detail 1: vpiInputEdge/vpiOutputEdge on a clocking block apply to
//     its default constructs (the 'default input/output' skew of 14.3); the
//     edge_identifier of a clocking_skew (14.3, A.6.11) is posedge, negedge or
//     edge (vpiPosedge, vpiNegedge, vpiAnyEdge)
//   - this holds for every clocking block: a non-default block in a module
//     (cb_m), the module's default clocking (cb_d), and blocks in an
//     interface (cb_i) and a program (cb_p)
//   - a skew with no edge keyword ('#1step') and a block with no default skew
//     at all (cb_none) leave the edge at vpiNoEdge

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/clocking_block.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/program.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class ClockingBlockEdgesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ClockingBlockEdges.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::ClockingBlock *getModuleBlock(std::string_view name) {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::ClockingBlock>(name, top->getClockingBlocks());
  }

  static void expectEdges(const hldb::ClockingBlock *cb, int32_t inputEdge, int32_t outputEdge) {
    ASSERT_NE(cb, nullptr);
    EXPECT_EQ(cb->getInputEdge(), inputEdge) << cb->getName() << ": vpiInputEdge";
    EXPECT_EQ(cb->getOutputEdge(), outputEdge) << cb->getName() << ": vpiOutputEdge";
  }
};

TEST_F(ClockingBlockEdgesTest, NonDefaultModuleBlock) {
  // 'default input #1step output negedge #2;'
  expectEdges(getModuleBlock("cb_m"), vpiNoEdge, vpiNegedge);
}

TEST_F(ClockingBlockEdgesTest, ModuleDefaultClocking) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  // 'default input posedge #1 output edge #1;'
  expectEdges(top->getDefaultClocking(), vpiPosedge, vpiAnyEdge);
}

TEST_F(ClockingBlockEdgesTest, BlockWithoutDefaultSkewHasNoEdge) {
  expectEdges(getModuleBlock("cb_none"), vpiNoEdge, vpiNoEdge);
}

TEST_F(ClockingBlockEdgesTest, InterfaceBlock) {
  const hldb::Interface *const ifc = hldb::findByName<hldb::Interface>("ifc", m_design->getAllInterfaces());
  ASSERT_NE(ifc, nullptr);
  // 'default input negedge output posedge;'
  expectEdges(hldb::findByName<hldb::ClockingBlock>("cb_i", ifc->getClockingBlocks()), vpiNegedge, vpiPosedge);
}

TEST_F(ClockingBlockEdgesTest, ProgramBlock) {
  const hldb::Program *const prg = hldb::findByName<hldb::Program>("prg", m_design->getAllPrograms());
  ASSERT_NE(prg, nullptr);
  // 'default output edge #3;'
  expectEdges(hldb::findByName<hldb::ClockingBlock>("cb_p", prg->getClockingBlocks()), vpiNoEdge, vpiAnyEdge);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
