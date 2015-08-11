#include <nettle/md5.h>

int main() {
	struct md5_ctx md5;
	unsigned char digest[MD5_DIGEST_SIZE];
	md5_digest(&md5, MD5_DIGEST_SIZE, digest);

	return 0;
}

