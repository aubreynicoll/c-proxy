# c-proxy
A rudimentary web proxy written in C.

This project is an attempt at one of the labs from *Computer Systems: A Programmer's Perspective* by Randal Bryant and David O'Hallaron.
As such, it is a learning project, and by no means robust or production-grade software.

More information about the lab (Proxy Lab) and the book can be found [here](http://csapp.cs.cmu.edu/3e/home.html). 
It's a fantastic book and well worth a read through.

## Objectives
The goal of the lab is to create a simple forward proxy in C. The proxy should be able to:
- Forward client requests to a remote host using HTTP/1.0
- Support only GET requests
- Otherwise support operations as specified in RFC 1945 (except for multi-line headers, which were not a requirement)
- Implement some form of concurrency
- Implement some form of caching (Not yet implemented)

## Outcomes
I learned a ton by doing this exercise, and while my solution is far from passable as a real, working proxy, 
it's certainly whetted my appetite to learn more about UNIX network programming.

In addition to pushing the limits of my knowledge of C programming, 
it was also the first time I'd predominantly used a white-paper as a technical reference, rather than "giving in" and looking for friendlier documentation. Perhaps that's why the code appears to be written by a complete git.

Key APIs used in the project include sockets and pthreads, as well as system-level I/O. 
Additionally, string processing played a major role, and in some ways
was the most difficult part of the project. I made some attempts at trying to keep things as speedy as possible, 
and in retrospect I feel like that led to some code of rather questionable quality. 
If you're a recruiter, please don't read the code in parse_proxy_request.

From here, I intend to continue learning more about system level network programming, but it is time to do so with a fresh project.

## Resources
*Computer Systems: A Programmer's Perspective*, by Randal Bryant & David O'Hallaron, as well as their excellent lectures  
*Beej's Guide to Network Programming*, by Brian "Beej" Hall  
The Linux Man Pages, maintained by Michael Kerrisk  
RFC 1945, by T. Berners-Lee, et al.
