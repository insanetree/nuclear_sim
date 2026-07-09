#include "controller/ipc.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <new>
#include <sys/mman.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace {

template<typename T>
T*
create_and_map(const char* name, int& fd)
{
	fd = shm_open(name, O_RDWR, 0666);
	if (fd == -1) {
		spdlog::error("shm_open({}) failed: {}", name, std::strerror(errno));
		return nullptr;
	}
	void* addr = mmap(nullptr, sizeof(T), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED) {
		spdlog::error("mmap({}) failed: {}", name, std::strerror(errno));
		close(fd);
		fd = -1;
		return nullptr;
	}
	return static_cast<T*>(addr);
}

}

int
main()
{
	int control_panel_fd = -1;
	int status_panel_fd = -1;
	
	controller::status_panel* status_panel = create_and_map<controller::status_panel>(controller::shm_status_panel_name, status_panel_fd);
	controller::control_panel* control_panel = create_and_map<controller::control_panel>(controller::shm_control_panel_name, control_panel_fd);

	if(!status_panel || !control_panel) {
		spdlog::error("Failed to map shared memory segments");
		return EXIT_FAILURE;
	}

	if(std::strncmp(status_panel->magic_value, "STATPNL", 8) != 0) {
		spdlog::error("Status panel magic value mismatch");
		return EXIT_FAILURE;
	}

	if(std::strncmp(control_panel->magic_value, "CTRLPNL", 8) != 0) {
		spdlog::error("Control panel magic value mismatch");
		return EXIT_FAILURE;
	}

	munmap(status_panel, sizeof(controller::status_panel));
	munmap(control_panel, sizeof(controller::control_panel));
	close(status_panel_fd);
	close(control_panel_fd);
	return 0;
}
