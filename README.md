Channel
=======

Lock-free ring buffer for inter-thread communication.

```c++
// Create a channel that can hold up to 15 string* at once. 
Channel<string*> c(15);
string* s = new string("hello there!");
// Send returns whether or not the channel is full.
while (!c.Send(s)) ;
// Receive returns whether or not there was anything to receive.
std::string* t;
while (!c.Receive(&t)) ;
```
