#include <uuid/uuid.h>

int main() {
	uuid_t uuid;
	uuid_generate(uuid);
	return 0;
}

