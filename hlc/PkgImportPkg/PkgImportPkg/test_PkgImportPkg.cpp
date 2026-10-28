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

// Validates the HLDB model built for tests/PkgImportPkg/dut.sv:
//
//   package otp_ctrl_pkg;
//     typedef struct packed {
//       logic [1:0] pwr_seq;
//     } otp_ast_req_t;
//   endpackage : otp_ctrl_pkg
//
//   package otp_ctrl_part_pkg;
//     import otp_ctrl_pkg::*;
//   endpackage : otp_ctrl_part_pkg
//
//   module otp_ctrl
//     import otp_ctrl_pkg::*;
//   (
//     output otp_ast_req_t otp_ast_pwr_seq_o,
//     input  otp_ast_rsp_t otp_ast_pwr_seq_h_i
//   );
//   endmodule : otp_ctrl
//
// The fixture imports a package into another package (26.3) and into a
// module header (26.4), where the import makes otp_ast_req_t visible in the
// list of ports. otp_ast_rsp_t is declared nowhere.
//
// What is checked, and why:
//   - otp_ctrl_pkg declares the typedef otp_ast_req_t (6.18)
//   - otp_ctrl_part_pkg and otp_ctrl each hold a wildcard import of
//     otp_ctrl_pkg (26.3, 26.4)
//   - otp_ctrl has 2 ports in list order: otp_ast_pwr_seq_o (output) and
//     otp_ast_pwr_seq_h_i (input) (23.2.2.2)
//   - otp_ast_pwr_seq_o's type binds to otp_ctrl_pkg's otp_ast_req_t through
//     the header import (26.4)
//   - the undeclared otp_ast_rsp_t is reported as an error (6.18)
//   - no fatal or syntax diagnostic
//
// KNOWN COMPILER BUG (undeclared type only warned), not a defect in this
// test: HLC reports the undeclared otp_ast_rsp_t of the port
// otp_ast_pwr_seq_h_i with a "Failed to bind" warning only, although 6.18
// says "The declaration of a user-defined data type shall precede any
// reference to its type_identifier." UndeclaredOtpAstRspTIsAnError is
// expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// What is NOT checked, and why:
//   - Which error identifier is used, and its wording: tool conventions.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/import_typespec.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/port.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PkgImportPkgTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PkgImportPkg.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Whether a diagnostic of error severity names 'symbol'. Which error
  // identifier HLC picks for it is a tool convention, so any is accepted.
  bool reportsErrorNaming(std::string_view symbol) {
    for (const ErrorDefinition::ErrorMap::value_type &entry : ErrorDefinition::getErrorInfoMap()) {
      if ((entry.second.m_severity == ErrorDefinition::ERROR) && (findError(entry.first, symbol) != nullptr)) {
        return true;
      }
    }
    return false;
  }

  static const hldb::Package *getPkg(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
  }

  static const hldb::Module *getOtpCtrl() {
    return hldb::findByDefName<hldb::Module>("otp_ctrl", m_design->getAllModules());
  }

  static void ExpectWildcardImport(const hldb::TypespecCollection *ts, std::string_view where) {
    const hldb::ImportTypespec *const imp = hldb::findByName<hldb::ImportTypespec>("otp_ctrl_pkg", ts);
    ASSERT_NE(imp, nullptr) << "26.3: " << where << " imports otp_ctrl_pkg";
    ASSERT_NE(imp->getItem(), nullptr);
    EXPECT_EQ(imp->getItem()->getDecompile(), "*") << "26.3: a wildcard import";
  }
};

TEST_F(PkgImportPkgTest, OtpCtrlPkgDeclaresOtpAstReqT) {
  const hldb::Package *const p = getPkg("otp_ctrl_pkg");
  ASSERT_NE(p, nullptr);
  EXPECT_NE(hldb::findByName<hldb::TypedefTypespec>("otp_ast_req_t", p->getTypespecs()), nullptr) << "6.18";
}

TEST_F(PkgImportPkgTest, PartPkgAndModuleImportOtpCtrlPkg) {
  const hldb::Package *const part = getPkg("otp_ctrl_part_pkg");
  ASSERT_NE(part, nullptr);
  ExpectWildcardImport(part->getTypespecs(), "otp_ctrl_part_pkg");
  ASSERT_NE(getOtpCtrl(), nullptr);
  ExpectWildcardImport(getOtpCtrl()->getTypespecs(), "otp_ctrl's header (26.4)");
}

TEST_F(PkgImportPkgTest, PortsAreOutputThenInput) {
  const hldb::Module *const m = getOtpCtrl();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 2u) << "23.2.2.2: 2 ports";
  EXPECT_EQ(m->getPorts()->at(0)->getName(), "otp_ast_pwr_seq_o");
  EXPECT_EQ(m->getPorts()->at(0)->getDirection(), vpiOutput);
  EXPECT_EQ(m->getPorts()->at(1)->getName(), "otp_ast_pwr_seq_h_i");
  EXPECT_EQ(m->getPorts()->at(1)->getDirection(), vpiInput);
}

TEST_F(PkgImportPkgTest, OutputTypeBindsThroughHeaderImport) {
  const hldb::Module *const m = getOtpCtrl();
  ASSERT_NE(m, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("otp_ast_pwr_seq_o", m->getVariables());
  ASSERT_NE(v, nullptr) << "23.2.2.3: an output of a data type is a variable";
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::Package *const p = getPkg("otp_ctrl_pkg");
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(v->getTypespec()->getActual(), hldb::findByName<hldb::TypedefTypespec>("otp_ast_req_t", p->getTypespecs()))
      << "26.4: the header import makes otp_ast_req_t visible in the list of ports";
}

TEST_F(PkgImportPkgTest, NoFatalOrSyntaxDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (undeclared
// type only warned) in the file header.
TEST_F(PkgImportPkgTest, UndeclaredOtpAstRspTIsAnError) {
  EXPECT_TRUE(reportsErrorNaming("otp_ast_rsp_t"))
      << "6.18: The declaration of a user-defined data type shall precede any reference to its type_identifier";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
