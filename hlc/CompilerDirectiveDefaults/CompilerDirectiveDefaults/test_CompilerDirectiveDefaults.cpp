/*
 Copyright 2026 Apotell

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

// Compiler directives that set a default on the design elements that follow them
// (tests/CompilerDirectiveDefaults/dut.sv):
//
//   IEEE 1800-2023 Sec 22.3  `resetall sets every directive with a default back to it.
//   IEEE 1800-2023 Sec 22.8  `default_nettype      -> vpiDefNetType   (default wire)
//   IEEE 1800-2023 Sec 22.9  `unconnected_drive    -> vpiUnconnDrive  (default highz)
//                            applies to modules, programs and interfaces;
//                            the most recent of `unconnected_drive / `nounconnected_drive wins.
//   IEEE 1800-2023 Sec 22.10 `celldefine           -> vpiCellInstance (default false)
//                            the most recent of `celldefine / `endcelldefine wins.
//   IEEE 1800-2023 E.4-E.7   `delay_mode_*         -> vpiDefDelayMode (default none)
//
// Every directive is also recorded, in source order, as a GenericDirective on the
// SourceFile's directive list. (`protected/`endprotected and `line are consumed by the
// preprocessor and never reach the design model, so they are not part of this fixture.)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/generic_directive.h>
#include <hldb/hldb_vpi_user.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/program.h>
#include <hldb/source_file.h>
#include <hldb/vpi_user.h>

#include <gtest/gtest.h>

#include <vector>

namespace hlc {

class CompilerDirectiveDefaultsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "CompilerDirectiveDefaults.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::Module *findModule(std::string_view name) const {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  const hldb::SourceFile *getSourceFile() const {
    if ((m_design->getSourceFiles() == nullptr) || m_design->getSourceFiles()->empty()) return nullptr;
    return m_design->getSourceFiles()->front();
  }

  const hldb::GenericDirective *getDirective(size_t index) const {
    const hldb::SourceFile *const sf = getSourceFile();
    if ((sf == nullptr) || (sf->getDirectives() == nullptr) || (sf->getDirectives()->size() <= index)) return nullptr;
    return any_cast<hldb::GenericDirective>(sf->getDirectives()->at(index));
  }
};

// ----
// Before any directive: every property at its default.
// ----
TEST_F(CompilerDirectiveDefaultsTest, PlainModuleHasDefaults) {
  const hldb::Module *const m = findModule("plain");
  ASSERT_NE(m, nullptr);
  EXPECT_FALSE(m->getCellInstance());
  EXPECT_EQ(m->getUnconnDrive(), vpiHighZ);
  EXPECT_EQ(m->getDefDelayMode(), vpiDelayModeNone);
  EXPECT_EQ(m->getDefNetType(), vpiWire);
}

// ----
// `celldefine, `unconnected_drive pull1, `delay_mode_zero, `default_nettype none
// ----
TEST_F(CompilerDirectiveDefaultsTest, CellModuleTakesActiveDirectives) {
  const hldb::Module *const m = findModule("cell_a");
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(m->getCellInstance());
  EXPECT_EQ(m->getUnconnDrive(), vpiPull1);
  EXPECT_EQ(m->getDefDelayMode(), vpiDelayModeZero);
  EXPECT_EQ(m->getDefNetType(), vpiNone);
}

// Sec 22.9: `unconnected_drive also applies to interfaces and programs.
TEST_F(CompilerDirectiveDefaultsTest, InterfaceTakesUnconnectedDrive) {
  const hldb::Interface *const i = hldb::findByName<hldb::Interface>("ifc", m_design->getAllInterfaces());
  ASSERT_NE(i, nullptr);
  EXPECT_EQ(i->getUnconnDrive(), vpiPull1);
}

TEST_F(CompilerDirectiveDefaultsTest, ProgramTakesUnconnectedDrive) {
  const hldb::Program *const p = hldb::findByName<hldb::Program>("prg", m_design->getAllPrograms());
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getUnconnDrive(), vpiPull1);
}

// ----
// `endcelldefine, `unconnected_drive pull0, `delay_mode_path -- the most recent
// directive of each family wins; `default_nettype none is still in effect.
// ----
TEST_F(CompilerDirectiveDefaultsTest, MostRecentDirectiveWins) {
  const hldb::Module *const m = findModule("b");
  ASSERT_NE(m, nullptr);
  EXPECT_FALSE(m->getCellInstance());
  EXPECT_EQ(m->getUnconnDrive(), vpiPull0);
  EXPECT_EQ(m->getDefDelayMode(), vpiDelayModePath);
  EXPECT_EQ(m->getDefNetType(), vpiNone);
}

// ----
// Sec 22.3: `resetall puts every directive back to its default.
// ----
TEST_F(CompilerDirectiveDefaultsTest, ResetallRestoresDefaults) {
  const hldb::Module *const m = findModule("after_reset");
  ASSERT_NE(m, nullptr);
  EXPECT_FALSE(m->getCellInstance());
  EXPECT_EQ(m->getUnconnDrive(), vpiHighZ);
  EXPECT_EQ(m->getDefDelayMode(), vpiDelayModeNone);
  EXPECT_EQ(m->getDefNetType(), vpiWire);
}

// ----
// `celldefine, `unconnected_drive pull1 then `nounconnected_drive,
// `delay_mode_distributed then `delay_mode_unit.
// ----
TEST_F(CompilerDirectiveDefaultsTest, NounconnectedDriveAndLastDelayMode) {
  const hldb::Module *const m = findModule("c");
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(m->getCellInstance());
  EXPECT_EQ(m->getUnconnDrive(), vpiHighZ);
  EXPECT_EQ(m->getDefDelayMode(), vpiDelayModeUnit);
  EXPECT_EQ(m->getDefNetType(), vpiWire);
}

// ----
// Every directive is recorded on the SourceFile, in source order.
// ----
TEST_F(CompilerDirectiveDefaultsTest, AllDirectivesRecordedInOrder) {
  const std::vector<int32_t> expected = {
      vpiDirectiveTypeCellDefine,         vpiDirectiveTypeUnconnectedDrive,     vpiDirectiveTypeDelayModeZero,
      vpiDirectiveTypeDefaultNetType,     vpiDirectiveTypeEndCellDefine,        vpiDirectiveTypeUnconnectedDrive,
      vpiDirectiveTypeDelayModePath,      vpiDirectiveTypeResetAll,             vpiDirectiveTypeCellDefine,
      vpiDirectiveTypeUnconnectedDrive,   vpiDirectiveTypeNoUnconnectedDrive,   vpiDirectiveTypeDelayModeDistributed,
      vpiDirectiveTypeDelayModeUnit,      vpiDirectiveTypeDefaultTriregStrength, vpiDirectiveTypeAccelerate,
      vpiDirectiveTypeNoAccelerate,       vpiDirectiveTypeAutoExpandVectorNets, vpiDirectiveTypeExpandVectorNets,
      vpiDirectiveTypeNoExpandVectorNets, vpiDirectiveTypeRemoveGateName,       vpiDirectiveTypeNoRemoveGateNames,
      vpiDirectiveTypeRemoveNetName,      vpiDirectiveTypeNoRemoveNetNames,     vpiDirectiveTypeSuppressFaults,
      vpiDirectiveTypeNoSuppressFaults,   vpiDirectiveTypeDisablePortDefaults,  vpiDirectiveTypeEnablePortDefaults,
      vpiDirectiveTypeSigned,             vpiDirectiveTypeUnsigned,             vpiDirectiveTypeDefaultDecayTime,
      vpiDirectiveTypeProtect,            vpiDirectiveTypeEndProtect,           vpiDirectiveTypeUselib,
      vpiDirectiveTypeBeginKeywords,      vpiDirectiveTypeEndKeywords,
  };

  const hldb::SourceFile *const sf = getSourceFile();
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getDirectives(), nullptr);
  ASSERT_EQ(sf->getDirectives()->size(), expected.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    const hldb::GenericDirective *const gd = getDirective(i);
    ASSERT_NE(gd, nullptr) << "directive #" << i;
    EXPECT_EQ(gd->getDirectiveType(), expected[i]) << "directive #" << i;
  }
}

// `unconnected_drive carries its pull1/pull0 argument as an integer constant.
TEST_F(CompilerDirectiveDefaultsTest, UnconnectedDriveValues) {
  const hldb::GenericDirective *const pull1 = getDirective(1);
  ASSERT_NE(pull1, nullptr);
  ASSERT_EQ(pull1->getDirectiveType(), vpiDirectiveTypeUnconnectedDrive);
  const hldb::Constant *const v1 = pull1->getValue<hldb::Constant>();
  ASSERT_NE(v1, nullptr);
  EXPECT_EQ(v1->getValue(), std::to_string(vpiPull1));

  const hldb::GenericDirective *const pull0 = getDirective(5);
  ASSERT_NE(pull0, nullptr);
  ASSERT_EQ(pull0->getDirectiveType(), vpiDirectiveTypeUnconnectedDrive);
  const hldb::Constant *const v0 = pull0->getValue<hldb::Constant>();
  ASSERT_NE(v0, nullptr);
  EXPECT_EQ(v0->getValue(), std::to_string(vpiPull0));
}

// Directives with no argument carry no value.
TEST_F(CompilerDirectiveDefaultsTest, ArgumentlessDirectivesHaveNoValue) {
  const hldb::GenericDirective *const gd = getDirective(7);
  ASSERT_NE(gd, nullptr);
  ASSERT_EQ(gd->getDirectiveType(), vpiDirectiveTypeResetAll);
  EXPECT_EQ(gd->getValue(), nullptr);
}

// Annex E.3: `default_trireg_strength carries its integer constant.
TEST_F(CompilerDirectiveDefaultsTest, DefaultTriregStrengthValue) {
  const hldb::GenericDirective *const gd = getDirective(13);
  ASSERT_NE(gd, nullptr);
  ASSERT_EQ(gd->getDirectiveType(), vpiDirectiveTypeDefaultTriregStrength);
  const hldb::Constant *const v = gd->getValue<hldb::Constant>();
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getDecompile(), "30");
}

// Annex E.2: `default_decay_time carries its constant.
TEST_F(CompilerDirectiveDefaultsTest, DefaultDecayTimeValue) {
  const hldb::GenericDirective *const gd = getDirective(29);
  ASSERT_NE(gd, nullptr);
  ASSERT_EQ(gd->getDirectiveType(), vpiDirectiveTypeDefaultDecayTime);
  const hldb::Constant *const v = gd->getValue<hldb::Constant>();
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getDecompile(), "100");
}

// Sec 22.14: `begin_keywords carries its version specifier as a string constant.
TEST_F(CompilerDirectiveDefaultsTest, BeginKeywordsValue) {
  const hldb::GenericDirective *const gd = getDirective(33);
  ASSERT_NE(gd, nullptr);
  ASSERT_EQ(gd->getDirectiveType(), vpiDirectiveTypeBeginKeywords);
  const hldb::Constant *const v = gd->getValue<hldb::Constant>();
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getConstType(), vpiStringConst);
  EXPECT_EQ(v->getValue(), "1800-2017");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
