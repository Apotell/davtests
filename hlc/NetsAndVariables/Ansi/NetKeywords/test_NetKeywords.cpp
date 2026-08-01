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
//   - wire -> vpiWire, tri1 -> vpiTri1 (the only net-type precedents
//     established elsewhere in this suite)
//   - tri, tri0, wand, wor, triand, trior, supply0, supply1, uwire are
//     checked for existence only: no test anywhere in this suite exercises
//     getNetType() for those net kinds
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
#include <hldb/vpi_user.h>

namespace hlc {

class AnsiNetKeywordsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NetKeywords.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("work@nets_and_variables_test", m_design->getAllModules());
  }
};

TEST_F(AnsiNetKeywordsTest, W0IsWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const w0 = hldb::findByName<hldb::Net>("w0", top->getNets());
  ASSERT_NE(w0, nullptr);
  EXPECT_EQ(w0->getNetType(), vpiWire);
}

TEST_F(AnsiNetKeywordsTest, T1zIsTri1) {
  // tri1 is the only non-wire net kind with an established getNetType() precedent
  // in this suite (6.9.2--vector_scalared / 6.9.2--vector_vectored).
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const t1z = hldb::findByName<hldb::Net>("t1z", top->getNets());
  ASSERT_NE(t1z, nullptr);
  EXPECT_EQ(t1z->getNetType(), vpiTri1);
}

TEST_F(AnsiNetKeywordsTest, RemainingNetKindsExist) {
  // tri, tri0, wand, wor, triand, trior, supply0, supply1, uwire: no test in
  // this suite exercises getNetType() for these, so only existence is checked.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  EXPECT_NE(hldb::findByName<hldb::Net>("t0", top->getNets()), nullptr) << "net 't0' (tri) not found";
  EXPECT_NE(hldb::findByName<hldb::Net>("t0z", top->getNets()), nullptr) << "net 't0z' (tri0) not found";
  EXPECT_NE(hldb::findByName<hldb::Net>("wand_net", top->getNets()), nullptr) << "net 'wand_net' (wand) not found";
  EXPECT_NE(hldb::findByName<hldb::Net>("wor_net", top->getNets()), nullptr) << "net 'wor_net' (wor) not found";
  EXPECT_NE(hldb::findByName<hldb::Net>("triand_net", top->getNets()), nullptr)
      << "net 'triand_net' (triand) not found";
  EXPECT_NE(hldb::findByName<hldb::Net>("trior_net", top->getNets()), nullptr) << "net 'trior_net' (trior) not found";
  EXPECT_NE(hldb::findByName<hldb::Net>("supply0_net", top->getNets()), nullptr)
      << "net 'supply0_net' (supply0) not found";
  EXPECT_NE(hldb::findByName<hldb::Net>("supply1_net", top->getNets()), nullptr)
      << "net 'supply1_net' (supply1) not found";
  EXPECT_NE(hldb::findByName<hldb::Net>("uwire_net", top->getNets()), nullptr) << "net 'uwire_net' (uwire) not found";
}

TEST_F(AnsiNetKeywordsTest, WBusIsVectorSevenToZero) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const wBus = hldb::findByName<hldb::Net>("w_bus", top->getNets());
  ASSERT_NE(wBus, nullptr);
  EXPECT_EQ(wBus->getNetType(), vpiWire);

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
  const hldb::Net *const triBus = hldb::findByName<hldb::Net>("tri_bus", top->getNets());
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
