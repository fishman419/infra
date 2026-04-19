#include "file_io.h"
#include <iostream>
#include <cassert>

struct TestData {
    uint32_t id;
    uint64_t value;
    char name[16];
};

void test_basic_rw() {
    std::cout << "Test 1: Basic read/write..." << std::endl;

    infra::FileIO<> file("/tmp/test_file_io.dat");

    // Write at offset 0
    const char* msg = "Hello, FileIO!";
    ssize_t written = file.write_at(0, msg, strlen(msg) + 1);
    assert(written == strlen(msg) + 1);

    // Read back at offset 0
    char buf[64] = {0};
    ssize_t bytes_read = file.read_at(0, buf, strlen(msg) + 1);
    assert(bytes_read == strlen(msg) + 1);
    assert(strcmp(buf, msg) == 0);

    std::cout << "Test 1 passed" << std::endl;
}

void test_offset_read_write() {
    std::cout << "Test 2: Offset read/write..." << std::endl;

    infra::FileIO<> file("/tmp/test_file_io.dat");

    // Write at specific offset
    uint64_t big_value = 0x123456789ABCDEF0;
    ssize_t written = file.write_at(100, &big_value, sizeof(big_value));
    assert(written == sizeof(big_value));

    // Read back at same offset
    uint64_t read_value = 0;
    ssize_t bytes_read = file.read_at(100, &read_value, sizeof(read_value));
    assert(bytes_read == sizeof(read_value));
    assert(read_value == big_value);

    std::cout << "Test 2 passed" << std::endl;
}

void test_exact读写() {
    std::cout << "Test 3: Exact read/write..." << std::endl;

    infra::FileIO<> file("/tmp/test_file_io.dat");

    // Write exact number of bytes
    const char* data = "ExactTest";
    bool success = file.write_at_exact(200, data, strlen(data));
    assert(success);

    // Read exact number of bytes
    char read_buf[64] = {0};
    success = file.read_at_exact(200, read_buf, strlen(data));
    assert(success);
    assert(strcmp(read_buf, data) == 0);

    std::cout << "Test 3 passed" << std::endl;
}

void test_seek_tell() {
    std::cout << "Test 4: Seek and tell..." << std::endl;

    infra::FileIO<> file("/tmp/test_file_io.dat");

    off_t pos = file.seek(50);
    assert(pos == 50);

    pos = file.seek(0, SEEK_CUR);
    assert(pos == 50);

    off_t size = file.size();
    assert(size > 0);

    std::cout << "Test 4 passed" << std::endl;
}

void test_truncate() {
    std::cout << "Test 5: Truncate..." << std::endl;

    infra::FileIO<> file("/tmp/test_file_io.dat");

    bool success = file.truncate(500);
    assert(success);

    assert(file.size() == 500);

    std::cout << "Test 5 passed" << std::endl;
}

void test_struct读写() {
    std::cout << "Test 6: Structured read/write..." << std::endl;

    infra::FileIO<> file("/tmp/test_file_io.dat");

    TestData original = {42, 0xDEADBEEF, "TestStruct"};
    bool success = file.write_at_exact(300, &original, sizeof(TestData));
    assert(success);

    TestData read_data;
    success = file.read_at_exact(300, &read_data, sizeof(TestData));
    assert(success);
    assert(read_data.id == original.id);
    assert(read_data.value == original.value);
    assert(strcmp(read_data.name, original.name) == 0);

    std::cout << "Test 6 passed" << std::endl;
}

void test_thread_safe() {
    std::cout << "Test 7: Thread-safe version..." << std::endl;

    infra::FileIO<true> file("/tmp/test_file_io.dat");

    uint64_t value = 12345;
    bool success = file.write_at(400, &value, sizeof(value));
    assert(success);

    uint64_t read_value = 0;
    success = file.read_at(400, &read_value, sizeof(read_value));
    assert(success);
    assert(read_value == value);

    std::cout << "Test 7 passed" << std::endl;
}

int main() {
    std::cout << "Running FileIO tests..." << std::endl;

    test_basic_rw();
    test_offset_read_write();
    test_seek_tell();
    test_truncate();
    test_struct读写();
    test_thread_safe();

    // Clean up
    unlink("/tmp/test_file_io.dat");

    std::cout << "\nAll FileIO tests passed!" << std::endl;
    return 0;
}
