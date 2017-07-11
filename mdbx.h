#ifndef MDBX_H
#define MDBX_H
#include <stdbool.h>
#include <lmdb.h>

/** Opaque struct wrapping lmdb env, txn, and dbi*/
struct mdbx_env;

/** Representation of dynamically allocated decoded data */
struct decoded_val {
	struct MDB_val val;
};


int mdbx_env_make(char *mdb_env_path, char *db_name, unsigned int env_flags, unsigned int txn_flags, unsigned int dbi_flags, struct mdbx_env *env);
void mdbx_env_destroy(struct mdbx_env *e, bool commit);

int mdbx_main(int argc, char *argv[]);

#endif
