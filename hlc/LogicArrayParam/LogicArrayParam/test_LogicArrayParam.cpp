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

// Tests for tests/LogicArrayParam/dut.sv (the trailing block comment holds
// a disabled 'rggen_or_reducer' module and must contribute nothing):
//
//   package alert_handler_reg_pkg;
//     parameter int NAlerts = 20;
//     parameter logic [NAlerts-1:0] AsyncOn = 20'b01100001100000001111;
//   endpackage
//
//   package alert_pkg;
//     localparam int unsigned      NAlerts   = alert_handler_reg_pkg::NAlerts;
//     localparam bit [NAlerts-1:0] AsyncOn   = alert_handler_reg_pkg::AsyncOn;
//   endpackage
//
//   module M();
//   endmodule
//
//   module alert_handler
//     import alert_pkg::*;
//   #() ();
//     for (genvar k = 0 ; k < NAlerts ; k++) begin : gen_alerts
//       localparam debug = AsyncOn[k];
//       if (debug) begin
//         M u();
//       end
//     end
//   endmodule
//
// -- rules under test ---------------------------------------------------
//
// IEEE 1800-2023 Sec 6.20.1: "All param_assignments appearing within a
//   generate block, package, or compilation-unit scope shall become
//   localparam declarations" -- the two 'parameter's in
//   alert_handler_reg_pkg and 'debug' in gen_alerts are all localparams.
// IEEE 1800-2023 Sec 6.11 / 6.11.3: 'int' is a signed 32-bit 2-state type;
//   'int unsigned' is the same type made unsigned. 'logic [NAlerts-1:0]' is
//   a 4-state vector, 'bit [NAlerts-1:0]' a 2-state vector, whose left
//   bound is the constant expression 'NAlerts-1' (Sec 7.4.1).
// IEEE 1800-2023 Sec 5.7.1: '20'b01100001100000001111' is a 20-bit sized
//   binary literal.
// IEEE 1800-2023 Sec 26.3: 'alert_handler_reg_pkg::NAlerts' / '::AsyncOn'
//   reference the parameters of package alert_handler_reg_pkg explicitly;
//   'NAlerts' inside alert_pkg's own range refers to alert_pkg::NAlerts.
// IEEE 1800-2023 Sec 26.3 / 26.4: 'import alert_pkg::*;' in the module
//   header makes alert_pkg's identifiers visible in alert_handler, so
//   'NAlerts' in the loop condition and 'AsyncOn' in the generate block
//   resolve to alert_pkg's localparams (not alert_handler_reg_pkg's).
// IEEE 1800-2023 Sec 27.4 "Loop generate constructs": the 'for (genvar k
//   = 0; k < NAlerts; k++) begin : gen_alerts' is a loop generate with
//   genvar 'k', condition 'k < NAlerts', increment 'k++', and named
//   generate block 'gen_alerts' (which contains the localparam and a
//   conditional generate, Sec 27.5, instantiating M as 'u').
//
// Not checked: elaborated values (this compile does not elaborate) and the
// implicit type of the untyped 'localparam debug' (Sec 6.20.2 takes it from
// the final value, which needs elaboration).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class LogicArrayParamTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LogicArrayParam.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
  }

  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Parameter *findParam(const hldb::Scope *s, std::string_view name) {
    if (s == nullptr || s->getParameters() == nullptr) return nullptr;
    for (const hldb::Any *const p : *s->getParameters()) {
      const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
      if (param != nullptr && param->getName() == name) return param;
    }
    return nullptr;
  }

  static const hldb::ParamAssign *findParamAssign(const hldb::Scope *s, std::string_view name) {
    return (s == nullptr) ? nullptr : hldb::findByName<hldb::ParamAssign>(name, s->getParamAssigns());
  }

  static const hldb::GenFor *getGenFor() {
    const hldb::Module *const m = getModule("alert_handler");
    if (m == nullptr || m->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      if (const hldb::GenFor *const gf = any_cast<hldb::GenFor>(stmt)) return gf;
    }
    return nullptr;
  }

  static const hldb::Begin *getGenBlock() {
    const hldb::GenFor *const gf = getGenFor();
    return (gf == nullptr) ? nullptr : gf->getStmt<hldb::Begin>();
  }

  // Checks that 'r' is '[NAlerts-1:0]' with 'NAlerts' bound to 'target'.
  static void expectNAlertsMinus1To0(const hldb::Range *r, const hldb::Parameter *target) {
    ASSERT_NE(r, nullptr);
    const hldb::Operation *const left = r->getLeftExpr<hldb::Operation>();
    ASSERT_NE(left, nullptr) << "left bound 'NAlerts-1' should be an Operation";
    EXPECT_EQ(left->getOpType(), vpiSubOp);
    ASSERT_NE(left->getOperands(), nullptr);
    ASSERT_EQ(left->getOperands()->size(), 2u);
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(left->getOperands()->at(0));
    ASSERT_NE(ref, nullptr);
    EXPECT_EQ(ref->getName(), std::string_view("NAlerts"));
    ASSERT_NE(ref->getActual(), nullptr);
    EXPECT_EQ(ref->getActual(), target);
    const hldb::Constant *const one = any_cast<hldb::Constant>(left->getOperands()->at(1));
    ASSERT_NE(one, nullptr);
    EXPECT_EQ(one->getDecompile(), std::string_view("1"));
    const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(right->getDecompile(), std::string_view("0"));
  }
};

// ---------------------------------------------------------------------------
// Existence (the commented-out module must not exist)
// ---------------------------------------------------------------------------

TEST_F(LogicArrayParamTest, PackagesAndModulesExist) {
  EXPECT_NE(getPkg("alert_handler_reg_pkg"), nullptr);
  EXPECT_NE(getPkg("alert_pkg"), nullptr);
  EXPECT_NE(getModule("M"), nullptr);
  EXPECT_NE(getModule("alert_handler"), nullptr);
  EXPECT_EQ(getModule("rggen_or_reducer"), nullptr) << "module inside a block comment must not be compiled";
}

// ---------------------------------------------------------------------------
// alert_handler_reg_pkg -- Sec 6.20.1, 6.11, 5.7.1
// ---------------------------------------------------------------------------

TEST_F(LogicArrayParamTest, RegPkgParametersBecomeLocalparams) {
  const hldb::Package *const pkg = getPkg("alert_handler_reg_pkg");
  ASSERT_NE(pkg, nullptr);
  const hldb::Parameter *const n = findParam(pkg, "NAlerts");
  const hldb::Parameter *const a = findParam(pkg, "AsyncOn");
  ASSERT_NE(n, nullptr);
  ASSERT_NE(a, nullptr);
  EXPECT_TRUE(n->getLocalParam()) << "package 'parameter' is a localparam (Sec 6.20.1)";
  EXPECT_TRUE(a->getLocalParam()) << "package 'parameter' is a localparam (Sec 6.20.1)";
}

TEST_F(LogicArrayParamTest, RegPkgNAlertsIsSignedIntEqual20) {
  const hldb::Package *const pkg = getPkg("alert_handler_reg_pkg");
  ASSERT_NE(pkg, nullptr);
  const hldb::Parameter *const n = findParam(pkg, "NAlerts");
  ASSERT_NE(n, nullptr);
  ASSERT_NE(n->getTypespec(), nullptr);
  ASSERT_NE(n->getTypespec()->getActual(), nullptr);
  ASSERT_EQ(n->getTypespec()->getActual()->getAnyType(), hldb::AnyType::IntTypespec);
  EXPECT_TRUE(n->getTypespec()->getActual<hldb::IntTypespec>()->getSigned()) << "'int' is signed (Sec 6.11.3)";
  const hldb::ParamAssign *const pa = findParamAssign(pkg, "NAlerts");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), std::string_view("20"));
}

TEST_F(LogicArrayParamTest, RegPkgAsyncOnIsLogicNAlertsMinus1To0) {
  const hldb::Package *const pkg = getPkg("alert_handler_reg_pkg");
  ASSERT_NE(pkg, nullptr);
  const hldb::Parameter *const a = findParam(pkg, "AsyncOn");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getTypespec(), nullptr);
  ASSERT_NE(a->getTypespec()->getActual(), nullptr);
  ASSERT_EQ(a->getTypespec()->getActual()->getAnyType(), hldb::AnyType::LogicTypespec);
  const hldb::LogicTypespec *const lts = a->getTypespec()->getActual<hldb::LogicTypespec>();
  EXPECT_FALSE(lts->getSigned());
  ASSERT_NE(lts->getRanges(), nullptr);
  ASSERT_EQ(lts->getRanges()->size(), 1u);
  expectNAlertsMinus1To0(lts->getRanges()->at(0), findParam(pkg, "NAlerts"));
}

TEST_F(LogicArrayParamTest, RegPkgAsyncOnDefaultIs20BitBinaryLiteral) {
  const hldb::ParamAssign *const pa = findParamAssign(getPkg("alert_handler_reg_pkg"), "AsyncOn");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiBinaryConst);
  EXPECT_EQ(c->getSize(), 20);
  EXPECT_EQ(c->getDecompile(), std::string_view("20'b01100001100000001111"));
}

// ---------------------------------------------------------------------------
// alert_pkg -- Sec 6.20.4, 6.11.3, 26.3
// ---------------------------------------------------------------------------

TEST_F(LogicArrayParamTest, AlertPkgNAlertsIsUnsignedIntFromRegPkg) {
  const hldb::Package *const pkg = getPkg("alert_pkg");
  ASSERT_NE(pkg, nullptr);
  const hldb::Parameter *const n = findParam(pkg, "NAlerts");
  ASSERT_NE(n, nullptr);
  EXPECT_TRUE(n->getLocalParam());
  ASSERT_NE(n->getTypespec(), nullptr);
  ASSERT_NE(n->getTypespec()->getActual(), nullptr);
  ASSERT_EQ(n->getTypespec()->getActual()->getAnyType(), hldb::AnyType::IntTypespec);
  EXPECT_FALSE(n->getTypespec()->getActual<hldb::IntTypespec>()->getSigned()) << "'int unsigned' (Sec 6.11.3)";
  const hldb::ParamAssign *const pa = findParamAssign(pkg, "NAlerts");
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const rhs = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getActual(), nullptr);
  EXPECT_EQ(rhs->getActual(), findParam(getPkg("alert_handler_reg_pkg"), "NAlerts"))
      << "'alert_handler_reg_pkg::NAlerts' must bind into that package (Sec 26.3)";
}

TEST_F(LogicArrayParamTest, AlertPkgAsyncOnIsBitNAlertsMinus1To0) {
  const hldb::Package *const pkg = getPkg("alert_pkg");
  ASSERT_NE(pkg, nullptr);
  const hldb::Parameter *const a = findParam(pkg, "AsyncOn");
  ASSERT_NE(a, nullptr);
  EXPECT_TRUE(a->getLocalParam());
  ASSERT_NE(a->getTypespec(), nullptr);
  ASSERT_NE(a->getTypespec()->getActual(), nullptr);
  ASSERT_EQ(a->getTypespec()->getActual()->getAnyType(), hldb::AnyType::BitTypespec);
  const hldb::BitTypespec *const bts = a->getTypespec()->getActual<hldb::BitTypespec>();
  EXPECT_FALSE(bts->getSigned());
  ASSERT_NE(bts->getRanges(), nullptr);
  ASSERT_EQ(bts->getRanges()->size(), 1u);
  expectNAlertsMinus1To0(bts->getRanges()->at(0), findParam(pkg, "NAlerts"));
}

TEST_F(LogicArrayParamTest, AlertPkgAsyncOnValueReferencesRegPkg) {
  const hldb::ParamAssign *const pa = findParamAssign(getPkg("alert_pkg"), "AsyncOn");
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const rhs = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getActual(), nullptr);
  EXPECT_EQ(rhs->getActual(), findParam(getPkg("alert_handler_reg_pkg"), "AsyncOn"))
      << "'alert_handler_reg_pkg::AsyncOn' must bind into that package (Sec 26.3)";
}

// ---------------------------------------------------------------------------
// alert_handler loop generate -- Sec 27.4, 26.3
// ---------------------------------------------------------------------------

TEST_F(LogicArrayParamTest, AlertHandlerHasLoopGenerate) {
  const hldb::Module *const m = getModule("alert_handler");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getGenStmts(), nullptr);
  EXPECT_NE(getGenFor(), nullptr) << "'for (genvar k ...)' loop generate not found";
}

TEST_F(LogicArrayParamTest, LoopConditionKLessThanImportedNAlerts) {
  const hldb::GenFor *const gf = getGenFor();
  ASSERT_NE(gf, nullptr);
  const hldb::Operation *const cond = gf->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiLtOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const k = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  const hldb::RefObj *const n = any_cast<hldb::RefObj>(cond->getOperands()->at(1));
  ASSERT_NE(k, nullptr);
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(k->getName(), std::string_view("k"));
  EXPECT_EQ(n->getName(), std::string_view("NAlerts"));
  ASSERT_NE(n->getActual(), nullptr);
  EXPECT_EQ(n->getActual(), findParam(getPkg("alert_pkg"), "NAlerts"))
      << "'NAlerts' is visible via 'import alert_pkg::*' (Sec 26.3)";
}

TEST_F(LogicArrayParamTest, LoopInitAndIncrement) {
  const hldb::GenFor *const gf = getGenFor();
  ASSERT_NE(gf, nullptr);
  ASSERT_NE(gf->getForInitStmts(), nullptr);
  EXPECT_EQ(gf->getForInitStmts()->size(), 1u);
  ASSERT_NE(gf->getForIncStmts(), nullptr);
  ASSERT_EQ(gf->getForIncStmts()->size(), 1u);
  const hldb::Operation *const inc = any_cast<hldb::Operation>(gf->getForIncStmts()->at(0));
  ASSERT_NE(inc, nullptr);
  EXPECT_EQ(inc->getOpType(), vpiPostIncOp);
}

TEST_F(LogicArrayParamTest, GenerateBlockIsNamedGenAlerts) {
  const hldb::GenFor *const gf = getGenFor();
  ASSERT_NE(gf, nullptr);
  ASSERT_NE(gf->getStmt(), nullptr);
  const hldb::Begin *const b = getGenBlock();
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getName(), std::string_view("gen_alerts"));
}

TEST_F(LogicArrayParamTest, DebugIsLocalparamFromImportedAsyncOnBitSelect) {
  const hldb::Begin *const b = getGenBlock();
  ASSERT_NE(b, nullptr);
  const hldb::Parameter *const debug = findParam(b, "debug");
  ASSERT_NE(debug, nullptr) << "'localparam debug' not found in gen_alerts";
  EXPECT_TRUE(debug->getLocalParam());
  const hldb::ParamAssign *const pa = findParamAssign(b, "debug");
  ASSERT_NE(pa, nullptr);
  const hldb::BitSelect *const sel = pa->getRhs<hldb::BitSelect>();
  ASSERT_NE(sel, nullptr) << "'AsyncOn[k]' should be a BitSelect";
  const hldb::RefObj *const prefix = sel->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  ASSERT_NE(prefix->getActual(), nullptr);
  EXPECT_EQ(prefix->getActual(), findParam(getPkg("alert_pkg"), "AsyncOn"))
      << "'AsyncOn' is visible via 'import alert_pkg::*' (Sec 26.3)";
  const hldb::RefObj *const idx = sel->getIndex<hldb::RefObj>();
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(idx->getName(), std::string_view("k"));
}

TEST_F(LogicArrayParamTest, ConditionalGenerateInstantiatesM) {
  const hldb::Begin *const b = getGenBlock();
  ASSERT_NE(b, nullptr);
  ASSERT_NE(b->getStmts(), nullptr);
  const hldb::GenIf *genIf = nullptr;
  for (const hldb::Any *const s : *b->getStmts()) {
    if (const hldb::GenIf *const gi = any_cast<hldb::GenIf>(s)) genIf = gi;
  }
  ASSERT_NE(genIf, nullptr) << "'if (debug) ...' conditional generate not found (Sec 27.5)";
  const hldb::RefObj *const cond = genIf->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getName(), std::string_view("debug"));
  const hldb::Begin *const body = genIf->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u);
  const hldb::RefInstance *const u = any_cast<hldb::RefInstance>(body->getStmts()->at(0));
  ASSERT_NE(u, nullptr);
  EXPECT_EQ(u->getName(), std::string_view("u"));
  ASSERT_NE(u->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mts = u->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mts, nullptr);
  EXPECT_EQ(mts->getModule(), getModule("M"));
}

// ---------------------------------------------------------------------------
// Diagnostics -- the source is legal
// ---------------------------------------------------------------------------

TEST_F(LogicArrayParamTest, NoIllegalPropertyValueOnGenerateBlock) {
  EXPECT_EQ(findError(ErrorDefinition::HLDB_ILLEGAL_PROPERTY_VALUE, 25, 42), nullptr)
      << "a legal loop generate block must not produce an invalid model";
}

TEST_F(LogicArrayParamTest, CompilerReportsZeroErrors) {
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
