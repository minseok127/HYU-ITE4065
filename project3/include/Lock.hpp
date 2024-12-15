#ifndef LOCK_HPP
#define LOCK_HPP

// Lock data structure which is used for 2-phase locking.
// When a reader wants to read, the reader must acquire a SHARED lock object on the target record.
// Also, if a writer wants to write, the writer must acquire a EXCLUSIVE lock object on the target record.

// These locks are not removed, but recycled.
// If some threads create its lock, these locks are reused by the same thread.

// However, if threads recycle locks without any restrictions, the order of the linked list can be twisted.
// So there are some restrictions about recycling locks.

// 1. The state of lock must be OBSOLETE
// Lock has states ACTIVE, WAIT, OBSOLETE.
// If the state of lock is not a OBSOLETE, it can't be recycled because it is in use.

// 2. It must have provided its id to the next lock
// Because the locks can be recycled, the physical location can be changed continuously.
// Therefore, by assigning an unique id to the locks, the logical location of the locks is decided.
// So, before the locks are recycled, they must tell their id to the next lock. Then the next lock can get the unique id.

// 3. The head must have passed the lock.
// All records have a head and a tail.
// Since all the locks passed by the head are OBSOLETE, 
// It doesn't matter if the connection between them is twisted.

#include <atomic>
#include <stdint.h>
#include <thread>


// Lock data structure which is used for 2 phase locking
struct lock_t
{
  // S-lock | X-lcok
  enum Mode
  {
    SHARED, EXCLUSIVE
  };

  // State of lock
  enum State
  {
    ACTIVE, WAIT, OBSOLETE
  };

  // Constructor
  lock_t(const int record_id, const std::thread::id thread_id) : record_id(record_id), thread_id(thread_id) {}

  // Record id
  int record_id;

  // The mode of lock
  lock_t::Mode mode = SHARED;

  // Lock id on a record. This id indicates a logical order
  // To use memory barrier, allocate it as an atomic type
  std::atomic<uint64_t> lock_id_on_record{0xffffffffffffffff};  // Set the initaial value to the maximum value

  // Next pointer
  lock_t* next = nullptr;

  // Thread id that created this lock
  const std::thread::id thread_id;

  // If this lock is EXCLUSIVE lock, the amount of change is written here.
  int64_t diff = 0;

  // The state of lock
  lock_t::State state = ACTIVE;

  // Whether this lock provided the lock-id for the next lock
  // To use memory barrier, allocate it as an atomic type
  std::atomic<bool> id_pass_flag{false};

  // Whether the head has passed this lock
  // To use memory barrier, allocate it as an atomic type
  std::atomic<bool> head_pass_flag{false};

  // Whether a signal has been received
  // To use memory barrier, allocate it as an atomic type
  std::atomic<bool> signal_flag{false};
};

#endif  // LOCK_HPP