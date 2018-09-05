# BZOT

This is apparently something similar to an HTTP server. A server which speaks HTTP. You send it HTTP requests, and it returns HTTP responses.

## Seriously?

I was once very good at C and I miss those years. What should I do, then? But write some random C, obviously.

But what can I do? A network server, of course. And why HTTP? Well, because…

- It's pretty straightforward (you know how it's supposed to work and how to _start_ testing its features)
- There are plenty of clients to test it, and it's easy to benchmark
- Can be quite complicated, addressing all those weird edge cases right?
- It's an incremental effort (start from HTTP, add HTTPS, then HTTP/2…)

## Goals

None. Just write some C and have fun parsing headers and read cconfiguration files.

I will probably stop once I feel the need of `automake/autoconf`.

Also[segmentation fault]

## Status

- [x] Learn how to compile and debug C in macOs
- [x] "Hello world!"
- [x] Basic networking
- [x] I have a server, and I can connect to it
- [x] Fork or threads? Fork
- [x] Blocking or not blocking? Blocking with fork
- [x] Read the HTTP request
- [ ] Parse the HTTP request
