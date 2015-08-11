#include <time.h>

int main() {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	return 0;
}

