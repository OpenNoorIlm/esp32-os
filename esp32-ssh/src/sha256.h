#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Self-contained SHA-256 (FIPS 180-4) implementation, plus HMAC-SHA256
// (RFC 2104). No external crypto dependency needed for the client.
class SHA256 {
public:
  SHA256();
  void update(const uint8_t* data, size_t len);
  void update(const std::string& data);
  std::vector<uint8_t> digest(); // 32 raw bytes; consumes the object

  static std::string hex(const std::vector<uint8_t>& bytes);
  static std::vector<uint8_t> hash(const std::string& data);
  static std::string hashHex(const std::string& data);

private:
  void transform(const uint8_t* chunk);
  uint32_t state_[8];
  uint64_t bitlen_;
  uint8_t buffer_[64];
  size_t bufferLen_;
};

std::vector<uint8_t> hmacSha256(const std::vector<uint8_t>& key, const std::string& msg);
