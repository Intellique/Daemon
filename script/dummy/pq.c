#include <stddef.h>
#include <libpq-fe.h>

int main() {
	PGconn * connect = PQsetdbLogin(NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	return 0;
}

