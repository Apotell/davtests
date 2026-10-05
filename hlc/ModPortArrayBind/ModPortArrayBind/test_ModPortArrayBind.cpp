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

// Tests for tests/ModPortArrayBind/dut.sv (tags: ModPortArrayBind)
//
//   interface r5p_bus_if ();
//    logic          vld;  // valid
//    modport  man (
//      output vld
//    );
//   endinterface: r5p_bus_if
//
//   module r5p_bus_dec #(
//     int unsigned BN = 2
//   )(
//     r5p_bus_if.man m[BN-1:0]
//   );
//   genvar i;
//   generate
//   for (i=0; i<BN; i++) begin: gen_loop
//     // forward path
//     assign m[i].vld = s_dec[i] ? 1 : '0;
//   end: gen_loop
//   endgenerate
//   endmodule
//
// What is checked:
//   - interface "r5p_bus_if" declares the scalar logic variable "vld"
//     (Sec 25.3, 6.11)
//   - Sec 25.5: the interface declares modport "man" with exactly one
//     modport port, "vld", of direction output
//   - module "r5p_bus_dec" exists and is named "r5p_bus_dec" (Sec 23.2.1:
//     the module_identifier is the module's name; parameter values are not
//     part of the name)
//   - Sec 6.20.2 / 6.11: parameter "BN" is declared "int unsigned" (an
//     unsigned IntTypespec) with default 2
//   - Sec 23.2.2.2 / 25.5 / 23.3.3.5: the single ANSI port "m" is an
//     interface port whose interface_port_header "r5p_bus_if.man" selects
//     interface r5p_bus_if restricted to modport man, declared as an
//     unpacked array with dimension [BN-1:0]:
//       * its typespec is an ArrayTypespec whose range is [BN-1:0]
//         (left = Operation(vpiSubOp) over RefObj BN bound to the Parameter
//         and Constant 1; right = Constant 0)
//       * its element typespec resolves to an InterfaceTypespec for
//         r5p_bus_if with modport "man"
//   - Sec 23.2.2.2: interface_port_header has no port_direction, so no
//     COMP_PORT_MISSING_DIRECTION is reported for "m"
//   - Sec 25.5: "man" (the modport) and "vld" (accessed through m[i]) are
//     declared in r5p_bus_if, so no COMP_FAILED_TO_BIND for either
//   - Sec 6.10: "s_dec" is never declared. It appears only on the RHS of a
//     continuous assignment, which is not one of the implicit-net contexts
//     (port expression, instance port connection, continuous-assignment
//     LHS), so the reference must fail to bind: COMP_FAILED_TO_BIND
//     "s_dec"
//   - Sec 27.4: the loop generate construct (inside the optional
//     generate region, Sec 27.3) is a GenFor with init "i=0", condition
//     Operation(vpiLtOp) over i and BN, increment Operation(vpiPostIncOp)
//     over i, and body the generate block named "gen_loop"
//   - Sec 10.3.2 / 11.4.11: gen_loop holds one continuous assignment whose
//     RHS is Operation(vpiConditionOp) with 3 operands; the third operand
//     is the unbased unsized literal "'0" (Sec 5.7.1)
//
// What is NOT checked and why:
//   - The unrolled generate scopes gen_loop[0], gen_loop[1] and the
//     per-element binding of m[i]: elaboration results; this compile stops
//     before elaboration.
//   - The exact node shape for "m[i].vld" (hierarchical select through an
//     interface-array port): a modeling choice not mandated by the
//     standard; only its presence is checked.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/gen_for.h>
#include <hldb/gen_region.h>
#include <hldb/int_typespec.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/modport.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ModPortArrayBindTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ModPortArrayBind.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getIf() {
    return hldb::findByDefName<hldb::Interface>("r5p_bus_if", m_design->getAllInterfaces());
  }

  static const hldb::Modport *getMan() {
    const hldb::Interface *const itf = getIf();
    if (itf == nullptr || itf->getModports() == nullptr) return nullptr;
    return hldb::findByName<hldb::Modport>("man", itf->getModports());
  }

  static const hldb::Module *getDec() {
    return hldb::findByDefName<hldb::Module>("r5p_bus_dec", m_design->getAllModules());
  }

  static const hldb::Parameter *getBN() {
    const hldb::Module *const dec = getDec();
    if (dec == nullptr || dec->getParameters() == nullptr) return nullptr;
    for (const hldb::Any *const p : *dec->getParameters()) {
      const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
      if (param != nullptr && param->getName() == "BN") return param;
    }
    return nullptr;
  }

  static const hldb::Port *getPortM() {
    const hldb::Module *const dec = getDec();
    if (dec == nullptr || dec->getPorts() == nullptr) return nullptr;
    return hldb::findByName<hldb::Port>("m", dec->getPorts());
  }

  static const hldb::ArrayTypespec *getPortMArray() {
    const hldb::Port *const m = getPortM();
    if (m == nullptr || m->getTypespec() == nullptr) return nullptr;
    return m->getTypespec()->getActual<hldb::ArrayTypespec>();
  }

  static const hldb::GenFor *getGenFor() {
    const hldb::Module *const dec = getDec();
    if (dec == nullptr || dec->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const s : *dec->getGenStmts()) {
      if (const hldb::GenFor *const gf = any_cast<hldb::GenFor>(s)) return gf;
      if (const hldb::GenRegion *const gr = any_cast<hldb::GenRegion>(s)) {
        if (const hldb::GenFor *const gf = gr->getStmt<hldb::GenFor>()) return gf;
      }
    }
    return nullptr;
  }

  static const hldb::Begin *getGenLoop() {
    const hldb::GenFor *const gf = getGenFor();
    return (gf == nullptr) ? nullptr : gf->getStmt<hldb::Begin>();
  }
};

// ---------------------------------------------------------------------------
// Interface r5p_bus_if and modport man -- Sec 25.3, 25.5
// ---------------------------------------------------------------------------

TEST_F(ModPortArrayBindTest, InterfaceDeclaresScalarLogicVld) {
  const hldb::Interface *const itf = getIf();
  ASSERT_NE(itf, nullptr) << "interface 'r5p_bus_if' not found";
  ASSERT_NE(itf->getVariables(), nullptr);
  const hldb::Variable *const vld = hldb::findByName<hldb::Variable>("vld", itf->getVariables());
  ASSERT_NE(vld, nullptr) << "'logic vld' not found";
  ASSERT_NE(vld->getTypespec(), nullptr);
  ASSERT_NE(vld->getTypespec()->getActual(), nullptr);
  EXPECT_EQ(vld->getTypespec()->getActual()->getAnyType(), hldb::AnyType::LogicTypespec);
  EXPECT_TRUE(vld->getScalar()) << "'vld' has no packed dimension";
}

TEST_F(ModPortArrayBindTest, ModportManHasOutputVld) {
  const hldb::Modport *const man = getMan();
  ASSERT_NE(man, nullptr) << "modport 'man' not found";
  EXPECT_EQ(man->getInterface(), getIf());
  ASSERT_NE(man->getIODecls(), nullptr);
  ASSERT_EQ(man->getIODecls()->size(), 1u);
  const hldb::IODecl *const io = man->getIODecls()->at(0);
  ASSERT_NE(io, nullptr);
  EXPECT_EQ(io->getName(), "vld");
  EXPECT_EQ(io->getDirection(), vpiOutput) << "Sec 25.5: 'output vld'";
}

// ---------------------------------------------------------------------------
// Module r5p_bus_dec and parameter BN
// ---------------------------------------------------------------------------

TEST_F(ModPortArrayBindTest, ModuleNameIsPlainIdentifier) {
  const hldb::Module *const dec = getDec();
  ASSERT_NE(dec, nullptr) << "module 'r5p_bus_dec' not found";
  EXPECT_EQ(dec->getName(), "r5p_bus_dec") << "Sec 23.2.1: the module name is its module_identifier";
}

TEST_F(ModPortArrayBindTest, ParameterBNIsIntUnsignedDefaultTwo) {
  const hldb::Module *const dec = getDec();
  ASSERT_NE(dec, nullptr);
  const hldb::Parameter *const bn = getBN();
  ASSERT_NE(bn, nullptr) << "parameter 'BN' not found";
  ASSERT_NE(bn->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = bn->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "'BN' is declared 'int unsigned'";
  EXPECT_FALSE(it->getSigned()) << "Sec 6.11: 'unsigned' qualifier";

  const hldb::ParamAssign *const pa = hldb::findByName<hldb::ParamAssign>("BN", hldb::getParamAssigns(dec));
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "2");
}

// ---------------------------------------------------------------------------
// Port m: r5p_bus_if.man m[BN-1:0] -- Sec 23.2.2.2, 25.5, 23.3.3.5
// ---------------------------------------------------------------------------

TEST_F(ModPortArrayBindTest, SinglePortMIsArray) {
  const hldb::Module *const dec = getDec();
  ASSERT_NE(dec, nullptr);
  ASSERT_NE(dec->getPorts(), nullptr);
  EXPECT_EQ(dec->getPorts()->size(), 1u);
  const hldb::Port *const m = getPortM();
  ASSERT_NE(m, nullptr) << "port 'm' not found";
  ASSERT_NE(m->getTypespec(), nullptr);
  EXPECT_NE(getPortMArray(), nullptr) << "'m[BN-1:0]' declares an unpacked array of interface ports";
}

TEST_F(ModPortArrayBindTest, PortMRangeIsBNMinusOneDownToZero) {
  const hldb::ArrayTypespec *const at = getPortMArray();
  ASSERT_NE(at, nullptr);
  const hldb::Range *const r = at->getRange();
  ASSERT_NE(r, nullptr);

  const hldb::Operation *const left = r->getLeftExpr<hldb::Operation>();
  ASSERT_NE(left, nullptr) << "left bound 'BN-1' is an Operation";
  EXPECT_EQ(left->getOpType(), vpiSubOp);
  ASSERT_NE(left->getOperands(), nullptr);
  ASSERT_EQ(left->getOperands()->size(), 2u);
  const hldb::RefObj *const bn = any_cast<hldb::RefObj>(left->getOperands()->at(0));
  ASSERT_NE(bn, nullptr);
  EXPECT_EQ(bn->getName(), "BN");
  ASSERT_NE(getBN(), nullptr);
  EXPECT_EQ(bn->getActual(), getBN());
  const hldb::Constant *const one = any_cast<hldb::Constant>(left->getOperands()->at(1));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");

  const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(ModPortArrayBindTest, PortMElementIsInterfaceRestrictedToModportMan) {
  const hldb::ArrayTypespec *const at = getPortMArray();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  ASSERT_NE(at->getElemTypespec()->getActual(), nullptr)
      << "Sec 25.5: 'r5p_bus_if.man' must resolve to interface r5p_bus_if / modport man";
  const hldb::InterfaceTypespec *const it = at->getElemTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(it, nullptr) << "element type of 'm' is an interface type";
  EXPECT_EQ(it->getInterface(), getIf());
  ASSERT_NE(getMan(), nullptr);
  EXPECT_EQ(it->getModport(), getMan()) << "element type is restricted to modport 'man'";
}

TEST_F(ModPortArrayBindTest, InterfacePortNeedsNoDirection) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_PORT_MISSING_DIRECTION, "m"), nullptr)
      << "Sec 23.2.2.2: interface_port_header has no port_direction";
}

TEST_F(ModPortArrayBindTest, ModportAndMemberBind) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "man"), nullptr)
      << "Sec 25.5: 'man' is a modport of r5p_bus_if";
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "vld"), nullptr)
      << "Sec 25.5: 'vld' is a member of r5p_bus_if listed in modport 'man'";
}

TEST_F(ModPortArrayBindTest, UndeclaredSDecFailsToBind) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "s_dec"), nullptr)
      << "Sec 6.10: an undeclared identifier on a continuous-assignment RHS is not an implicit net";
}

// ---------------------------------------------------------------------------
// Sec 27.4: for (i=0; i<BN; i++) begin: gen_loop ... end
// ---------------------------------------------------------------------------

TEST_F(ModPortArrayBindTest, GenForHeader) {
  const hldb::GenFor *const gf = getGenFor();
  ASSERT_NE(gf, nullptr) << "loop generate construct not found";

  ASSERT_NE(gf->getForInitStmts(), nullptr);
  ASSERT_EQ(gf->getForInitStmts()->size(), 1u);
  const hldb::Assignment *const init = any_cast<hldb::Assignment>(gf->getForInitStmts()->at(0));
  ASSERT_NE(init, nullptr) << "'i=0' is an assignment";
  const hldb::Constant *const zero = init->getRhs<hldb::Constant>();
  ASSERT_NE(zero, nullptr);
  EXPECT_EQ(zero->getDecompile(), "0");

  const hldb::Operation *const cond = gf->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiLtOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const ci = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  const hldb::RefObj *const cbn = any_cast<hldb::RefObj>(cond->getOperands()->at(1));
  ASSERT_NE(ci, nullptr);
  ASSERT_NE(cbn, nullptr);
  EXPECT_EQ(ci->getName(), "i");
  EXPECT_EQ(cbn->getName(), "BN");
  ASSERT_NE(getBN(), nullptr);
  EXPECT_EQ(cbn->getActual(), getBN());

  ASSERT_NE(gf->getForIncStmts(), nullptr);
  ASSERT_EQ(gf->getForIncStmts()->size(), 1u);
  const hldb::Operation *const inc = any_cast<hldb::Operation>(gf->getForIncStmts()->at(0));
  ASSERT_NE(inc, nullptr) << "'i++' is an Operation";
  EXPECT_EQ(inc->getOpType(), vpiPostIncOp);
}

TEST_F(ModPortArrayBindTest, GenForBodyIsNamedBlockGenLoop) {
  const hldb::GenFor *const gf = getGenFor();
  ASSERT_NE(gf, nullptr);
  ASSERT_NE(gf->getStmt(), nullptr);
  const hldb::Begin *const blk = getGenLoop();
  ASSERT_NE(blk, nullptr) << "generate block body not found";
  EXPECT_EQ(blk->getName(), "gen_loop");
  EXPECT_EQ(blk->getEndLabel(), "gen_loop");
}

TEST_F(ModPortArrayBindTest, GenLoopHoldsConditionalContAssign) {
  const hldb::Begin *const blk = getGenLoop();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 1u);
  const hldb::ContAssign *const ca = any_cast<hldb::ContAssign>(blk->getStmts()->at(0));
  ASSERT_NE(ca, nullptr) << "'assign m[i].vld = ...' is a continuous assignment";
  EXPECT_NE(ca->getLhs(), nullptr);

  const hldb::Operation *const rhs = ca->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getOpType(), vpiConditionOp) << "Sec 11.4.11: '?:'";
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 3u);
  const hldb::Constant *const t = any_cast<hldb::Constant>(rhs->getOperands()->at(1));
  const hldb::Constant *const f = any_cast<hldb::Constant>(rhs->getOperands()->at(2));
  ASSERT_NE(t, nullptr);
  ASSERT_NE(f, nullptr);
  EXPECT_EQ(t->getDecompile(), "1");
  EXPECT_EQ(f->getDecompile(), "'0") << "Sec 5.7.1: unbased unsized literal";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
