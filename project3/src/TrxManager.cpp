#include <assert.h>
#include <fstream>
#include <queue>

#include "../include/LockManager.hpp"
#include "../include/TrxManager.hpp"


TrxManager trx_manager;

extern LockManager lock_manager;


// From thread id, return transaction node.
TrxNode* TrxManager::GetTrxNode(std::thread::id thread_id)
{
  // If transaction table has a node corresponding the argument, return it.

  {
    std::shared_lock<std::shared_mutex> read_lock(trx_table_latch);

    if (trx_node_table.count(thread_id))
      return trx_node_table[thread_id];
  }

  // But if transaction table does not have it, make a new node

  TrxNode* trx_node = new TrxNode(thread_id);

  std::unique_lock<std::shared_mutex> write_lock(trx_table_latch);

  trx_node->thread_number = this->thread_number++;

  trx_node_table.insert(std::make_pair(thread_id, trx_node));

  return trx_node;
}

// Initialize transaction node's information
void TrxManager::Begin()
{
  TrxNode* trx = GetTrxNode(std::this_thread::get_id());

  assert(trx != nullptr);

  trx->conflict_lock.store(nullptr);

  trx->trx_lock_deque.clear();
}

// Acquire a shared lock about the record.
// If the acquisition is successful, the record value is stored in ret and SUCCESS is returned.
// If it fails, MUST_ABORTED is returned. Then caller must call Abort().
TrxManager::Response TrxManager::Find(int record_id, int64_t* ret)
{
  lock_t* lock = lock_manager.AcquireLock(lock_t::Mode::SHARED, record_id, GetTrxNode(std::this_thread::get_id()));

  if (lock == nullptr)
    return TrxManager::Response::MUST_ABORTED;

  *ret = lock_manager.GetRecord(lock);

  return TrxManager::Response::SUCCESS;
}

// Acquire an exclusive lock on the record and change the value by 'diff'.
// If the acquisition is successful, the changed record value is stored in ret and SUCCESS is returned.
// If it fails, MUST_ABORTED is returned. Then caller must call Abort().
TrxManager::Response TrxManager::Update(int record_id, int64_t diff, int64_t* ret)
{
  lock_t* lock = lock_manager.AcquireLock(lock_t::Mode::EXCLUSIVE, record_id, GetTrxNode(std::this_thread::get_id()));
  
  if (lock == nullptr)
    return TrxManager::Response::MUST_ABORTED;

  *ret = lock_manager.ChangeRecord(lock, diff);

  lock->diff = diff;

  return TrxManager::Response::SUCCESS;
}

// This function should be called when Find() or Update() fails.
// Undo all modifications of this transaction.
void TrxManager::Abort()
{
  TrxNode* trx = GetTrxNode(std::this_thread::get_id());

  std::deque<lock_t*>& trx_lock_deque = trx->trx_lock_deque;

  lock_t* target_lock = nullptr;

  int64_t diff = 0;

  // Undo all modifications, then release the locks

  for (int i = 0; i < trx_lock_deque.size(); i++)
  {
    target_lock = trx_lock_deque[i];

    if (target_lock->mode == lock_t::Mode::EXCLUSIVE && target_lock->state != lock_t::State::OBSOLETE)
      lock_manager.ChangeRecord(target_lock, -target_lock->diff); // rollback

    lock_manager.ReleaseLock(target_lock);
  }
}

// When transactions are doen, this function should be called.
// This function returns the commit id on success.
// However, even if the commit was successful, if the commit id exceeds last_global_execution_order, -1 is returned.
int TrxManager::Commit()
{
  TrxNode* trx = GetTrxNode(std::this_thread::get_id());

  std::deque<lock_t*>& trx_lock_deque = trx->trx_lock_deque;

  lock_t* target_lock = nullptr;

  int64_t diff = 0;

  std::queue<int> record_id_queue;

  std::queue<int64_t> record_value_queue;

  std::string file_name = "thread" + std::to_string(trx->thread_number) + ".txt";

  std::ofstream fout(file_name, std::ios::app);


  // Get a commit id

  int commit_id = global_execution_count.fetch_add(1) + 1;

  // If commit id is bigger than last_global_execution_order,
  // Undo all modifications, then release the locks

  if (commit_id > last_global_execution_order)
  {
    for (int i = 0; i < trx_lock_deque.size(); i++)
    {
      target_lock = trx_lock_deque[i];

      if (target_lock->mode == lock_t::Mode::EXCLUSIVE && target_lock->state != lock_t::State::OBSOLETE)
        lock_manager.ChangeRecord(target_lock, -target_lock->diff); // rollback

      lock_manager.ReleaseLock(target_lock);
    }

    return -1;
  }

  // Otherwise, just release all locks.

  for (int i = 0; i < trx_lock_deque.size(); i++)
  {
    target_lock = trx_lock_deque[i];

    record_id_queue.push(target_lock->record_id);

    record_value_queue.push(lock_manager.GetRecord(target_lock));

    lock_manager.ReleaseLock(target_lock);
  }

  // Publish a commit log
  // [commit_id] [record_ids] [record_values]

  fout << commit_id;

  while (!record_id_queue.empty())
  {
    fout << " " << record_id_queue.front();

    record_id_queue.pop();
  }

  while (!record_value_queue.empty())
  {
    fout << " " << record_value_queue.front();

    record_value_queue.pop();
  }

  fout << "\n";

  fout.close();

  return commit_id;
}

// Find with global mutex
// Acquire a shared lock about the record.
// If the acquisition is successful, the record value is stored in ret and SUCCESS is returned.
// If it fails, FAIL is returned. Then caller must call Abort().
TrxManager::Response TrxManager::Find(int record_id, int64_t* ret, std::mutex& global_mutex)
{
  lock_t* lock = lock_manager.AcquireLock(lock_t::Mode::SHARED, record_id, GetTrxNode(std::this_thread::get_id()), global_mutex);

  if (lock == nullptr)
    return TrxManager::Response::MUST_ABORTED;

  *ret = lock_manager.GetRecord(lock);

  return TrxManager::Response::SUCCESS;
}

// Update with global mutex
// Acquire an exclusive lock on the record and change the value by 'diff'.
// If the acquisition is successful, the changed record value is stored in ret and SUCCESS is returned.
// If it fails, FAIL is returned. Then caller must call Abort().
TrxManager::Response TrxManager::Update(int record_id, int64_t diff, int64_t* ret, std::mutex& global_mutex)
{
  lock_t* lock = lock_manager.AcquireLock(lock_t::Mode::EXCLUSIVE, record_id, GetTrxNode(std::this_thread::get_id()), global_mutex);
  
  if (lock == nullptr)
    return TrxManager::Response::MUST_ABORTED;

  *ret = lock_manager.ChangeRecord(lock, diff);

  lock->diff = diff;

  return TrxManager::Response::SUCCESS;
}

// Abort with global mutex
// This function should be called when Find() or Update() fails.
// Undo all modifications of this transaction.
void TrxManager::Abort(std::mutex& global_mutex)
{
  TrxNode* trx = GetTrxNode(std::this_thread::get_id());

  std::deque<lock_t*>& trx_lock_deque = trx->trx_lock_deque;

  lock_t* target_lock = nullptr;

  int64_t diff = 0;

  // Undo all modifications, then release the locks

  for (int i = 0; i < trx_lock_deque.size(); i++)
  {
    target_lock = trx_lock_deque[i];

    if (target_lock->mode == lock_t::Mode::EXCLUSIVE && target_lock->state != lock_t::State::OBSOLETE)
      lock_manager.ChangeRecord(target_lock, -target_lock->diff); // rollback

    lock_manager.ReleaseLock(target_lock, global_mutex);
  }
}

// Commit with global mutex
// When transactions are doen, this function should be called.
// This function returns the commit id on success.
// However, even if the commit was successful, if the commit id exceeds last_global_execution_order, -1 is returned.
int TrxManager::Commit(std::mutex& global_mutex)
{
  TrxNode* trx = GetTrxNode(std::this_thread::get_id());

  std::deque<lock_t*>& trx_lock_deque = trx->trx_lock_deque;

  lock_t* target_lock = nullptr;

  int64_t diff = 0;

  std::queue<int> record_id_queue;

  std::queue<int64_t> record_value_queue;

  std::string file_name = "thread" + std::to_string(trx->thread_number) + ".txt";

  std::ofstream fout(file_name, std::ios::app);


  // Get a commit id

  int commit_id = global_execution_count.fetch_add(1) + 1;

  // If commit id is bigger than last_global_execution_order,
  // Undo all modifications, then release the locks

  if (commit_id > last_global_execution_order)
  {
    for (int i = 0; i < trx_lock_deque.size(); i++)
    {
      target_lock = trx_lock_deque[i];

      if (target_lock->mode == lock_t::Mode::EXCLUSIVE && target_lock->state != lock_t::State::OBSOLETE)
        lock_manager.ChangeRecord(target_lock, -target_lock->diff); // rollback

      lock_manager.ReleaseLock(target_lock, global_mutex);
    }

    return -1;
  }

  // Otherwise, just release all locks.

  for (int i = 0; i < trx_lock_deque.size(); i++)
  {
    target_lock = trx_lock_deque[i];

    record_id_queue.push(target_lock->record_id);

    record_value_queue.push(lock_manager.GetRecord(target_lock));

    lock_manager.ReleaseLock(target_lock, global_mutex);
  }

  // Publish a commit log
  // [commit_id] [record_ids] [record_values]

  fout << commit_id;

  while (!record_id_queue.empty())
  {
    fout << " " << record_id_queue.front();

    record_id_queue.pop();
  }

  while (!record_value_queue.empty())
  {
    fout << " " << record_value_queue.front();

    record_value_queue.pop();
  }

  fout << "\n";

  fout.close();

  return commit_id;
}