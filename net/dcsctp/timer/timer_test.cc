/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/timer/timer.h"

#include <memory>

#include "absl/types/optional.h"
#include "net/dcsctp/public/timeout.h"
#include "net/dcsctp/timer/fake_timeout.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::Return;

class TimerTest : public testing::Test {
 protected:
  TimerTest()
      : timeout_manager_([this]() { return now_; }),
        manager_([this]() { return timeout_manager_.CreateTimeout(); }) {
    ON_CALL(on_expired_, Call).WillByDefault(Return(absl::nullopt));
  }

  void AdvanceTimeAndRunTimers(DurationMs duration) {
    now_ = TimeMs(*now_ + *duration);

    for (TimeoutID timeout_id : timeout_manager_.RunTimers()) {
      manager_.HandleTimeout(timeout_id);
    }
  }

  TimeMs now_ = TimeMs(0);
  FakeTimeoutManager timeout_manager_;
  TimerManager manager_;
  testing::MockFunction<absl::optional<DurationMs>()> on_expired_;
};

TEST_F(TimerTest, TimerIsInitiallyStopped) {
  std::unique_ptr<Timer> t1 = manager_.CreateTimer(
      "t1", on_expired_.AsStdFunction(),
      TimerOptions(DurationMs(5000), TimerBackoffAlgorithm::kFixed));

  EXPECT_FALSE(t1->is_running());
}

TEST_F(TimerTest, TimerExpiresAtGivenTime) {
  std::unique_ptr<Timer> t1 = manager_.CreateTimer(
      "t1", on_expired_.AsStdFunction(),
      TimerOptions(DurationMs(5000), TimerBackoffAlgorithm::kFixed));

  EXPECT_CALL(on_expired_, Call).Times(0);
  t1->Start();
  EXPECT_TRUE(t1->is_running());

  AdvanceTimeAndRunTimers(DurationMs(4000));

  EXPECT_CALL(on_expired_, Call).Times(1);
  AdvanceTimeAndRunTimers(DurationMs(1000));
}

TEST_F(TimerTest, TimerReschedulesAfterExpiredWithFixedBackoff) {
  std::unique_ptr<Timer> t1 = manager_.CreateTimer(
      "t1", on_expired_.AsStdFunction(),
      TimerOptions(DurationMs(5000), TimerBackoffAlgorithm::kFixed));

  EXPECT_CALL(on_expired_, Call).Times(0);
  t1->Start();
  EXPECT_EQ(t1->expiration_count(), 0);

  AdvanceTimeAndRunTimers(DurationMs(4000));

  // Fire first time
  EXPECT_CALL(on_expired_, Call).Times(1);
  AdvanceTimeAndRunTimers(DurationMs(1000));
  EXPECT_TRUE(t1->is_running());
  EXPECT_EQ(t1->expiration_count(), 1);

  EXPECT_CALL(on_expired_, Call).Times(0);
  AdvanceTimeAndRunTimers(DurationMs(4000));

  // Second time
  EXPECT_CALL(on_expired_, Call).Times(1);
  AdvanceTimeAndRunTimers(DurationMs(1000));
  EXPECT_TRUE(t1->is_running());
  EXPECT_EQ(t1->expiration_count(), 2);

  EXPECT_CALL(on_expired_, Call).Times(0);
  AdvanceTimeAndRunTimers(DurationMs(4000));

  // Third time
  EXPECT_CALL(on_expired_, Call).Times(1);
  AdvanceTimeAndRunTimers(DurationMs(1000));
  EXPECT_TRUE(t1->is_running());
  EXPECT_EQ(t1->expiration_count(), 3);
}

TEST_F(TimerTest, TimerWithNoRestarts) {
  std::unique_ptr<Timer> t1 = manager_.CreateTimer(
      "t1", on_expired_.AsStdFunction(),
      TimerOptions(DurationMs(5000), TimerBackoffAlgorithm::kFixed,
                   /*max_restart=*/0));

  EXPECT_CALL(on_expired_, Call).Times(0);
  t1->Start();
  AdvanceTimeAndRunTimers(DurationMs(4000));

  // Fire first time
  EXPECT_CALL(on_expired_, Call).Times(1);
  AdvanceTimeAndRunTimers(DurationMs(1000));

  EXPECT_FALSE(t1->is_running());

  // Second time - shouldn't fire
  EXPECT_CALL(on_expired_, Call).Times(0);
  AdvanceTimeAndRunTimers(DurationMs(5000));
  EXPECT_FALSE(t1->is_running());
}

TEST_F(TimerTest, TimerWithOneRestart) {
  std::unique_ptr<Timer> t1 = manager_.CreateTimer(
      "t1", on_expired_.AsStdFunction(),
      TimerOptions(DurationMs(5000), TimerBackoffAlgorithm::kFixed,
                   /*max_restart=*/1));

  EXPECT_CALL(on_expired_, Call).Times(0);
  t1->Start();
  AdvanceTimeAndRunTimers(DurationMs(4000));

  // Fire first time
  EXPECT_CALL(on_expired_, Call).Times(1);
  AdvanceTimeAndRunTimers(DurationMs(1000));
  EXPECT_TRUE(t1->is_running());

  EXPECT_CALL(on_expired_, Call).Times(0);
  AdvanceTimeAndRunTimers(DurationMs(4000));

  // Second time - max restart limit reached.
  EXPECT_CALL(on_expired_, Call).Times(1);
  AdvanceTimeAndRunTimers(DurationMs(1000));
  EXPECT_FALSE(t1->is_running());

  // Third time - should not fire.
  EXPECT_CALL(on_expired_, Call).Times(0);
  AdvanceTimeAndRunTimers(DurationMs(5000));
  EXPECT_FALSE(t1->is_running());
}

TEST_F(TimerTest, TimerWithTwoRestart) {
  std::unique_ptr<Timer> t1 = manager_.CreateTimer(
      "t1", on_expired_.AsStdFunction(),
      TimerOptions(DurationMs(5000), TimerBackoffAlgorithm::kFixed,
                   /*max_restart=*/2));

  EXPECT_CALL(on_expired_, Call).Times(0);
  t1->Start();
  AdvanceTimeAndRunTimers(DurationMs(4000));

  // Fire first time
  EXPECT_CALL(on_expired_, Call).Times(1);
  AdvanceTimeAndRunTimers(DurationMs(1000));
  EXPECT_TRUE(t1->is_running());

  EXPECT_CALL(on_expired_, Call).Times(0);
  AdvanceTimeAndRunTimers(DurationMs(4000));

  // Second time
  EXPECT_CALL(on_expired_, Call).Times(1);
  AdvanceTimeAndRunTimers(DurationMs(1000));
  EXPECT_TRUE(t1->is_running());

  EXPECT_CALL(on_expired_, Call).Times(0);
  AdvanceTimeAndRunTimers(DurationMs(4000));

  // Third time
  EXPECT_CALL(on_expired_, Call).Times(1);
  AdvanceTimeAndRunTimers(DurationMs(1000));
  EXPECT_FALSE(t1->is_running());
}

TEST_F(TimerTest, TimerWithExponentialBackoff) {
  std::unique_ptr<Timer> t1 = manager_.CreateTimer(
      "t1", on_expired_.AsStdFunction(),
      TimerOptions(DurationMs(5000), TimerBackoffAlgorithm::kExponential));

  t1->Start();

  // Fire first time at 5 seconds
  EXPECT_CALL(on_expired_, Call).Times(1);
  AdvanceTimeAndRunTimers(DurationMs(5000));

  // Second time at 5*2^1 = 10 seconds later.
  EXPECT_CALL(on_expired_, Call).Times(0);
  AdvanceTimeAndRunTimers(DurationMs(9000));
  EXPECT_CALL(on_expired_, Call).Times(1);
  AdvanceTimeAndRunTimers(DurationMs(1000));

  // Third time at 5*2^2 = 20 seconds later.
  EXPECT_CALL(on_expired_, Call).Times(0);
  AdvanceTimeAndRunTimers(DurationMs(19000));
  EXPECT_CALL(on_expired_, Call).Times(1);
  AdvanceTimeAndRunTimers(DurationMs(1000));

  // Fourth time at 5*2^3 = 40 seconds later.
  EXPECT_CALL(on_expired_, Call).Times(0);
  AdvanceTimeAndRunTimers(DurationMs(39000));
  EXPECT_CALL(on_expired_, Call).Times(1);
  AdvanceTimeAndRunTimers(DurationMs(1000));
}

TEST_F(TimerTest, StartTimerWillStopAndStart) {
  std::unique_ptr<Timer> t1 = manager_.CreateTimer(
      "t1", on_expired_.AsStdFunction(),
      TimerOptions(DurationMs(5000), TimerBackoffAlgorithm::kExponential));

  t1->Start();

  AdvanceTimeAndRunTimers(DurationMs(3000));

  t1->Start();

  EXPECT_CALL(on_expired_, Call).Times(0);
  AdvanceTimeAndRunTimers(DurationMs(2000));

  EXPECT_CALL(on_expired_, Call).Times(1);
  AdvanceTimeAndRunTimers(DurationMs(3000));
}

TEST_F(TimerTest, ExpirationCounterWillResetIfStopped) {
  std::unique_ptr<Timer> t1 = manager_.CreateTimer(
      "t1", on_expired_.AsStdFunction(),
      TimerOptions(DurationMs(5000), TimerBackoffAlgorithm::kExponential));

  t1->Start();

  // Fire first time at 5 seconds
  EXPECT_CALL(on_expired_, Call).Times(1);
  AdvanceTimeAndRunTimers(DurationMs(5000));
  EXPECT_EQ(t1->expiration_count(), 1);

  // Second time at 5*2^1 = 10 seconds later.
  EXPECT_CALL(on_expired_, Call).Times(0);
  AdvanceTimeAndRunTimers(DurationMs(9000));
  EXPECT_CALL(on_expired_, Call).Times(1);
  AdvanceTimeAndRunTimers(DurationMs(1000));
  EXPECT_EQ(t1->expiration_count(), 2);

  t1->Start();
  EXPECT_EQ(t1->expiration_count(), 0);

  // Third time at 5*2^0 = 5 seconds later.
  EXPECT_CALL(on_expired_, Call).Times(0);
  AdvanceTimeAndRunTimers(DurationMs(4000));
  EXPECT_CALL(on_expired_, Call).Times(1);
  AdvanceTimeAndRunTimers(DurationMs(1000));
  EXPECT_EQ(t1->expiration_count(), 1);
}

TEST_F(TimerTest, StopTimerWillMakeItNotExpire) {
  std::unique_ptr<Timer> t1 = manager_.CreateTimer(
      "t1", on_expired_.AsStdFunction(),
      TimerOptions(DurationMs(5000), TimerBackoffAlgorithm::kExponential));

  t1->Start();
  EXPECT_TRUE(t1->is_running());

  EXPECT_CALL(on_expired_, Call).Times(0);
  AdvanceTimeAndRunTimers(DurationMs(4000));
  t1->Stop();
  EXPECT_FALSE(t1->is_running());

  EXPECT_CALL(on_expired_, Call).Times(0);
  AdvanceTimeAndRunTimers(DurationMs(1000));
}

TEST_F(TimerTest, ReturningNewDurationWhenExpired) {
  std::unique_ptr<Timer> t1 = manager_.CreateTimer(
      "t1", on_expired_.AsStdFunction(),
      TimerOptions(DurationMs(5000), TimerBackoffAlgorithm::kFixed));

  EXPECT_CALL(on_expired_, Call).Times(0);
  t1->Start();
  EXPECT_EQ(t1->duration(), DurationMs(5000));

  AdvanceTimeAndRunTimers(DurationMs(4000));

  // Fire first time
  EXPECT_CALL(on_expired_, Call).WillOnce(Return(DurationMs(2000)));
  AdvanceTimeAndRunTimers(DurationMs(1000));
  EXPECT_EQ(t1->duration(), DurationMs(2000));

  EXPECT_CALL(on_expired_, Call).Times(0);
  AdvanceTimeAndRunTimers(DurationMs(1000));

  // Second time
  EXPECT_CALL(on_expired_, Call).WillOnce(Return(DurationMs(10000)));
  AdvanceTimeAndRunTimers(DurationMs(1000));
  EXPECT_EQ(t1->duration(), DurationMs(10000));

  EXPECT_CALL(on_expired_, Call).Times(0);
  AdvanceTimeAndRunTimers(DurationMs(9000));
  EXPECT_CALL(on_expired_, Call).Times(1);
  AdvanceTimeAndRunTimers(DurationMs(1000));
}

}  // namespace
}  // namespace dcsctp
