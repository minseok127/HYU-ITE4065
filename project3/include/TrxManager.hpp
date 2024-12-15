#ifndef TRXMANAGER_HPP
#define TRXMANAGER_HPP

// In this project, transactions are simply thought as threads.
// Therefore, we will use the terms 'thread' and 'transaction' as a same word.

#include <shared_mutex>
#include <thread>
#include <unordered_map>

#include "TrxNode.hpp"


// Transaction Manager
// This manager provides transaction APIs
class TrxManager
{
public:

  // A value indicating whether find() or update() succeeded.
  enum Response
  {
    SUCCESS, MUST_ABORTED
  };

  // Constructor
  TrxManager() {};

  // Set last_global_execution_order
  void SetLastCommitId(int last_global_execution_order) { this->last_global_execution_order = last_global_execution_order; }

  // Using thread id, return transaction node.
  TrxNode* GetTrxNode(std::thread::id thread_id);

  // Initialize transaction node's information
  void Begin();

  // Acquire a shared lock about the record.
  // If the acquisition is successful, the record value is stored in ret and SUCCESS is returned.
  // If it fails, MUST_ABORTED is returned. Then caller must call Abort().
  Response Find(int record_id, int64_t* ret);

  // Acquire an exclusive lock on the record and change the value by 'diff'.
  // If the acquisition is successful, the changed record value is stored in ret and SUCCESS is returned.
  // If it fails, MUST_ABORTED is returned. Then caller must call Abort().
  Response Update(int record_id, int64_t diff, int64_t* ret);

  // This function should be called when Find() or Update() fails.
  // Undo all modifications of this transaction.
  void Abort();

  // When transactions are doen, this function should be called.
  // This function returns the commit id on success.
  // However, even if the commit was successful, if the commit id exceeds last_global_execution_order, -1 is returned.
  int Commit();

  // Find with global mutex
  // Acquire a shared lock about the record.
  // If the acquisition is successful, the record value is stored in ret and SUCCESS is returned.
  // If it fails, MUST_ABORTED is returned. Then caller must call Abort().
  Response Find(int record_id, int64_t* ret, std::mutex& global_mutex);

  // Update with global mutex
  // Acquire an exclusive lock on the record and change the value by 'diff'.
  // If the acquisition is successful, the changed record value is stored in ret and SUCCESS is returned.
  // If it fails, MUST_ABORTED is returned. Then caller must call Abort().
  Response Update(int record_id, int64_t diff, int64_t* ret, std::mutex& global_mutex);

  // Abort with global mutex
  // This function should be called when Find() or Update() fails.
  // Undo all modifications of this transaction.
  void Abort(std::mutex& global_mutex);

  // Commit with global mutex
  // When transactions are doen, this function should be called.
  // This function returns the commit id on success.
  // However, even if the commit was successful, if the commit id exceeds last_global_execution_order, -1 is returned.
  int Commit(std::mutex& global_mutex);

private:

  // rwlock for transaction table
  std::shared_mutex trx_table_latch;

  // Transaction table. Key is a thread id and value is a transaction node.
  std::unordered_map<std::thread::id, TrxNode*> trx_node_table;

  // According to the task specification, 
  // The total number of commits of all transactions must be applied until this value.
  // If a commit that exceeds this value occurs, that will be aborted.
  int last_global_execution_order = -1;

  // The total number of commits of all transactions.
  std::atomic<int> global_execution_count{0};

  // thread#
  int thread_number = 1;
};

#endif  // TRXMANAGER_HPP