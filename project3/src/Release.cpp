#include <assert.h>
#include <iostream>
#include <thread>
#include <vector>

#include "../include/LockManager.hpp"
#include "../include/TrxManager.hpp"


extern TrxManager trx_manager;


// This function logically removes the lock, not a physical removing.
// In addition, it wakes up waiting threads and moves the head.
void LockManager::ReleaseLock(lock_t* release_lock)
{
  lock_t* prev = nullptr;

  lock_t* target = nullptr;

  lock_t* new_head = nullptr;

  TrxNode* trx = nullptr;

  LockTableNode* lock_table_node = lock_table[release_lock->record_id];


  // Change the state of lock

  release_lock->state = lock_t::State::OBSOLETE;


  // Use head_mutex for synchronization only between threads releasing on the same record.

  std::unique_lock<std::mutex> lock(lock_table_node->head_mutex);

  target = lock_table_node->head.load();

  assert(target != nullptr);


  // If head is not a OBSOLETE, we can't move the head, because another thread is still working.

  if (target->state != lock_t::State::OBSOLETE)
    return;

  
  // Otherwise, move the head

  do
  {
    prev = target;
      
    target = target->next;
      
    // If target is the end of the list, do not move the head.

    if (target == nullptr)
      break;

    lock_table_node->head.store(target);

    prev->head_pass_flag.store(true);

  } while (target->state == lock_t::State::OBSOLETE);

  if (target == nullptr) // If head is the end of the list
    return;

  assert(target == lock_table_node->head.load());


  // If the new head is incompatible with releasing lock, wake up the waiting threads

  new_head = target;

  // Wake up new_head

  trx = trx_manager.GetTrxNode(new_head->thread_id);

  {
    std::unique_lock<std::mutex> lock(trx->mutex);

    if (new_head->state == lock_t::State::WAIT)
      trx->cond.notify_one();

    new_head->signal_flag.store(true);
  }
  
  // If new_head was SHARED mode, wake up other compatible transactions

  if (new_head->mode == lock_t::Mode::SHARED)
  {
    target = new_head->next;

    while (target != nullptr)
    {
      if (target->state == lock_t::State::OBSOLETE)
      {
        target = target->next;

        continue;
      }
      
      if (target->mode == lock_t::Mode::EXCLUSIVE)
        break;

      trx = trx_manager.GetTrxNode(target->thread_id);

      std::unique_lock<std::mutex> lock(trx->mutex);

      if (target->state == lock_t::State::WAIT)
        trx->cond.notify_one();

      target->signal_flag.store(true);

      target = target->next;
    }
  }
}

// Lock releasing with global mutex
// This function logically removes the lock, not a physical removing.
// In addition, it wakes up waiting threads and moves the head.
void LockManager::ReleaseLock(lock_t* release_lock, std::mutex& global_mutex)
{
  lock_t* prev = nullptr;

  lock_t* target = nullptr;

  lock_t* new_head = nullptr;

  TrxNode* trx = nullptr;

  LockTableNode* lock_table_node = lock_table[release_lock->record_id];


  // Acquire the global mutex that protects a lock table

  std::unique_lock<std::mutex> global_lock(global_mutex);


  // Change the state of lock

  release_lock->state = lock_t::State::OBSOLETE;


  // If head is not a OBSOLETE, we can't move the head, because another thread is still working.

  target = lock_table_node->head.load();

  assert(target != nullptr);

  if (target->state != lock_t::State::OBSOLETE)
    return;

  
  // Otherwise, move the head

  do
  {
    prev = target;
      
    target = target->next;
      
    // If target is the end of the list, do not move the head.

    if (target == nullptr)
      break;

    lock_table_node->head.store(target);

    prev->head_pass_flag.store(true);

  } while (target->state == lock_t::State::OBSOLETE);

  if (target == nullptr) // If head is the end of the list
    return;

  assert(target == lock_table_node->head.load());


  // If the new head is incompatible with releasing lock, wake up the waiting threads

  new_head = target;

  // Wake up new_head

  trx = trx_manager.GetTrxNode(new_head->thread_id);

  {
    if (new_head->state == lock_t::State::WAIT)
      trx->cond.notify_one();

    new_head->signal_flag.store(true);
  }
  
  // If new_head was SHARED mode, wake up other compatible transactions

  if (new_head->mode == lock_t::Mode::SHARED)
  {
    target = new_head->next;

    while (target != nullptr)
    {
      if (target->state == lock_t::State::OBSOLETE)
      {
        target = target->next;

        continue;
      }
      
      if (target->mode == lock_t::Mode::EXCLUSIVE)
        break;

      trx = trx_manager.GetTrxNode(target->thread_id);

      if (target->state == lock_t::State::WAIT)
        trx->cond.notify_one();

      target->signal_flag.store(true);

      target = target->next;
    }
  }
}