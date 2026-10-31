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

// Validates the HLDB model built for tests/PkgImportFunc/dut.sv:
//
//   package prim_util_pkg;
//      function automatic int get_5();
//         int result = 5;
//         return result;
//      endfunction // get_5
//   endpackage // prim_utilt_pkg
//
//   package lc_ctrl_state_pkg;
//      import prim_util_pkg::get_5;
//   endpackage // lc_ctrl_state_pkg
//
//   module top(output int o);
//      assign o = lc_ctrl_state_pkg::get_5();
//   endmodule; // top
//
// The fixture calls, through a package scope (26.3), a function that the
// named package only imports by name from another package.
//
// What is checked, and why:
//   - prim_util_pkg declares the automatic function get_5 (13.4.2) with a
//     local int result initialized to 5 and a single return statement
//     returning result (13.4.1)
//   - lc_ctrl_state_pkg holds the explicit import prim_util_pkg::get_5
//     (26.3)
//   - top's only port is the output variable o (23.2.2.3)
//   - the continuous assignment targets o; its RHS is the package-scoped
//     name whose prefix binds to the package lc_ctrl_state_pkg and whose
//     last element is a call of get_5 (26.3)
//   - no fatal or syntax diagnostic
//
// What is NOT checked, and why:
//   - Whether 'lc_ctrl_state_pkg::get_5' is legal and which function the call
//     resolves to. 26.6 says "By default, declarations imported into a
//     package are not visible by way of subsequent imports of that package",
//     but this is a package-scoped reference, not an import, and the standard
//     does not plainly settle whether a scoped reference can reach an
//     imported name. HLC leaves the call unbound and reports nothing. Left for
//     a human to decide; RhsCallsGet5ThroughLcCtrlStatePkg pins the rest.
//   - The value 5 the call returns is a runtime/elaboration property.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/import_typespec.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/return_stmt.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PkgImportFuncTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PkgImportFunc.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
  }

  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(PkgImportFuncTest, PrimUtilPkgDeclaresGet5) {
  const hldb::Package *const p = getPkg("prim_util_pkg");
  ASSERT_NE(p, nullptr);
  ASSERT_NE(p->getTaskFuncs(), nullptr);
  const hldb::Function *const f = hldb::findByName<hldb::Function>("get_5", p->getTaskFuncs());
  ASSERT_NE(f, nullptr) << "13.4: the package declares the function get_5";
  EXPECT_TRUE(f->getAutomatic()) << "13.4.2: 'function automatic'";
  ASSERT_NE(f->getVariables(), nullptr);
  const hldb::Variable *const result = hldb::findByName<hldb::Variable>("result", f->getVariables());
  ASSERT_NE(result, nullptr) << "the function declares result";
  const hldb::Constant *const init = any_cast<hldb::Constant>(result->getValue());
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getDecompile(), "5");
  const hldb::ReturnStmt *const ret = any_cast<hldb::ReturnStmt>(f->getStmt());
  ASSERT_NE(ret, nullptr) << "13.4.1: the body is one return statement";
  const hldb::RefObj *const r = any_cast<hldb::RefObj>(ret->getCondition());
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(r->getActual(), result) << "the return value is the local result";
}

TEST_F(PkgImportFuncTest, LcCtrlStatePkgImportsGet5ByName) {
  const hldb::Package *const p = getPkg("lc_ctrl_state_pkg");
  ASSERT_NE(p, nullptr);
  const hldb::ImportTypespec *const imp = hldb::findByName<hldb::ImportTypespec>("prim_util_pkg", p->getTypespecs());
  ASSERT_NE(imp, nullptr) << "26.3: 'import prim_util_pkg::get_5'";
  ASSERT_NE(imp->getItem(), nullptr);
  EXPECT_EQ(imp->getItem()->getDecompile(), "get_5") << "26.3: an explicit import of one name";
}

TEST_F(PkgImportFuncTest, OIsTheOnlyPortAnOutputVariable) {
  const hldb::Module *const m = getTop();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 1u);
  EXPECT_EQ(m->getPorts()->at(0)->getName(), "o");
  EXPECT_EQ(m->getPorts()->at(0)->getDirection(), vpiOutput);
  EXPECT_NE(hldb::findByName<hldb::Variable>("o", m->getVariables()), nullptr) << "23.2.2.3: 'output int'";
}

TEST_F(PkgImportFuncTest, RhsCallsGet5ThroughLcCtrlStatePkg) {
  const hldb::Module *const m = getTop();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getContAssigns(), nullptr);
  ASSERT_EQ(m->getContAssigns()->size(), 1u) << "10.3: one continuous assignment";
  const hldb::ContAssign *const ca = m->getContAssigns()->at(0);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual(), hldb::findByName<hldb::Variable>("o", m->getVariables()));
  const hldb::RefObj *const rhs = ca->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "26.3: a package-scoped name";
  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u);
  const hldb::RefObj *const pkg = any_cast<hldb::RefObj>(rhs->getPathElems()->at(0));
  ASSERT_NE(pkg, nullptr);
  EXPECT_EQ(pkg->getActual(), getPkg("lc_ctrl_state_pkg")) << "26.3: the scope is the package";
  const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(rhs->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "13.4: a function call";
  EXPECT_EQ(call->getName(), "get_5");
  EXPECT_TRUE((call->getArguments() == nullptr) || call->getArguments()->empty()) << "get_5() has no argument";
}

TEST_F(PkgImportFuncTest, NoFatalOrSyntaxDiagnostics) {
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
