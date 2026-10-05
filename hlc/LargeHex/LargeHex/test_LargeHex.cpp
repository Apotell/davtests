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

// Tests for dut.sv (tags: LargeHex)
//   module tlul_fifo_sync #(parameter int unsigned ReqDepth = 2) ();
//      logic [ReqDepth-1:0] storage;
//   endmodule
//
//   module tlul_socket_1n #(
//        parameter bit [75:0] DReqDepth = 76'h2000000000000000F)();
//        parameter int unsigned ReqDepthKO = DReqDepth[8*4+:4];
//        logic [ReqDepthKO-1:0] storageKO;
//        parameter int unsigned ReqDepthOK = DReqDepth[0+:4];
//        logic [ReqDepthOK-1:0] storageOK;
//       tlul_fifo_sync #(
//         .ReqDepth(DReqDepth[0+:4])
//       ) fifo_d ();
//   endmodule
//
// What is checked (IEEE 1800-2023):
//   - both module definitions exist (looked up by definition name)
//   - tlul_fifo_sync: ReqDepth is a value parameter port (6.20.2, 23.2.3),
//     not local, of type "int unsigned" (6.11, 6.11.3: unsigned), default 2;
//     "storage" is a logic variable whose packed range left bound is
//     ReqDepth - 1 bound to the parameter
//   - tlul_socket_1n: DReqDepth is a parameter port (not local) of type
//     "bit [75:0]" (unsigned 2-state vector, 6.11.3); its default value is a
//     76-bit sized hexadecimal literal (5.7.1): vpiHexConst, size 76, and the
//     68 bits of given digits are zero-padded on the left
//   - 6.20.1: tlul_socket_1n has a parameter_port_list, so the body
//     "parameter" declarations ReqDepthKO / ReqDepthOK are synonyms for
//     localparam (6.20.4)
//   - ReqDepthKO = DReqDepth[8*4+:4] and ReqDepthOK = DReqDepth[0+:4] are
//     indexed part-selects (11.5.1) of type "+:" with base expressions 8*4 /
//     0 and width 4, prefixed by the parameter DReqDepth
//   - storageKO / storageOK packed ranges reference the localparams
//   - fifo_d is an instance of tlul_fifo_sync whose parameter value
//     assignment is by name (23.10.2.2): .ReqDepth(DReqDepth[0+:4])
//
// What is NOT checked and why:
//   - evaluated parameter values (e.g. ReqDepthOK == 15, ReqDepthKO == 0)
//     and resulting variable widths: this .hlc does not elaborate, so no
//     elaborated/evaluated values are available.
//   - Module::getName(): HLC decorates definition names with their
//     parameter defaults; the definition name is looked up via getDefName().

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/indexed_part_select.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class LargeHexTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LargeHex.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModuleDef(std::string_view defName) {
    if (m_design->getAllModules() == nullptr) return nullptr;
    for (const hldb::Module *const m : *m_design->getAllModules()) {
      if (m->getDefName() == defName) return m;
    }
    return nullptr;
  }

  static const hldb::Parameter *getParam(const hldb::Module *m, std::string_view name) {
    return hldb::findByName<hldb::Parameter>(name, m->getParameters());
  }

  static const hldb::ParamAssign *getParamAssign(const hldb::Module *m, std::string_view name) {
    return hldb::findByName<hldb::ParamAssign>(name, m->getParamAssigns());
  }

  static const hldb::Typespec *actualTs(const hldb::RefTypespec *rt) {
    return (rt == nullptr) ? nullptr : rt->getActual();
  }

  // Checks "<lhs>[<base>+:4]" with prefix DReqDepth bound to a Parameter.
  static void checkPosIndexed(const hldb::Any *any, std::string_view what) {
    const hldb::IndexedPartSelect *const ips = any_cast<hldb::IndexedPartSelect>(any);
    ASSERT_NE(ips, nullptr) << what << " should be an indexed part-select";
    EXPECT_EQ(ips->getIndexedPartSelectType(), vpiPosIndexed) << what;
    const hldb::RefObj *const prefix = ips->getPrefix<hldb::RefObj>();
    ASSERT_NE(prefix, nullptr) << what;
    EXPECT_EQ(prefix->getName(), "DReqDepth");
    ASSERT_NE(prefix->getActual(), nullptr) << what;
    EXPECT_EQ(prefix->getActual()->getAnyType(), hldb::AnyType::Parameter) << what;
    const hldb::Constant *const width = ips->getWidthExpr<hldb::Constant>();
    ASSERT_NE(width, nullptr) << what;
    EXPECT_EQ(width->getDecompile(), "4") << what;
  }

  // Checks that var 'name' is a logic vector whose range is [<param>-1:0].
  static void checkStorage(const hldb::Module *m, std::string_view name, std::string_view param) {
    const hldb::Variable *const v = hldb::findByName<hldb::Variable>(name, m->getVariables());
    ASSERT_NE(v, nullptr) << name;
    EXPECT_TRUE(v->getVector()) << name;
    const hldb::LogicTypespec *const lt = any_cast<hldb::LogicTypespec>(actualTs(v->getTypespec()));
    ASSERT_NE(lt, nullptr) << name << " is declared 'logic [...]'";
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    const hldb::Range *const r = lt->getRanges()->at(0);
    ASSERT_NE(r, nullptr);
    const hldb::Operation *const left = r->getLeftExpr<hldb::Operation>();
    ASSERT_NE(left, nullptr) << name;
    EXPECT_EQ(left->getOpType(), vpiSubOp);
    ASSERT_NE(left->getOperands(), nullptr);
    ASSERT_EQ(left->getOperands()->size(), 2u);
    const hldb::RefObj *const p = any_cast<hldb::RefObj>(left->getOperands()->at(0));
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->getName(), param);
    ASSERT_NE(p->getActual(), nullptr);
    EXPECT_EQ(p->getActual()->getAnyType(), hldb::AnyType::Parameter);
    const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(right->getDecompile(), "0");
  }
};

TEST_F(LargeHexTest, BothModulesExist) {
  EXPECT_NE(getModuleDef("tlul_fifo_sync"), nullptr);
  EXPECT_NE(getModuleDef("tlul_socket_1n"), nullptr);
}

// ===========================================================================
// tlul_fifo_sync
// ===========================================================================

TEST_F(LargeHexTest, FifoReqDepthIsIntUnsignedParameterPort) {
  const hldb::Module *const m = getModuleDef("tlul_fifo_sync");
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const p = getParam(m, "ReqDepth");
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam()) << "a 'parameter' in the parameter_port_list is overridable";
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(actualTs(p->getTypespec()));
  ASSERT_NE(it, nullptr) << "ReqDepth is declared 'int unsigned'";
  EXPECT_FALSE(it->getSigned()) << "6.11.3: 'int unsigned' is unsigned";

  const hldb::ParamAssign *const pa = getParamAssign(m, "ReqDepth");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "2");
}

TEST_F(LargeHexTest, FifoStorageRangeUsesReqDepth) {
  const hldb::Module *const m = getModuleDef("tlul_fifo_sync");
  ASSERT_NE(m, nullptr);
  checkStorage(m, "storage", "ReqDepth");
}

// ===========================================================================
// tlul_socket_1n: DReqDepth = 76'h2000000000000000F
// ===========================================================================

TEST_F(LargeHexTest, DReqDepthIsBit76ParameterPort) {
  const hldb::Module *const m = getModuleDef("tlul_socket_1n");
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const p = getParam(m, "DReqDepth");
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam());
  const hldb::BitTypespec *const bt = any_cast<hldb::BitTypespec>(actualTs(p->getTypespec()));
  ASSERT_NE(bt, nullptr) << "DReqDepth is declared 'bit [75:0]'";
  EXPECT_FALSE(bt->getSigned()) << "6.11.3: bit vectors are unsigned by default";
  ASSERT_NE(bt->getRanges(), nullptr);
  ASSERT_EQ(bt->getRanges()->size(), 1u);
  const hldb::Range *const r = bt->getRanges()->at(0);
  ASSERT_NE(r, nullptr);
  const hldb::Constant *const l = r->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const rr = r->getRightExpr<hldb::Constant>();
  ASSERT_NE(l, nullptr);
  ASSERT_NE(rr, nullptr);
  EXPECT_EQ(l->getDecompile(), "75");
  EXPECT_EQ(rr->getDecompile(), "0");
}

TEST_F(LargeHexTest, DReqDepthDefaultIs76BitHexLiteral) {
  const hldb::Module *const m = getModuleDef("tlul_socket_1n");
  ASSERT_NE(m, nullptr);
  const hldb::ParamAssign *const pa = getParamAssign(m, "DReqDepth");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiHexConst);
  EXPECT_EQ(c->getSize(), 76) << "5.7.1: the size constant gives the exact number of bits";
  EXPECT_EQ(c->getDecompile(), "76'h2000000000000000F");
}

// ===========================================================================
// 6.20.1: body parameters of a module with a parameter_port_list are local
// ===========================================================================

TEST_F(LargeHexTest, BodyParametersAreLocalIntUnsigned) {
  const hldb::Module *const m = getModuleDef("tlul_socket_1n");
  ASSERT_NE(m, nullptr);
  for (std::string_view name : {"ReqDepthKO", "ReqDepthOK"}) {
    const hldb::Parameter *const p = getParam(m, name);
    ASSERT_NE(p, nullptr) << name;
    EXPECT_TRUE(p->getLocalParam()) << "6.20.1: '" << name << "' is a synonym for localparam";
    const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(actualTs(p->getTypespec()));
    ASSERT_NE(it, nullptr) << name;
    EXPECT_FALSE(it->getSigned()) << name;
  }
}

TEST_F(LargeHexTest, ReqDepthKOIsPosIndexedPartSelect) {
  const hldb::Module *const m = getModuleDef("tlul_socket_1n");
  ASSERT_NE(m, nullptr);
  const hldb::ParamAssign *const pa = getParamAssign(m, "ReqDepthKO");
  ASSERT_NE(pa, nullptr);
  checkPosIndexed(pa->getRhs(), "DReqDepth[8*4+:4]");
  const hldb::IndexedPartSelect *const ips = pa->getRhs<hldb::IndexedPartSelect>();
  ASSERT_NE(ips, nullptr);
  const hldb::Operation *const base = ips->getBaseExpr<hldb::Operation>();
  ASSERT_NE(base, nullptr) << "base expression is 8*4";
  EXPECT_EQ(base->getOpType(), vpiMultOp);
  ASSERT_NE(base->getOperands(), nullptr);
  ASSERT_EQ(base->getOperands()->size(), 2u);
  const hldb::Constant *const a = any_cast<hldb::Constant>(base->getOperands()->at(0));
  const hldb::Constant *const b = any_cast<hldb::Constant>(base->getOperands()->at(1));
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(a->getDecompile(), "8");
  EXPECT_EQ(b->getDecompile(), "4");
}

TEST_F(LargeHexTest, ReqDepthOKIsPosIndexedPartSelect) {
  const hldb::Module *const m = getModuleDef("tlul_socket_1n");
  ASSERT_NE(m, nullptr);
  const hldb::ParamAssign *const pa = getParamAssign(m, "ReqDepthOK");
  ASSERT_NE(pa, nullptr);
  checkPosIndexed(pa->getRhs(), "DReqDepth[0+:4]");
  const hldb::IndexedPartSelect *const ips = pa->getRhs<hldb::IndexedPartSelect>();
  ASSERT_NE(ips, nullptr);
  const hldb::Constant *const base = ips->getBaseExpr<hldb::Constant>();
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->getDecompile(), "0");
}

TEST_F(LargeHexTest, StorageRangesUseLocalParams) {
  const hldb::Module *const m = getModuleDef("tlul_socket_1n");
  ASSERT_NE(m, nullptr);
  checkStorage(m, "storageKO", "ReqDepthKO");
  checkStorage(m, "storageOK", "ReqDepthOK");
}

// ===========================================================================
// 23.10.2.2: tlul_fifo_sync #(.ReqDepth(DReqDepth[0+:4])) fifo_d ();
// ===========================================================================

TEST_F(LargeHexTest, InstanceFifoDOverridesReqDepthByName) {
  const hldb::Module *const m = getModuleDef("tlul_socket_1n");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getRefInstances(), nullptr);
  ASSERT_EQ(m->getRefInstances()->size(), 1u);
  const hldb::RefInstance *const inst = m->getRefInstances()->at(0);
  ASSERT_NE(inst, nullptr);
  EXPECT_EQ(inst->getName(), "fifo_d");
  ASSERT_NE(inst->getTypespec(), nullptr);
  EXPECT_EQ(inst->getTypespec()->getName(), "tlul_fifo_sync");
  const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr);
  ASSERT_NE(mt->getParamAssigns(), nullptr);
  ASSERT_EQ(mt->getParamAssigns()->size(), 1u);
  const hldb::ParamAssign *const pa = mt->getParamAssigns()->at(0);
  ASSERT_NE(pa, nullptr);
  EXPECT_TRUE(pa->getConnByName()) << "23.10.2.2: .ReqDepth(...) is a by-name parameter value assignment";
  EXPECT_TRUE(pa->getOverridden());
  ASSERT_NE(pa->getLhs(), nullptr);
  EXPECT_EQ(pa->getLhs()->getName(), "ReqDepth");
  checkPosIndexed(pa->getRhs(), ".ReqDepth(DReqDepth[0+:4])");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
