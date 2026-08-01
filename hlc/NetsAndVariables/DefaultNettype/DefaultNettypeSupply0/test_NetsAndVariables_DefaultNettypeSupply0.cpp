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

// Validates the UHDM graph produced for tests/NetsAndVariables/DefaultNettypeSupply0.sv,
// split out of the former combined DefaultNettypeNetTypes.sv suite so the
// 'supply0' `default_nettype testing point stands on its own.
//
// Checked:
//   - the implicit net (implicit_net_supply0) exists; no test anywhere in
//     this suite exercises getNetType() for supply0, so no exact-value
//     assertion is made here either (matching the conservative precedent
//     NetKeywords.cpp established for this exact net kind)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>

namespace hlc {

class DefaultNettypeSupply0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DefaultNettypeSupply0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(DefaultNettypeSupply0Test, ImplicitNetExists) {
  const hldb::Module *const mod =
      hldb::findByName<hldb::Module>("default_nettype_supply0_test", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("implicit_net_supply0", mod->getNets()), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
