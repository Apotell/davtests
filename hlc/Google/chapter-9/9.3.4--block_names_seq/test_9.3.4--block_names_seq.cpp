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

// ============================================================================
// SystemVerilog source under test:
// tests/Google/chapter-9/9.3.4--block_names_seq.sv
// ----------------------------------------------------------------------------
// // Copyright (C) 2019-2021  The SymbiFlow Authors.
// //
// // Use of this source code is governed by a ISC-style
// // license that can be found in the LICENSE file or at
// // https://opensource.org/licenses/ISC
// //
// // SPDX-License-Identifier: ISC
//
// /*
// :name: block_names_seq
// :description: sequential block names check
// :tags: 9.3.4
// */
// module block_tb ();
// 	reg a = 0;
// 	reg b = 0;
// 	reg c = 0;
// 	initial
// 		begin: name
// 			a = 1;
// 			b = a;
// 			c = b;
// 		end: name
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test (Sec 9.3.4, "Naming named blocks"):
// `begin : name ... end : name` -- a sequential block given a name
// directly in its block header ("begin : block_identifier"), with the same
// identifier optionally repeated after the closing "end" keyword.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "block_tb" exists.
//   - the initial statement (no additional begin/end wrapping) binds a
//     Begin directly.
//   - the Begin's own name (Scope::getName()) is "name" -- the
//     block-header identifier from "begin : name".
//   - the Begin's getEndLabel() is also "name" -- the repeated identifier
//     from "end : name".
//   - the Begin contains exactly three sequential statements: "a = 1;",
//     "b = a;", "c = b;", each a plain Assignment.
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - Whether HLC enforces the (separate, simulation-time) IEEE rule that a
//     mismatched end label would be illegal: this source's start and end
//     labels already match, so no such check applies here.
//   - Runtime behavior of the three sequential assignments (does "b" end
//     up equal to "a", etc.): HLC is a compiler/elaborator with no
//     simulation, so no execution ever happens for this test to observe.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>

#include <hlc/Tests/Test.h>

namespace hlc {
class BlockNamesSeqTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.3.4--block_names_seq.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to BlockNamesSeqTest go here!

TEST_F(BlockNamesSeqTest, ModuleBlockTbExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'block_tb' not found";
}

TEST_F(BlockNamesSeqTest, InitialBindsNamedBeginDirectlyWithThreeAssignments) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);

  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);

  ASSERT_NE(init->getStmt(), nullptr) << "'initial begin: name ... end: name' should bind the Begin directly as "
                                          "the Initial's statement";
  const hldb::Begin *const body = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(body, nullptr) << "'begin: name ... end: name' should produce a Begin";

  EXPECT_EQ(body->getName(), "name") << "'begin : name' should set the Begin's own name to 'name'";
  EXPECT_EQ(body->getEndLabel(), "name") << "'end : name' should set the Begin's end label to 'name'";

  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 3u) << "'a = 1;', 'b = a;', 'c = b;' are exactly three statements";

  const hldb::Assignment *const assignA = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(assignA, nullptr) << "'a = 1;' should be a plain Assignment";
  const hldb::RefObj *const lhsA = assignA->getLhs<hldb::RefObj>();
  ASSERT_NE(lhsA, nullptr);
  EXPECT_EQ(lhsA->getName(), "a");
  EXPECT_NE(any_cast<hldb::Constant>(assignA->getRhs()), nullptr) << "'1' should be a Constant";

  const hldb::Assignment *const assignB = any_cast<hldb::Assignment>(body->getStmts()->at(1));
  ASSERT_NE(assignB, nullptr) << "'b = a;' should be a plain Assignment";
  const hldb::RefObj *const lhsB = assignB->getLhs<hldb::RefObj>();
  ASSERT_NE(lhsB, nullptr);
  EXPECT_EQ(lhsB->getName(), "b");
  const hldb::RefObj *const rhsB = any_cast<hldb::RefObj>(assignB->getRhs());
  ASSERT_NE(rhsB, nullptr) << "'a' (on the right-hand side) should be a RefObj";
  EXPECT_EQ(rhsB->getName(), "a");

  const hldb::Assignment *const assignC = any_cast<hldb::Assignment>(body->getStmts()->at(2));
  ASSERT_NE(assignC, nullptr) << "'c = b;' should be a plain Assignment";
  const hldb::RefObj *const lhsC = assignC->getLhs<hldb::RefObj>();
  ASSERT_NE(lhsC, nullptr);
  EXPECT_EQ(lhsC->getName(), "c");
  const hldb::RefObj *const rhsC = any_cast<hldb::RefObj>(assignC->getRhs());
  ASSERT_NE(rhsC, nullptr) << "'b' (on the right-hand side) should be a RefObj";
  EXPECT_EQ(rhsC->getName(), "b");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
