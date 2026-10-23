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

// Edge of module path terms (tests/SpecifyPathEdge/dut.sv):
//
//   IEEE 1800-2023 Sec 37.39   path term -> int vpiEdge
//   IEEE 1800-2023 Sec 30.4.3  edge-sensitive paths: the edge identifier (posedge, negedge,
//                              edge) qualifies the input terminal(s); a path without an edge
//                              identifier is active on any input transition
//   vpi_user.h                 vpiEdge values: vpiNoEdge, vpiPosedge, vpiNegedge, vpiAnyEdge
//
// The specify items are matched by source order (line 3 .. line 7 of dut.sv).

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/mod_path.h>
#include <hldb/module.h>
#include <hldb/path_term.h>
#include <hldb/specify_block.h>
#include <hldb/vpi_user.h>

#include <gtest/gtest.h>

#include <vector>

namespace hlc {

class SpecifyPathEdgeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "SpecifyPathEdge.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Mod paths of the single specify block, in source order.
  std::vector<const hldb::ModPath *> getModPaths() const {
    std::vector<const hldb::ModPath *> result;
    const hldb::Module *const m = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
    if ((m == nullptr) || (m->getSpecifyBlocks() == nullptr)) return result;
    for (const hldb::SpecifyBlock *sb : *m->getSpecifyBlocks()) {
      if (sb->getStmts() == nullptr) continue;
      for (const hldb::Any *item : *sb->getStmts()) {
        if (const hldb::ModPath *const mp = any_cast<hldb::ModPath>(item)) result.emplace_back(mp);
      }
    }
    return result;
  }

  static void expectTermEdges(const hldb::PathTermCollection *terms, int32_t edge) {
    ASSERT_NE(terms, nullptr);
    ASSERT_FALSE(terms->empty());
    for (const hldb::PathTerm *pt : *terms) {
      EXPECT_EQ(pt->getEdge(), edge);
    }
  }
};

TEST_F(SpecifyPathEdgeTest, PosedgeParallelPath) {
  const std::vector<const hldb::ModPath *> paths = getModPaths();
  ASSERT_EQ(paths.size(), 5u);
  expectTermEdges(paths[0]->getPathInTerms(), vpiPosedge);
  expectTermEdges(paths[0]->getPathOutTerms(), vpiNoEdge);
}

TEST_F(SpecifyPathEdgeTest, NegedgeFullPath) {
  const std::vector<const hldb::ModPath *> paths = getModPaths();
  ASSERT_EQ(paths.size(), 5u);
  expectTermEdges(paths[1]->getPathInTerms(), vpiNegedge);
  expectTermEdges(paths[1]->getPathOutTerms(), vpiNoEdge);
}

TEST_F(SpecifyPathEdgeTest, AnyEdgePath) {
  const std::vector<const hldb::ModPath *> paths = getModPaths();
  ASSERT_EQ(paths.size(), 5u);
  expectTermEdges(paths[2]->getPathInTerms(), vpiAnyEdge);
}

TEST_F(SpecifyPathEdgeTest, EdgeSensitivePathWithoutEdgeIdentifier) {
  const std::vector<const hldb::ModPath *> paths = getModPaths();
  ASSERT_EQ(paths.size(), 5u);
  expectTermEdges(paths[3]->getPathInTerms(), vpiNoEdge);
}

TEST_F(SpecifyPathEdgeTest, SimplePathHasNoEdge) {
  const std::vector<const hldb::ModPath *> paths = getModPaths();
  ASSERT_EQ(paths.size(), 5u);
  expectTermEdges(paths[4]->getPathInTerms(), vpiNoEdge);
  expectTermEdges(paths[4]->getPathOutTerms(), vpiNoEdge);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
