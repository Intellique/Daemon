#include <openssl/whrlpool.h>

int main() {
	WHIRLPOOL_CTX whirlpool;
	unsigned char digest[WHIRLPOOL_DIGEST_LENGTH];
	WHIRLPOOL_Final(digest, &whirlpool);

	return 0;
}

