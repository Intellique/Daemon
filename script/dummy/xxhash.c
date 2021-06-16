#include <xxhash.h>

struct XXH64_state_s {
    unsigned long long total_len;
    unsigned long long seed;
    unsigned long long v1;
    unsigned long long v2;
    unsigned long long v3;
    unsigned long long v4;
    unsigned long long mem64[4];   /* defined as U64 for alignment */
    unsigned int memsize;
};   /* typedef'd to XXH64_state_t within xxhash.h */

int main() {
	struct XXH64_state_s xxhash;
	XXH64_reset(&xxhash, 0);
	XXH64_digest(&xxhash);

	return 0;
}
