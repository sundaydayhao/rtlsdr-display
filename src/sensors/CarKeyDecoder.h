#pragma once
#include <string>
#include <vector>
#include <cstdint>

/**
 * @brief 汽车钥匙/遥控器解码器
 *
 * 支持协议：
 *  - EV1527 / Learning Code (24-bit OOK, 最常见的学习码遥控)
 *  - PT2262 固定码
 *  - KeeLoq 滚动码 (OEM 车钥匙)
 *  - Generic OOK 通用解码
 */

enum class CarKeyButton {
    Unknown = 0,
    Lock,
    Unlock,
    Trunk,
    Panic,
    Start,
    Custom1,
    Custom2
};

struct CarKeyFrame {
    bool        valid        = false;
    std::string protocol;       // "EV1527", "PT2262", "KeeLoq", "Generic OOK"
    uint32_t    address      = 0;
    uint8_t     buttonCode   = 0;
    CarKeyButton button      = CarKeyButton::Unknown;
    std::string buttonName;
    bool        rollingCode  = false;
    uint32_t    encryptedWord = 0;  // KeeLoq only
    std::string rawHex;
};

class CarKeyDecoder
{
public:
    CarKeyDecoder() = default;

    // 从比特流解码（每个元素为 0 或 1）
    CarKeyFrame decode(const std::vector<uint8_t>& bits);

    // 从字节数组解码
    CarKeyFrame decodeBytes(const uint8_t* bytes, size_t len);

private:
    CarKeyFrame decodeEV1527(const uint8_t* bytes, size_t len);
    CarKeyFrame decodePT2262(const uint8_t* bytes, size_t len);
    CarKeyFrame decodeKeeLoq(const uint8_t* bytes, size_t len);
    CarKeyFrame decodeGenericPWM(const std::vector<uint8_t>& bits);

    static std::vector<uint8_t> bitsToBytes(const std::vector<uint8_t>& bits);
    static std::string toHex(const uint8_t* data, size_t len);
    static std::string buttonToName(CarKeyButton btn);
    static CarKeyButton codeToButton(uint8_t code);
};