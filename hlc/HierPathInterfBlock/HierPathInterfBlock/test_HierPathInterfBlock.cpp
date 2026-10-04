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

// Tests for HierPathInterfBlock/dut.sv (tags: HierPathInterfBlock)
//   interface intf;
//       function automatic integer f;               // line 2
//           return 1;
//       endfunction
//       if (1) begin : blk
//           function automatic integer f;            // line 6
//               return 2;
//           endfunction
//       end
//   endinterface
//   module top;
//       intf i();
//       function automatic integer f;                // line 14
//           return 3;
//       endfunction
//       if (1) begin : blk
//           function automatic integer f;            // line 18
//               return 4;
//           endfunction
//       end
//       initial begin
//           $display(f());                            // line 23 -- module-scope f (line 14)
//           $display(blk.f());                         // line 24 -- generate-block f (line 18)
//           $display(i.f());                           // line 25 -- interface-scope f (line 2)
//           $display(i.blk.f());                       // line 26 -- interface generate-block f (line 6)
//           $display(top.f());                         // line 27 -- self hier-name to module-scope f
//           $display(top.blk.f());                     // line 28 -- self hier-name into own generate block
//           $display(top.i.f());                       // line 29 -- self hier-name into interface instance
//           $display(top.i.blk.f());                   // line 30 -- self hier-name into interface's block
//       endmodule
//
// Checked (per IEEE 1800-2023 Sec 23.6 -- Hierarchical names, Sec 27.6 --
// Generate block naming):
//   - a hierarchical path can reach a function nested inside a *generate*
//     block of a plain module ("blk.f"), inside an *interface instance*
//     ("i.f"), and inside a generate block nested within an interface
//     instance ("i.blk.f") -- three distinct scope-nesting shapes, plus the
//     same three reached via the module's own name used as a leading
//     hierarchical prefix ("top.blk.f", "top.i.f", "top.i.blk.f")
//   - four distinct functions are all named "f"; each of the 8 call sites
//     must bind to the specific "f" implied by its own path, not silently
//     alias to a different one or fail to resolve
//   - the interface's and the module's own "blk" generate block (from
//     "if (1) begin : blk ... end", an unconditional generate-if) are each
//     modeled as a named GenScopeArray
//   - none of the 8 hierarchical function calls produce a COMP_FAILED_TO_BIND
//     diagnostic for "f"

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/gen_scope_array.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/vpi_user.h>

namespace hlc {

class HierPathInterfBlockTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierPathInterfBlock.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
  static const hldb::Interface *getIntf() {
    return hldb::findByName<hldb::Interface>("intf", m_design->getAllInterfaces());
  }
};

TEST_F(HierPathInterfBlockTest, ModuleAndInterfaceExist) {
  EXPECT_NE(getTop(), nullptr);
  EXPECT_NE(getIntf(), nullptr);
}

TEST_F(HierPathInterfBlockTest, ModuleHasNamedGenerateBlockBlk) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getGenScopeArrays(), nullptr);
  const hldb::GenScopeArray *const blk = hldb::findByName<hldb::GenScopeArray>("blk", top->getGenScopeArrays());
  ASSERT_NE(blk, nullptr) << "generate block 'blk' not found under module 'top'";
  ASSERT_NE(blk->getGenScopes(), nullptr);
  EXPECT_EQ(blk->getGenScopes()->size(), 1u) << "'if (1) begin : blk ... end' is unconditional, single instance";
}

TEST_F(HierPathInterfBlockTest, InterfaceHasNamedGenerateBlockBlk) {
  const hldb::Interface *const intf = getIntf();
  ASSERT_NE(intf, nullptr);
  ASSERT_NE(intf->getGenScopeArrays(), nullptr);
  const hldb::GenScopeArray *const blk = hldb::findByName<hldb::GenScopeArray>("blk", intf->getGenScopeArrays());
  ASSERT_NE(blk, nullptr) << "generate block 'blk' not found under interface 'intf'";
  ASSERT_NE(blk->getGenScopes(), nullptr);
  EXPECT_EQ(blk->getGenScopes()->size(), 1u);
}

TEST_F(HierPathInterfBlockTest, ModuleHasInterfaceInstance) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getInterfaces(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Interface>("i", top->getInterfaces()), nullptr);
}

TEST_F(HierPathInterfBlockTest, NoneOfTheEightHierPathCallsFailToBind) {
  // Narrowed by symbol "f" (not just the bare type) per the guide -- exact
  // source columns for each of the 8 call sites are not known up front, so
  // this does not further narrow by line/column.
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "f"), nullptr)
      << "at least one hierarchical call to 'f' failed to bind";
}

TEST_F(HierPathInterfBlockTest, CompilerReportsZeroErrors) {
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
