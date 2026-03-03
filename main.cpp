#include <iostream>
#include <fstream>
#include <cstdint>
#include <array>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>

#define PasswordKey_Offset 0x00135120

pid_t get_pid(std::string process_name) {
    try {
        std::string command = "pidof " + process_name + " 2>/dev/null";
        FILE* process = popen(command.c_str(), "r");

        if (!process)
            throw std::runtime_error("popen() failed.");

        int pid_string_size = 32;
		char pid_string[pid_string_size] = {0};

		if (fgets(pid_string, pid_string_size, process) == NULL)
			throw std::runtime_error("fgets() failed.");

		pid_t pid = atoi(pid_string);
		
		if (pclose(process) == -1)
			throw std::runtime_error("pclose() failed.");

		return pid;
    }
    catch (std::runtime_error& e) {
        std::cerr << "get_pid() failed: " << e.what() << std::endl;
        perror("perror()");

        return 0;
    }
}

void* get_module_base(std::string module_name) {
    try {
        pid_t pid = get_pid("keepassxc");

        std::string filename = "/proc/" + std::to_string(pid) + "/maps";
        std::ifstream maps_file(filename);

        if (!maps_file.good())
            throw std::runtime_error("Failed to open maps file (are you root?).");

        std::string line;
        while (std::getline(maps_file, line)) {
            if (line.find(module_name) != std::string::npos) {
                break;
            }
        }

        std::string base_address_str = line.substr(0, line.find('-'));
        return reinterpret_cast<void*>(std::stoull(base_address_str, nullptr, 16));
    }
    catch (std::runtime_error& e) {
        std::cerr << "get_module_base() failed: " << e.what() << std::endl;
        return nullptr;
    }
}

using trampoline_type = void (*)(void* rdi, void* rsi, void* rdx, void* rcx, void* r8, void* r9);
static trampoline_type trampoline{};

void password_key_hook(void* rdi, void* rsi, void* rdx, void* rcx, void* r8, void* r9) {
    try {
        // password is at (*rsi)+(0x8*3)
        auto password_addr = reinterpret_cast<char16_t*>((*reinterpret_cast<uintptr_t*>(rsi))+(0x8*3));
        std::u16string password(password_addr);

        // create tmp file
        char password_filename[] = "/tmp/password_XXXXXX";
        int fd = mkstemp(password_filename);
        if (fd == -1)
            throw std::runtime_error("mkstemp() failed.");

        write(fd, password.c_str(), password.length() * 2); // x2 because the password is stored in utf-16
        write(fd, "\x00\x00", 2); // null terminator
        close(fd);

        trampoline(rdi, rsi, rdx, rcx, r8, r9);
    }
    catch (std::runtime_error& e) {
        //std::cerr << "get_pid() failed: " << e.what() << std::endl;
        return;
    }
}

std::array<unsigned char, 13> create_jump_instr(const void* const dst) {
    // mov r10, 0xaabbccddeeff; jmp r10
    std::array<unsigned char, 13> jump_instr{{0x49, 0xba, 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x00, 0x41, 0xff, 0xe2}};

    const auto address = reinterpret_cast<uintptr_t>(dst);
    std::memcpy(&jump_instr[2], &address, sizeof(dst));

    return jump_instr;
}

void* create_trampoline(void* const target_addr, size_t size) {
    auto jump_back = create_jump_instr(reinterpret_cast<unsigned char*>(target_addr) + size);

    // trampoline needs to be big enough to store overwritten instructions and jmp back instructions
    void* trampolineStub = mmap(NULL, size+jump_back.size(), PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    std::memcpy(trampolineStub, target_addr, size);
    std::memcpy(reinterpret_cast<unsigned char*>(trampolineStub)+size, jump_back.data(), jump_back.size());

    return trampolineStub;
}

__attribute__ ((constructor))
void startup()
{
    // get location of PasswordKey function
    uintptr_t base_addr = reinterpret_cast<uintptr_t>(get_module_base("keepassxc"));
    uintptr_t PasswordKey_addr = base_addr + PasswordKey_Offset;

    // make function writeable
    size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    void* PasswordKey_page_addr = reinterpret_cast<void*>(PasswordKey_addr & -page_size);
    mprotect(PasswordKey_page_addr, page_size, PROT_READ | PROT_WRITE | PROT_EXEC);

    trampoline = reinterpret_cast<trampoline_type>(create_trampoline(reinterpret_cast<void*>(PasswordKey_addr), 14));

    // set hook
    //std::memset(reinterpret_cast<void*>(PasswordKey_addr), 0x90, 14); // replace overwritten instructions with NOPs (helps debugging)
    auto jump_instr = create_jump_instr(reinterpret_cast<void*>(&password_key_hook));
    std::memcpy(reinterpret_cast<void*>(PasswordKey_addr), jump_instr.data(), jump_instr.size());
}

__attribute__ ((destructor))
void shutdown()
{
}
