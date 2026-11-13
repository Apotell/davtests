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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.1--non-ansi-signedness.sv
//
// THE SOURCE
//
//   21: module test(a, b, c, d);
//   22:   input [7:0] a;
//   23:   input [7:0] b;
//   24:   input signed [7:0] c;
//   25:   input signed [7:0] d;
//   26:   wire signed [7:0] b;
//   27:   wire [7:0] c;
//   28: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "The signed attribute can be attached either to a port declaration or the
// corresponding net or variable declaration or to both. If either the port
// or the net/variable is declared as signed, then the other shall also be
// considered signed." and "Nets connected to ports without an explicit net
// declaration shall be considered unsigned, unless the port is declared as
// signed." (23.2.2.1). Example 1 of 23.2.2.1 annotates the same four cases:
//
//   a: no net declaration, port unsigned   -> net unsigned
//   b: port unsigned, net declared signed  -> "port b inherits signed
//                                              attribute from net decl."
//   c: port signed, net declared unsigned  -> "net c inherits signed
//                                              attribute from port"
//   d: no net declaration, port signed     -> "net is signed"
//
// The object model gives a net the Boolean vpiSigned (37.17; vpi_user.h:
// "TRUE for vpiIODecl and any object in the expression class if the object
// has the signed attribute").
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   FourInputPorts             a, b, c, d, all vpiInput
//   NetAIsUnsigned             vpiSigned FALSE
//   NetBIsSigned               vpiSigned TRUE (from its net declaration)
//   NetCIsSigned               vpiSigned TRUE (from its port declaration)
//   NetDIsSigned               vpiSigned TRUE (from its port declaration)
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The signedness of the port objects. 37.14 gives a port no vpiSigned
//     property; the LRM states the inheritance in terms of the net.
//   - The ranges. Every declaration uses [7:0], so they agree, as 23.2.2.1
//     requires.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2.1--non-ansi-signedness.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
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

uint32_t lineOf(const Event &event) { return event.getArguments().empty() ? 0 : event.getArguments().front().m_line; }

bool isErrorClass(const Event &event) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  if (errorInfo == nullptr) return false;
  const EventSeverity severity = errorInfo->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}
}  // namespace

class NonAnsiSignednessTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "non_ansi_signedness.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string allEvents() {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const hldb::Event &event : listener->getEvents()) {
      if (isErrorClass(event)) {
        std::vector<std::string> msgs = hldb::renderEvent(event);
        std::copy(msgs.cbegin(), msgs.cend(), std::ostream_iterator<std::string>(oss));
      }
    }
    return oss.str();
  }

  static const hldb::Module *findModule() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllModules());
  }

  static const hldb::Net *findNet(std::string_view name) {
    const hldb::Module *const module = findModule();
    return (module == nullptr) ? nullptr : hldb::findByName<hldb::Net>(name, module->getNets());
  }
};

TEST_F(NonAnsiSignednessTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(NonAnsiSignednessTest, FourInputPorts) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getDefName(), std::string_view("test"));
  ASSERT_NE(module->getPorts(), nullptr);
  EXPECT_EQ(module->getPorts()->size(), 4u);
  for (std::string_view name : {"a", "b", "c", "d"}) {
    const hldb::Port *const port = hldb::findByName<hldb::Port>(name, module->getPorts());
    ASSERT_NE(port, nullptr) << name;
    EXPECT_EQ(port->getDirection(), vpiInput) << name;
  }
}

TEST_F(NonAnsiSignednessTest, NetAIsUnsigned) {
  const hldb::Net *const net = findNet("a");
  ASSERT_NE(net, nullptr);
  const hldb::Typespec *const ts = hldb::getTypespec(net);
  ASSERT_NE(ts, nullptr);
  EXPECT_FALSE(hldb::getSigned(ts));
}

TEST_F(NonAnsiSignednessTest, NetBIsSigned) {
  const hldb::Net *const net = findNet("b");
  ASSERT_NE(net, nullptr);
  const hldb::Typespec *const ts = hldb::getTypespec(net);
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE(hldb::getSigned(ts));
}

TEST_F(NonAnsiSignednessTest, NetCIsSigned) {
  const hldb::Net *const net = findNet("c");
  ASSERT_NE(net, nullptr);
  const hldb::Typespec *const ts = hldb::getTypespec(net);
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE(hldb::getSigned(ts));
}

TEST_F(NonAnsiSignednessTest, NetDIsSigned) {
  const hldb::Net *const net = findNet("d");
  ASSERT_NE(net, nullptr);
  const hldb::Typespec *const ts = hldb::getTypespec(net);
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE(hldb::getSigned(ts));
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
