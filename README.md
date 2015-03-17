# Channel

Wait-free ring buffer for inter-thread communication using C++11
atomics.

```c++
Channel<string*> c(15);
string* s = new string("hello there!");
if (!c.Send(s)) {
  // Send returns false if the channel is full
}
string* t;
if (!c.Receive(&t)) {
  // Receive returns false if the channel is empty
}
```

See https://github.com/rynlbrwn/spkr to see a real example.
