**This repository contains projects implemented during a parallel programming course.**

**The projects include:**

**1. Optimizing a join algorithm implemented in memory.**
- Morsel driven parallelism.

**2. Implementing a wait-free atomic snapshot.**
- Multi-versioning.

**3. Developing a lock-free two-phase locking (2PL) mechanism.**
- It doesn't matter determining a non-deadlock as a deadlock. Because you can try again.
- But it's not ok to decide that a real deadlock as a non-deadlock.

**4. Making the MariaDB buffer pool initialization process scalable.**
- Use wait-free algorithm for linked list.
- Release malloc() page fault overhead.
