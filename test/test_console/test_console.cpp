#include "../../lib/RemoteConsole/src/ConsoleObservers.h"
#include <unity.h>

using namespace remote_console;

void setUp() {}
void tearDown() {}

static void observers_share_stream_until_last_viewer_leaves() {
    Observers observers;
    TEST_ASSERT_TRUE(observers.viewers(0).empty());
    observers.subscribe(1, true, 100);
    observers.subscribe(2, true, 200);
    observers.subscribe(1, false, 300);
    auto viewers = observers.viewers(300);
    TEST_ASSERT_EQUAL(1, viewers.size());
    TEST_ASSERT_EQUAL(2, viewers[0]);
    observers.subscribe(2, false, 400);
    TEST_ASSERT_TRUE(observers.viewers(400).empty());
}

static void leases_renew_and_expire_independently() {
    Observers observers;
    observers.subscribe(1, true, 0);
    observers.subscribe(2, true, 0);
    observers.subscribe(2, true, RENEW_MS);
    auto viewers = observers.viewers(LEASE_MS);
    TEST_ASSERT_EQUAL(1, viewers.size());
    TEST_ASSERT_EQUAL(2, viewers[0]);
    TEST_ASSERT_TRUE(observers.viewers(RENEW_MS + LEASE_MS).empty());
}

static void lease_survives_millis_rollover() {
    const uint32_t renewed = UINT32_MAX - 100;
    TEST_ASSERT_TRUE(leaseActive(50, renewed));
    TEST_ASSERT_FALSE(leaseActive(renewed + LEASE_MS, renewed));
}

static void overflow_retains_recent_records_and_reports_all_losses() {
    Buffer<2> buffer;
    Record record;
    record.sequence = 1;
    record.dropped = 5; // Already lost upstream; retain this information when this record is evicted.
    buffer.push(record);
    record.sequence = 2;
    record.dropped = 0;
    buffer.push(record);
    record.sequence = 3;
    buffer.push(record);
    TEST_ASSERT_TRUE(buffer.pop(record));
    TEST_ASSERT_EQUAL(2, record.sequence);
    TEST_ASSERT_EQUAL(6, record.dropped);
    TEST_ASSERT_TRUE(buffer.pop(record));
    TEST_ASSERT_EQUAL(3, record.sequence);
    TEST_ASSERT_EQUAL(0, record.dropped);
    TEST_ASSERT_FALSE(buffer.pop(record));
}

static void stopping_clears_old_session_and_loss_counters() {
    Buffer<1> buffer;
    Record record;
    buffer.push(record);
    buffer.push(record);
    buffer.clear();
    TEST_ASSERT_FALSE(buffer.pop(record));
    record.sequence = 42;
    buffer.push(record);
    TEST_ASSERT_TRUE(buffer.pop(record));
    TEST_ASSERT_EQUAL(42, record.sequence);
    TEST_ASSERT_EQUAL(0, record.dropped);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(observers_share_stream_until_last_viewer_leaves);
    RUN_TEST(leases_renew_and_expire_independently);
    RUN_TEST(lease_survives_millis_rollover);
    RUN_TEST(overflow_retains_recent_records_and_reports_all_losses);
    RUN_TEST(stopping_clears_old_session_and_loss_counters);
    return UNITY_END();
}
