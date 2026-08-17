#ifndef SOME_HPP
#define SOME_HPP

#include <cstdlib>
#include <stddef.h>
#include <type_traits>
#include <cstdint>

template <int SIZE, typename DataType>
class RingBuffer {
private:
    DataType data[SIZE];
    size_t write_count; 
    size_t read_count;
    size_t mask = SIZE - 1;

public:
    static_assert(SIZE > 0 && (SIZE & (SIZE - 1)) == 0, "SIZE has to be a power of 2");
    static_assert(std::is_trivially_copyable_v<DataType>);
    static_assert(std::is_standard_layout_v<DataType>);
    
    static constexpr uint32_t magic_value = 0xdeadbeef;
    uint32_t magic;

    inline RingBuffer(): write_count{0}, read_count{0}, magic{magic_value} {}

    inline int is_empty() {
        if (read_count == write_count)
            return 1;
        return 0;
    }

    inline int is_full() {

        if ((write_count - read_count) & (~mask))
            return 1;
        return 0;
    }

    inline int write_data(const DataType &data) {
        if (is_full())
            return EXIT_FAILURE;
        this -> data[write_count++ & mask] = data;
        return EXIT_SUCCESS;
    }

    inline int read_data(DataType& data) {
        if (is_empty())
            return EXIT_FAILURE;
        data = this -> data[read_count++ & mask]; 
        return EXIT_SUCCESS;
    }
};

#endif