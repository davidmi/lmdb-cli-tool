mdbx(1) -- LMDB Explorer
====

## SYNOPSIS

`mdbx` ACTION [OPTION [ARG] [OPTION [ARG] ...]]  <lmdb env path><br>
ACTIONS p|print a|printall w|write  d|delete h|help<br>
`-d`|`--db-name` <name><br>
`-k`|`--key` <key> -- string key, required for print<br>
`-N`|`--num-data` -- print/input data as number<br>
`-n`|`--num-key`  -- input key as number<br>
`-X`|`--hex-data` -- print/input data as number<br>
`-x`|`--hex-key`  -- input key as number)<br>

## DESCRIPTION

Command-line tool for inspecting and editing the contents of
(LMDB)[http://www.lmdb.tech/doc/] databases.

The default mode of operation is to interpret keys and values as ASCII
strings. Using the -N, -n, -x, and -X flags, they can be coded as 32-bit
integers or hex values. 

## COMPILING

Requires `pkg-config`, `getopt`, and `liblmdb`.
Run `make` to compile.

To install, run `make install`. Requires `ruby-ronn` to build the man page.

## TESTS

Requires `check`.
Run `make test` to compile.
`./test` to run tests.

## EXAMPLES

### Inspecting data

List all databases:

    ./mdbx p DB_DIR

Print a value decoded as a string:

    ./mdbx p -k foo DB_DIR

Print a value decoded as a 32-bit integer: 

    ./mdbx p -N -k foo DB_DIR

Print a value using a 32-bit integer key: 

    ./mdbx p -nk 123 DB_DIR

Print a value decoded as hex: 

    ./mdbx p -X -k foo DB_DIR

Print a value using a key encoded as hex: 

    ./mdbx p -X -k a0f0a1 DB_DIR

### Editing data

Write a value (this creates a new database if there isn't one
at the location already): 

    ./mdbx w -k foo -v bar DB_DIR

Write a value to a named database (this creates the : 

    ./mdbx w -d somename -k foo -v bar DB_DIR

Write a value with a binary key encoded as hex: 

    ./mdbx w -xk a0f0a1 -v bar DB_DIR

Write a value with a binary value encoded as hex: 

    ./mdbx w -xk a0f0a1 -Xv 0a120b DB_DIR

Write a value with a 32-bit integer key: 

    ./mdbx w -nk 123 -v bar DB_DIR

Write a 32-bit integer value: 

    ./mdbx w -k foo -Nv bar DB_DIR

## CONTRIBUTING

Reporting Issues: Please create a GitHub Issue with details as to how
to reproduce your issue.

Submitting Code: Please create a GitHub pull request.

Please install the pre-commit hook, which does the following:

1. Runs all code through astyle --style=linux. Please conform
to the LMDB project style for anything not covered by Astyle.

2. Generates and commit the man page (mdbx.1) using the ruby-ronn tool

All code submissions require a corresponding test, and must pass all existing
tests.
