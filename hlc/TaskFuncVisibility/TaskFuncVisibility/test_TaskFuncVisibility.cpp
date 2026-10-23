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

// Tests for dut.sv (tags: TaskFuncVisibility)
//
// What is checked (IEEE 1800-2023):
//   - 37.41 detail 4: vpiVisibility of a task or function shall be
//     vpiPublicVis for a task/function that is not a class member (module,
//     interface and package tasks here)
//   - 37.41 detail 4 / 8.18: a class method (task or constructor) is
//     vpiPublicVis unless qualified 'local' (vpiLocalVis) or 'protected'
//     (vpiProtectedVis)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class TaskFuncVisibilityTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "TaskFuncVisibility.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() { return hldb::findByName<hldb::Package>("pkg", m_design->getAllPackages()); }

  static const hldb::ClassDefn *getClass(std::string_view name) {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::ClassDefn>(name, pkg->getClassDefns());
  }

  static const hldb::Function *getCtor(std::string_view className) {
    const hldb::ClassDefn *const cd = getClass(className);
    if (cd == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("new", cd->getMethods());
  }
};

TEST_F(TaskFuncVisibilityTest, ModuleTasksArePublic) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  for (std::string_view name : {"mod_task", "mod_auto_task"}) {
    const hldb::Task *const t = hldb::findByName<hldb::Task>(name, top->getTaskFuncs());
    ASSERT_NE(t, nullptr) << name;
    EXPECT_EQ(t->getVisibility(), vpiPublicVis) << "37.41 detail 4: '" << name << "' is not a class member";
  }
}

TEST_F(TaskFuncVisibilityTest, InterfaceTaskIsPublic) {
  const hldb::Interface *const ifc = hldb::findByName<hldb::Interface>("ifc", m_design->getAllInterfaces());
  ASSERT_NE(ifc, nullptr);
  const hldb::Task *const t = hldb::findByName<hldb::Task>("ifc_task", ifc->getTaskFuncs());
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->getVisibility(), vpiPublicVis) << "37.41 detail 4: not a class member";
}

TEST_F(TaskFuncVisibilityTest, PackageTaskIsPublic) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  const hldb::Task *const t = hldb::findByName<hldb::Task>("pkg_task", pkg->getTaskFuncs());
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->getVisibility(), vpiPublicVis) << "37.41 detail 4: not a class member";
}

TEST_F(TaskFuncVisibilityTest, ClassTaskVisibilityFollowsQualifier) {
  const hldb::ClassDefn *const c = getClass("C");
  ASSERT_NE(c, nullptr);
  const std::pair<std::string_view, int32_t> expected[] = {
      {"public_task", vpiPublicVis}, {"local_task", vpiLocalVis}, {"protected_task", vpiProtectedVis}};
  for (const std::pair<std::string_view, int32_t> &e : expected) {
    const hldb::Task *const t = hldb::findByName<hldb::Task>(e.first, c->getMethods());
    ASSERT_NE(t, nullptr) << e.first;
    EXPECT_EQ(t->getVisibility(), e.second) << "37.41 detail 4 / 8.18: '" << e.first << "'";
  }
}

TEST_F(TaskFuncVisibilityTest, ConstructorVisibilityFollowsQualifier) {
  const hldb::Function *const c = getCtor("C");
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getVisibility(), vpiPublicVis) << "37.41 detail 4: unqualified C::new";

  const hldb::Function *const d = getCtor("D");
  ASSERT_NE(d, nullptr);
  EXPECT_EQ(d->getVisibility(), vpiLocalVis) << "8.18: 'local function new'";

  const hldb::Function *const e = getCtor("E");
  ASSERT_NE(e, nullptr);
  EXPECT_EQ(e->getVisibility(), vpiProtectedVis) << "8.18: 'protected function new'";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
