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

// Validates the UHDM graph produced for tests/NetsAndVariables/DefaultNettypeNetTypes.sv,
// which completes legal `default_nettype coverage (wand, wor, tri0, tri1,
// triand, trior, uwire, trireg, supply0) beyond what DefaultNettype.sv shows.
//
// Checked:
//   - each module's implicit net exists
//   - implicit_net_tri1 -> vpiTri1, the only non-wire net kind with an
//     established getNetType() precedent anywhere in this suite (see
//     NetKeywords.cpp / 6.9.2--vector_scalared / 6.9.2--vector_vectored)
//   - the remaining net kinds (wand, wor, tri0, triand, trior, uwire,
//     trireg, supply0) are checked for existence only, matching the same
//     conservative precedent NetKeywords.cpp already established for those
//     exact net kinds: no test anywhere in this suite exercises
//     getNetType() for them, so no exact-value assertion is made here either

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/vpi_user.h>

namespace hlc {

class DefaultNettypeNetTypesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DefaultNettypeNetTypes.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(const char *const name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }
};

TEST_F(DefaultNettypeNetTypesTest, WandModuleImplicitNetExists) {
  const hldb::Module *const mod = getModule("work@default_nettype_wand_test");
  ASSERT_NE(mod, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("implicit_net_wand", mod->getNets()), nullptr);
}

TEST_F(DefaultNettypeNetTypesTest, WorModuleImplicitNetExists) {
  const hldb::Module *const mod = getModule("work@default_nettype_wor_test");
  ASSERT_NE(mod, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("implicit_net_wor", mod->getNets()), nullptr);
}

TEST_F(DefaultNettypeNetTypesTest, Tri0ModuleImplicitNetExists) {
  const hldb::Module *const mod = getModule("work@default_nettype_tri0_test");
  ASSERT_NE(mod, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("implicit_net_tri0", mod->getNets()), nullptr);
}

TEST_F(DefaultNettypeNetTypesTest, Tri1ModuleImplicitNetIsTri1) {
  const hldb::Module *const mod = getModule("work@default_nettype_tri1_test");
  ASSERT_NE(mod, nullptr);
  const hldb::Net *const net = hldb::findByName<hldb::Net>("implicit_net_tri1", mod->getNets());
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getNetType(), vpiTri1);
}

TEST_F(DefaultNettypeNetTypesTest, TriandModuleImplicitNetExists) {
  const hldb::Module *const mod = getModule("work@default_nettype_triand_test");
  ASSERT_NE(mod, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("implicit_net_triand", mod->getNets()), nullptr);
}

TEST_F(DefaultNettypeNetTypesTest, TriorModuleImplicitNetExists) {
  const hldb::Module *const mod = getModule("work@default_nettype_trior_test");
  ASSERT_NE(mod, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("implicit_net_trior", mod->getNets()), nullptr);
}

TEST_F(DefaultNettypeNetTypesTest, UwireModuleImplicitNetExists) {
  const hldb::Module *const mod = getModule("work@default_nettype_uwire_test");
  ASSERT_NE(mod, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("implicit_net_uwire", mod->getNets()), nullptr);
}

TEST_F(DefaultNettypeNetTypesTest, TriregModuleImplicitNetExists) {
  const hldb::Module *const mod = getModule("work@default_nettype_trireg_test");
  ASSERT_NE(mod, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("implicit_net_trireg", mod->getNets()), nullptr);
}

TEST_F(DefaultNettypeNetTypesTest, Supply0ModuleImplicitNetExists) {
  const hldb::Module *const mod = getModule("work@default_nettype_supply0_test");
  ASSERT_NE(mod, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("implicit_net_supply0", mod->getNets()), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
