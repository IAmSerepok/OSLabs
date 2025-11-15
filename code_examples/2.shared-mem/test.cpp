#include "my_shmem.hpp"
#include <iostream>
#include <string>

#define MEMO_TEXT "Init memo"
struct my_data
{
	my_data() {
		memset(memo,0,sizeof(memo));
        memcpy(memo,MEMO_TEXT,strlen(MEMO_TEXT));
		counter = 0;
		value = 0.0;
	}
	void print() {
		std::cout << memo << "; " << counter << "; " << value;
	}
	char        memo[256];
	int         counter;
	double      value;
};

int main(int argc, char** argv)
{
	cplib::SharedMem<my_data> shared_data("mymemo");
    if (!shared_data.IsValid()) {
        std::cout << "Failed to create shared memory block!" << std::endl;
        return -1;
    }
	std::string param;
	while (true) {
		std::cin >> param;
		if (param == "show" || param == "s") {
			std::cout << "Shared data: {";
			shared_data.Lock();
			shared_data.Data()->print();
			shared_data.Unlock();
			std::cout << "}" << std::endl;
		} else if (param == "modify" || param == "m" ) {
			shared_data.Lock();
			shared_data.Data()->counter++;
			shared_data.Data()->value += 2.0;
            sprintf(shared_data.Data()->memo,"Memo val %d",shared_data.Data()->counter);
			shared_data.Unlock();
			std::cout << "Shared data modified!" << std::endl;
		} else if (param == "exit" || param == "quite" || param == "e" || param == "q")
			break;
		else
			std::cout << "unknown param! Use 's' to show or 'm' to modify!" << std::endl;
	}
    return 0;
}