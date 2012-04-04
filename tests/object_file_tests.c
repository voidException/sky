#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <database.h>
#include <object_file.h>
#include <bstring.h>

#include "minunit.h"

//==============================================================================
//
// Constants
//
//==============================================================================

struct tagbstring ROOT = bsStatic("tmp/db");
struct tagbstring OBJECT_TYPE = bsStatic("users");

struct tagbstring foo = bsStatic("foo");
struct tagbstring bar = bsStatic("bar");
struct tagbstring baz = bsStatic("baz");
struct tagbstring google = bsStatic("http://www.google.com/this is a test yay!!!");


//==============================================================================
//
// Test Cases
//
//==============================================================================

//--------------------------------------
// Open
//--------------------------------------

int test_ObjectFile_open() {
    struct stat buffer;
    int rc;
    
    copydb("simple");
    
    Database *database = Database_create(&ROOT);
    ObjectFile *object_file = ObjectFile_create(database, &OBJECT_TYPE);
    mu_assert(object_file->state == OBJECT_FILE_STATE_CLOSED, "Expected state initialize as closed");
    mu_assert(object_file->block_size == DEFAULT_BLOCK_SIZE, "Expected block size to be reset");

    rc = ObjectFile_open(object_file);
    mu_assert(rc == 0, "Object file could not be opened");

    mu_assert(object_file->state == OBJECT_FILE_STATE_OPEN, "Expected state to be open");
    mu_assert(object_file->block_count == 9, "Expected 9 blocks");
    mu_assert(object_file->block_size == 0x10000, "Expected block size to be 64K");

    rc = ObjectFile_lock(object_file);
    mu_assert(rc == 0, "Object file could not be locked");
    mu_assert(object_file->state == OBJECT_FILE_STATE_LOCKED, "Expected state to be locked");

    // Verify lock file.
    rc = stat("tmp/db/users/.lock", &buffer);
    mu_assert(rc == 0, "Expected lock file to exist");

    // Verify block info.
    mu_assert_block_info(0, 1, 1, 3, 1325376000000000LL, 1328054400000000LL, false);
    mu_assert_block_info(1, 8, 4, 5, 1325376000000000LL, 1328054400000000LL, false);
    mu_assert_block_info(2, 0, 6, 6, 1325376000000000LL, 1328054400000000LL, true);
    mu_assert_block_info(3, 3, 6, 6, 1330560000000000LL, 1333238400000000LL, true);
    mu_assert_block_info(4, 5, 6, 6, 1338508800000000LL, 1341100800000000LL, true);
    mu_assert_block_info(5, 2, 7, 9, 1325376000000000LL, 1328054400000000LL, false);
    mu_assert_block_info(6, 4, 10, 10, 1325376000000000LL, 1328054400000000LL, true);
    mu_assert_block_info(7, 7, 10, 10, 1333238400000000LL, 1335830400000000LL, true);
    mu_assert_block_info(8, 6, 10, 10, 1335830400000000LL, 1338508800000000LL, true);

    // Verify actions.
    mu_assert(object_file->action_count == 3, "Expected 3 actions");
    mu_assert_action(0, 1, "home_page");
    mu_assert_action(1, 2, "sign_up");
    mu_assert_action(2, 3, "sign_in");

    // Verify properties.
    mu_assert(object_file->property_count == 3, "Expected 3 properties");
    mu_assert_property(0, 1, "first_name");
    mu_assert_property(1, 2, "last_name");
    mu_assert_property(2, 3, "salary");

    rc = ObjectFile_unlock(object_file);
    mu_assert(rc == 0, "Object file could not be unlocked");
    mu_assert(object_file->state == OBJECT_FILE_STATE_OPEN, "Expected state to be open after unlock");

    // Verify lock is gone.
    rc = stat("tmp/db/users/.lock", &buffer);
    mu_assert(rc == -1, "Expected lock file to not exist");
    mu_assert(errno == ENOENT, "Expected stat error on lock file to be ENOENT");

    rc = ObjectFile_close(object_file);
    mu_assert(rc == 0, "Object file could not be closed");
    mu_assert(object_file->state == OBJECT_FILE_STATE_CLOSED, "Expected state to be closed");
    mu_assert(object_file->block_size == DEFAULT_BLOCK_SIZE, "Expected block size to be reset");

    ObjectFile_destroy(object_file);
    Database_destroy(database);

    return 0;
}


//--------------------------------------
// Add events
//--------------------------------------

int test_ObjectFile_add_event() {
    Event *event;
    
    cleandb();
    
    Database *database = Database_create(&ROOT);
    ObjectFile *object_file = ObjectFile_create(database, &OBJECT_TYPE);
    object_file->block_size = 128;

    mu_assert(ObjectFile_open(object_file) == 0, "");
    mu_assert(ObjectFile_lock(object_file) == 0, "");

    // Action-only event.
    event = Event_create(946684800000000LL, 10, 20);
    mu_assert(ObjectFile_add_event(object_file, event) == 0, "");
    Event_destroy(event);

    // Data-only event.
    event = Event_create(946684800000000LL, 11, 0);
    Event_set_data(event, 1, &foo);
    Event_set_data(event, 2, &bar);
    mu_assert(ObjectFile_add_event(object_file, event) == 0, "");
    Event_destroy(event);

    // Action+data event.
    event = Event_create(946688400000000LL, 11, 20);
    Event_set_data(event, 1, &foo);
    mu_assert(ObjectFile_add_event(object_file, event) == 0, "");
    Event_destroy(event);
    
    // More events added to test block splits.
    event = Event_create(946688400000000LL, 10, 21);
    Event_set_data(event, 1, &google);
    mu_assert(ObjectFile_add_event(object_file, event) == 0, "");
    Event_destroy(event);

    event = Event_create(946692000000000LL, 10, 22);
    mu_assert(ObjectFile_add_event(object_file, event) == 0, "");
    Event_destroy(event);

    // Verify database files.
    mu_assert_file("tmp/db/users/data", "tests/fixtures/db/object_file_test0/users/data");
    
    mu_assert(ObjectFile_unlock(object_file) == 0, "");
    mu_assert(ObjectFile_close(object_file) == 0, "");

    ObjectFile_destroy(object_file);
    Database_destroy(database);
    
    return 0;
}


//==============================================================================
//
// Setup
//
//==============================================================================

int all_tests() {
    mu_run_test(test_ObjectFile_open);
    mu_run_test(test_ObjectFile_add_event);
    return 0;
}

RUN_TESTS()