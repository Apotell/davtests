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

// Tests for HierPathCont/dut.sv (tags: HierPathCont)
//   typedef struct packed {logic [3:0] a;} my_struct_packed_t;
//   module unsized_single_bit_1 (
//       output wire [3:0] out1,
//       output wire out2,
//       output my_struct_packed_t out3,
//       output wire out4
//   );
//     assign out1   = '1;
//     assign out2   = (out1 == 4'b1111);
//     assign out3.a = '1;
//     assign out4   = (out3 == '1);
//   endmodule : unsized_single_bit_1
//
// Checked (per IEEE 1800-2023 Sec 10.3.2 -- Continuous assignment, Sec 23.6
// -- Hierarchical names):
//   - "out3.a" is a hierarchical-path-shaped reference to a member of the
//     packed-struct-typed port "out3", used as the LHS of a continuous
//     assignment. It resolves the same way any other hierarchical path does:
//     a RefObj with getPathElems() == ["out3", "a"], resolving via
//     getActual() to a TypespecMember for "a"
//   - the module has exactly 4 continuous assignments, one per port
//   - the non-hierarchical rhs of "out4 = (out3 == '1)" still resolves "out3"
//     (whole struct) as a plain (non-hierarchical, single-segment) RefObj

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/typespec_member.h>
#include <hldb/vpi_user.h>

namespace hlc {

class HierPathContTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierPathCont.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("unsized_single_bit_1", m_design->getAllModules());
  }

  static const hldb::ContAssign *findContAssignByLhsName(const hldb::Module *top, std::string_view lhsName) {
    if ((top == nullptr) || (top->getContAssigns() == nullptr)) return nullptr;
    for (const hldb::ContAssign *const ca : *top->getContAssigns()) {
      const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == lhsName)) return ca;
    }
    return nullptr;
  }
};

TEST_F(HierPathContTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(HierPathContTest, ModuleHasFourContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 4u);
}

TEST_F(HierPathContTest, Out3ADotHierPathLhsResolvesToStructMember) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::ContAssign *const ca = findContAssignByLhsName(top, "out3.a");
  ASSERT_NE(ca, nullptr) << "continuous assignment 'out3.a = ...' not found";

  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(ca->getLhs(), nullptr);
  ASSERT_NE(lhs, nullptr) << "lhs of 'out3.a = ...' is not a RefObj hierarchical path";
  EXPECT_EQ(lhs->getName(), std::string_view{"out3.a"});

  ASSERT_NE(lhs->getPathElems(), nullptr);
  ASSERT_EQ(lhs->getPathElems()->size(), 2u);
  EXPECT_EQ(lhs->getPathElems()->at(0)->getName(), std::string_view{"out3"});
  EXPECT_EQ(lhs->getPathElems()->at(1)->getName(), std::string_view{"a"});

  ASSERT_NE(lhs->getActual(), nullptr);
  const hldb::TypespecMember *const member = lhs->getActual<hldb::TypespecMember>();
  ASSERT_NE(member, nullptr) << "'out3.a' should resolve to the struct member 'a'";
  EXPECT_EQ(member->getName(), std::string_view{"a"});
}

TEST_F(HierPathContTest, Out4EqualsOut3UsesPlainNonHierRefToOut3) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::ContAssign *const ca = findContAssignByLhsName(top, "out4");
  ASSERT_NE(ca, nullptr) << "continuous assignment 'out4 = (out3 == '1)' not found";

  const hldb::Operation *const eq = ca->getRhs<hldb::Operation>();
  ASSERT_NE(ca->getRhs(), nullptr);
  ASSERT_NE(eq, nullptr);
  EXPECT_EQ(eq->getOpType(), vpiEqOp);
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);

  const hldb::RefObj *const out3Ref = any_cast<hldb::RefObj>(eq->getOperands()->at(0));
  ASSERT_NE(out3Ref, nullptr);
  EXPECT_EQ(out3Ref->getName(), std::string_view{"out3"});
  // A plain (whole-variable) reference is not a hierarchical path: no path
  // elements to walk, since there is no dotted member access here.
  EXPECT_EQ(out3Ref->getPathElems(), nullptr);
}

TEST_F(HierPathContTest, CompilerReportsZeroErrors) {
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
