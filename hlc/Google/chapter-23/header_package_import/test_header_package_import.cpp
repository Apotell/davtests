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

// Test for
// tests/Google/chapter-23/chapter-23_testsuit-1/23.2.1--header-package-import.sv
//
// THE SOURCE
//
//   21: package p;
//   22:   parameter int W = 8;
//   23: endpackage
//
//   25: module m;
//   26:   import p::*;
//   27:   localparam int W2 = W;
//   28: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "The module header defines the following: ... A package import list of
// the module" (23.2.1), and module_nonansi_header / module_ansi_header both
// place { package_import_declaration } before the parameter port list
// (Syntax 23-1). "import p::*;" makes every member of package p, here
// parameter W, visible inside module m, so localparam W2's constant
// expression can reference W directly without a p:: prefix.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   BothAreAccepted            no such diagnostic on lines 21-28
//   PackagePIsDeclared         exactly one package from this file, named p,
//                              with parameter W
//   ModuleMIsDeclared          exactly one module from this file, named m
//   ModuleHeaderCarriesImportOfP   the module carries an import_typespec
//                              naming package p (23.2.1's "package import
//                              list of the module")
//   LocalparamW2ResolvesThroughTheImport   W2's vpiExpr (37.28 detail 3)
//                              resolves W to the very Parameter object
//                              declared inside package p, proving the
//                              import made it visible
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The numeric value of W2 (8). Evaluating a resolved constant
//     expression is a different concern (Clause 11) from the module
//     header's import mechanism (23.2.1) that this file targets; the
//     identity check on the resolved Parameter is the stronger and more
//     directly relevant assertion.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/import_typespec.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.1--header-package-import.sv";
constexpr uint32_t kFirstLine = 21;
constexpr uint32_t kLastLine = 28;

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

template <typename T>
size_t countFromSourceFile(const std::vector<T *> *collection) {
  size_t count = 0;
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (endsWith(object->getFile(), kSourceFileName)) ++count;
    }
  }
  return count;
}

template <typename T>
const T *findFromSourceFile(const std::vector<T *> *collection) {
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (endsWith(object->getFile(), kSourceFileName)) return object;
    }
  }
  return nullptr;
}

<<<<<<< Updated upstream:hlc/Google/chapter-23/header_package_import/test_header_package_import.cpp
=======
const hldb::Parameter *findParameterByName(const hldb::AnyCollection *parameters, std::string_view name) {
  if (parameters == nullptr) return nullptr;
  for (const hldb::Any *any : *parameters) {
    if (const hldb::Parameter *const parameter = any_cast<hldb::Parameter>(any)) {
      if (parameter->getName() == name) return parameter;
    }
  }
  return nullptr;
}

>>>>>>> Stashed changes:hlc/Google/chapter-23/chapter-23_testsuit-1/header_package_import/test_header_package_import.cpp
// The module's own vpiTypespec relation (37.10), which HLDB uses to attach a
// package_import_declaration's ImportTypespec directly to the module header.
const hldb::ImportTypespec *findImportOfPackage(const hldb::Any *scope, std::string_view packageName) {
  const hldb::Any::get_by_vpi_type_return_t result = scope->getByVpiType(vpiTypespec);
  if (const std::vector<const hldb::Any *> *const objects = std::get<2>(result)) {
    for (const hldb::Any *object : *objects) {
      if (const hldb::ImportTypespec *const importTypespec = any_cast<hldb::ImportTypespec>(object)) {
        if (importTypespec->getName() == packageName) return importTypespec;
      }
    }
  } else if (const hldb::Any *const single = std::get<1>(result)) {
    if (const hldb::ImportTypespec *const importTypespec = any_cast<hldb::ImportTypespec>(single)) {
      if (importTypespec->getName() == packageName) return importTypespec;
    }
  }
  return nullptr;
}

uint32_t lineOf(const Event &event) { return event.getArguments().empty() ? 0 : event.getArguments().front().m_line; }

bool isErrorClass(const Event &event) {
  const EventDefinition *const eventDefinition = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  if (eventDefinition == nullptr) return false;
  const EventSeverity severity = eventDefinition->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

std::string describeEvent(const Event &event) {
  const EventDefinition *const eventDefinition = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  std::string description = " [" + std::to_string(static_cast<int32_t>(event.getType()));
  description += " line " + std::to_string(lineOf(event));
  if (eventDefinition != nullptr) description += ": " + std::string(eventDefinition->m_lines[0].m_text);
  return description + "]";
}
}  // namespace

class HeaderPackageImportTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "header_package_import.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string errorsOnLines(uint32_t firstLine, uint32_t lastLine) {
    if ((m_session == nullptr) || (m_session->getEventListener() == nullptr)) return "no event listener";
    std::string description;
    for (const Event &event : m_session->getEventListener()->getEvents()) {
      const uint32_t line = lineOf(event);
      if (isErrorClass(event) && (line >= firstLine) && (line <= lastLine)) description += describeEvent(event);
    }
    return description;
  }

  static std::string allErrors() {
    if ((m_session == nullptr) || (m_session->getEventListener() == nullptr)) return "no event listener";
    std::string description;
    for (const Event &event : m_session->getEventListener()->getEvents()) {
      if (isErrorClass(event)) description += describeEvent(event);
    }
    return description;
  }

  static const hldb::Package *findPackage() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllPackages());
  }
  static const hldb::Module *findModule() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllModules());
  }
};

TEST_F(HeaderPackageImportTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(HeaderPackageImportTest, BothAreAccepted) { EXPECT_EQ(errorsOnLines(kFirstLine, kLastLine), ""); }

TEST_F(HeaderPackageImportTest, PackagePIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 1u);

  const hldb::Package *const package = findPackage();
  ASSERT_NE(package, nullptr);
  EXPECT_EQ(package->getName(), "p");
  EXPECT_NE(hldb::findByName<hldb::Parameter>("W", package->getParameters()), nullptr);
}

TEST_F(HeaderPackageImportTest, ModuleMIsDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);

  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "m");
}

TEST_F(HeaderPackageImportTest, ModuleHeaderCarriesImportOfP) {
  ASSERT_NE(m_design, nullptr);
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);

  const hldb::ImportTypespec *const importTypespec = findImportOfPackage(module, "p");
  EXPECT_NE(importTypespec, nullptr);
}

TEST_F(HeaderPackageImportTest, LocalparamW2ResolvesThroughTheImport) {
  const hldb::Package *const package = findPackage();
  const hldb::Module *const module = findModule();
  ASSERT_NE(package, nullptr);
  ASSERT_NE(module, nullptr);

  const hldb::Parameter *const packageW = hldb::findByName<hldb::Parameter>("W", package->getParameters());
  ASSERT_NE(packageW, nullptr);

<<<<<<< Updated upstream:hlc/Google/chapter-23/header_package_import/test_header_package_import.cpp
  const hldb::Parameter *const w2Assign = hldb::findByName<hldb::Parameter>("W2", module->getParameters());
  ASSERT_NE(w2Assign, nullptr);

  const hldb::RefObj *const rhs = w2Assign->getExpr<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "W");
=======
  const hldb::Parameter *const w2 = findParameterByName(module->getParameters(), "W2");
  ASSERT_NE(w2, nullptr);

  // vpiExpr of a parameter is its default value expression (37.28 detail 3).
  const hldb::RefObj *const value = w2->getExpr<hldb::RefObj>();
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getName(), "W");
>>>>>>> Stashed changes:hlc/Google/chapter-23/chapter-23_testsuit-1/header_package_import/test_header_package_import.cpp

  const hldb::Parameter *const resolved = value->getActual<hldb::Parameter>();
  ASSERT_NE(resolved, nullptr);
  EXPECT_EQ(resolved, packageW) << "W2's initializer did not resolve to package p's own parameter W";
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
