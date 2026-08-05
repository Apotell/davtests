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

// Validates the UHDM graph produced for tests/NetsAndVariables/DefaultNettypeTri1.sv,
// split out of the former combined DefaultNettypeNetTypes.sv suite so the
// 'tri1' `default_nettype testing point stands on its own.
//
// Checked:
//   - implicit_net_tri1 -> vpiTri1, the only non-wire net kind with an
//     established getNetType() precedent anywhere in this suite (see
//     NetKeywords.cpp / 6.9.2--vector_scalared / 6.9.2--vector_vectored)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/vpi_user.h>

namespace hlc {

class DefaultNettypeTri1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Tri1.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(DefaultNettypeTri1Test, ImplicitNetIsTri1) {
  const hldb::Module *const mod =
      hldb::findByName<hldb::Module>("default_nettype_tri1_test", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const hldb::Net *const net = hldb::findByName<hldb::Net>("implicit_net_tri1", mod->getNets());
  ASSERT_EQ(net, nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
