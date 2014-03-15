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
string* t;
while (!c.Receive(&t)) ;
```

The example in `example/example.cc` shows how to use it to make a .wav file player.
