**This repository contains projects implemented during a parallel programming course.**

**The projects include:**

**1. Optimizing a join algorithm implemented in memory.**
- Morsel driven parallelism.

**2. Implementing a wait-free atomic snapshot.**
```
// Writers have their own snapshot and this snapshot can be read by multiple readers.
// So if the writers just deallocate existing snapshots to replace it, the readers may access to the wrong memory.

// To avoid accessing the wrong memeory, use multiple versions of snapshot.
// The snapshots are managed by shared_snapshot object.

// shared_snapshot has 8byte control block which contains outer reference counter and index of current version of snapshot.
// If some readers want to access current version, they will increase outer reference counter by using fetch_add().
// When the fetch_add() is returned, lower 4byte of that return value represent the index of the snapshot whose reference count is increased.
// So, The reader can increment the reference counter while obtaining information about the index at the same time.

// After the reader used the acquired snapshot. It must be released.
// When releasing, the reader increases the inner reference counter by 1.
// If increased counter is 0, it means that there are no other threads who are referencing this snapshot.
// Then, set the recycle bit for recycling.

// But why we increase the inner reference counter in releasing phase?
// In replacing version of snapshot, the writer will exchange the 8byte control block to the new one atomically using exchange().
// Then it will return the old 8byte control block. It has old reference count and old index of old version.

// Because this change occured atomically, other new readers can't access to this old version anymore.
// In here, the writer will decrease the inner counter of old snapshot as much as old control block's outer reference count.
// So when increasing inner reference count, if increased inner count is 0, it means that this thread is the last thread who used that snapshot.

// cf) Why divide the reference counter into two?
// Because it is only possible to increment the reference counter in an 8-byte control block, but it is not possible to decrement.
// The reader wants to decrease the reference count at the end. But the writer may changed the control block to the other index, so the reader can't use it.
// So, the reader must notice to the other reference counter, inner counter.
```

**3. Developing a lock-free two-phase locking (2PL) mechanism.**
```
// Deadlock means that more than two transactions are waiting for each other.
// In acquire(), we got a vector containing the locks that the new_lock might be waiting for.
// For each lock in that vector, we will check whether the transaction of that locks is waiting for our new transaction.

// For this check, it is necessary to know where that transactions are conflicting.
// However, because we do not use global mutex, that conflict lock's location may change during deadlock checking.
// That is, the location viewed in the deadlock checking and the actual location may be different.
// So it can be a problem?

// The important thing in deadlock checking is that it doesn't matter determining a non-deadlock as a deadlock. Because you can try again.
// But it's a problem to decide that a real deadlock as a non-deadlock.

// If conflict lock is changed while deadlock checking, this transaction was not a deadlock with a new transaction until that point.
// Because if it was a deadlock, it can't change its states.
// Then, does the fact that the conflict lock has changed guarantees that it won't create a new deadlock until the deadlock check is done?

// No. A new deadlock can be created by a changing conflict lock during the deadlock checking.
// Let's assume that the conflict lock is changed, but deadlock checker can't recognize it.
// So a new wait cycle has been created, but the deadlock checker is not aware of it.
// Then the deadlock checker will tell us 'there are no deadlock', even if the new deadlock is created.

// But do you remember we first setup our conflict lock before deadlock checking in Aquire()?
// Because we pre-set the conflict lock before the deadlock check and we won't touch it until deadlock check is done, our conflict lock is fixed during the deadlock check. 
// If the above situation occurs, our deadlock checking will not find a new created deadlock,
// But this is not a problem because the transaction that changed the conflict lock will look at our fixed conflict lock, then it will find a new deadlock on its deadlock checking.
```

**4. Making the MariaDB buffer pool initialization process scalable.**
- Use wait-free algorithm for linked list.
- Release malloc() page fault overhead.
