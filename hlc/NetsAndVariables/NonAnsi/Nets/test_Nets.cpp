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

// Validates the UHDM graph produced for tests/NetsAndVariables/NonAnsi/Nets.sv,
// split out of the combined NetsAndVariablesNonAnsi.sv suite so the internal
// net declaration testing point stands on its own.
//
// Checked:
//   - w0 (wire), w_bus (wire [3:0]), t0 (tri, existence only -- see the ANSI
//     suite's NetKeywords test for why no getNetType() assertion is made for
//     plain tri)

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

class NonAnsiNetsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Nets.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getMod() {
    return hldb::findByName<hldb::Module>("work@nets_and_variables_nonansi", m_design->getAllModules());
  }
};

TEST_F(NonAnsiNetsTest, W0IsWire) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  const hldb::Net *const w0 = hldb::findByName<hldb::Net>("w0", mod->getNets());
  ASSERT_NE(w0, nullptr);
  EXPECT_EQ(w0->getNetType(), vpiWire);
}

TEST_F(NonAnsiNetsTest, WBusIsVectorThreeToZero) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  const hldb::Net *const wBus = hldb::findByName<hldb::Net>("w_bus", mod->getNets());
  ASSERT_NE(wBus, nullptr);
  EXPECT_EQ(wBus->getNetType(), vpiWire);

  const hldb::RefTypespec *const rts = wBus->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::LogicTypespec *const ls = rts->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_TRUE(ls->getVector());
  ASSERT_NE(ls->getRanges(), nullptr);
  ASSERT_EQ(ls->getRanges()->size(), 1u);
  EXPECT_EQ(ls->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "3");
  EXPECT_EQ(ls->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(NonAnsiNetsTest, T0Exists) {
  // Plain 'tri': no test anywhere in this suite exercises getNetType() for
  // it (see the ANSI suite's NetKeywords test), so only existence is checked here.
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("t0", mod->getNets()), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
