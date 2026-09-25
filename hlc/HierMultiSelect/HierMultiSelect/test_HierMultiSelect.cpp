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

// Tests for HierMultiSelect/dut.sv:
//   module dm_csrs ();
//     assign dmi_req_i.addr1 = dmi_req_i.addr2 ;
//     assign kmac_mask_o[8*i+:8] = {8{keymgr_data_i.strb[i]}};
//     assign o = keymgr_key_i.key[0][1 * 32 +: 32]; // Multi select
//     assign sram_otp_key_o[2-2].nonce = 1;
//     logic c = a[0].source[6 -: 2];
//     byte o = b.and;
//   endmodule
//
// None of "dmi_req_i", "kmac_mask_o", "keymgr_data_i", "keymgr_key_i", "i",
// "sram_otp_key_o" or "a" are declared anywhere in this file, and each is
// used with dotted member access and/or an indexed part-select
// ("[base +: width]" / "[base -: width]") on top -- constructs that
// presuppose an interface- or struct-typed declaration that simply is not
// present. Per IEEE 1800-2023 ss.23.8 "Overriding..."/ss.6.10, dot-member
// access is only legal on an identifier whose type (interface, struct,
// etc.) declares that member; with no such declaration, name resolution
// for "dmi_req_i", "keymgr_data_i", "keymgr_key_i", "kmac_mask_o",
// "sram_otp_key_o" and "a" must fail. This test confirms the module still
// parses (structural existence, despite the binding failures) and that
// HLC reports a binding failure for each of these concrete undeclared
// names.
//
// "byte o = b.and;" additionally uses "and" -- a reserved keyword per IEEE
// 1800-2023 ss.5.6, Table 5-1 -- as a struct member name after ".". Per
// ss.5.6, keywords are reserved and may not be used as ordinary identifiers
// (escaped identifiers such as "\and" are the sanctioned way to use a
// keyword spelling as a name), so this is expected to be at least a failed
// bind for "b" (itself undeclared) independent of whatever HLC does with
// the "and" token; no assumption is made here about how HLC specifically
// diagnoses the keyword-as-member-name spelling itself.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>

namespace hlc {

class HierMultiSelectTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierMultiSelect.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getDut() {
    return hldb::findByName<hldb::Module>("dm_csrs", m_design->getAllModules());
  }
};

// --- module still parses despite the binding failures below --------------

TEST_F(HierMultiSelectTest, ModuleDmCsrsExists) { EXPECT_NE(getDut(), nullptr); }

// --- undeclared interface-/struct-like identifiers fail to bind ----------

TEST_F(HierMultiSelectTest, DmiReqIFailsToBind) {
  ASSERT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, std::string_view{"dmi_req_i"}), nullptr)
      << "'dmi_req_i' is never declared, so 'dmi_req_i.addr1'/'dmi_req_i.addr2' cannot resolve; per "
         "IEEE 1800-2023 ss.6.10 there is no implicit-declaration rule that can conjure an "
         "interface/struct-typed identifier whose members ('addr1', 'addr2') would make this legal.";
}

TEST_F(HierMultiSelectTest, KeymgrDataIFailsToBind) {
  ASSERT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, std::string_view{"keymgr_data_i"}), nullptr)
      << "'keymgr_data_i' is never declared, so 'keymgr_data_i.strb[i]' cannot resolve.";
}

TEST_F(HierMultiSelectTest, KeymgrKeyIFailsToBind) {
  ASSERT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, std::string_view{"keymgr_key_i"}), nullptr)
      << "'keymgr_key_i' is never declared, so 'keymgr_key_i.key[0][1 * 32 +: 32]' cannot resolve.";
}

TEST_F(HierMultiSelectTest, KmacMaskOFailsToBind) {
  ASSERT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, std::string_view{"kmac_mask_o"}), nullptr)
      << "'kmac_mask_o' is never declared, so the indexed part-select 'kmac_mask_o[8*i+:8]' cannot "
         "resolve.";
}

TEST_F(HierMultiSelectTest, SramOtpKeyOFailsToBind) {
  ASSERT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, std::string_view{"sram_otp_key_o"}), nullptr)
      << "'sram_otp_key_o' is never declared, so 'sram_otp_key_o[2-2].nonce' cannot resolve.";
}

TEST_F(HierMultiSelectTest, AFailsToBind) {
  ASSERT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, std::string_view{"a"}), nullptr)
      << "'a' is never declared, so 'a[0].source[6 -: 2]' cannot resolve.";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
