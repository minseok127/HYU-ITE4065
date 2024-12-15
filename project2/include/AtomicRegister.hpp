#ifndef ATOMICREGISTER_HPP
#define ATOMICREGISTER_HPP

// Single Writer Multi Reader (SWMR) Atomic register
// Writer writes a value to the register with its own time stamp

// So, atomic register has two variable, time stamp and value.
// Then is it neccessary to read or write these two variable atomically?

// Atomic register is used for atomic snapshot.
// Atomic snapshot wants to get 'clean collect' which means no writes occured while reading.
// So let's solve this problem from making atomic snapshot.

// In this example, value1 is created at timestamp1 and value2 is created at timestamp2.
// At first, register has value1 and timestamp1. After writer changes the register, it has value2 and timestamp2.

// 1. Writer writes timestamp first, then writes the value.
// If the reader reads timestamp2 - value1 and the atomic register has timestamp2 - value2 at comparing,
// It means that the writer changed only the timestamp at first, then reader reads that register.
// After that, the writer writes the value2 into the register and the reader starts comparing.

// In this case, if value1 and value2 are same, it can be a problem.
// Because the reader's value is created at timestamp1 and the register's value is created at timestamp2.
// But the reader can't distinguish them.

// 2. Writer writes the value first, then writes the timestamp
// If the reader reads value1 - timestamp1 and the atomic register has value2 - timestamp1 at comparing,
// It means that the reader reads both value and timestamp correctly, then writer writes only the value.
// After that, the reader starts comparing.

// Also, if value1 and value2 are same, it can be a problem.
// Because the reader's value is created at timestamp1 and the register's value is created at timstamp2.
// But the reader can't distinguish them.

// Fortunetly, this project use the type of value as 'int', 4byte.
// So we can combine timestamp and value into the one 8byte variable.
// Then atomic reading or atomic writing about timestamp and value are possible without any concurrency control.


#include <assert.h>
#include <stdint.h>


#define TIMESTAMP_INC                         ((uint64_t)(0x0000000100000000))

#define TIMESTAMP_MASK(timestamp_with_value)  ((uint64_t)(0xffffffff00000000) & static_cast<uint64_t>(timestamp_with_value))

#define VALUE_MASK(timestamp_with_value)      ((uint64_t)(0x00000000ffffffff) & static_cast<uint64_t>(timestamp_with_value))


class AtomicRegister
{
public:

  // Default constructor, value is initialized to 0
  AtomicRegister() {}

  // Constructor with value.
  AtomicRegister(int value) { write(value); }

  // Copy constructor.
  AtomicRegister(const AtomicRegister& r) { this->timestamp_with_value = r.timestamp_with_value; }

  // Move constructor.
  AtomicRegister(AtomicRegister&& r) { this->timestamp_with_value = r.timestamp_with_value; r.timestamp_with_value = 0; }

  // Compare whether the atomic registers are same.
  bool operator==(const AtomicRegister& r) { return this->timestamp_with_value == r.timestamp_with_value; }

  // Compare whether the atomic registers are different.
  bool operator!=(const AtomicRegister& r) { return this->timestamp_with_value != r.timestamp_with_value; }

  // Copy the timestamp and value of atomic register.
  void operator=(const AtomicRegister& r) { this->timestamp_with_value = r.timestamp_with_value; }

  // Read only the value.
  int read() { return static_cast<int>(VALUE_MASK(timestamp_with_value)); }

  // Write the value with increased timestamp.
  void write(int value)
  {
    uint64_t new_timestamp_mask = TIMESTAMP_MASK(timestamp_with_value) + TIMESTAMP_INC;

    assert(new_timestamp_mask != 0); // check overflow

    uint64_t new_value_mask = VALUE_MASK(value);

    timestamp_with_value = new_timestamp_mask | new_value_mask;
  }

private:

  uint64_t timestamp_with_value = 0;

};


#undef TIMESTAMP_INC  // ((uint64_t)(0x0000000100000000))

#undef TIMESTAMP_MASK // ((uint64_t)(0xffffffff00000000) & static_cast<uint64_t>(timestamp_with_value))

#undef VALUE_MASK     // ((uint64_t)(0x00000000ffffffff) & static_cast<uint64_t>(timestamp_with_value))


#endif  // ATOMICREGISTER_HPP
