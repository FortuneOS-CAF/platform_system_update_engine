//
// Copyright (C) 2010 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "update_engine/payload_generator/payload_signer.h"

#include <string>
#include <vector>

#include <base/logging.h>
#include <gtest/gtest.h>

#include "update_engine/common/hash_calculator.h"
#include "update_engine/common/test_utils.h"
#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/payload_constants.h"
#include "update_engine/payload_consumer/payload_verifier.h"
#include "update_engine/payload_generator/payload_file.h"
#include "update_engine/update_metadata.pb.h"

using chromeos_update_engine::test_utils::GetBuildArtifactsPath;
using std::string;
using std::vector;

// Note: the test key was generated with the following command:
// openssl genrsa -out unittest_key.pem 2048
// The public-key version is created by the build system.

namespace chromeos_update_engine {

const char* kUnittestPrivateKeyPath = "unittest_key.pem";
const char* kUnittestPublicKeyPath = "unittest_key.pub.pem";
const char* kUnittestPrivateKey2Path = "unittest_key2.pem";
const char* kUnittestPublicKey2Path = "unittest_key2.pub.pem";

// Some data and its corresponding hash and signature:
const char kDataToSign[] = "This is some data to sign.";

// Generated by:
// echo -n 'This is some data to sign.' | openssl dgst -sha256 -binary |
//   hexdump -v -e '" " 8/1 "0x%02x, " "\n"'
const uint8_t kDataHash[] = {
  0x7a, 0x07, 0xa6, 0x44, 0x08, 0x86, 0x20, 0xa6,
  0xc1, 0xf8, 0xd9, 0x02, 0x05, 0x63, 0x0d, 0xb7,
  0xfc, 0x2b, 0xa0, 0xa9, 0x7c, 0x9d, 0x1d, 0x8c,
  0x01, 0xf5, 0x78, 0x6d, 0xc5, 0x11, 0xb4, 0x06
};

// Generated with openssl 1.0, which at the time of this writing, you need
// to download and install yourself. Here's my command:
// echo -n 'This is some data to sign.' | openssl dgst -sha256 -binary |
//    ~/local/bin/openssl pkeyutl -sign -inkey unittest_key.pem -pkeyopt
//    digest:sha256 | hexdump -v -e '" " 8/1 "0x%02x, " "\n"'
const uint8_t kDataSignature[] = {
  0x9f, 0x86, 0x25, 0x8b, 0xf3, 0xcc, 0xe3, 0x95,
  0x5f, 0x45, 0x83, 0xb2, 0x66, 0xf0, 0x2a, 0xcf,
  0xb7, 0xaa, 0x52, 0x25, 0x7a, 0xdd, 0x9d, 0x65,
  0xe5, 0xd6, 0x02, 0x4b, 0x37, 0x99, 0x53, 0x06,
  0xc2, 0xc9, 0x37, 0x36, 0x25, 0x62, 0x09, 0x4f,
  0x6b, 0x22, 0xf8, 0xb3, 0x89, 0x14, 0x98, 0x1a,
  0xbc, 0x30, 0x90, 0x4a, 0x43, 0xf5, 0xea, 0x2e,
  0xf0, 0xa4, 0xba, 0xc3, 0xa7, 0xa3, 0x44, 0x70,
  0xd6, 0xc4, 0x89, 0xd8, 0x45, 0x71, 0xbb, 0xee,
  0x59, 0x87, 0x3d, 0xd5, 0xe5, 0x40, 0x22, 0x3d,
  0x73, 0x7e, 0x2a, 0x58, 0x93, 0x8e, 0xcb, 0x9c,
  0xf2, 0xbb, 0x4a, 0xc9, 0xd2, 0x2c, 0x52, 0x42,
  0xb0, 0xd1, 0x13, 0x22, 0xa4, 0x78, 0xc7, 0xc6,
  0x3e, 0xf1, 0xdc, 0x4c, 0x7b, 0x2d, 0x40, 0xda,
  0x58, 0xac, 0x4a, 0x11, 0x96, 0x3d, 0xa0, 0x01,
  0xf6, 0x96, 0x74, 0xf6, 0x6c, 0x0c, 0x49, 0x69,
  0x4e, 0xc1, 0x7e, 0x9f, 0x2a, 0x42, 0xdd, 0x15,
  0x6b, 0x37, 0x2e, 0x3a, 0xa7, 0xa7, 0x6d, 0x91,
  0x13, 0xe8, 0x59, 0xde, 0xfe, 0x99, 0x07, 0xd9,
  0x34, 0x0f, 0x17, 0xb3, 0x05, 0x4c, 0xd2, 0xc6,
  0x82, 0xb7, 0x38, 0x36, 0x63, 0x1d, 0x9e, 0x21,
  0xa6, 0x32, 0xef, 0xf1, 0x65, 0xe6, 0xed, 0x95,
  0x25, 0x9b, 0x61, 0xe0, 0xba, 0x86, 0xa1, 0x7f,
  0xf8, 0xa5, 0x4a, 0x32, 0x1f, 0x15, 0x20, 0x8a,
  0x41, 0xc5, 0xb0, 0xd9, 0x4a, 0xda, 0x85, 0xf3,
  0xdc, 0xa0, 0x98, 0x5d, 0x1d, 0x18, 0x9d, 0x2e,
  0x42, 0xea, 0x69, 0x13, 0x74, 0x3c, 0x74, 0xf7,
  0x6d, 0x43, 0xb0, 0x63, 0x90, 0xdb, 0x04, 0xd5,
  0x05, 0xc9, 0x73, 0x1f, 0x6c, 0xd6, 0xfa, 0x46,
  0x4e, 0x0f, 0x33, 0x58, 0x5b, 0x0d, 0x1b, 0x55,
  0x39, 0xb9, 0x0f, 0x43, 0x37, 0xc0, 0x06, 0x0c,
  0x29, 0x93, 0x43, 0xc7, 0x43, 0xb9, 0xab, 0x7d
};

namespace {
void SignSampleData(brillo::Blob* out_signature_blob,
                    const vector<string>& private_keys) {
  brillo::Blob data_blob(std::begin(kDataToSign),
                         std::begin(kDataToSign) + strlen(kDataToSign));
  uint64_t length = 0;
  EXPECT_TRUE(PayloadSigner::SignatureBlobLength(private_keys, &length));
  EXPECT_GT(length, 0U);
  brillo::Blob hash_blob;
  EXPECT_TRUE(HashCalculator::RawHashOfBytes(data_blob.data(),
                                             data_blob.size(),
                                             &hash_blob));
  EXPECT_TRUE(PayloadSigner::SignHashWithKeys(
      hash_blob,
      private_keys,
      out_signature_blob));
  EXPECT_EQ(length, out_signature_blob->size());
}
}  // namespace

class PayloadSignerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    PayloadVerifier::PadRSA2048SHA256Hash(&padded_hash_data_);
  }

  brillo::Blob padded_hash_data_{std::begin(kDataHash), std::end(kDataHash)};
};

TEST_F(PayloadSignerTest, SignSimpleTextTest) {
  brillo::Blob signature_blob;
  SignSampleData(&signature_blob,
                 {GetBuildArtifactsPath(kUnittestPrivateKeyPath)});

  // Check the signature itself
  Signatures signatures;
  EXPECT_TRUE(signatures.ParseFromArray(signature_blob.data(),
                                        signature_blob.size()));
  EXPECT_EQ(1, signatures.signatures_size());
  const Signatures_Signature& signature = signatures.signatures(0);
  EXPECT_EQ(1U, signature.version());
  const string& sig_data = signature.data();
  ASSERT_EQ(arraysize(kDataSignature), sig_data.size());
  for (size_t i = 0; i < arraysize(kDataSignature); i++) {
    EXPECT_EQ(kDataSignature[i], static_cast<uint8_t>(sig_data[i]));
  }
}

TEST_F(PayloadSignerTest, VerifyAllSignatureTest) {
  brillo::Blob signature_blob;
  SignSampleData(&signature_blob,
                 {GetBuildArtifactsPath(kUnittestPrivateKeyPath),
                  GetBuildArtifactsPath(kUnittestPrivateKey2Path)});

  // Either public key should pass the verification.
  EXPECT_TRUE(PayloadVerifier::VerifySignature(
      signature_blob,
      GetBuildArtifactsPath(kUnittestPublicKeyPath),
      padded_hash_data_));
  EXPECT_TRUE(PayloadVerifier::VerifySignature(
      signature_blob,
      GetBuildArtifactsPath(kUnittestPublicKey2Path),
      padded_hash_data_));
}

TEST_F(PayloadSignerTest, VerifySignatureTest) {
  brillo::Blob signature_blob;
  SignSampleData(&signature_blob,
                 {GetBuildArtifactsPath(kUnittestPrivateKeyPath)});

  EXPECT_TRUE(PayloadVerifier::VerifySignature(
      signature_blob,
      GetBuildArtifactsPath(kUnittestPublicKeyPath),
      padded_hash_data_));
  // Passing the invalid key should fail the verification.
  EXPECT_FALSE(PayloadVerifier::VerifySignature(
      signature_blob,
      GetBuildArtifactsPath(kUnittestPublicKey2Path),
      padded_hash_data_));
}

TEST_F(PayloadSignerTest, SkipMetadataSignatureTest) {
  test_utils::ScopedTempFile payload_file("payload.XXXXXX");
  PayloadGenerationConfig config;
  config.version.major = kBrilloMajorPayloadVersion;
  PayloadFile payload;
  EXPECT_TRUE(payload.Init(config));
  uint64_t metadata_size;
  EXPECT_TRUE(payload.WritePayload(
      payload_file.path(), "/dev/null", "", &metadata_size));
  const vector<int> sizes = {256};
  brillo::Blob unsigned_payload_hash, unsigned_metadata_hash;
  EXPECT_TRUE(PayloadSigner::HashPayloadForSigning(payload_file.path(),
                                                   sizes,
                                                   &unsigned_payload_hash,
                                                   &unsigned_metadata_hash));
  EXPECT_TRUE(
      payload.WritePayload(payload_file.path(),
                           "/dev/null",
                           GetBuildArtifactsPath(kUnittestPrivateKeyPath),
                           &metadata_size));
  brillo::Blob signed_payload_hash, signed_metadata_hash;
  EXPECT_TRUE(PayloadSigner::HashPayloadForSigning(
      payload_file.path(), sizes, &signed_payload_hash, &signed_metadata_hash));
  EXPECT_EQ(unsigned_payload_hash, signed_payload_hash);
  EXPECT_EQ(unsigned_metadata_hash, signed_metadata_hash);
}

TEST_F(PayloadSignerTest, VerifySignedPayloadTest) {
  test_utils::ScopedTempFile payload_file("payload.XXXXXX");
  PayloadGenerationConfig config;
  config.version.major = kBrilloMajorPayloadVersion;
  PayloadFile payload;
  EXPECT_TRUE(payload.Init(config));
  uint64_t metadata_size;
  EXPECT_TRUE(
      payload.WritePayload(payload_file.path(),
                           "/dev/null",
                           GetBuildArtifactsPath(kUnittestPrivateKeyPath),
                           &metadata_size));
  EXPECT_TRUE(PayloadSigner::VerifySignedPayload(
      payload_file.path(), GetBuildArtifactsPath(kUnittestPublicKeyPath)));
}

}  // namespace chromeos_update_engine
