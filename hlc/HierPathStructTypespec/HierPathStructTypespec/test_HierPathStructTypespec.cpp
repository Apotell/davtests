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

// Tests for tests/HierPathStructTypespec/dut.sv:
//   package kmac_reg_pkg;
//     typedef struct packed {
//       struct packed { logic d; } sha3_idle;
//       struct packed { logic [4:0] d; } fifo_depth;
//     } kmac_hw2reg_status_reg_t;
//     typedef struct packed {
//       kmac_hw2reg_status_reg_t intr_state;
//       kmac_hw2reg_status_reg_t status;
//     } kmac_hw2reg_t;
//   endpackage
//   module top();
//     kmac_hw2reg_t hw2reg;
//     if ($bits(hw2reg.status.fifo_depth.d) != MsgFifoDepthW+1) begin : gen_fifo_depth_tie
//       $error(...);
//     end
//   endmodule
//
// This exercises a hierarchical path where the *resolved typespec* of the
// struct member reached at the end of the path matters: hw2reg -> status
// (a nested struct-typed member) -> fifo_depth (another nested struct-typed
// member) -> d (a 5-bit logic member, distinct from sha3_idle's 1-bit d).
// IEEE 1800-2023 Sec 23.6 (hierarchical names) / Sec 7.2.1 (structures).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/gen_if.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>

namespace hlc {

class HierPathStructTypespecTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierPathStructTypespec.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::StructTypespec *getHw2regStructTypespec() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getVariables() == nullptr) return nullptr;
    const hldb::Variable *const hw2reg = hldb::findByName<hldb::Variable>("hw2reg", top->getVariables());
    if (hw2reg == nullptr || hw2reg->getTypespec() == nullptr) return nullptr;
    return hw2reg->getTypespec<hldb::StructTypespec>();
  }

  static const hldb::GenIf *getFirstGenIf() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getGenStmts() == nullptr || top->getGenStmts()->empty()) return nullptr;
    return any_cast<hldb::GenIf>(top->getGenStmts()->at(0));
  }
};

TEST_F(HierPathStructTypespecTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(HierPathStructTypespecTest, VariableHw2regResolvesToStructTypespec) {
  const hldb::StructTypespec *const st = getHw2regStructTypespec();
  ASSERT_NE(st, nullptr) << "hw2reg's typespec should resolve (via TypedefTypespec kmac_hw2reg_t) to a StructTypespec";
}

TEST_F(HierPathStructTypespecTest, Hw2regStructHasIntrStateAndStatusMembers) {
  const hldb::StructTypespec *const st = getHw2regStructTypespec();
  ASSERT_NE(st, nullptr);
  const hldb::Struct *const s = st->getStruct();
  ASSERT_NE(s, nullptr);
  EXPECT_TRUE(s->getPacked());
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 2u);
  EXPECT_EQ(s->getMembers()->at(0)->getName(), std::string_view("intr_state"));
  EXPECT_EQ(s->getMembers()->at(1)->getName(), std::string_view("status"));
}

// Both intr_state and status share the same nested typedef
// (kmac_hw2reg_status_reg_t), which itself contains sha3_idle/fifo_depth.
TEST_F(HierPathStructTypespecTest, StatusMemberResolvesToNestedStructWithFifoDepth) {
  const hldb::StructTypespec *const st = getHw2regStructTypespec();
  ASSERT_NE(st, nullptr);
  const hldb::Struct *const s = st->getStruct();
  ASSERT_NE(s, nullptr);
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 2u);

  const hldb::TypespecMember *const status = s->getMembers()->at(1);
  ASSERT_NE(status, nullptr);
  ASSERT_NE(status->getTypespec(), nullptr);
  const hldb::StructTypespec *const statusSt = status->getTypespec<hldb::StructTypespec>();
  ASSERT_NE(statusSt, nullptr) << "'status' should resolve to a nested StructTypespec";
  const hldb::Struct *const statusStruct = statusSt->getStruct();
  ASSERT_NE(statusStruct, nullptr);
  ASSERT_NE(statusStruct->getMembers(), nullptr);
  ASSERT_EQ(statusStruct->getMembers()->size(), 2u);
  EXPECT_EQ(statusStruct->getMembers()->at(0)->getName(), std::string_view("sha3_idle"));
  EXPECT_EQ(statusStruct->getMembers()->at(1)->getName(), std::string_view("fifo_depth"));
}

TEST_F(HierPathStructTypespecTest, GenIfConditionUsesBitsOnFourElemHierPath) {
  const hldb::GenIf *const gi = getFirstGenIf();
  ASSERT_NE(gi, nullptr) << "expected one GenIf for the 'if ($bits(...) != ...)' generate construct";
  ASSERT_NE(gi->getCondition(), nullptr);
  const hldb::Operation *const cond = gi->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);

  const hldb::SysFuncCall *const bits = any_cast<hldb::SysFuncCall>(cond->getOperands()->at(0));
  ASSERT_NE(bits, nullptr) << "left operand of the '!=' should be the $bits(...) call";
  EXPECT_EQ(bits->getName(), std::string_view("$bits"));
  ASSERT_NE(bits->getArguments(), nullptr);
  ASSERT_EQ(bits->getArguments()->size(), 1u);

  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(bits->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), std::string_view("hw2reg.status.fifo_depth.d"));
  ASSERT_NE(arg->getPathElems(), nullptr);
  ASSERT_EQ(arg->getPathElems()->size(), 4u) << "expected hw2reg -> status -> fifo_depth -> d";
}

TEST_F(HierPathStructTypespecTest, BitsArgumentPathElemsResolveInOrder) {
  const hldb::GenIf *const gi = getFirstGenIf();
  ASSERT_NE(gi, nullptr);
  const hldb::Operation *const cond = gi->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::SysFuncCall *const bits = any_cast<hldb::SysFuncCall>(cond->getOperands()->at(0));
  ASSERT_NE(bits, nullptr);
  ASSERT_NE(bits->getArguments(), nullptr);
  ASSERT_EQ(bits->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(bits->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  ASSERT_NE(arg->getPathElems(), nullptr);
  ASSERT_EQ(arg->getPathElems()->size(), 4u);

  const char *const names[4] = {"hw2reg", "status", "fifo_depth", "d"};
  for (uint32_t i = 0; i < 4u; ++i) {
    const hldb::RefObj *const elem = any_cast<hldb::RefObj>(arg->getPathElems()->at(i));
    ASSERT_NE(elem, nullptr) << "path elem " << i;
    EXPECT_EQ(elem->getName(), std::string_view(names[i])) << "path elem " << i;
    ASSERT_NE(elem->getActual(), nullptr) << "path elem " << i;
  }
  // hw2reg is the top-level Variable; the remaining elements are struct members.
  const hldb::RefObj *const elem0 = any_cast<hldb::RefObj>(arg->getPathElems()->at(0));
  EXPECT_NE(elem0->getActual<hldb::Variable>(), nullptr);
  for (uint32_t i = 1; i < 4u; ++i) {
    const hldb::RefObj *const elem = any_cast<hldb::RefObj>(arg->getPathElems()->at(i));
    EXPECT_NE(elem->getActual<hldb::TypespecMember>(), nullptr) << "path elem " << i;
  }
}

TEST_F(HierPathStructTypespecTest, CompilerReportsZeroErrors) {
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
