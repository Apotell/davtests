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

// Validates the UHDM graph produced for tests/NetsAndVariables/Ansi/NetKeywords.sv:
// every standard net keyword (wire, tri, tri0, tri1, wand, wor, triand,
// trior, supply0, supply1, uwire) and packed net buses, split out of the
// combined NetsAndVariablesAnsi.sv suite so net-keyword coverage stands on
// its own.
//
// Checked:
//   - every net keyword resolves to its exact IEEE vpiNetType constant
//     (vpi_user.h: vpiWire=1, vpiWand=2, vpiWor=3, vpiTri=4, vpiTri0=5,
//     vpiTri1=6, vpiTriAnd=8, vpiTriOr=9, vpiSupply1=10, vpiSupply0=11,
//     vpiUwire=13) -- a direct 1:1 keyword-to-constant mapping fixed by the
//     standard
//   - every net is present in getNets() and absent from getVariables() (no
//     duplicate)
//   - w_bus ([7:0]) and tri_bus ([3:0]) are vector nets with the expected
//     range bounds

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AnsiNetKeywordsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NetKeywords.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("nets_and_variables_test", m_design->getAllModules());
  }

  // Asserts 'name' is a hldb::Net in top->getNets() with the given
  // vpiNetType, and that no hldb::Variable with the same name exists in
  // top->getVariables() (no duplicate).
  static const hldb::Net *getNetOfType(const hldb::Module *top, std::string_view name, int32_t expectedNetType) {
    const hldb::Net *const n = hldb::findByName<hldb::Net>(name, top->getNets());
    EXPECT_NE(n, nullptr) << "net '" << name << "' not found";
    if (n != nullptr) {
      EXPECT_EQ(n->getNetType(), expectedNetType);
    }
    EXPECT_EQ(hldb::findByName<hldb::Variable>(name, top->getVariables()), nullptr)
        << "'" << name << "' is net-declared -- it must not also appear in vpiVariables";
    return n;
  }
};

TEST_F(AnsiNetKeywordsTest, W0IsWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getNetOfType(top, "w0", vpiWire), nullptr);
}

TEST_F(AnsiNetKeywordsTest, T0IsTri) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getNetOfType(top, "t0", vpiTri), nullptr);
}

TEST_F(AnsiNetKeywordsTest, T0zIsTri0) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getNetOfType(top, "t0z", vpiTri0), nullptr);
}

TEST_F(AnsiNetKeywordsTest, T1zIsTri1) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getNetOfType(top, "t1z", vpiTri1), nullptr);
}

TEST_F(AnsiNetKeywordsTest, WandNetIsWand) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getNetOfType(top, "wand_net", vpiWand), nullptr);
}

TEST_F(AnsiNetKeywordsTest, WorNetIsWor) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getNetOfType(top, "wor_net", vpiWor), nullptr);
}

TEST_F(AnsiNetKeywordsTest, TriandNetIsTriAnd) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getNetOfType(top, "triand_net", vpiTriAnd), nullptr);
}

TEST_F(AnsiNetKeywordsTest, TriorNetIsTriOr) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getNetOfType(top, "trior_net", vpiTriOr), nullptr);
}

TEST_F(AnsiNetKeywordsTest, Supply0NetIsSupply0) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getNetOfType(top, "supply0_net", vpiSupply0), nullptr);
}

TEST_F(AnsiNetKeywordsTest, Supply1NetIsSupply1) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getNetOfType(top, "supply1_net", vpiSupply1), nullptr);
}

TEST_F(AnsiNetKeywordsTest, UwireNetIsUwire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(getNetOfType(top, "uwire_net", vpiUwire), nullptr);
}

TEST_F(AnsiNetKeywordsTest, WBusIsVectorSevenToZero) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const wBus = getNetOfType(top, "w_bus", vpiWire);
  ASSERT_NE(wBus, nullptr);

  const hldb::RefTypespec *const rts = wBus->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::LogicTypespec *const ls = rts->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_TRUE(ls->getVector());
  ASSERT_NE(ls->getRanges(), nullptr);
  ASSERT_EQ(ls->getRanges()->size(), 1u);
  EXPECT_EQ(ls->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(ls->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(AnsiNetKeywordsTest, TriBusIsVectorThreeToZero) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const triBus = getNetOfType(top, "tri_bus", vpiTri);
  ASSERT_NE(triBus, nullptr);

  const hldb::RefTypespec *const rts = triBus->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::LogicTypespec *const ls = rts->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_TRUE(ls->getVector());
  ASSERT_NE(ls->getRanges(), nullptr);
  ASSERT_EQ(ls->getRanges()->size(), 1u);
  EXPECT_EQ(ls->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "3");
  EXPECT_EQ(ls->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
