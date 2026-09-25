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

// Tests for tests/HierPathUnpackedInPacked/dut.sv:
//   package keymgr_pkg;
//     parameter int Shares = 2;
//     parameter int KeyWidth = 16;
//     typedef struct packed {
//       logic [Shares-1:0][KeyWidth-1:0] key;
//     } hw_key_req_t;
//     typedef struct packed {
//       logic [31:0] pair[2];  // << Illegal
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
// This exercises a hierarchical path into an unpacked dimension nested
// inside an otherwise-packed type: unlike HierPathUnpacked's foo_t, this
// foo_t is declared 'struct packed', and its member 'pair' has an unpacked
// dimension (placed after the member name). Per IEEE 1800-2023 Sec 7.2.1,
// "All members of a packed structure ... shall be packed types" -- an
// unpacked-dimensioned member is illegal inside a packed struct/union, so
// this must be flagged as a compile error rather than silently accepted or
// modeled as if it were legal.
//
// The legal hw_key_req_t/keymgr_key_i.key[0] hierarchical path (same shape
// as HierPathUnpacked's p1) is also checked here since it is unaffected by
// foo_t's illegal member.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>

namespace hlc {

class HierPathUnpackedInPackedTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierPathUnpackedInPacked.hlc"}); }
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

TEST_F(HierPathUnpackedInPackedTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr); }

// The illegal unpacked-in-packed member must be diagnosed at its
// declaration (line 10, member 'pair'), per IEEE 1800-2023 Sec 7.2.1.
TEST_F(HierPathUnpackedInPackedTest, IllegalUnpackedMemberInPackedStructIsReported) {
  ASSERT_NE(findError(ErrorDefinition::COMP_UNPACKED_IN_PACKED, "pair"), nullptr)
      << "expected an error for the unpacked dimension on 'pair' inside packed struct 'foo_t'";
}

// Unaffected by foo_t's error: the legal hw_key_req_t.key[0] hierarchical
// path into a packed 2D-dimensioned struct member.
TEST_F(HierPathUnpackedInPackedTest, ParamP1ValueIsBitsOfKeymgrKeyIDotKeyZero) {
  const hldb::ParamAssign *const pa = getParamAssign("p1");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p1' not found";
  ASSERT_NE(pa->getRhs(), nullptr);
  const hldb::SysFuncCall *const bits = pa->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(bits, nullptr);
  EXPECT_EQ(bits->getName(), std::string_view("$bits"));
  ASSERT_NE(bits->getArguments(), nullptr);
  ASSERT_EQ(bits->getArguments()->size(), 1u);

  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(bits->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), std::string_view("keymgr_key_i.key[0]"));
  ASSERT_NE(arg->getPathElems(), nullptr);
  ASSERT_EQ(arg->getPathElems()->size(), 2u) << "expected keymgr_key_i -> key[0]";

  const hldb::RefObj *const varElem = any_cast<hldb::RefObj>(arg->getPathElems()->at(0));
  ASSERT_NE(varElem, nullptr);
  EXPECT_EQ(varElem->getName(), std::string_view("keymgr_key_i"));
  ASSERT_NE(varElem->getActual(), nullptr);
  EXPECT_NE(varElem->getActual<hldb::Variable>(), nullptr);

  const hldb::BitSelect *const idxElem = any_cast<hldb::BitSelect>(arg->getPathElems()->at(1));
  ASSERT_NE(idxElem, nullptr);
  ASSERT_NE(idxElem->getPrefix(), nullptr);
  EXPECT_EQ(idxElem->getPrefix<hldb::RefObj>()->getName(), std::string_view("key"));
  ASSERT_NE(idxElem->getIndex(), nullptr);
  const hldb::Constant *const idx = idxElem->getIndex<hldb::Constant>();
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(idx->getDecompile(), std::string_view("0"));
}

TEST_F(HierPathUnpackedInPackedTest, HwKeyReqTStructIsPacked) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  const hldb::Variable *const keymgrKeyI = hldb::findByName<hldb::Variable>("keymgr_key_i", top->getVariables());
  ASSERT_NE(keymgrKeyI, nullptr);
  ASSERT_NE(keymgrKeyI->getTypespec(), nullptr);
  const hldb::StructTypespec *const st = keymgrKeyI->getTypespec<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr);
  const hldb::Struct *const s = st->getStruct();
  ASSERT_NE(s, nullptr);
  EXPECT_TRUE(s->getPacked());
}

// The overall diagnostic set is otherwise clean -- only the one expected
// unpacked-in-packed error, nothing fatal or syntactic.
TEST_F(HierPathUnpackedInPackedTest, NoFatalOrSyntaxErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
