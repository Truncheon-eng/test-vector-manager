#include "server.hpp"

int main() {
    bool condition = true;
    
    int fd = shared_memory_open();
    if (fd < 0) {
        return EXIT_FAILURE;
    }
    shared_memory_truncate(fd, SIZE);

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
            case 0:
                write_data(fd, ARR_SIZE);
                break;
            case 1:
                read_data(fd, ARR_SIZE);
                break;
            case 2:
                cout << INFO << " Goodbye" << endl;
                condition = false;
                break;
            default:
                cout << INFO  << " No such option" << endl;
                break;
        }
    }
    close_shared_memory_fd(fd);
}