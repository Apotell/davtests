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

// Tests for dut.sv (tags: AssertionFormalDirection)
//
// What is checked (IEEE 1800-2023):
//   - 37.51 detail 5: a property formal's vpiDirection is vpiNoDirection
//     unless it is a local variable formal argument, which is vpiInput
//     ('local input' or plain 'local', 16.12.19)
//   - 37.51 detail 4: a property formal's default is reachable through vpiExpr,
//     whether it is an expr, a clocked sequence or a sequence instance

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/clocked_seq.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/prop_formal_decl.h>
#include <hldb/property_decl.h>
#include <hldb/ref_obj.h>
#include <hldb/sequence_inst.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <string_view>
#include <utility>

namespace hlc {

class AssertionFormalDirectionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "AssertionFormalDirection.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(AssertionFormalDirectionTest, PropertyFormalDirection) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::PropertyDecl *const p = hldb::findByName<hldb::PropertyDecl>("p", top->getPropertyDecls());
  ASSERT_NE(p, nullptr);
  const std::pair<std::string_view, int32_t> expected[] = {
      {"a", vpiNoDirection}, {"b", vpiInput}, {"c", vpiInput}, {"s", vpiNoDirection}};
  for (const std::pair<std::string_view, int32_t> &e : expected) {
    const hldb::PropFormalDecl *const f = hldb::findByName<hldb::PropFormalDecl>(e.first, p->getPropFormalDecls());
    ASSERT_NE(f, nullptr) << e.first;
    EXPECT_EQ(f->getDirection(), e.second) << "37.51 detail 5: formal '" << e.first << "'";
  }
}

TEST_F(AssertionFormalDirectionTest, PropertyFormalDefaultsAreKept) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::PropertyDecl *const pd = hldb::findByName<hldb::PropertyDecl>("pd", top->getPropertyDecls());
  ASSERT_NE(pd, nullptr);
  const std::pair<std::string_view, hldb::AnyType> expected[] = {{"s1", hldb::AnyType::ClockedSeq},
                                                                  {"r2", hldb::AnyType::Operation},
                                                                  {"s2", hldb::AnyType::SequenceInst},
                                                                  {"u", hldb::AnyType::RefObj}};
  for (const std::pair<std::string_view, hldb::AnyType> &e : expected) {
    const hldb::PropFormalDecl *const f = hldb::findByName<hldb::PropFormalDecl>(e.first, pd->getPropFormalDecls());
    ASSERT_NE(f, nullptr) << e.first;
    ASSERT_NE(f->getExpr(), nullptr) << "37.51 detail 4: default of '" << e.first << "'";
    EXPECT_EQ(f->getExpr()->getAnyType(), e.second) << e.first;
    EXPECT_EQ(f->getExpr()->getParent(), f) << e.first;
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
