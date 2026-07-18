#include "server.hpp"

int main() {
    bool condition = true;
    
    int fd = shared_memory_open();
    if (fd < 0) {
        return EXIT_FAILURE;
    }
    shared_memory_truncate(fd, PAGE_SIZE);

    while (condition) {
        print_menu();

        int choice;
        cout << "> ";
        cin >> choice;

        if (!cin) {
            cin.clear();
            cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        switch (choice) {
            case 0: {
                uint32_t value;
                cout << INFO << " Value to write - ";
                while (!(cin >> std::hex >> value)) {
                    cin.clear();
                    cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    cout << INFO << " Value to write - ";
                }
                write_data(fd, value);
                break;
            }
                
            case 1: {
                uint32_t value {};
                read_data(fd, &value);
                break;
            }
            case 2: {
                clear_ring_buffer(fd);
                break;
            }
            case 3: {
                cout << INFO << " Goodbye" << endl;
                condition = false;
                break;
            }
            default: {
                cout << INFO  << " No such option" << endl;
                break;
            }
        }
    }
    close_shared_memory_fd(fd);
}