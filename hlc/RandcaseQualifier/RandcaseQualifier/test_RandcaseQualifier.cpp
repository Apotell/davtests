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

// Tests for dut.sv (tags: RandcaseQualifier)
//
// What is checked (IEEE 1800-2023):
//   - 18.16 / 37.72: the randcase is a case stmt with vpiQualifier ==
//     vpiRandQualifier, holding its two weighted items
//   - the ordinary 'case (y)' has vpiQualifier == vpiNoQualifier and
//     vpiCaseType == vpiCaseExact

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/case_stmt.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <cstddef>

namespace hlc {

class RandcaseQualifierTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "RandcaseQualifier.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // The n-th statement of the initial block's begin-end.
  static const hldb::CaseStmt *getCase(size_t index) {
    const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
    if ((top == nullptr) || (top->getProcesses() == nullptr) || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
    if ((blk == nullptr) || (blk->getStmts() == nullptr) || (blk->getStmts()->size() <= index)) return nullptr;
    return any_cast<hldb::CaseStmt>(blk->getStmts()->at(index));
  }
};

TEST_F(RandcaseQualifierTest, RandcaseHasRandQualifier) {
  const hldb::CaseStmt *const rc = getCase(0);
  ASSERT_NE(rc, nullptr);
  EXPECT_EQ(rc->getQualifier(), vpiRandQualifier) << "18.16 / 37.72";
  ASSERT_NE(rc->getCaseItems(), nullptr);
  EXPECT_EQ(rc->getCaseItems()->size(), 2u);
}

TEST_F(RandcaseQualifierTest, OrdinaryCaseHasNoQualifier) {
  const hldb::CaseStmt *const c = getCase(1);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getQualifier(), vpiNoQualifier);
  EXPECT_EQ(c->getCaseType(), vpiCaseExact);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
