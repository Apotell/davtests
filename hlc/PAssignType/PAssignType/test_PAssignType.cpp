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

// Validates the HLDB model built for tests/PAssignType/dut.sv:
//
//   package aes_pkg;
//   typedef struct packed {
//     logic[31:0] manual_operation;
//   } ctrl_reg_t;
//
//   parameter ctrl_reg_t CTRL_RESET = '{
//     manual_operation: '0
//   };
//   endpackage
//
//   module subreg #(
//     parameter logic [31:0] RESVAL = '0
//   ) ();
//   endmodule
//
//   module shadow #(
//     parameter logic [31:0] RESVAL = '0
//   ) ();
//     subreg #(.RESVAL(RESVAL)) commit_reg();
//   endmodule
//
//   module aes_core;
//     import aes_pkg::*;
//     ctrl_reg_t ctrl_q;
//     shadow #(.RESVAL(CTRL_RESET)) shadow_reg();
//   endmodule // aes_core
//
// The fixture passes a structure-typed package parameter, CTRL_RESET, to a
// logic [31:0] parameter through two levels of named parameter overrides
// (23.10.2.2). A packed structure is assignment compatible with a packed
// vector of the same width (7.2.1, 6.22.2), so the overrides are legal.
//
// What is checked, and why:
//   - CTRL_RESET is a package parameter, hence local (6.20.1), of type
//     ctrl_reg_t, the package typedef
//   - subreg and shadow each declare RESVAL of type logic [31:0] with
//     default '0; RESVAL is a parameter port, so it is not local (6.20.4)
//   - shadow instantiates commit_reg of subreg; '.RESVAL(RESVAL)' binds its
//     name to subreg's RESVAL and its value to shadow's own RESVAL
//     (23.10.2.2)
//   - aes_core imports aes_pkg::*, declares ctrl_q of type ctrl_reg_t, and
//     instantiates shadow_reg of shadow; '.RESVAL(CTRL_RESET)' binds its
//     name to shadow's RESVAL and its value to the package's CTRL_RESET
//     through the wildcard import (26.3)
//   - no fatal, syntax or error diagnostic: the file is legal
//
// Model convention: an unelaborated instance's overrides are held on the
// ModuleTypespec its RefInstance names.
//
// What is NOT checked, and why:
//   - The reduced value each instance receives (32'h0): no .hlc in this
//     suite elaborates.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PAssignTypeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PAssignType.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("aes_pkg", m_design->getAllPackages());
  }

  static const hldb::Parameter *getResval(std::string_view module) {
    const hldb::Module *const m = getModule(module);
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>("RESVAL", m->getParameters());
  }

  // The only override written by the only instance in 'module'.
  static const hldb::ParamAssign *getOverride(std::string_view module, std::string_view instance) {
    const hldb::Module *const m = getModule(module);
    if ((m == nullptr) || (m->getRefInstances() == nullptr) || (m->getRefInstances()->size() != 1)) return nullptr;
    const hldb::RefInstance *const ri = m->getRefInstances()->at(0);
    if ((ri->getName() != instance) || (ri->getTypespec() == nullptr)) return nullptr;
    const hldb::ModuleTypespec *const mt = ri->getTypespec()->getActual<hldb::ModuleTypespec>();
    if ((mt == nullptr) || (mt->getParamAssigns() == nullptr) || (mt->getParamAssigns()->size() != 1)) return nullptr;
    return mt->getParamAssigns()->at(0);
  }

  static void ExpectConstant(const hldb::Expr *e, std::string_view text, std::string_view why) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(e);
    ASSERT_NE(c, nullptr) << why << ": a literal";
    EXPECT_EQ(c->getDecompile(), text) << why;
  }

  static void ExpectResvalPort(std::string_view module) {
    const hldb::Parameter *const p = getResval(module);
    ASSERT_NE(p, nullptr) << module << " declares RESVAL";
    EXPECT_FALSE(p->getLocalParam()) << "6.20.4: RESVAL is a parameter port of " << module;
    ASSERT_NE(p->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = p->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << "6.20.2: RESVAL is of type logic [31:0]";
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    ExpectConstant(lt->getRanges()->at(0)->getLeftExpr(), "31", "left bound");
    ExpectConstant(lt->getRanges()->at(0)->getRightExpr(), "0", "right bound");
    const hldb::Module *const m = getModule(module);
    ASSERT_NE(m->getParamAssigns(), nullptr);
    ASSERT_EQ(m->getParamAssigns()->size(), 1u);
    ExpectConstant(m->getParamAssigns()->at(0)->getRhs<hldb::Expr>(), "'0", "the default value");
  }
};

TEST_F(PAssignTypeTest, CtrlResetIsLocalPackageParameterOfCtrlRegT) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "26.2: package aes_pkg";
  const hldb::Parameter *const p = hldb::findByName<hldb::Parameter>("CTRL_RESET", pkg->getParameters());
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getLocalParam()) << "6.20.1: 'parameter' in a package is a local parameter";
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::TypedefTypespec *const td = p->getTypespec()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(td, nullptr) << "6.20.2: CTRL_RESET is of type ctrl_reg_t";
  EXPECT_EQ(td->getName(), "ctrl_reg_t");
}

TEST_F(PAssignTypeTest, SubregResvalIsLogic31To0Port) { ExpectResvalPort("subreg"); }

TEST_F(PAssignTypeTest, ShadowResvalIsLogic31To0Port) { ExpectResvalPort("shadow"); }

TEST_F(PAssignTypeTest, CommitRegPassesShadowsOwnResval) {
  const hldb::ParamAssign *const pa = getOverride("shadow", "commit_reg");
  ASSERT_NE(pa, nullptr) << "23.10.2.2: commit_reg writes one override";
  const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual(), getResval("subreg")) << "23.10.2.2: the name is subreg's parameter";
  const hldb::RefObj *const rhs = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getActual(), getResval("shadow")) << "the value is shadow's own RESVAL";
}

TEST_F(PAssignTypeTest, ShadowRegPassesPackageCtrlReset) {
  const hldb::ParamAssign *const pa = getOverride("aes_core", "shadow_reg");
  ASSERT_NE(pa, nullptr) << "23.10.2.2: shadow_reg writes one override";
  const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual(), getResval("shadow")) << "23.10.2.2: the name is shadow's parameter";
  const hldb::RefObj *const rhs = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getActual(), hldb::findByName<hldb::Parameter>("CTRL_RESET", getPkg()->getParameters()))
      << "26.3: CTRL_RESET is visible through the wildcard import";
}

TEST_F(PAssignTypeTest, CtrlQIsImportedTypedef) {
  const hldb::Module *const m = getModule("aes_core");
  ASSERT_NE(m, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("ctrl_q", m->getVariables());
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::TypedefTypespec *const td = v->getTypespec()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(td, nullptr) << "26.3: ctrl_reg_t is visible through the import";
  EXPECT_EQ(td->getName(), "ctrl_reg_t");
}

TEST_F(PAssignTypeTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
