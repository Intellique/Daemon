#include <time.h>

int main() {
	struct timespec clock;
	clock_gettime(CLOCK_MONOTONIC, &clock);
	return 0;
}

