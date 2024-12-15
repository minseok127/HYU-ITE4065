#include <assert.h>
#include <thread>
#include <vector>
#include <iostream>

#include "../include/LockManager.hpp"


// Each thread has its own vectors which contain pointers of lock on a each record.
// When the thread wants to create a new lock, it recycles its own lock in that vectors.
// The vectors are stored in a hash table. Key is a record id and value is a corresponding recycle vector.
// thread_local std::vector<lock_t*> thread_local_lock_saved_vector; // Commented code : Non record by record recycling method
thread_local std::unordered_map<int, std::vector<lock_t*>> thread_local_lock_saved_vector_hash_table;

// The number of recycled lock. It will used to know the percentage of locks being recycled
thread_local int thread_local_recycled_lock_count = 0;

// The number of used as new_lock. IT will used to knwo the percentage of locks being recycled
thread_local int thread_local_total_created_lock_count = 0;


// If the lock is acquired successfully, the address of the lock is returned.
// However, if a deadlock occurs during the insertion process, nullptr is returned.
lock_t* LockManager::AcquireLock(lock_t::Mode mode, int record_id, TrxNode* trx)
{
  assert(trx->thread_id == std::this_thread::get_id());

  lock_t* new_lock = nullptr;

  lock_t* prev_tail = nullptr;

  lock_t* target = nullptr;

  std::vector<lock_t*> waiting_lock_vector;

  LockTableNode* lock_table_node = lock_table[record_id];

  std::vector<lock_t*>& thread_local_lock_saved_vector = thread_local_lock_saved_vector_hash_table[record_id]; // Record by record recycling method

  bool is_recycled = false;


  // Find recylable lock

  for (int i = 0; i < thread_local_lock_saved_vector.size(); i++)
  {
    new_lock = thread_local_lock_saved_vector[i];

    // If this lock can be recycled, use it. More details on recycling restrictions are written in Lock.hpp.
      
    if (new_lock->state == lock_t::State::OBSOLETE && new_lock->id_pass_flag.load() && new_lock->head_pass_flag.load())
    {
      is_recycled = true;

      break;
    }
  }

  if (is_recycled)
  {
    //new_lock->record_id = record_id; // Commented code : Non record by record recycling method

    new_lock->lock_id_on_record.store(0xffffffffffffffff);

    new_lock->id_pass_flag.store(false);

    new_lock->state = lock_t::State::ACTIVE;

    new_lock->next = nullptr;

    new_lock->diff = 0;

    new_lock->head_pass_flag.store(false);

    new_lock->signal_flag.store(false);

    thread_local_recycled_lock_count++;
  }
  else
  {
    new_lock = new lock_t(record_id, trx->thread_id);

    thread_local_lock_saved_vector.push_back(new_lock);
  }

  thread_local_total_created_lock_count++;

  new_lock->mode = mode;

  trx->trx_lock_deque.push_back(new_lock); // Add a new lock to the list of locks that are inserted on the lock-table by this transaction.


  // Atomically insert the new_lock into the tail and acquire the previous tail.

  prev_tail = lock_table_node->tail.exchange(new_lock);


  // If prev_tail is not a nullptr, set the new_lock based on the information in prev_tail
  // We will connect the prev_tail's next first, then we will set the lock-id
  // If we set the lock-id first, each lock may has only lock-id and may not be connected.
  // However, if we set the connection first and wait for the id to be set, all locks are guaranteed to be connected in later search.

  if (prev_tail)
  {
    // Connect

    prev_tail->next = new_lock;

    // Wait for the pev_tail's id and head to be set, because it may not have been set
    // In addition, waits for the setting of the head.

    while (prev_tail->lock_id_on_record.load() == 0xffffffffffffffff || lock_table_node->head.load() == nullptr)
      sched_yield();

    // Wait is over, set the lock-id

    new_lock->lock_id_on_record.store(prev_tail->lock_id_on_record.load() + 1);

    assert(prev_tail->id_pass_flag.load() == false);

    prev_tail->id_pass_flag.store(true);
  }

  // If the prev_tail does not exist, set the lock-id to 0 and set the head to the new lock.

  else
  {
    new_lock->lock_id_on_record.store(0);
    
    lock_table_node->head.store(new_lock);
  }


  // Since the new lock has been completely inserted into the list, set the transaction's conflict lock to the corresponding lock.
  // ** Note that we set the conflict lock before the deadlock checking and after the inserting the new lock. ** //

  assert(trx->conflict_lock.load() == nullptr);

  trx->conflict_lock.store(new_lock);


  // Save the locks from head to new_lock

  target = lock_table_node->head.load();

  assert(target != nullptr);

  // Since new_lock is not recycled at this point, the search starting from head must reach to the new_lock

  while (target != new_lock)
  {
    // If the target's id is greater than the new_lock's id, it means that the target has been recycled.
    // Then it means that the head has already passed the target.
    // So, new lock does not have to wait for the locks that have been collected up to this target lock.

    if (target->lock_id_on_record.load() > new_lock->lock_id_on_record.load()) // || target->record_id != new_lock->record_id) // Commented code : Non record by record recycling method
    {
      // Restart from new head

      waiting_lock_vector.clear();

      target = lock_table_node->head.load();

      assert(target != nullptr);
    
      continue;
    }

    // If the target id is still smaller than the new lock, store it.

    waiting_lock_vector.push_back(target);

    target = target->next;

    // If the target is moved to the next lock and the address is nullptr, 
    // It means that this target is not recycled until we store the target's address in the vector.
    // But it means that the address is recycled after saving, so discard all the locks collected and restart from the head.

    if (target == nullptr)
    {
      // Restart from new head

      waiting_lock_vector.clear();

      target = lock_table_node->head.load();

      assert(target != nullptr);
    }
  }


  // Through the above process, watting_lock_vector contains all locks except for those that are definitely behind new_lock.
  // But this does not guarantee that all locks contained in the vector are located before the new_lock.
  // With this in mind, let's check compatibility.

  for (int i = waiting_lock_vector.size() - 1; i >= 0; i--)
  {
    target = waiting_lock_vector[i];

    // This cases do not need to be checked

    if (target->state == lock_t::State::OBSOLETE || target->lock_id_on_record.load() > new_lock->lock_id_on_record.load()) // || target->record_id != new_lock->record_id) // Commented code : Non record by record recycling method
      continue;

    // If target is incompatible with new_lock

    if (new_lock->mode == lock_t::Mode::EXCLUSIVE || target->mode == lock_t::Mode::EXCLUSIVE)
    {
      new_lock->state = lock_t::State::WAIT;

      if (target->state == lock_t::State::OBSOLETE || target->lock_id_on_record.load() > new_lock->lock_id_on_record.load()) // || target->record_id != new_lock->record_id) // Commented code : Non record by record recycling method
      {
        new_lock->state = lock_t::State::ACTIVE;

        continue;
      }
      else if (IsDeadlock(waiting_lock_vector))
      {
        // Deadlock is occured

        new_lock->state = lock_t::State::OBSOLETE;

        trx->conflict_lock.store(nullptr);

        return nullptr; // The caller must recognize deadlock with this nullptr
      }

      break;
    }
  }


  // If there is an incompatible lock, wait with a conditional variable

  if (new_lock->state == lock_t::State::WAIT)
  {
    // Get the mutex

    std::unique_lock<std::mutex> lock(trx->mutex);

    // If no other threads has woken this thread yet, go to sleep

    if (!new_lock->signal_flag.load())
      trx->cond.wait(lock);

    new_lock->state = lock_t::State::ACTIVE;
  }

  // Waiting is over, initailize confilct lock

  trx->conflict_lock.store(nullptr);
  
  return new_lock;
}

// Lock acquring with global mutex
// If the lock is acquired successfully, the address of the lock is returned.
// However, if a deadlock occurs during the insertion process, nullptr is returned.
lock_t* LockManager::AcquireLock(lock_t::Mode mode, int record_id, TrxNode* trx, std::mutex& global_mutex)
{
  assert(trx->thread_id == std::this_thread::get_id());

  lock_t* new_lock = nullptr;

  lock_t* prev_tail = nullptr;

  lock_t* target = nullptr;

  std::vector<lock_t*> waiting_lock_vector;

  LockTableNode* lock_table_node = lock_table[record_id];

  std::vector<lock_t*>& thread_local_lock_saved_vector = thread_local_lock_saved_vector_hash_table[record_id]; // Record by record recycling method

  bool is_recycled = false;


  // Acquire the global mutex that protects a lock table

  std::unique_lock<std::mutex> global_lock(global_mutex);


  // Find recylable lock

  for (int i = 0; i < thread_local_lock_saved_vector.size(); i++)
  {
    new_lock = thread_local_lock_saved_vector[i];

    // If this lock can be recycled, use it. More details on recycling restrictions are written in Lock.hpp.
      
    if (new_lock->state == lock_t::State::OBSOLETE && new_lock->head_pass_flag.load())
    {
      is_recycled = true;

      break;
    }
  }

  if (is_recycled)
  {
    new_lock->state = lock_t::State::ACTIVE;

    new_lock->next = nullptr;

    new_lock->diff = 0;

    new_lock->head_pass_flag.store(false);

    new_lock->signal_flag.store(false);

    thread_local_recycled_lock_count++;
  }
  else
  {
    new_lock = new lock_t(record_id, trx->thread_id);

    thread_local_lock_saved_vector.push_back(new_lock);
  }

  thread_local_total_created_lock_count++;

  new_lock->mode = mode;

  trx->trx_lock_deque.push_back(new_lock); // Add a new lock to the list of locks that are inserted on the lock-table by this transaction.


  // Insert the new_lock into the tail and acquire the previous tail.

  prev_tail = lock_table_node->tail.load();

  if (prev_tail)
    prev_tail->next = new_lock;
  else
    lock_table_node->head.store(new_lock);

  lock_table_node->tail.store(new_lock);


  // Since the new lock has been completely inserted into the list, set the transaction's conflict lock to the corresponding lock.
  // ** Note that we set the conflict lock before the deadlock checking and after the inserting the new lock. ** //

  assert(trx->conflict_lock.load() == nullptr);

  trx->conflict_lock.store(new_lock);


  // Save the locks from head to new_lock

  target = lock_table_node->head.load();

  assert(target != nullptr);

  // Since new_lock is not recycled at this point, the search starting from head must reach to the new_lock

  while (target != new_lock)
  {
    waiting_lock_vector.push_back(target);

    target = target->next;
  }


  // Through the above process, watting_lock_vector contains all locks except for those that are definitely behind new_lock.
  // But this does not guarantee that all locks contained in the vector are located before the new_lock.
  // With this in mind, let's check compatibility.

  for (int i = waiting_lock_vector.size() - 1; i >= 0; i--)
  {
    target = waiting_lock_vector[i];

    // This cases do not need to be checked

    if (target->state == lock_t::State::OBSOLETE)
      continue;

    // If target is incompatible with new_lock

    if (new_lock->mode == lock_t::Mode::EXCLUSIVE || target->mode == lock_t::Mode::EXCLUSIVE)
    {
      new_lock->state = lock_t::State::WAIT;

      if (IsDeadlock2(waiting_lock_vector))
      {
        // Deadlock is occured

        new_lock->state = lock_t::State::OBSOLETE;

        trx->conflict_lock.store(nullptr);

        return nullptr; // The caller must recognize deadlock with this nullptr
      }

      break;
    }
  }


  // If there is an incompatible lock, wait with a conditional variable

  if (new_lock->state == lock_t::State::WAIT)
  {
    // If no other threads has woken this thread yet, go to sleep

    if (!new_lock->signal_flag.load())
      trx->cond.wait(global_lock);

    new_lock->state = lock_t::State::ACTIVE;
  }

  // Waiting is over, initailize confilct lock

  trx->conflict_lock.store(nullptr);
  
  return new_lock;
}