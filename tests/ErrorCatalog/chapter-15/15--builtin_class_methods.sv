/*
:name: builtin_class_methods
:description: IEEE 1800-2023 Clause 15 built-in class methods the chapter-15 Google tests do not reach
:tags: 15.3 15.4.7 15.5.3
*/

// Google/chapter-15 covers the mailbox (15.4, blocking and non-blocking) and the named-event
// trigger and wait forms (15.5.1, 15.5.2). It never declares a semaphore, never calls
// try_peek, and never uses the triggered method. Those are what this file exercises, so the
// built-in classes hosting them stay covered without duplicating what Google already tests.

module top();
  semaphore sem;
  mailbox   mbx;
  event     e;
  string    msg;
  int       n;
  bit       b;

  initial begin
    // 15.3 Semaphores -- the whole class; Google has no semaphore test at all.
    sem = new(1);       // 15.3.1 function new(int keyCount = 0)
    sem.put(1);         // 15.3.2 function void put(int keyCount = 1)
    sem.get(1);         // 15.3.3 task get(int keyCount = 1)
    n = sem.try_get(1); // 15.3.4 function int try_get(int keyCount = 1)

    // 15.4.7 Peek() -- Google's non-blocking mailbox test calls try_put/try_get but not try_peek.
    mbx = new(0);
    n = mbx.try_peek(msg);

    // 15.5.3 Persistent trigger -- "function bit triggered()". The standard writes it without
    // parentheses inside a wait, so both spellings are exercised here.
    b = e.triggered;
    wait (e.triggered);
  end
endmodule
