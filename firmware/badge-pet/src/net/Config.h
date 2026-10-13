#pragma once
// The brain's address is not a secret; the device token is (stored in NVS, set over serial).
namespace net {
constexpr const char* BRAIN_HOST = "charm-brain.REPLACE_SUBDOMAIN.workers.dev";   // Task 12 Step 1 fills this in from brain/DEPLOYED_URL
constexpr const char* FW_VERSION = "2.0.0";
}
