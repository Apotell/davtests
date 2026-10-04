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

// Tests for tests/HierPathUnpacked/dut.sv:
//   package keymgr_pkg;
//     parameter int Shares = 2;
//     parameter int KeyWidth = 16;
//     typedef struct packed {
//       logic [Shares-1:0][KeyWidth-1:0] key;
//     } hw_key_req_t;
//     typedef struct {
//       logic [31:0] pair[2];
//     } foo_t;
//   endpackage
//   module top(output int o);
//     keymgr_pkg::hw_key_req_t keymgr_key_i;
//     keymgr_pkg::foo_t a;
//     parameter p1 = $bits(keymgr_key_i.key[0]);
//     parameter p2 = $bits(a.pair[0]);
//     if (p1 == 16) begin GOOD p1u(); end
//     if (p2 == 32) begin GOOD p2u(); end
//   endmodule
//
// This exercises a hierarchical path selecting into an *unpacked* array:
// 'a.pair[0]' selects element 0 of foo_t's unpacked member array 'pair'
// (declared with the dimension after the member name -> unpacked, per IEEE
// 1800-2023 Sec 7.4.2 / Sec 10.9.2). 'foo_t' itself (unlike hw_key_req_t) has
// no 'packed' keyword, so per Sec 7.2.1 an unpacked-array member is legal
// inside it.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>

namespace hlc {

class HierPathUnpackedTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierPathUnpacked.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::ParamAssign *getParamAssign(std::string_view name) {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *top->getParamAssigns()) {
      if (const hldb::Any *const lhs = pa->getLhs()) {
        if (lhs->getName() == name) return pa;
      }
    }
    return nullptr;
  }
};

TEST_F(HierPathUnpackedTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(HierPathUnpackedTest, VariableAExistsWithFooTStruct) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getTypespec(), nullptr);
  const hldb::StructTypespec *const st = a->getTypespec<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr);
  const hldb::Struct *const s = st->getStruct();
  ASSERT_NE(s, nullptr);
  EXPECT_FALSE(s->getPacked()) << "foo_t has no 'packed' keyword";
}

TEST_F(HierPathUnpackedTest, PairMemberIsUnpackedArrayOfTwo32BitLogic) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::StructTypespec *const st = a->getTypespec<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr);
  const hldb::Struct *const s = st->getStruct();
  ASSERT_NE(s, nullptr);
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 1u);

  const hldb::TypespecMember *const pair = s->getMembers()->at(0);
  ASSERT_NE(pair, nullptr);
  EXPECT_EQ(pair->getName(), std::string_view("pair"));
  ASSERT_NE(pair->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = pair->getTypespec<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "'pair' should resolve to an ArrayTypespec";
  EXPECT_FALSE(at->getPacked()) << "dimension placed after the member name is an unpacked dimension";

  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::LogicTypespec *const elem = at->getElemTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(elem, nullptr);
}

TEST_F(HierPathUnpackedTest, ParamP2ValueIsBitsOfADotPairZero) {
  const hldb::ParamAssign *const pa = getParamAssign("p2");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p2' not found";
  ASSERT_NE(pa->getRhs(), nullptr);
  const hldb::SysFuncCall *const bits = pa->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(bits, nullptr);
  EXPECT_EQ(bits->getName(), std::string_view("$bits"));
  ASSERT_NE(bits->getArguments(), nullptr);
  ASSERT_EQ(bits->getArguments()->size(), 1u);

  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(bits->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), std::string_view("a.pair[0]"));
  ASSERT_NE(arg->getPathElems(), nullptr);
  ASSERT_EQ(arg->getPathElems()->size(), 2u) << "expected a -> pair[0]";
}

TEST_F(HierPathUnpackedTest, ParamP2ArgPathElemsResolveThroughUnpackedIndex) {
  const hldb::ParamAssign *const pa = getParamAssign("p2");
  ASSERT_NE(pa, nullptr);
  const hldb::SysFuncCall *const bits = pa->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(bits, nullptr);
  ASSERT_NE(bits->getArguments(), nullptr);
  ASSERT_EQ(bits->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(bits->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  ASSERT_NE(arg->getPathElems(), nullptr);
  ASSERT_EQ(arg->getPathElems()->size(), 2u);

  const hldb::RefObj *const varElem = any_cast<hldb::RefObj>(arg->getPathElems()->at(0));
  ASSERT_NE(varElem, nullptr);
  EXPECT_EQ(varElem->getName(), std::string_view("a"));
  ASSERT_NE(varElem->getActual(), nullptr);
  EXPECT_NE(varElem->getActual<hldb::Variable>(), nullptr);

  const hldb::BitSelect *const idxElem = any_cast<hldb::BitSelect>(arg->getPathElems()->at(1));
  ASSERT_NE(idxElem, nullptr) << "'pair[0]' should be a bit-select";
  ASSERT_NE(idxElem->getPrefix(), nullptr);
  const hldb::RefObj *const prefix = idxElem->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), std::string_view("pair"));
  ASSERT_NE(prefix->getActual(), nullptr);
  EXPECT_NE(prefix->getActual<hldb::TypespecMember>(), nullptr);

  ASSERT_NE(idxElem->getIndex(), nullptr);
  const hldb::Constant *const idx = idxElem->getIndex<hldb::Constant>();
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(idx->getDecompile(), std::string_view("0"));
}

TEST_F(HierPathUnpackedTest, CompilerReportsZeroErrors) {
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
