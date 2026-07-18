#ifndef SOME_HPP
#define SOME_HPP

#include <cstdlib>
#include <stddef.h>
#include <type_traits>
#include <cstdint>


/*
    Template, представляющий из себя реализацию кольцевого буфера.
    Имеются два параметра:
        SIZE - типа int; размер кольцевого буфера; сам размер должен являться 
        степенью двойки
        DataType - тип хранимых данных;
*/
template <int SIZE, typename DataType>
class RingBuffer {
private:
    DataType data[SIZE];
    size_t write_count; // указтель на место записи данных
    size_t read_count; // указатель на место, из которого происходит чтение данных
    size_t mask = SIZE - 1; // маска

public:
    static_assert(SIZE > 0 && (SIZE & (SIZE - 1)) == 0, "SIZE has to be a power of 2"); // проверка на степень 2-ки
    static_assert(std::is_trivially_copyable_v<DataType>);
    static_assert(std::is_standard_layout_v<DataType>);
    
    static constexpr uint32_t magic_value = 0xdeadbeef;
    uint32_t magic;

    inline RingBuffer(): write_count{0}, read_count{0}, magic{magic_value} {}

    inline int is_empty() {
        /*
            В случае, если "указатели" равны межу собой, считаем, что кольцевой
            буфер пустой.
        */
        if (read_count == write_count)
            return 1;
        return 0;
    }

    inline int is_full() {
        /*
            Если "указатель" записи обгоняет "указтель" чтения ровно на
            размер буфера, то считаем, что буфер полностью заполнен.
        */
        if ((write_count - read_count) & (~mask))
            return 1;
        return 0;
    }

    inline int write_data(const DataType &data) {
        // запись данных с предварительной проверкой на "заполненность"
        if (is_full())
            return EXIT_FAILURE;
        this -> data[write_count++] = data;
        return EXIT_SUCCESS;
    }

    inline int read_data(DataType& data) {
        // чтение данных с предварительной проверкой на "пустоту"  
        if (is_empty())
            return EXIT_FAILURE;
        data = this -> data[read_count++]; 
        return EXIT_SUCCESS;
    }
};

#endif