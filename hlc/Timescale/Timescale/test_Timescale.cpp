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

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>

namespace hlc {
class Timescale : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Timescale.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(Timescale, default) {
  const hldb::SourceFile *const s = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(s, nullptr) << "SourceFile s is null";

  const hldb::Module *const m1 = hldb::findByName<hldb::Module>("work@m1", m_design->getAllModules());
  ASSERT_NE(m1, nullptr) << "Module m1 is null";

  const hldb::Module *const m11 = hldb::findByName<hldb::Module>("work@m11", m1->getModules());
  ASSERT_NE(m11, nullptr) << "Module m11 is null";

  const hldb::Module *const m12 = hldb::findByName<hldb::Module>("work@m12", m1->getModules());
  ASSERT_NE(m12, nullptr) << "Module m12 is null";

  const hldb::Module *const m2 = hldb::findByName<hldb::Module>("work@m2", m_design->getAllModules());
  ASSERT_NE(m2, nullptr) << "Module m2 is null";

  ASSERT_EQ(s->getTimeUnit(), -9);
  ASSERT_EQ(s->getTimePrecision(), -12);

  ASSERT_EQ(m1->getTimeUnit(), -8);
  ASSERT_EQ(m1->getTimePrecision(), -12);

  ASSERT_EQ(m11->getTimeUnit(), -8);
  ASSERT_EQ(m11->getTimePrecision(), -11);

  ASSERT_EQ(m12->getTimeUnit(), 0);
  ASSERT_EQ(m12->getTimePrecision(), -12);

  ASSERT_EQ(m2->getTimeUnit(), -9);
  ASSERT_EQ(m2->getTimePrecision(), -12);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
