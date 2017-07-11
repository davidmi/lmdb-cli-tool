#!/bin/bash
set -xe;

export TEST_DB_DIR=/tmp/mdbx_test_db
if [ "$1" = "valg" ];
then
    export TOOL='valgrind --tool=memcheck --leak-check=yes --show-reachable=yes --num-callers=20 --track-fds=yes --error-exitcode=1'
fi;

# Run unit tests
./test

# Run integration tests
rm -rf ${TEST_DB_DIR};
mkdir -p ${TEST_DB_DIR};

$TOOL ./mdbx w -k foo -v bar ${TEST_DB_DIR};
$TOOL ./mdbx p ${TEST_DB_DIR} | grep foo;
$TOOL ./mdbx p -k foo ${TEST_DB_DIR};

# Test delete
$TOOL ./mdbx d -k foo ${TEST_DB_DIR};
$TOOL ./mdbx p ${TEST_DB_DIR} | test ! $(grep foo);

$TOOL ./mdbx w -d db1 -k foo2 -v bar ${TEST_DB_DIR};
$TOOL ./mdbx p  ${TEST_DB_DIR} | test ! $(grep foo2);
$TOOL ./mdbx p -d db1 ${TEST_DB_DIR} | test ! $(grep bar);
$TOOL ./mdbx p -d db1 ${TEST_DB_DIR} | grep foo2;
$TOOL ./mdbx p -d db1 -k foo2 ${TEST_DB_DIR} | grep bar;

$TOOL ./mdbx w -d db1 -k foo2 -v bar ${TEST_DB_DIR};
$TOOL ./mdbx p  ${TEST_DB_DIR} | test ! $(grep foo2);
$TOOL ./mdbx p -d db1 ${TEST_DB_DIR} | test ! $(grep bar);
$TOOL ./mdbx p -d db1 ${TEST_DB_DIR} | grep foo2;
$TOOL ./mdbx p -d db1 -k foo2 ${TEST_DB_DIR} | grep bar;

# Test numeric
$TOOL ./mdbx w -nk 1 -Nv 10 ${TEST_DB_DIR};
$TOOL ./mdbx p -N ${TEST_DB_DIR} | grep 1;
$TOOL ./mdbx p -N ${TEST_DB_DIR} | test ! $(grep foo);
$TOOL ./mdbx p -nk 1 -X  ${TEST_DB_DIR} | grep 0000000a;
$TOOL ./mdbx p -nk 1 -N  ${TEST_DB_DIR} | grep 10;

# Test numeric delete
$TOOL ./mdbx d -nk 1 ${TEST_DB_DIR};
$TOOL ./mdbx p -N ${TEST_DB_DIR} | test ! $(grep 1);

# Test hex
$TOOL ./mdbx w -nk 2 -Xv 414243 ${TEST_DB_DIR};
$TOOL ./mdbx p -nk 2 ${TEST_DB_DIR} | grep ABC;
$TOOL ./mdbx w -xk 414243 -Nv 10 ${TEST_DB_DIR};
$TOOL ./mdbx p ${TEST_DB_DIR} | grep ABC;

# Test hex delete
$TOOL ./mdbx d -xk 414243 ${TEST_DB_DIR};
$TOOL ./mdbx p ${TEST_DB_DIR} | test ! $(grep ABC);

# Test dump all data
$TOOL ./mdbx w -k foo -v abc -d dumpdb ${TEST_DB_DIR};
$TOOL ./mdbx w -k bar -v 123 -d dumpdb ${TEST_DB_DIR};
$TOOL ./mdbx a -d dumpdb ${TEST_DB_DIR} | grep abc;
$TOOL ./mdbx a -d dumpdb ${TEST_DB_DIR} | grep 123;

rm -rf ${TEST_DB_DIR};
