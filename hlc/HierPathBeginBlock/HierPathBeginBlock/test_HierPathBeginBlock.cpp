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

// Tests for HierPathBeginBlock/dut.sv (tags: HierPathBeginBlock)
//   module matching_end_labels_top(output reg [7:0] out1, out2, out3, out4);
//     initial begin
//       begin : blk1
//         reg x;
//         x = 1;
//       end
//       out1 = blk1.x;
//       begin : blk2
//         reg x;
//         x = 2;
//       end : blk2
//       out2 = blk2.x;
//     end
//     if (1) begin
//       if (1) begin : blk3
//         reg x;
//         assign x = 3;
//       end
//       assign out3 = blk3.x;
//       if (1) begin : blk4
//         reg x;
//         assign x = 4;
//       end : blk4
//       assign out4 = blk4.x;
//     end
//   endmodule
//
// Checked (per IEEE 1800-2023 Sec 23.6 -- Hierarchical names):
//   - a hierarchical path reference into a named begin/end block ("blk1.x",
//     "blk2.x") resolves as a RefObj whose getPathElems() holds one segment
//     per dotted component ("blk1"/"blk2", then "x"), and whose getActual()
//     resolves to the Variable "x" declared inside that block -- regardless
//     of whether the block's "end" carries a repeated label ("end" vs
//     "end : blk2")
//   - the same holds for named blocks nested inside an unconditional
//     generate-if ("blk3.x", "blk4.x"), reached via continuous assignment
//     instead of a procedural blocking assignment
//   - "reg x" is a variable (never a net) per Sec 6.8, irrespective of
//     `default_nettype`

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/process.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class HierPathBeginBlockTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierPathBeginBlock.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("matching_end_labels_top", m_design->getAllModules());
  }

  // Finds the Assignment/ContAssign whose lhs RefObj name matches lhsName, walking
  // the statement list of "stmts" (a Begin's getStmts() collection).
  static const hldb::Assignment *findAssignment(const hldb::AnyCollection *stmts, std::string_view lhsName) {
    if (stmts == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *stmts) {
      if (const hldb::Assignment *const assign = any_cast<hldb::Assignment>(stmt)) {
        const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
        if ((lhs != nullptr) && (lhs->getName() == lhsName)) return assign;
      }
    }
    return nullptr;
  }
};

TEST_F(HierPathBeginBlockTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(HierPathBeginBlockTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(HierPathBeginBlockTest, ProceduralHierPathBlk1XResolvesToVariableInBlk1) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const outer = init->getStmt<hldb::Begin>();
  ASSERT_NE(init->getStmt(), nullptr);
  ASSERT_NE(outer, nullptr) << "initial body is not a Begin";
  const hldb::Assignment *const assign = findAssignment(outer->getStmts(), "out1");
  ASSERT_NE(assign, nullptr) << "assignment 'out1 = blk1.x' not found";

  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(assign->getRhs(), nullptr);
  ASSERT_NE(rhs, nullptr) << "rhs of 'out1 = blk1.x' is not a RefObj hierarchical path";
  EXPECT_EQ(rhs->getName(), std::string_view{"blk1.x"});

  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u);
  EXPECT_EQ(rhs->getPathElems()->at(0)->getName(), std::string_view{"blk1"});
  EXPECT_EQ(rhs->getPathElems()->at(1)->getName(), std::string_view{"x"});

  ASSERT_NE(rhs->getActual(), nullptr);
  const hldb::Variable *const actual = rhs->getActual<hldb::Variable>();
  ASSERT_NE(actual, nullptr) << "'blk1.x' should resolve to the Variable 'x' declared inside 'blk1'";
  EXPECT_EQ(actual->getName(), std::string_view{"x"});
}

TEST_F(HierPathBeginBlockTest, ProceduralHierPathBlk2XResolvesEvenWithRepeatedEndLabel) {
  // "blk2" is closed with "end : blk2" (a repeated end label) instead of a
  // bare "end" -- per Sec 9.3.1 this must not change how "blk2.x" resolves.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const outer = init->getStmt<hldb::Begin>();
  ASSERT_NE(outer, nullptr);
  const hldb::Assignment *const assign = findAssignment(outer->getStmts(), "out2");
  ASSERT_NE(assign, nullptr) << "assignment 'out2 = blk2.x' not found";

  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(assign->getRhs(), nullptr);
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view{"blk2.x"});
  ASSERT_NE(rhs->getActual(), nullptr);
  const hldb::Variable *const actual = rhs->getActual<hldb::Variable>();
  ASSERT_NE(actual, nullptr);
  EXPECT_EQ(actual->getName(), std::string_view{"x"});
}

TEST_F(HierPathBeginBlockTest, ContinuousHierPathBlk3XResolvesToVariableInGenerateBlock) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const hldb::ContAssign *ca3 = nullptr;
  for (const hldb::ContAssign *const ca : *top->getContAssigns()) {
    const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
    if ((lhs != nullptr) && (lhs->getName() == "out3")) {
      ca3 = ca;
      break;
    }
  }
  ASSERT_NE(ca3, nullptr) << "continuous assignment 'out3 = blk3.x' not found";

  const hldb::RefObj *const rhs = ca3->getRhs<hldb::RefObj>();
  ASSERT_NE(ca3->getRhs(), nullptr);
  ASSERT_NE(rhs, nullptr) << "rhs of 'out3 = blk3.x' is not a RefObj hierarchical path";
  EXPECT_EQ(rhs->getName(), std::string_view{"blk3.x"});
  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u);
  EXPECT_EQ(rhs->getPathElems()->at(0)->getName(), std::string_view{"blk3"});
  EXPECT_EQ(rhs->getPathElems()->at(1)->getName(), std::string_view{"x"});

  ASSERT_NE(rhs->getActual(), nullptr);
  const hldb::Variable *const actual = rhs->getActual<hldb::Variable>();
  ASSERT_NE(actual, nullptr) << "'blk3.x' should resolve to the Variable 'x' inside the generate block 'blk3'";
}

TEST_F(HierPathBeginBlockTest, ContinuousHierPathBlk4XResolvesEvenWithRepeatedEndLabel) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const hldb::ContAssign *ca4 = nullptr;
  for (const hldb::ContAssign *const ca : *top->getContAssigns()) {
    const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
    if ((lhs != nullptr) && (lhs->getName() == "out4")) {
      ca4 = ca;
      break;
    }
  }
  ASSERT_NE(ca4, nullptr) << "continuous assignment 'out4 = blk4.x' not found";

  const hldb::RefObj *const rhs = ca4->getRhs<hldb::RefObj>();
  ASSERT_NE(ca4->getRhs(), nullptr);
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view{"blk4.x"});
  ASSERT_NE(rhs->getActual(), nullptr);
  const hldb::Variable *const actual = rhs->getActual<hldb::Variable>();
  ASSERT_NE(actual, nullptr) << "'blk4.x' should resolve to the Variable 'x' inside the generate block 'blk4'";
}

TEST_F(HierPathBeginBlockTest, CompilerReportsZeroErrors) {
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
