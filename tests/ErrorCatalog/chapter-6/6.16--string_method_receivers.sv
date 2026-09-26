/*
:name: string_method_receivers
:description: IEEE 1800-2023 Sec 6.16 string methods reached through every receiver shape
:tags: 6.16
*/

// The per-subclause 6.16.x fixtures each call one method on a module-level
// "string" variable. That leaves the METHOD names covered but the RECEIVER
// shapes untested: Sec 6.16 puts no restriction on where the string comes
// from, so a string reached through a typedef, a class property, a struct
// member or a subroutine formal must bind exactly the same way. This file
// varies the receiver instead of the method, so a regression in receiver
// resolution is caught even though every call here is legal.

typedef string s_t;

class holder;
  string prop = "abcdef";

  // Receiver is a property of the enclosing class, named without a handle.
  function int via_own_prop();
    return prop.len();
  endfunction
endclass

// A user-declared method whose name collides with a Sec 6.16 string method.
// Nothing about a string may capture it: its receiver is a class handle.
class shadow;
  int data = 7;
  function int len();
    return data;
  endfunction
endclass

module top();
  typedef struct { string name; } rec_t;

  s_t      via_typedef = "abcdef";
  rec_t    rec;
  holder   h;
  shadow   sh;
  string   plain = "abcdef";
  string   q[$];
  int      n;

  // Receiver is a subroutine formal, and a subroutine local.
  function int via_subroutine(string formal);
    string local_str = "abcdef";
    n = formal.len();
    return local_str.len();
  endfunction

  // The handles are never constructed: Sec 6.16 binding is a static, declared-type
  // question, so h.prop resolves through holder's own declaration alone. Calling new()
  // here would only add unrelated constructor-binding noise to the golden.
  initial begin
    n = via_typedef.len();        // typedef of string
    n = rec.name.len();           // struct member
    n = h.prop.len();             // class property through a handle
    n = h.via_own_prop();         // class property without a handle
    n = via_subroutine("abcdef"); // subroutine formal + local
    n = plain.substr(1, 2).len(); // chained: the receiver is a method result

    n = sh.len();                 // user method wins; not a string method
    n = q[1].len();               // element of a queue of strings
  end
endmodule
