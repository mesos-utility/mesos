// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gmock/gmock.h>

#include <mesos/authentication/http/basic_authenticator_factory.hpp>

#include <process/future.hpp>
#include <process/gtest.hpp>
#include <process/http.hpp>
#include <process/pid.hpp>
#include <process/process.hpp>

#include "logging/logging.hpp"

#include "tests/mesos.hpp"

namespace authentication = process::http::authentication;

using mesos::http::authentication::BasicAuthenticatorFactory;

using process::http::BadRequest;
using process::http::OK;
using process::http::Response;
using process::http::Unauthorized;

namespace mesos {
namespace internal {
namespace tests {

class LoggingTest : public mesos::internal::tests::MesosTest
{
protected:
  void setBasicHttpAuthenticator(
      const std::string& realm,
      const Credentials& credentials)
  {
    Try<authentication::Authenticator*> authenticator =
      BasicAuthenticatorFactory::create(realm, credentials);

    ASSERT_SOME(authenticator);

    // Add this realm to the set of realms which will be unset during teardown.
    realms.insert(realm);

    // Pass ownership of the authenticator to libprocess.
    AWAIT_READY(authentication::setAuthenticator(
        realm,
        process::Owned<authentication::Authenticator>(authenticator.get())));
  }

  virtual void TearDown()
  {
    foreach (const std::string& realm, realms) {
      // We need to wait in order to ensure that the operation completes before
      // we leave `TearDown`. Otherwise, we may leak a mock object.
      AWAIT_READY(authentication::unsetAuthenticator(realm));
    }

    realms.clear();

    MesosTest::TearDown();
  }

private:
  hashset<std::string> realms;
};


TEST_F(LoggingTest, Toggle)
{
  process::PID<> pid;
  pid.id = "logging";
  pid.address = process::address();

  process::Future<Response> response = process::http::get(pid, "toggle");

  AWAIT_EXPECT_RESPONSE_STATUS_EQ(OK().status, response);

  response = process::http::get(pid, "toggle", "level=0");

  AWAIT_EXPECT_RESPONSE_STATUS_EQ(BadRequest().status, response);
  AWAIT_EXPECT_RESPONSE_BODY_EQ(
      "Expecting 'duration=value' in query.\n",
      response);

  response = process::http::get(pid, "toggle", "duration=10secs");

  AWAIT_EXPECT_RESPONSE_STATUS_EQ(BadRequest().status, response);
  AWAIT_EXPECT_RESPONSE_BODY_EQ(
      "Expecting 'level=value' in query.\n",
      response);

  response = process::http::get(pid, "toggle", "duration=10secs");

  AWAIT_EXPECT_RESPONSE_STATUS_EQ(BadRequest().status, response);
  AWAIT_EXPECT_RESPONSE_BODY_EQ(
      "Expecting 'level=value' in query.\n",
      response);

  response = process::http::get(pid, "toggle", "level=-1&duration=10secs");

  AWAIT_EXPECT_RESPONSE_STATUS_EQ(BadRequest().status, response);
  AWAIT_EXPECT_RESPONSE_BODY_EQ(
      "Invalid level '-1'.\n",
      response);

  response = process::http::get(pid, "toggle", "level=-1&duration=10secs");

  AWAIT_EXPECT_RESPONSE_STATUS_EQ(BadRequest().status, response);
  AWAIT_EXPECT_RESPONSE_BODY_EQ(
      "Invalid level '-1'.\n",
      response);
}


// Tests that the `/logging/toggle` endpoint rejects unauthenticated requests
// when HTTP authentication is enabled.
TEST_F(LoggingTest, ToggleAuthenticationEnabled)
{
  Credentials credentials;
  credentials.add_credentials()->CopyFrom(DEFAULT_CREDENTIAL);

  // Create a basic HTTP authenticator with the specified credentials and set it
  // as the authenticator for `DEFAULT_HTTP_AUTHENTICATION_REALM`.
  setBasicHttpAuthenticator(DEFAULT_HTTP_AUTHENTICATION_REALM, credentials);

  process::PID<> pid;
  pid.id = "logging";
  pid.address = process::address();

  process::Future<Response> response = process::http::get(pid, "toggle");

  AWAIT_EXPECT_RESPONSE_STATUS_EQ(Unauthorized({}).status, response);
}

} // namespace tests {
} // namespace internal {
} // namespace mesos {
