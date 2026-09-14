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
// tests/Google/chapter-9/9.3.4--block_names_par.sv
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
// :name: block_names_par
// :description: parallel block names check
// :tags: 9.3.4
// */
// module block_tb ();
// 	reg a = 0;
// 	initial
// 		fork: name
// 			a = 1;
// 		join: name
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test (Sec 9.3.4, "Naming named blocks"):
// `fork : name ... join : name` -- a parallel block given a name directly
// in its block header ("fork : block_identifier"), with the same
// identifier optionally repeated after the closing "join" keyword.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "block_tb" exists.
//   - the initial statement (no begin/end at the initial level) binds a
//     ForkStmt directly, with getJoinType() == vpiJoin.
//   - the ForkStmt's own name (Scope::getName(), inherited since ForkStmt
//     extends Scope) is "name" -- the block-header identifier from
//     "fork : name".
//   - the ForkStmt's getEndLabel() is also "name" -- the repeated
//     identifier from "join : name".
//   - the ForkStmt contains exactly one statement, the Assignment "a = 1;".
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - Whether HLC enforces the (separate, simulation-time) IEEE rule that a
//     mismatched end label would be illegal: this source's start and end
//     labels already match, so no such check applies here.
//   - Runtime behavior of the assignment: HLC is a compiler/elaborator
//     with no simulation, so no execution ever happens for this test to
//     observe.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/assignment.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/fork_stmt.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>

#include <hlc/Tests/Test.h>

namespace hlc {
class BlockNamesParTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.3.4--block_names_par.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to BlockNamesParTest go here!

TEST_F(BlockNamesParTest, ModuleBlockTbExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'block_tb' not found";
}

TEST_F(BlockNamesParTest, InitialForkBindsDirectlyWithNamedForkJoin) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);

  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);

  ASSERT_NE(init->getStmt(), nullptr) << "'initial fork : name ... join : name' with no begin/end should bind the "
                                          "ForkStmt directly as the Initial's statement";
  const hldb::ForkStmt *const fork = any_cast<hldb::ForkStmt>(init->getStmt());
  ASSERT_NE(fork, nullptr) << "'fork : name ... join : name' should produce a ForkStmt";
  EXPECT_EQ(fork->getJoinType(), vpiJoin) << "plain 'join' should be vpiJoin, not join_any/join_none";

  EXPECT_EQ(fork->getName(), "name") << "'fork : name' should set the ForkStmt's own name to 'name'";
  EXPECT_EQ(fork->getEndLabel(), "name") << "'join : name' should set the ForkStmt's end label to 'name'";

  ASSERT_NE(fork->getStmts(), nullptr);
  ASSERT_EQ(fork->getStmts()->size(), 1u) << "'a = 1;' is the block's single statement";
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(fork->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "'a = 1;' should be a plain Assignment";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a");
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(assign->getRhs());
  ASSERT_NE(rhs, nullptr) << "'1' should be a Constant";
  EXPECT_EQ(rhs->getDecompile(), "1");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
