#ifndef LOCKMANAGER_HPP
#define LOCKMANAGER_HPP

// Lock Manager manages a Lock Table
// The Lock Table has several nodes representing records.

// The node has a record.
// Transactions can access to the record by acquiring a lock on this node.

// So, these nodes need to manage locks in an appropriate way, linked lists.
// To manage linked list, each node has a head and a tail.

#include <unordered_map>
#include <mutex>
#include <vector>

#include "Lock.hpp"
#include "TrxNode.hpp"


constexpr std::size_t hardware_destructive_interference_size = 128; // To avoid false sharing, set the cache line's size.

// Node on a Lock Table
struct LockTableNode
{
  // Default constructor
  LockTableNode(const int record_id) : record_id(record_id) {}
  
  // Tail of linked list. New locks will be inserted into this tail.
  // To use atomic instructions, allocate it as an atomic type
  alignas(hardware_destructive_interference_size) std::atomic<lock_t*> tail{nullptr};

  // Head of linked list. Logical removal occurs at this head.
  // To use memory barrier, allocate it as an atomic type
  alignas(hardware_destructive_interference_size) std::atomic<lock_t*> head{nullptr};

  // Mutex for synchronization of head
  std::mutex head_mutex;

  int64_t record_value = 100; // According to the assignment specification, the initial value is set to 100.

  const int record_id;
};

// Lock manager that manages access to records with 2-phase locking
// This manager provides lock acquiring and lock releasing APIs
class LockManager
{
public:

  // Return the value of record
  int64_t GetRecord(lock_t* lock);

  // Change the value of record and get the changed value
  int64_t ChangeRecord(lock_t* lock, int64_t diff);

  // Add record
  void MakeRecords(int record_count);

  // If the lock is acquired successfully, the address of the lock is returned.
  // However, if a deadlock occurs during the insertion process, nullptr is returned.
  lock_t* AcquireLock(lock_t::Mode mode, int record_id, TrxNode* trx);

  // This function logically removes the lock, not a physical removing.
  // In addition, it wakes up waiting threads and moves the head.
  void ReleaseLock(lock_t* release_lock);

  // Lock acquring with global mutex
  // If the lock is acquired successfully, the address of the lock is returned.
  // However, if a deadlock occurs during the insertion process, nullptr is returned.
  lock_t* AcquireLock(lock_t::Mode mode, int record_id, TrxNode* trx, std::mutex& global_mutex);

  // Lock releasing with global mutex
  // This function logically removes the lock, not a physical removing.
  // In addition, it wakes up waiting threads and moves the head.
  void ReleaseLock(lock_t* release_lock, std::mutex& global_mutex);

private:

  // Deadlock check
  bool IsDeadlock(std::vector<lock_t*>& waiting_lock_vector);

  // Deadlock check, global mutex version
  // When this function is called, the global mutex is acquired.
  bool IsDeadlock2(std::vector<lock_t*>& waiting_lock_vector);

  // Key is a record id, value is a pointer of corresponding Node
  std::unordered_map<int, LockTableNode*> lock_table;

  // The number of records
  int record_count;
};

#endif  // LOCKMANAGER_HPP