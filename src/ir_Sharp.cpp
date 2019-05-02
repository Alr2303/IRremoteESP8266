// Copyright 2009 Ken Shirriff
// Copyright 2017, 2019 David Conran

#include "ir_Sharp.h"
#include <algorithm>
#ifndef ARDUINO
#include <string>
#endif
#include "IRrecv.h"
#include "IRsend.h"
#include "IRutils.h"

//                       SSSS  H   H   AAA   RRRR   PPPP
//                      S      H   H  A   A  R   R  P   P
//                       SSS   HHHHH  AAAAA  RRRR   PPPP
//                          S  H   H  A   A  R  R   P
//                      SSSS   H   H  A   A  R   R  P

// Equipment it seems compatible with:
//  * Sharp LC-52D62U
//  * <Add models (devices & remotes) you've gotten it working with here>
//

// Constants
// period time = 1/38000Hz = 26.316 microseconds.
// Ref:
//   GlobalCache's IR Control Tower data.
//   http://www.sbprojects.com/knowledge/ir/sharp.php
const uint16_t kSharpTick = 26;
const uint16_t kSharpBitMarkTicks = 10;
const uint16_t kSharpBitMark = kSharpBitMarkTicks * kSharpTick;
const uint16_t kSharpOneSpaceTicks = 70;
const uint16_t kSharpOneSpace = kSharpOneSpaceTicks * kSharpTick;
const uint16_t kSharpZeroSpaceTicks = 30;
const uint16_t kSharpZeroSpace = kSharpZeroSpaceTicks * kSharpTick;
const uint16_t kSharpGapTicks = 1677;
const uint16_t kSharpGap = kSharpGapTicks * kSharpTick;
// Address(5) + Command(8) + Expansion(1) + Check(1)
const uint64_t kSharpToggleMask =
    ((uint64_t)1 << (kSharpBits - kSharpAddressBits)) - 1;
const uint64_t kSharpAddressMask = ((uint64_t)1 << kSharpAddressBits) - 1;
const uint64_t kSharpCommandMask = ((uint64_t)1 << kSharpCommandBits) - 1;

#if (SEND_SHARP || SEND_DENON)
// Send a (raw) Sharp message
//
// Args:
//   data:   Contents of the message to be sent.
//   nbits:  Nr. of bits of data to be sent. Typically kSharpBits.
//   repeat: Nr. of additional times the message is to be sent.
//
// Status: BETA / Previously working fine.
//
// Notes:
//   This procedure handles the inversion of bits required per protocol.
//   The protocol spec says to send the LSB first, but legacy code & usage
//   has us sending the MSB first. Grrrr. Normal invocation of encodeSharp()
//   handles this for you, assuming you are using the correct/standard values.
//   e.g. sendSharpRaw(encodeSharp(address, command));
//
// Ref:
//   http://www.sbprojects.com/knowledge/ir/sharp.htm
//   http://lirc.sourceforge.net/remotes/sharp/GA538WJSA
//   http://www.mwftr.com/ucF08/LEC14%20PIC%20IR.pdf
//   http://www.hifi-remote.com/johnsfine/DecodeIR.html#Sharp
void IRsend::sendSharpRaw(uint64_t data, uint16_t nbits, uint16_t repeat) {
  for (uint16_t i = 0; i <= repeat; i++) {
    // Protocol demands that the data be sent twice; once normally,
    // then with all but the address bits inverted.
    // Note: Previously this used to be performed 3 times (normal, inverted,
    //       normal), however all data points to that being incorrect.
    for (uint8_t n = 0; n < 2; n++) {
      sendGeneric(0, 0,  // No Header
                  kSharpBitMark, kSharpOneSpace, kSharpBitMark, kSharpZeroSpace,
                  kSharpBitMark, kSharpGap, data, nbits, 38, true,
                  0,  // Repeats are handled already.
                  33);
      // Invert the data per protocol. This is always called twice, so it's
      // retured to original upon exiting the inner loop.
      data ^= kSharpToggleMask;
    }
  }
}

// Encode a (raw) Sharp message from it's components.
//
// Args:
//   address:   The value of the address to be sent.
//   command:   The value of the address to be sent. (8 bits)
//   expansion: The value of the expansion bit to use. (0 or 1, typically 1)
//   check:     The value of the check bit to use. (0 or 1, typically 0)
//   MSBfirst:  Flag indicating MSB first or LSB first order. (Default: false)
// Returns:
//   An uint32_t containing the raw Sharp message for sendSharpRaw().
//
// Status: BETA / Should work okay.
//
// Notes:
//   Assumes the standard Sharp bit sizes.
//   Historically sendSharp() sends address & command in
//     MSB first order. This is actually incorrect. It should be sent in LSB
//     order. The behaviour of sendSharp() hasn't been changed to maintain
//     backward compatibility.
//
// Ref:
//   http://www.sbprojects.com/knowledge/ir/sharp.htm
//   http://lirc.sourceforge.net/remotes/sharp/GA538WJSA
//   http://www.mwftr.com/ucF08/LEC14%20PIC%20IR.pdf
uint32_t IRsend::encodeSharp(uint16_t address, uint16_t command,
                             uint16_t expansion, uint16_t check,
                             bool MSBfirst) {
  // Mask any unexpected bits.
  address &= ((1 << kSharpAddressBits) - 1);
  command &= ((1 << kSharpCommandBits) - 1);
  expansion &= 1;
  check &= 1;

  if (!MSBfirst) {  // Correct bit order if needed.
    address = reverseBits(address, kSharpAddressBits);
    command = reverseBits(command, kSharpCommandBits);
  }
  // Concatinate all the bits.
  return (address << (kSharpCommandBits + 2)) | (command << 2) |
         (expansion << 1) | check;
}

// Send a Sharp message
//
// Args:
//   address:  Address value to be sent.
//   command:  Command value to be sent.
//   nbits:    Nr. of bits of data to be sent. Typically kSharpBits.
//   repeat:   Nr. of additional times the message is to be sent.
//
// Status:  DEPRICATED / Previously working fine.
//
// Notes:
//   This procedure has a non-standard invocation style compared to similar
//     sendProtocol() routines. This is due to legacy, compatibility, & historic
//     reasons. Normally the calling syntax version is like sendSharpRaw().
//   This procedure transmits the address & command in MSB first order, which is
//     incorrect. This behaviour is left as-is to maintain backward
//     compatibility with legacy code.
//   In short, you should use sendSharpRaw(), encodeSharp(), and the correct
//     values of address & command instead of using this, & the wrong values.
//
// Ref:
//   http://www.sbprojects.com/knowledge/ir/sharp.htm
//   http://lirc.sourceforge.net/remotes/sharp/GA538WJSA
//   http://www.mwftr.com/ucF08/LEC14%20PIC%20IR.pdf
void IRsend::sendSharp(uint16_t address, uint16_t command, uint16_t nbits,
                       uint16_t repeat) {
  sendSharpRaw(encodeSharp(address, command, 1, 0, true), nbits, repeat);
}
#endif  // (SEND_SHARP || SEND_DENON)

#if (DECODE_SHARP || DECODE_DENON)
// Decode the supplied Sharp message.
//
// Args:
//   results:   Ptr to the data to decode and where to store the decode result.
//   nbits:     Nr. of data bits to expect. Typically kSharpBits.
//   strict:    Flag indicating if we should perform strict matching.
//   expansion: Should we expect the expansion bit to be set. Default is true.
// Returns:
//   boolean: True if it can decode it, false if it can't.
//
// Status: STABLE / Working fine.
//
// Note:
//   This procedure returns a value suitable for use in sendSharpRaw().
// TODO(crankyoldgit): Need to ensure capture of the inverted message as it can
//   be missed due to the interrupt timeout used to detect an end of message.
//   Several compliance checks are disabled until that is resolved.
// Ref:
//   http://www.sbprojects.com/knowledge/ir/sharp.php
//   http://www.mwftr.com/ucF08/LEC14%20PIC%20IR.pdf
//   http://www.hifi-remote.com/johnsfine/DecodeIR.html#Sharp
bool IRrecv::decodeSharp(decode_results *results, uint16_t nbits, bool strict,
                         bool expansion) {
  if (results->rawlen < 2 * nbits + kFooter - 1)
    return false;  // Not enough entries to be a Sharp message.
  // Compliance
  if (strict) {
    if (nbits != kSharpBits) return false;  // Request is out of spec.
    // DISABLED - See TODO
#ifdef UNIT_TEST
    // An in spec message has the data sent normally, then inverted. So we
    // expect twice as many entries than to just get the results.
    if (results->rawlen < 2 * (2 * nbits + kFooter)) return false;
#endif
  }

  uint64_t data = 0;
  uint16_t offset = kStartOffset;

  // No header
  // But try to auto-calibrate off the initial mark signal.
  if (!matchMark(results->rawbuf[offset], kSharpBitMark, 35)) return false;
  // Calculate how long the common tick time is based on the header mark.
  uint32_t tick = results->rawbuf[offset] * kRawTick / kSharpBitMarkTicks;
  // Data
  for (uint16_t i = 0; i < nbits; i++, offset++) {
    // Use a higher tolerance value for kSharpBitMark as it is quite small.
    if (!matchMark(results->rawbuf[offset++], kSharpBitMarkTicks * tick, 35))
      return false;
    if (matchSpace(results->rawbuf[offset], kSharpOneSpaceTicks * tick))
      data = (data << 1) | 1;  // 1
    else if (matchSpace(results->rawbuf[offset], kSharpZeroSpaceTicks * tick))
      data <<= 1;  // 0
    else
      return false;
  }

  // Footer
  if (!match(results->rawbuf[offset++], kSharpBitMarkTicks * tick))
    return false;
  if (offset < results->rawlen &&
      !matchAtLeast(results->rawbuf[offset], kSharpGapTicks * tick))
    return false;

  // Compliance
  if (strict) {
    // Check the state of the expansion bit is what we expect.
    if ((data & 0b10) >> 1 != expansion) return false;
    // The check bit should be cleared in a normal message.
    if (data & 0b1) return false;
      // DISABLED - See TODO
#ifdef UNIT_TEST
    // Grab the second copy of the data (i.e. inverted)
    // Header
    // i.e. The inter-data/command repeat gap.
    if (!matchSpace(results->rawbuf[offset++], kSharpGapTicks * tick))
      return false;

    // Data
    uint64_t second_data = 0;
    for (uint16_t i = 0; i < nbits; i++, offset++) {
      // Use a higher tolerance value for kSharpBitMark as it is quite small.
      if (!matchMark(results->rawbuf[offset++], kSharpBitMarkTicks * tick, 35))
        return false;
      if (matchSpace(results->rawbuf[offset], kSharpOneSpaceTicks * tick))
        second_data = (second_data << 1) | 1;  // 1
      else if (matchSpace(results->rawbuf[offset], kSharpZeroSpaceTicks * tick))
        second_data <<= 1;  // 0
      else
        return false;
    }
    // Footer
    if (!match(results->rawbuf[offset++], kSharpBitMarkTicks * tick))
      return false;
    if (offset < results->rawlen &&
        !matchAtLeast(results->rawbuf[offset], kSharpGapTicks * tick))
      return false;

    // Check that second_data has been inverted correctly.
    if (data != (second_data ^ kSharpToggleMask)) return false;
#endif  // UNIT_TEST
  }

  // Success
  results->decode_type = SHARP;
  results->bits = nbits;
  results->value = data;
  // Address & command are actually transmitted in LSB first order.
  results->address = reverseBits(data, nbits) & kSharpAddressMask;
  results->command =
      reverseBits((data >> 2) & kSharpCommandMask, kSharpCommandBits);
  return true;
}
#endif  // (DECODE_SHARP || DECODE_DENON)

#if SEND_SHARP_AC
// Send a Sharp A/C message.
//
// Args:
//   data: An array of kSharpAcStateLength bytes containing the IR command.
//   nbytes: Nr. of bytes of data to send. i.e. length of `data`.
//   repeat: Nr. of times the message should be repeated.
//
// Status: Alpha / Untested.
//
// Ref:
//   https://github.com/markszabo/IRremoteESP8266/issues/638
//   https://github.com/ToniA/arduino-heatpumpir/blob/master/SharpHeatpumpIR.cpp
void IRsend::sendSharpAc(const unsigned char data[], const uint16_t nbytes,
                         const uint16_t repeat) {
  if (nbytes < kSharpAcStateLength)
    return;  // Not enough bytes to send a proper message.

  sendGeneric(kSharpAcHdrMark, kSharpAcHdrSpace,
              kSharpAcBitMark, kSharpAcOneSpace,
              kSharpAcBitMark, kSharpAcZeroSpace,
              kSharpAcBitMark, kSharpAcGap,
              data, nbytes, 38000, false, repeat, 50);
}
#endif  // SEND_SHARP_AC

IRSharpAc::IRSharpAc(uint16_t pin) : _irsend(pin) { stateReset(); }

void IRSharpAc::begin(void) { _irsend.begin(); }

#if SEND_SHARP_AC
void IRSharpAc::send(const uint16_t repeat) {
  this->checksum();
  _irsend.sendSharpAc(remote, kSharpAcStateLength, repeat);
}
#endif  // SEND_SHARP_AC

// Calculate the checksum for a given state.
// Args:
//   state:  The array to verify the checksums of.
//   length: The size of the state.
// Returns:
//   The 4 bit checksum.
uint8_t IRSharpAc::calcChecksum(uint8_t state[], const uint16_t length) {
  uint8_t xorsum = xorBytes(state, length - 1);
  xorsum ^= (state[length - 1] & 0xF);
  xorsum ^= xorsum >> 4;
  return xorsum & 0xF;
}

// Verify the checksums are valid for a given state.
// Args:
//   state:  The array to verify the checksums of.
//   length: The size of the state.
// Returns:
//   A boolean.
bool IRSharpAc::validChecksum(uint8_t state[], const uint16_t length) {
  return (state[length - 1] >> 4) == calcChecksum(state, length);
}

// Calculate and set the checksum values for the internal state.
void IRSharpAc::checksum(void) {
  remote[kSharpAcStateLength - 1] &= 0x0F;
  remote[kSharpAcStateLength - 1] |= calcChecksum(remote) << 4;
}

void IRSharpAc::stateReset(void) {
  static const uint8_t reset[kSharpAcStateLength] = {
      0xAA, 0x5A, 0xCF, 0x10, 0x00, 0x01, 0x00, 0x00, 0x08, 0x80, 0x00, 0xE0,
      0x01};
  for (uint8_t i = 0; i < kSharpAcStateLength; i++) remote[i] = reset[i];
}

uint8_t *IRSharpAc::getRaw(void) {
  this->checksum();  // Ensure correct settings before sending.
  return remote;
}

void IRSharpAc::setRaw(const uint8_t new_code[], const uint16_t length) {
  for (uint8_t i = 0; i < length && i < kSharpAcStateLength; i++)
    remote[i] = new_code[i];
}

void IRSharpAc::on(void) { remote[kSharpAcBytePower] |= kSharpAcBitPower; }

void IRSharpAc::off(void) { remote[kSharpAcBytePower] &= ~kSharpAcBitPower; }

void IRSharpAc::setPower(const bool on) {
  if (on)
    this->on();
  else
    this->off();
}

bool IRSharpAc::getPower(void) {
  return remote[kSharpAcBytePower] & kSharpAcBitPower;
}

// Set the temp in deg C
void IRSharpAc::setTemp(const uint8_t temp) {
  switch (this->getMode()) {
    // Auto & Dry don't allow temp changes and have a special temp.
    case kSharpAcAuto:
    case kSharpAcDry:
      remote[kSharpAcByteTemp] = 0;
      remote[kSharpAcByteManual] = 0;  // When in Dry/Auto this byte is 0.
      return;
    default:
      remote[kSharpAcByteTemp] = 0xC0;
      remote[kSharpAcByteManual] |= kSharpAcBitTempManual;
  }
  uint8_t degrees = std::max(temp, kSharpAcMinTemp);
  degrees = std::min(degrees, kSharpAcMaxTemp);
  remote[kSharpAcByteTemp] &= ~kSharpAcMaskTemp;
  remote[kSharpAcByteTemp] |= (degrees - kSharpAcMinTemp);
}

uint8_t IRSharpAc::getTemp(void) {
  return (remote[kSharpAcByteTemp] & kSharpAcMaskTemp) + kSharpAcMinTemp;
}

uint8_t IRSharpAc::getMode(void) {
  return remote[kSharpAcByteMode] & kSharpAcMaskMode;
}

void IRSharpAc::setMode(const uint8_t mode) {
  const uint8_t special = 0x20;  // Non-auto modes have this bit set.
  remote[kSharpAcBytePower] |= special;
  switch (mode) {
    case kSharpAcAuto:
      remote[kSharpAcBytePower] &= ~special;  // Auto has this bit cleared.
      // FALLTHRU
    case kSharpAcDry:
      this->setTemp(0);  // Dry/Auto have no temp setting.
      // FALLTHRU
    case kSharpAcCool:
    case kSharpAcHeat:
      remote[kSharpAcByteMode] &= ~kSharpAcMaskMode;
      remote[kSharpAcByteMode] |= mode;

      break;
    default:
      this->setMode(kSharpAcAuto);
  }
}

// Set the speed of the fan
void IRSharpAc::setFan(const uint8_t speed) {
  remote[kSharpAcByteManual] |= kSharpAcBitFanManual;  // Manual fan mode.
  switch (speed) {
    case kSharpAcFanAuto:
      // Clear the manual fan bit.
      remote[kSharpAcByteManual] &= ~kSharpAcBitFanManual;
      // FALLTHRU
    case kSharpAcFanMin:
    case kSharpAcFanMed:
    case kSharpAcFanHigh:
    case kSharpAcFanMax:
      remote[kSharpAcByteFan] &= ~kSharpAcMaskFan;
      remote[kSharpAcByteFan] |= (speed << 4);
      break;
    default:
      this->setFan(kSharpAcFanAuto);
  }
}

uint8_t IRSharpAc::getFan(void) {
  return (remote[kSharpAcByteFan] & kSharpAcMaskFan) >> 4;
}

// Convert a standard A/C mode into its native mode.
uint8_t IRSharpAc::convertMode(const stdAc::opmode_t mode) {
  switch (mode) {
    case stdAc::opmode_t::kCool:
      return kSharpAcCool;
    case stdAc::opmode_t::kHeat:
      return kSharpAcHeat;
    case stdAc::opmode_t::kDry:
      return kSharpAcDry;
    // No Fan mode.
    default:
      return kSharpAcAuto;
  }
}

// Convert a standard A/C Fan speed into its native fan speed.
uint8_t IRSharpAc::convertFan(const stdAc::fanspeed_t speed) {
  switch (speed) {
    case stdAc::fanspeed_t::kMin:
    case stdAc::fanspeed_t::kLow:
      return kSharpAcFanMin;
    case stdAc::fanspeed_t::kMedium:
      return kSharpAcFanMed;
    case stdAc::fanspeed_t::kHigh:
      return kSharpAcFanHigh;
    case stdAc::fanspeed_t::kMax:
      return kSharpAcFanMax;
    default:
      return kSharpAcFanAuto;
  }
}

// Convert the internal state into a human readable string.
#ifdef ARDUINO
String IRSharpAc::toString(void) {
  String result = "";
#else   // ARDUINO
std::string IRSharpAc::toString(void) {
  std::string result = "";
#endif  // ARDUINO
  result.reserve(60);  // Reserve some heap for the string to reduce fragging.
  result += F("Power: ");
  result += this->getPower() ? F("On") : F("Off");
  result += F(", Mode: ");
  result += uint64ToString(this->getMode());
  switch (this->getMode()) {
    case kSharpAcAuto:
      result += F(" (AUTO)");
      break;
    case kSharpAcCool:
      result += F(" (COOL)");
      break;
    case kSharpAcHeat:
      result += F(" (HEAT)");
      break;
    case kSharpAcDry:
      result += F(" (DRY)");
      break;
    default:
      result += F(" (UNKNOWN)");
  }
  result += F(", Temp: ");
  result += uint64ToString(this->getTemp());
  result += F("C, Fan: ");
  result += uint64ToString(this->getFan());
  switch (this->getFan()) {
    case kSharpAcFanAuto:
      result += F(" (AUTO)");
      break;
    case kSharpAcFanMin:
      result += F(" (MIN)");
      break;
    case kSharpAcFanMed:
      result += F(" (MED)");
      break;
    case kSharpAcFanHigh:
      result += F(" (HIGH)");
      break;
    case kSharpAcFanMax:
      result += F(" (MAX)");
      break;
  }
  return result;
}

#if DECODE_SHARP_AC
// Decode the supplied Sharp A/C message.
// Args:
//   results: Ptr to the data to decode and where to store the decode result.
//   nbits:   Nr. of bits to expect in the data portion. (kSharpAcBits)
//   strict:  Flag to indicate if we strictly adhere to the specification.
// Returns:
//   boolean: True if it can decode it, false if it can't.
//
// Status: BETA / Should be working.
//
// Ref:
//   https://github.com/markszabo/IRremoteESP8266/issues/638
//   https://github.com/ToniA/arduino-heatpumpir/blob/master/SharpHeatpumpIR.cpp
bool IRrecv::decodeSharpAc(decode_results *results, const uint16_t nbits,
                           const bool strict) {
  // Is there enough data to match successfully?
  DPRINTLN(2 * nbits + kHeader + kFooter - 1);
  if (results->rawlen < 2 * nbits + kHeader + kFooter - 1)
    return false;

  // Compliance
  if (strict && nbits != kSharpAcBits) return false;

  uint16_t offset = kStartOffset;
  match_result_t data_result;
  uint16_t dataBitsSoFar = 0;
  // Header
  if (!matchMark(results->rawbuf[offset++], kSharpAcHdrMark)) return false;
  if (!matchSpace(results->rawbuf[offset++], kSharpAcHdrSpace)) return false;

  // Data
  // Keep reading bytes until we either run out of state to fill.
  for (uint16_t i = 0; offset <= results->rawlen - 16 && i < nbits;
       i++, dataBitsSoFar += 8, offset += data_result.used) {
    // Read in a byte at a time.
    data_result =
        matchData(&(results->rawbuf[offset]), 8,
                  kSharpAcBitMark, kSharpAcOneSpace,
                  kSharpAcBitMark, kSharpAcZeroSpace,
                  kTolerance, kMarkExcess, false);
    if (data_result.success == false) break;  // Fail
    results->state[i] = (uint8_t)data_result.data;
  }

  // Footer
  if (!matchMark(results->rawbuf[offset++], kSharpAcBitMark)) return false;
  if (offset < results->rawlen &&
      !matchAtLeast(results->rawbuf[offset], kSharpAcGap))
    return false;

  // Compliance
  if (strict) {
    // Re-check we got the correct size/length due to the way we read the data.
    if (dataBitsSoFar != kSharpAcBits) return false;
    if (!IRSharpAc::validChecksum(results->state)) return false;
  }

  // Success
  results->decode_type = SHARP_AC;
  results->bits = dataBitsSoFar;
  // No need to record the state as we stored it as we decoded it.
  // As we use result->state, we don't record value, address, or command as it
  // is a union data type.
  return true;
}
#endif  // DECODE_SHARP_AC
